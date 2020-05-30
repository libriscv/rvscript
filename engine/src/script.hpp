#pragma once
#include <cassert>
#include <functional>
#include <libriscv/machine.hpp>

class Script {
public:
	static constexpr uint64_t MAX_INSTRUCTIONS = 800'000;
	static constexpr uint32_t READONLY_AREA   = 0x20000;
	static constexpr uint32_t HIDDEN_AREA     = 0x10000;

	// call any script function, with any parameters
	template <typename... Args>
	inline long call(const std::string& name, Args&&...);

	template <typename... Args>
	inline long call(uint32_t addr, Args&&...);

	template <typename... Args>
	inline long preempt(uint32_t addr, Args&&...);

	// run for a bit, then stop
	inline void resume(uint64_t instruction_count);

	void set_tick_event(uint32_t addr, int reason) {
		this->m_tick_event = addr;
		this->m_tick_block_reason = reason;
	}
	void each_tick_event();

	auto& machine() { return *m_machine; }
	const auto& machine() const { return *m_machine; }

	const auto& name() const noexcept { return m_name; }
	uint32_t    hash() const noexcept { return m_hash; }
	bool crashed() const noexcept { return m_crashed; }

	void  add_shared_memory();
	bool  reset(bool shared); // true if the reset was successful

	void hash_public_api_symbols(const std::string& file);
	std::string symbol_name(uint32_t address) const;
	uint32_t resolve_address(const std::string& name) const;
	uint32_t api_function_from_hash(uint32_t);

	void print_backtrace(const uint32_t addr);
	long measure(uint32_t address);

	static Script& current_script() {
	    assert(g_current_script != nullptr && "Access nullptr");
	    return *g_current_script;
	}

	static auto&    shared_memory_page() noexcept { return g_shared_area; }
	static size_t   shared_memory_size() noexcept { return g_shared_area.size(); };
	static uint32_t shared_memory_location() noexcept { return 0x2000; };
	static auto&    hidden_area() noexcept { return g_hidden_stack; }

	Script(std::shared_ptr<std::vector<uint8_t>>& binary, const std::string& name);
	~Script();
	static void init();

private:
	void handle_exception(uint32_t);
	void handle_timeout(uint32_t);
	bool install_binary(const std::string& file, bool shared = true);
	bool machine_initialize(bool shared);
	void machine_setup(riscv::Machine<riscv::RISCV32>&);
	void setup_syscall_interface(riscv::Machine<riscv::RISCV32>&);
	static Script* g_current_script;
	static std::array<riscv::Page, 2> g_shared_area; // shared memory area
	static riscv::Page g_hidden_stack; // page used by the internal APIs

	std::unique_ptr<riscv::Machine<riscv::RISCV32>> m_machine = nullptr;
	std::shared_ptr<std::vector<uint8_t>> m_binary;
	void*       m_threads;
	uint32_t    m_tick_event = 0;
	int         m_tick_block_reason = 0;
	std::string m_name;
	uint32_t    m_hash;
	bool        m_crashed = false;
	int         m_budget_overruns = 0;
	// hash to public API direct function map
	eastl::unordered_map<uint32_t, uint32_t> m_public_api;
};

template <typename... Args>
inline long Script::call(uint32_t address, Args&&... args)
{
	const auto oldval = Script::g_current_script;
	Script::g_current_script = this; // set current context
	try {
		const long ret = machine().vmcall<MAX_INSTRUCTIONS>(
			address, std::forward<Args>(args)...);
		Script::g_current_script = oldval;
		return ret;
	}
	catch (const riscv::MachineTimeoutException& e) {
		this->handle_timeout(address);
	}
	catch (const std::exception& e) {
		this->handle_exception(address);
	}
	Script::g_current_script = oldval;
	return -1;
}
template <typename... Args>
inline long Script::call(const std::string& func, Args&&... args)
{
	const auto address = machine().address_of(func.c_str());
	if (UNLIKELY(address == 0)) {
		fprintf(stderr, "Script::call could not find: %s!\n", func.c_str());
		return -1;
	}
	return call(address, std::forward<Args>(args)...);
}

template <typename... Args>
inline long Script::preempt(uint32_t address, Args&&... args)
{
	const auto oldval = Script::g_current_script;
	const auto regs = machine().cpu.registers();
	Script::g_current_script = this; // set current context
	try {
		const long ret = machine().preempt<MAX_INSTRUCTIONS, true, false>(
			address, std::forward<Args>(args)...);
		machine().cpu.registers() = regs;
		Script::g_current_script = oldval;
		return ret;
	}
	catch (const riscv::MachineTimeoutException& e) {
		this->handle_timeout(address);
	}
	catch (const std::exception& e) {
		this->handle_exception(address);
	}
	machine().cpu.registers() = regs;
	Script::g_current_script = oldval;
	return -1;
}

inline void Script::resume(uint64_t cycles)
{
	const auto oldval = Script::g_current_script;
	Script::g_current_script = this; // set current context
	try {
		machine().simulate<false>(cycles);
	}
	catch (const std::exception& e) {
		this->handle_exception(machine().cpu.pc());
		this->m_crashed = true;
	}
	Script::g_current_script = oldval;
}
