#pragma once
#include <cassert>
#include <functional>
#include <fmt/core.h>
#include <libriscv/machine.hpp>
#include <EASTL/unordered_map.h>

class Script {
public:
	static constexpr int MARCH = (RISCV_ARCH == 32) ? 4 : 8;
	using gaddr_t = riscv::address_type<MARCH>;
	using machine_t = riscv::Machine<MARCH>;
	using ghandler_t = std::function<void(Script&)>;
	static constexpr uint64_t MAX_INSTRUCTIONS = 16'000'000;
	static constexpr gaddr_t READONLY_AREA   = 0x20000;
	static constexpr gaddr_t HIDDEN_AREA     = 0x10000;

	// Call any script function, with any parameters
	template <typename... Args>
	inline long call(const std::string& name, Args&&...);

	template <typename... Args>
	inline long call(gaddr_t addr, Args&&...);

	template <typename... Args>
	inline long preempt(gaddr_t addr, Args&&...);

	// Run for a bit, then stop
	inline void resume(uint64_t instruction_count);

	void set_tick_event(gaddr_t addr, int reason) {
		this->m_tick_event = addr;
		this->m_tick_block_reason = reason;
	}
	void each_tick_event();

	// Install a callback function using a string name
	// Can be invoked from the guest using the same string name
	void set_dynamic_call(const std::string& name, ghandler_t);
	void set_dynamic_calls(std::vector<std::pair<std::string, ghandler_t>>);
	void reset_dynamic_call(const std::string& name, ghandler_t = nullptr);
	void dynamic_call(uint32_t hash);

	auto& machine() { return *m_machine; }
	const auto& machine() const { return *m_machine; }

	const auto& name() const noexcept { return m_name; }
	uint32_t    hash() const noexcept { return m_hash; }

	bool crashed() const noexcept { return m_crashed; }
	bool reset(); // true if the reset was successful
	void print_backtrace(const gaddr_t addr);
	void stdout_enable(bool e) noexcept { m_stdout = e; }
	bool stdout_enabled() const noexcept { return m_stdout; }
	long vmbench(gaddr_t address, size_t ntimes = 30);
	static long benchmark(std::function<void()>, size_t ntimes = 2000);

	void hash_public_api_symbols_file(const std::string& file);
	void hash_public_api_symbols(std::string_view lines);
	std::string symbol_name(gaddr_t address) const;
	gaddr_t resolve_address(const std::string& name) const;
	gaddr_t api_function_from_hash(uint32_t);

	static auto&   shared_memory_page() noexcept { return g_shared_area; }
	static size_t  shared_memory_size() noexcept { return g_shared_area.size(); };
	static gaddr_t shared_memory_location() noexcept { return 0x2000; };
	static auto&   hidden_area() noexcept { return g_hidden_stack; }

	void add_shared_memory();
	void enable_debugging();

	Script(const machine_t&, const std::string& name);
	~Script();

private:
	void handle_exception(gaddr_t);
	void handle_timeout(gaddr_t);
	bool install_binary(const std::string& file, bool shared = true);
	bool machine_initialize();
	void machine_setup();
	void setup_syscall_interface(machine_t&);
	static long finish_benchmark(std::vector<long>&);
	static std::array<riscv::Page, 2> g_shared_area; // shared memory area
	static riscv::Page g_hidden_stack; // page used by the internal APIs

	std::unique_ptr<machine_t> m_machine = nullptr;
	const machine_t& m_source_machine;
	void*       m_threads;
	gaddr_t     m_tick_event = 0;
	int         m_tick_block_reason = 0;
	std::string m_name;
	uint32_t    m_hash;
	bool        m_crashed = false;
	bool        m_stdout = true;
	int         m_budget_overruns = 0;
	// hash to public API direct function map
	eastl::unordered_map<uint32_t, gaddr_t> m_public_api;
	// map of functions that extend engine using string hashes
	eastl::unordered_map<uint32_t, ghandler_t> m_dynamic_functions;
};
static_assert(RISCV_ARCH == 32 || RISCV_ARCH == 64, "Architecture must be 32- or 64-bit");

template <typename... Args>
inline long Script::call(gaddr_t address, Args&&... args)
{
	try {
		return machine().vmcall<MAX_INSTRUCTIONS>(
			address, std::forward<Args>(args)...);
	}
	catch (const std::exception& e) {
		this->handle_exception(address);
	}
	return -1;
}
template <typename... Args>
inline long Script::call(const std::string& func, Args&&... args)
{
	const auto address = machine().address_of(func.c_str());
	if (UNLIKELY(address == 0)) {
		fmt::print(stderr,
			"Script::call(): Could not find: '{}' in '{}'\n",
			func, name());
		return -1;
	}
	return call(address, std::forward<Args>(args)...);
}

template <typename... Args>
inline long Script::preempt(gaddr_t address, Args&&... args)
{
	const auto regs = machine().cpu.registers();
	try {
		const long ret = machine().preempt<MAX_INSTRUCTIONS, true, false>(
			address, std::forward<Args>(args)...);
		machine().cpu.registers() = regs;
		return ret;
	}
	catch (const std::exception& e) {
		this->handle_exception(address);
	}
	machine().cpu.registers() = regs;
	return -1;
}

inline void Script::resume(uint64_t cycles)
{
	try {
		machine().simulate<false>(cycles);
	}
	catch (const std::exception& e) {
		this->handle_exception(machine().cpu.pc());
		this->m_crashed = true;
	}
}
