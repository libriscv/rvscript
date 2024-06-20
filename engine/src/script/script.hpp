#pragma once
#include <any>
#include <functional>
#include <libriscv/machine.hpp>
#include <libriscv/prepared_call.hpp>
#include <optional>
#include <unordered_set>
#include "script_depth.hpp"
template <typename T> struct GuestObjects;

struct Script
{
	static constexpr int MARCH = RISCV_ARCH / 8;

	/// @brief A virtual address inside a Script program
	using gaddr_t		= riscv::address_type<MARCH>;
	using sgaddr_t      = riscv::signed_address_type<MARCH>;
	/// @brief A virtual machine running a program
	using machine_t		= riscv::Machine<MARCH>;
	/// @brief A dynamic call callback function
	using ghandler_t	= std::function<void(Script&)>;
	/// @brief A callback for when Game::exit() is called inside a Script program
	using exit_func_t 	= std::function<void(Script&)>;

	/// @brief The total physical memory of the program
	static constexpr gaddr_t MAX_MEMORY	= 1024 * 1024 * 24ull;
	/// @brief A virtual memory area set aside for the initial stack
	static constexpr gaddr_t STACK_SIZE	= 1024 * 1024 * 2ull;
	/// @brief A virtual memory area set aside for the heap
	static constexpr gaddr_t MAX_HEAP	= MAX_MEMORY * 2ull;
	/// @brief The max number of instructions allowed during startup
	static constexpr uint64_t MAX_BOOT_INSTR = 32'000'000ull;
	/// @brief The max number of instructions allowed during calls
	static constexpr uint64_t MAX_CALL_INSTR = 32'000'000ull;
	/// @brief The max number of recursive calls into the Machine allowed
	static constexpr uint8_t  MAX_CALL_DEPTH = 8;

	/// @brief Make a function call into the script
	/// @param func The function to call. Must be a visible symbol in the program.
	/// @param args The arguments to pass to the function.
	/// @return The optional integral return value.
	template <typename... Args>
	std::optional<Script::sgaddr_t> call(const std::string& func, Args&&... args);

	/// @brief Make a function call into the script
	/// @param addr The functions direct address.
	/// @param args The arguments to pass to the function.
	/// @return The optional integral return value.
	template <typename... Args>
	std::optional<Script::sgaddr_t> call(gaddr_t addr, Args&&... args);

	/// @brief Make a preempted function call into the script, saving and
	/// restoring the current execution state.
	/// Preemption allows callers to temporarily interrupt the virtual machine,
	/// such that it can be resumed like normal later on.
	/// @param func The function to call. Must be a visible symbol in the program.
	/// @param args The arguments to the function call.
	/// @return The optional integral return value.
	template <typename... Args>
	std::optional<Script::sgaddr_t> preempt(const std::string& func, Args&&... args);

	/// @brief Make a preempted function call into the script, saving and
	/// restoring the current execution state.
	/// Preemption allows callers to temporarily interrupt the virtual machine,
	/// such that it can be resumed like normal later on.
	/// @param addr The functions address to call.
	/// @param args The arguments to the function call.
	/// @return The optional integral return value.
	template <typename... Args>
	std::optional<Script::sgaddr_t> preempt(gaddr_t addr, Args&&... args);

	/// @brief Resume execution of the script, until @param instruction_count has been reached,
	/// then stop execution and return. This function can be used to drive long-running tasks
	/// over time, by continually resuming them.
	/// @param instruction_count The max number of instructions to execute before returning.
	bool resume(uint64_t instruction_count);

	/// @brief Returns the pointer provided at instantiation of the Script instance.
	/// @tparam T The real type of the user-provided pointer.
	/// @return Returns the user-provided pointer.
	template <typename T> T* userptr() noexcept
	{
		return (T*)m_userptr;
	}

	/// @brief Returns the pointer provided at instantiation of the Script instance.
	/// @tparam T The real type of the user-provided pointer.
	/// @return Returns the user-provided pointer.
	template <typename T> const T* userptr() const noexcept
	{
		return (const T*)m_userptr;
	}

