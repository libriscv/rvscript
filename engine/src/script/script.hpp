#pragma once
#include <any>
#include <functional>
#include <libriscv/machine.hpp>
#include <optional>
#include <unordered_set>
template <typename T> struct GuestObjects;

struct Script
{
	static constexpr int MARCH				   = RISCV_ARCH / 8;
	using gaddr_t							   = riscv::address_type<MARCH>;
	using machine_t							   = riscv::Machine<MARCH>;
	using ghandler_t						   = std::function<void(Script&)>;
	using exit_func_t                          = std::function<void(Script&)>;
	static constexpr gaddr_t MAX_MEMORY		   = 1024 * 1024 * 16ul;
	static constexpr gaddr_t MAX_HEAP		   = 1024 * 1024 * 256ul;
	static constexpr uint64_t MAX_INSTRUCTIONS = 8000000;

	// Call any script function, with any parameters
	template <typename... Args>
	inline long call(const std::string& name, Args&&...);

	template <typename... Args> inline long call(gaddr_t addr, Args&&...);

	template <typename... Args> inline long preempt(gaddr_t addr, Args&&...);

	// Run for a bit, then stop
	inline void resume(uint64_t instruction_count);

	template <typename T> T* userptr() noexcept
	{
		return (T*)m_userptr;
	}

	template <typename T> const T* userptr() const noexcept
	{
		return (const T*)m_userptr;
	}

	void set_tick_event(gaddr_t addr, int reason)
	{
		this->m_tick_event		= addr;
		this->m_tick_block_word = reason;
	}

	void each_tick_event();

	// Install a callback function using a string name
	// Can be invoked from the guest using the same string name
	static void set_dynamic_call(const std::string& def, ghandler_t);
	static void set_dynamic_call(std::string name, std::string def, ghandler_t);
	static void
		set_dynamic_calls(std::vector<std::tuple<std::string, std::string, ghandler_t>>);
	void dynamic_call_hash(uint32_t hash, gaddr_t strname);
	void dynamic_call_array(uint32_t idx);

	auto& dynargs()
	{
		return m_arguments;
	}

	auto& machine()
	{
		return *m_machine;
	}

	const auto& machine() const
	{
		return *m_machine;
	}

	const auto& name() const noexcept
	{
		return m_name;
	}

	uint32_t hash() const noexcept
	{
		return m_hash;
	}

	const auto& filename() const noexcept
	{
		return m_filename;
	}

	bool is_debug() const noexcept
	{
		return m_is_debug;
	}

	bool crashed() const noexcept
	{
		return m_crashed;
	}

	bool reset(); // true if the reset was successful
	void print(std::string_view text);
	void print_backtrace(const gaddr_t addr);

	void stdout_enable(bool e) noexcept
	{
		m_stdout = e;
	}

	bool stdout_enabled() const noexcept
	{
		return m_stdout;
	}

	long vmbench(gaddr_t address, size_t ntimes = 30);
	static long benchmark(std::function<void()>, size_t ntimes = 1000);

	std::string symbol_name(gaddr_t address) const;
	gaddr_t address_of(const std::string& name) const;

	void add_shared_memory();

	gaddr_t heap_area() const noexcept
	{
		return m_heap_area;
	}

	/// @brief Make a global setting available to all programs
	/// @param setting 
	/// @param value 
	static void set_global_setting(std::string_view setting, gaddr_t value);

	/// @brief Retrieve the value of a global setting
	/// @param setting 
	static std::optional<gaddr_t> get_global_setting(std::string_view setting);
	static std::optional<gaddr_t> get_global_setting(uint32_t hash);

	/// @brief Make it possible to access and make function calls to the given
	/// script from with another The access is two-way.
	/// @param remote The remote script
	void setup_remote_calls_to(Script& remote);

	/// @brief Make it possible make function calls to the given script from
	/// with another This access is one-way and only the remote can write back
	/// results, as if it has a higher privilege level
	/// @param remote The remote script
	void setup_strict_remote_calls_to(Script& remote);

	void add_allowed_remote_function(gaddr_t addr)
	{
		m_remote_access.insert(addr);
	}

	/* The guest heap is managed outside using system calls. */
	gaddr_t guest_alloc(gaddr_t bytes);
	gaddr_t guest_alloc_sequential(gaddr_t bytes);
	bool guest_free(gaddr_t addr);

	/* Allocate n uninitialized objects in the guest and return the
	   location. A managing object is returned, which will free all
	   objects on destruction. The object can be moved. */
	template <typename T> GuestObjects<T> guest_alloc(size_t n = 1);

	void
	gdb_remote_debugging(std::string message, bool one_up, uint16_t port = 0);

	static void on_exit(exit_func_t callback) { Script::m_exit = std::move(callback); }
	void exit() { Script::m_exit(*this); } // Called by Game::exit() from the script.

	static void setup_syscall_interface();
	bool initialize();
	Script(
		const machine_t&, void* userptr, const std::string& name,
		const std::string& filename, bool = false);
	~Script();

  private:
	void could_not_find(std::string_view);
	void handle_exception(gaddr_t);
	void handle_timeout(gaddr_t);
	bool install_binary(const std::string& file, bool shared = true);
	void machine_setup();
	void machine_remote_setup();
	void resolve_dynamic_calls();
	void dynamic_call_error(uint32_t idx, const std::exception& e);
	static long finish_benchmark(std::vector<long>&);