	/// @brief Tries to find the name of a symbol at the given virtual address.
	/// @param address The virtual address to find in the symbol table.
	/// @return The closest symbol to the given address.
	std::string symbol_name(gaddr_t address) const;

	/// @brief Look up the address of a name. Returns 0x0 if not found.
	/// Uses an unordered_map to remember lookups in order to speed up future lookups.
	/// @param name The name to find the virtual address for.
	/// @return The virtual address of name, or 0x0 if not found.
	gaddr_t address_of(const std::string& name) const;

	// Install a callback function using a string name
	// Can be invoked from the guest using the same string name
	static void set_dynamic_call(const std::string& def, ghandler_t);
	static void set_dynamic_call(std::string name, std::string def, ghandler_t);
	static void
		set_dynamic_calls(std::vector<std::tuple<std::string, std::string, ghandler_t>>);
	void dynamic_call_hash(uint32_t hash, gaddr_t strname);
	void dynamic_call_array(uint32_t idx);

	/// @brief Retrieve arguments passed to a dynamic call, specifying each type.
	/// @tparam ...Args The types of arguments to retrieve.
	/// @return A tuple of arguments.
	template <typename... Args>
	auto args() const;

	auto& dynargs()
	{
		return m_arguments;
	}

	/// @brief The virtual machine hosting the Scripts program.
	/// @return The underlying virtual machine.
	auto& machine()
	{
		return *m_machine;
	}

	/// @brief The virtual machine hosting the Scripts program.
	/// @return The underlying virtual machine.
	const auto& machine() const
	{
		return *m_machine;
	}

	/// @brief The name given to this Script instance during creation.
	/// @return The name of this Script instance.
	const auto& name() const noexcept
	{
		return m_name;
	}

	/// @brief The CRC32 hash of the name given to this Script instance during creation.
	/// @return The hashed name of this Script instance.
	uint32_t hash() const noexcept
	{
		return m_hash;
	}

	/// @brief The filename passed to this Script instance during creation.
	/// @return The filename of this Script instance.
	const auto& filename() const noexcept
	{
		return m_filename;
	}

	bool is_debug() const noexcept
	{
		return m_is_debug;
	}

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

	/// @brief Make it possible to access and make function calls to the given
	/// script from with another The access is two-way.
	/// @param remote The remote script
	void setup_remote_calls_to(Script& remote);

	/// @brief Make it possible make function calls to the given script from
	/// with another This access is one-way and only the remote can write back
	/// results, as if it has a higher privilege level
	/// @param remote The remote script
	void setup_strict_remote_calls_to(Script& remote);

	/// @brief Allow a remote Script to call the given local function on this Script instance.
	/// Only applies if setup_strict_remote_calls_to() is used on the caller Script.
	/// @param func The local function on this instance to allow remote callers to call.
	void add_allowed_remote_function(const std::string& func);
	void add_allowed_remote_function(gaddr_t addr);

	/* The guest heap is managed outside using system calls. */

	/// @brief Allocate bytes inside the program. All allocations are at least 8-byte aligned.
	/// @param bytes The number of 8-byte aligned bytes to allocate.
	/// @return The address of the allocated bytes.
	gaddr_t guest_alloc(gaddr_t bytes);

	/// @brief Allocate bytes inside the program. All allocations are at least 8-byte aligned.
	/// This allocation is sequentially accessible outside of the script. Using
	/// this function the host engine can view and use the allocation as a single
	/// consecutive array of bytes, allowing it to be used with many (if not most)
	/// APIs that do not handle fragmented memory.
	/// @param bytes The number of sequentially allocated 8-byte aligned bytes.
	/// @return The address of the sequentially allocated bytes.
	gaddr_t guest_alloc_sequential(gaddr_t bytes);
	bool guest_free(gaddr_t addr);

	/// @brief Create a wrapper object that manages an allocation of n objects of type T
	/// inside the program. All objects are default-constructed.
	/// All objects are accessible in the host and in the script. The script can read and write
	/// all objects at will, making the state of these objects fundamentally untrustable.
	/// When the wrapper destructs, the allocation in the program is also freed. Can be moved.
	/// @tparam T The type of the allocated objects.
	/// @param n The number of allocated objects in the array.
	/// @return A wrapper managing the program-hosted objects. Can be moved.
	template <typename T> GuestObjects<T> guest_alloc(size_t n = 1);

	/// @brief Start debugging with GDB right now. Only works on Linux.
	/// @param message Message to print inside GDB.
	/// @param one_up Always go one stack frame up? This can be used to leave a wrapper function.
	/// @param port The port to use. By default the RSP port 2159.
	void gdb_remote_debugging(std::string message, bool one_up, uint16_t port = 0);

	/// @brief Decide what happens when Game::exit() is called in the script.
	/// @param callback The function to call when Game::exit() is called.
	static void on_exit(exit_func_t callback) { Script::m_exit = std::move(callback); }
	void exit() { Script::m_exit(*this); } // Called by Game::exit() from the script.

	/// @brief Create a thread-local fork of this script instance.
	/// @return A new Script instance that is a fork of this instance.
	Script& create_fork();
	/// @brief Retrieve the fork of this script instance.
	/// @return The fork of this instance.
	Script& get_fork();
	/// @brief Retrieve an instance of a script by its program name.
	/// @param  name The name of the script to find.
	/// @return The script instance with the given name.
	static Script& Find(const std::string& name);

	/// @brief Make a prepared function call into the script
	/// @param pcall The prepared call object.
	/// @param args The arguments to pass to the function.
	/// @return The optional integral return value.
	template <typename F, typename... Args>
	std::optional<Script::sgaddr_t> prepared_call(riscv::PreparedCall<MARCH, F>& pcall, Args&&... args);

	// Create new Script instance from file
	Script(
		const std::string& name, const std::string& filename,
		bool dbg = false, void* userptr = nullptr);
	// Create new Script instance from existing binary
	Script(
		std::shared_ptr<const std::vector<uint8_t>> binary, const std::string& name,
		const std::string& filename, bool dbg = false, void* userptr = nullptr);
	// Create new Script instance from cloning another Script
	Script clone(const std::string& name, void* userptr = nullptr);
	~Script();

  private:
	static void setup_syscall_interface();
	void reset(); // true if the reset was successful
	void initialize();
	void could_not_find(std::string_view);
	void handle_exception(gaddr_t);
	void handle_timeout(gaddr_t);
	void max_depth_exceeded(gaddr_t);
	void machine_setup();
	void machine_remote_setup();
	void resolve_dynamic_calls(bool initialization, bool client_side, bool verbose);
	void dynamic_call_error(uint32_t idx, const std::exception& e);
	static long finish_benchmark(std::vector<long>&);