	std::unique_ptr<machine_t> m_machine = nullptr;
	const machine_t& m_source_machine;
	void* m_userptr;
	gaddr_t m_heap_area		   = 0;
	gaddr_t m_tick_event	   = 0;
	uint32_t m_tick_block_word = 0;
	std::string m_name;
	std::string m_filename;
	uint32_t m_hash;
	bool m_is_debug			= false;
	bool m_crashed			= false;
	bool m_stdout			= true;
	bool m_last_newline		= true;
	int m_budget_overruns	= 0;
	Script* m_remote_script = nullptr;
	/// @brief Functions accessible when remote access is *strict*
	std::unordered_set<gaddr_t> m_remote_access;
	// dynamic call arguments
	std::vector<std::any> m_arguments;
	// dynamic call array
	struct DyncallDesc {
		uint32_t hash;
		uint32_t resv;
		uint32_t strname;
	};
	std::vector<ghandler_t> m_dyncall_array;
	gaddr_t m_g_dyncall_table = 0x0;
	// Map of functions that extend engine using string hashes
	// The host-side implementation:
	struct HostDyncall {
		std::string name;
		std::string definition;
		ghandler_t  func;
	};
	static inline std::map<uint32_t, HostDyncall> m_dynamic_functions;
	// map of globally accessible run-time settings
	static inline std::unordered_map<uint32_t, gaddr_t> m_runtime_settings;
	static inline exit_func_t m_exit = nullptr;
};

static_assert(
	RISCV_ARCH == 32 || RISCV_ARCH == 64,
	"Architecture must be 32- or 64-bit");

template <typename... Args>
inline long Script::call(gaddr_t address, Args&&... args)
{
	try
	{
		return machine().vmcall<MAX_INSTRUCTIONS>(
			address, std::forward<Args>(args)...);
	}
	catch (const std::exception& e)
	{
		this->handle_exception(address);
	}
	return -1;
}

template <typename... Args>
inline long Script::call(const std::string& func, Args&&... args)
{
	const auto address = machine().address_of(func.c_str());
	if (UNLIKELY(address == 0))
	{
		this->could_not_find(func);
		return -1;
	}
	return call(address, std::forward<Args>(args)...);
}

template <typename... Args>
inline long Script::preempt(gaddr_t address, Args&&... args)
{
	const auto regs = machine().cpu.registers();
	try
	{
		const long ret = machine().preempt<true, false>(
			MAX_INSTRUCTIONS, address, std::forward<Args>(args)...);
		machine().cpu.registers() = regs;
		return ret;
	}
	catch (const std::exception& e)
	{
		this->handle_exception(address);
	}
	machine().cpu.registers() = regs;
	return -1;
}

inline void Script::resume(uint64_t cycles)
{
	try
	{
		machine().resume<false>(cycles);
	}
	catch (const std::exception& e)
	{
		this->handle_exception(machine().cpu.pc());
		this->m_crashed = true;
	}
}

/**
 * This uses RAII to sequentially allocate a range
 * for the objects, which is freed on destruction.
 * If the object range is larger than a page (4k),
 * the guarantee will no longer hold, and things will
 * stop working if the pages are not sequential on the
 * host as well. If the objects are somewhat large or
 * you need many, simply manage 1 or more objects at a
 * time. This is a relatively inexpensive abstraction.
 *
 * Example:
 * myscript.guest_alloc<GameObject>(16) allocates one
 * single memory range on the managed heap of size:
 *  sizeof(GameObject) * 16
 * The range is properly aligned and holds all objects.
 * It is heap-allocated inside VM guest virtual memory.
 *
 * The returned GuestObjects<T> object manages all the
 * objects, and on destruction will free all objects.
 * It can be moved. The moved-from object manages nothing.
 *
 * All objects are potentially uninitialized, like all
 * heap allocations, and will need to be individually
 * initialized.
 **/
template <typename T> struct GuestObjects
{
	T& at(size_t n)
	{
		if (n < m_count)
			return m_object[n];
		throw riscv::MachineException(
			riscv::ILLEGAL_OPERATION, "at(): Object is out of range", n);
	}

	const T& at(size_t n) const
	{
		if (n < m_count)
			return m_object[n];
		throw riscv::MachineException(
			riscv::ILLEGAL_OPERATION, "at(): Object is out of range", n);
	}

	Script::gaddr_t address(size_t n) const
	{
		if (n < m_count)
			return m_address + sizeof(T) * n;
		throw riscv::MachineException(
			riscv::ILLEGAL_OPERATION, "address(): Object is out of range", n);
	}

	GuestObjects(Script& s, Script::gaddr_t a, T* o, size_t c)
	  : m_script(s), m_address(a), m_object(o), m_count(c)
	{
	}

	GuestObjects(GuestObjects&& other)
	  : m_script(other.m_script), m_address(other.m_address),
		m_object(other.m_object), m_count(other.m_count)
	{
		other.m_address = 0x0;
		other.m_count	= 0u;
	}

	~GuestObjects()
	{
		if (this->m_address != 0x0)
		{
			m_script.guest_free(this->m_address);
			this->m_address = 0x0;
		}
	}

	Script& m_script;
	Script::gaddr_t m_address;
	T* m_object;
	size_t m_count;
};

template <typename T> inline GuestObjects<T> Script::guest_alloc(size_t n)
{
	// XXX: If n is too large, it will always overflow a page,
	// and we will need another strategy in order to guarantee
	// sequential memory.
	auto addr = this->guest_alloc_sequential(sizeof(T) * n);
	if (addr != 0x0)
	{
		const auto pageno	= machine().memory.page_number(addr);
		const size_t offset = addr & (riscv::Page::size() - 1);
		// Lazily create zero-initialized page
		auto& page	 = machine().memory.create_writable_pageno(pageno, true);
		auto* object = (T*)&page.data()[offset];
		// Note: this can fail and throw, but we don't care
		return {*this, addr, object, n};
	}
	return {*this, 0x0, nullptr, 0u};
}