	std::unique_ptr<machine_t> m_machine = nullptr;
	std::shared_ptr<const std::vector<uint8_t>> m_binary;
	void* m_userptr;
	gaddr_t m_heap_area		   = 0;
	std::string m_name;
	std::string m_filename;
	uint32_t m_hash;
	uint8_t  m_call_depth   = 0;
	bool m_is_debug			= false;
	bool m_stdout			= true;
	bool m_last_newline		= true;
	int m_budget_overruns	= 0;
	Script* m_remote_script = nullptr;
	/// @brief Functions accessible when remote access is *strict*
	std::unordered_set<gaddr_t> m_remote_access;
	/// @brief List of arguments added by dynamic arguments feature
	std::vector<std::any> m_arguments;
	/// @brief Cached addresses for symbol lookups
	/// This could probably be improved by doing it per-binary instead
	/// of a separate cache per instance. But at least it's thread-safe.
	mutable std::unordered_map<std::string, gaddr_t> m_lookup_cache;
	// dynamic call array, lazily resolved at run-time
	struct DyncallDesc {
		uint32_t hash;
		uint32_t resv;
		uint32_t strname;
		bool initialization_only;
		bool client_side_only;
		bool server_side_only;
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
	static inline std::map<std::string, gaddr_t, std::less<>> m_runtime_settings;
	static inline exit_func_t m_exit = nullptr;
};

static_assert(
	RISCV_ARCH == 32 || RISCV_ARCH == 64,
	"Architecture must be 32- or 64-bit");

template <typename... Args>
inline std::optional<Script::sgaddr_t> Script::call(gaddr_t address, Args&&... args)
{
	ScriptDepthMeter meter(this->m_call_depth);
	try
	{
		if (LIKELY(meter.is_one()))
			return {machine().vmcall<MAX_CALL_INSTR>(
				address, std::forward<Args>(args)...)};
		else if (LIKELY(meter.get() < MAX_CALL_DEPTH))
			return {machine().preempt(MAX_CALL_INSTR,
				address, std::forward<Args>(args)...)};
		else
			this->max_depth_exceeded(address);
	}
	catch (const std::exception& e)
	{
		this->handle_exception(address);
	}
	return std::nullopt;
}

template <typename... Args>
inline std::optional<Script::sgaddr_t> Script::call(const std::string& func, Args&&... args)
{
	const auto address = this->address_of(func.c_str());
	if (UNLIKELY(address == 0x0))
	{
		this->could_not_find(func);
		return std::nullopt;
	}
	return {this->call(address, std::forward<Args>(args)...)};
}

template <typename F, typename... Args>
inline std::optional<Script::sgaddr_t> Script::prepared_call(riscv::PreparedCall<MARCH, F>& pcall, Args&&... args)
{
	ScriptDepthMeter meter(this->m_call_depth);
	try
	{
		if (LIKELY(meter.is_one()))
			return {pcall.vmcall(std::forward<Args>(args)...)};
		else if (LIKELY(meter.get() < MAX_CALL_DEPTH))
			return {machine().preempt(MAX_CALL_INSTR, pcall.address(),
				std::forward<Args>(args)...)};
		else
			this->max_depth_exceeded(pcall.address());
	}
	catch (const std::exception& e)
	{
		this->handle_exception(pcall.address());
	}
	return std::nullopt;
}

template <typename... Args>
inline std::optional<Script::sgaddr_t> Script::preempt(gaddr_t address, Args&&... args)
{
	try
	{
		return {machine().preempt(
			MAX_CALL_INSTR, address, std::forward<Args>(args)...)};
	}
	catch (const std::exception& e)
	{
		this->handle_exception(address);
	}
	return std::nullopt;
}

template <typename... Args>
inline std::optional<Script::sgaddr_t> Script::preempt(const std::string& func, Args&&... args)
{
	const auto address = this->address_of(func.c_str());
	if (UNLIKELY(address == 0x0))
	{
		this->could_not_find(func);
		return std::nullopt;
	}
	return {this->preempt(address, std::forward<Args>(args)...)};
}

inline bool Script::resume(uint64_t cycles)
{
	try
	{
		machine().resume<false>(cycles);
		return true;
	}
	catch (const std::exception& e)
	{
		this->handle_exception(machine().cpu.pc());
		return false;
	}
}

template <typename... Args>
inline auto Script::args() const
{
	return machine().sysargs<Args ...> ();
}

/**
 * This uses RAII to sequentially allocate a range
 * for the objects, which is freed on destruction.
 * This is a relatively inexpensive abstraction.
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
		// Default-initialize all objects
		for (auto *o = object; o < object + n; o++)
			new (o) T{};
		// Note: this can fail and throw, but we don't care
		return {*this, addr, object, n};
	}
	throw std::runtime_error("Unable to allocate aligned sequential data");
}

inline void Script::add_allowed_remote_function(gaddr_t addr)
{
	m_remote_access.insert(addr);
}
inline void Script::add_allowed_remote_function(const std::string& func)
{
	const auto addr = this->address_of(func);
	if (addr != 0x0)
		this->m_remote_access.insert(addr);
	else
		throw std::runtime_error("No such function: " + func);
}
