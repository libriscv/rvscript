#include "script.hpp"
using gaddr_t = Script::gaddr_t;

#include "../../api/api_structs.h"
#include "../../api/syscalls.h"
#include <fstream> // Windows doesn't implement C getline()
#include <libriscv/native_heap.hpp>
#include <libriscv/threads.hpp>
#include <libriscv/util/crc32.hpp>
#include <strf/to_cfile.hpp>
static std::vector<uint8_t> load_file(const std::string& filename);
// Some dynamic calls are currently enabled late in initialization
static constexpr bool WARN_ON_UNIMPLEMENTED_DYNCALL = false;
/// @brief The shared memory area is 8KB and read+write
static constexpr gaddr_t SHM_BASE	  = 0x2000;
static constexpr gaddr_t SHM_SIZE	  = 2 * riscv::Page::size();
static const int HEAP_SYSCALLS_BASE	  = 570;
static const int MEMORY_SYSCALLS_BASE = 575;
static const int THREADS_SYSCALL_BASE = 590;
// Memory area shared between all script instances
static std::array<uint8_t, SHM_SIZE> shared_memory {};
static const std::vector<std::string> env = {
	"LC_CTYPE=C", "LC_ALL=C", "USER=groot"
};
using riscv::crc32;

Script::Script(
	std::shared_ptr<const std::vector<uint8_t>> binary, const std::string& name,
	const std::string& filename, bool debug, void* userptr)
  : m_binary(binary),
    m_userptr(userptr), m_name(name),
	m_filename(filename), m_hash(crc32(name.c_str(), name.size())), m_is_debug(debug)
{
	static bool init = false;
	if (!init)
	{
		init = true;
		Script::setup_syscall_interface();
	}
	this->reset();
	this->initialize();
}

Script::Script(
	const std::string& name, const std::string& filename, bool debug, void* userptr)
  : Script(std::make_shared<const std::vector<uint8_t>> (load_file(filename)), name, filename, debug, userptr)
{}

Script Script::clone(const std::string& name, void* userptr)
{
	return Script(this->m_binary, name, this->m_filename, this->m_is_debug, userptr);
}

Script::~Script() {}

void Script::reset()
{
	// If the reset fails, this object is still valid:
	// m_machine.reset() will not happen if new machine_t fails
	try
	{
		// Create a new machine based on m_binary */
		riscv::MachineOptions<MARCH> options {
			.memory_max		  = MAX_MEMORY,
			.stack_size		  = STACK_SIZE,
			.use_memory_arena = true,
			.default_exit_function = "fast_exit",
		};
		m_machine = std::make_unique<machine_t> (*m_binary, options);

		// setup system calls and traps
		this->machine_setup();
		// setup program argv *after* setting new stack pointer
		machine().setup_linux({name()}, env);
	}
	catch (std::exception& e)
	{
		strf::to(stdout)(
			">>> Exception during initialization: ", e.what(), "\n");
		throw;
	}
}

void Script::add_shared_memory()
{
	auto& mem			  = machine().memory;
	const auto stack_addr = mem.stack_initial();
	const auto stack_base = stack_addr - STACK_SIZE;

	// Each thread should have its own stack, but the boundaries are
	// usually not known. To help out things like forked multiprocessing
	// we will add the boundaries for the main thread manually.
	auto* main_thread		= machine().threads().get_thread();
	main_thread->stack_size = stack_addr - stack_base;
	main_thread->stack_base = stack_base;

	// Shared memory area between all programs
	mem.insert_non_owned_memory(SHM_BASE, &shared_memory[0], SHM_SIZE);
}

void Script::initialize()
{
	// run through the initialization
	try
	{
		machine().simulate(MAX_BOOT_INSTR);
	}
	catch (riscv::MachineTimeoutException& me)
	{
		strf::to(stdout)(
			">>> Exception: Instruction limit reached on ", name(), "\n",
			"Instruction count: ", machine().max_instructions(), "\n");
		throw;
	}
	catch (riscv::MachineException& me)
	{
		strf::to(stdout)(
			">>> Machine exception ", me.type(), ": ", me.what(),
			" (data: ", strf::hex(me.data()), "\n");
		// Remote debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			gdb_remote_debugging("", false);
		throw;
	}
	catch (std::exception& e)
	{
		strf::to(stdout)(">>> Exception: ", e.what(), "\n");
		throw;
	}

	strf::to(stdout)(">>> ", name(), " initialized.\n");
}

void Script::machine_setup()
{
	machine().set_userdata<Script>(this);
	machine().set_printer((machine_t::printer_func)[](
		const machine_t&, const char* p, size_t len) {
		strf::to(stdout)(std::string_view {p, len});
	});
	machine().set_debug_printer(machine().get_printer());
	machine().on_unhandled_csr = [](machine_t& machine, int csr, int, int)
	{
		auto& script = *machine.template get_userdata<Script>();
		strf::to(stdout)(script.name(), ": Unhandled CSR: ", csr, "\n");
	};
	machine().on_unhandled_syscall = [](machine_t& machine, size_t num)
	{
		auto& script = *machine.get_userdata<Script>();
		strf::to(stdout)(script.name(), ": Unhandled system call: ", num, "\n");
	};
	// Allocate heap area using mmap
	this->m_heap_area = machine().memory.mmap_allocate(MAX_HEAP);

	// Add POSIX system call interfaces (no filesystem or network access)
	machine().setup_linux_syscalls(false, false);
	machine().setup_posix_threads();
	// Add native system call interfaces
	machine().setup_native_heap(HEAP_SYSCALLS_BASE, heap_area(), MAX_HEAP);
	machine().setup_native_memory(MEMORY_SYSCALLS_BASE);
	machine().setup_native_threads(THREADS_SYSCALL_BASE);

	// Remote communication
	this->machine_remote_setup();

	// Install shared memory area and guard pages
	this->add_shared_memory();

	// Figure out the local indices based on dynamic call table
	// We are pretending to be initializing the client-side
	this->resolve_dynamic_calls(true, true, false);
}

void Script::could_not_find(std::string_view func)
{
	strf::to(stdout)(
		"Script::call(): Could not find: '", func, "' in '", name(), "'\n");
}

void Script::handle_exception(gaddr_t address)
{
	auto callsite = machine().memory.lookup(address);
	strf::to(stdout)(
		"[", name(), "] Exception when calling:\n  ", callsite.name, " (0x",
		strf::hex(callsite.address), ")\n", "Backtrace:\n");
	this->print_backtrace(address);

	try
	{
		throw; // re-throw
	}
	catch (const riscv::MachineTimeoutException& e)
	{
		this->handle_timeout(address);
		return; // NOTE: might wanna stay
	}
	catch (const riscv::MachineException& e)
	{
		strf::to(stdout)(
			"\nException: ", e.what(), "  (data: ", strf::hex(e.data()), ")\n",
			">>> ", machine().cpu.current_instruction_to_string(), "\n",
			">>> Machine registers:\n[PC\t", strf::hex(machine().cpu.pc()) > 8,
			"] ", machine().cpu.registers().to_string(), "\n");

		// Remote debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			gdb_remote_debugging("", false);
	}
	catch (const std::exception& e)
	{
		strf::to(stdout)("\nMessage: ", e.what(), "\n\n");
	}
	strf::to(stdout)(
		"Program page: ", machine().memory.get_page_info(machine().cpu.pc()),
		"\n");
	strf::to(stdout)(
		"Stack page: ", machine().memory.get_page_info(machine().cpu.reg(2)),
		"\n");
	// Close active non-main thread (XXX: Probably not what we want)
	auto& mt = machine().threads();
	while (mt.get_tid() != 0)
	{
		auto* thread = mt.get_thread();
		strf::to(stdout)(
			"Script::call: Closing running thread: ", thread->tid, "\n");
		thread->exit();
	}
}

void Script::handle_timeout(gaddr_t address)
{
	this->m_budget_overruns++;
	auto callsite = machine().memory.lookup(address);
	strf::to(stdout)(
		"Script::call: Max instructions for: ", callsite.name,
		" (Overruns: ", m_budget_overruns, "\n");
	// Check if we need to suspend a thread
	auto& mt	 = machine().threads();
	auto* thread = mt.get_thread();
	if (thread->tid != 0)
	{
		// try to do the right thing here
		if (thread->block_word != 0)
		{
			thread->block(thread->block_word);
		}
		else
		{
			thread->suspend();
		}
		// resume some other thread
		mt.wakeup_next();
	}
}

void Script::max_depth_exceeded(gaddr_t address)
{
	auto callsite = machine().memory.lookup(address);
	strf::to(stderr)(
		"Script::call(): Max call depth exceeded when calling: ", callsite.name,
		" (0x", strf::hex(callsite.address), ")\n");
}

void Script::print_backtrace(const gaddr_t addr)
{
	machine().memory.print_backtrace(
		[](std::string_view line)
		{
			strf::to(stdout)("-> ", line, "\n");
		});
	auto origin = machine().memory.lookup(addr);
	strf::to(stdout)(
		"-> [-] ", strf::hex(origin.address), " + ", strf::hex(origin.offset),
		": ", origin.name, "\n");
}

void Script::print(std::string_view text)
{
	if (this->m_last_newline)
	{
		strf::to(stdout)("[", name(), "] says: ", text);
	}
	else
	{
		strf::to(stdout)(text);
	}
	this->m_last_newline = (text.back() == '\n');
}

gaddr_t Script::address_of(const std::string& name) const
{
	// We need to cache lookups because they are fairly expensive
	// Dynamic executables usually have a hash lookup table for symbols,
	// but no such thing for static executables. So, we compensate by
	// storing symbols in a local cache in order to reduce latencies.
	auto it = m_lookup_cache.find(name);
	if (it != m_lookup_cache.end())
		return it->second;

	const auto addr = machine().address_of(name.c_str());
	m_lookup_cache.try_emplace(name, addr);
	return addr;
}

std::string Script::symbol_name(gaddr_t address) const
{
	auto callsite = machine().memory.lookup(address);
	return callsite.name;
}

static std::string single_spaced_string(std::string line)
{
	std::size_t loc = 0;
	while ((loc = line.find("  ", loc)) != std::string::npos)
	{
		line.replace(loc, 2, " ");
	}
	return line;
}

void Script::set_dynamic_call(const std::string& def, ghandler_t handler)
{
	// Uses the definition as both name and hash
	set_dynamic_call(def, def, std::move(handler));
}

void Script::set_dynamic_call(std::string name, std::string def, ghandler_t handler)
{
	// Allow unsetting a dynamic call by using an uncallable/invalid callback function
	if (handler == nullptr) {
		handler = [] (auto&) {
			throw std::runtime_error("Unimplemented-trap");
		};
	}

	// Turn definition into a single-spaced string
	def = single_spaced_string(def);
	// Calculate hash from definition
	const uint32_t hash = crc32(def.c_str(), def.size());
	auto it				= m_dynamic_functions.find(hash);
	if (it != m_dynamic_functions.end())
	{
		if (it->second.name != name) {
			strf::to(stdout)(
				"Dynamic function '", name, "' with hash ", strf::hex(hash),
				" already exists with another name '", it->second.name, "'\n");
			throw std::runtime_error(
				"Script::set_dynamic_call failed: Hash collision for " + name);
		}
		it->second.func = std::move(handler);
	} else {
		m_dynamic_functions.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(hash),
			std::forward_as_tuple(std::move(name), std::move(def), std::move(handler)));
	}
}

void Script::set_dynamic_calls(
	std::vector<std::tuple<std::string, std::string, ghandler_t>> vec)
{
	for (auto& t : vec)
	{
		set_dynamic_call(std::move(std::get<0>(t)), std::move(std::get<1>(t)), std::move(std::get<2>(t)));
	}
}

void Script::dynamic_call_hash(uint32_t hash, gaddr_t straddr)
{
	auto it = m_dynamic_functions.find(hash);
	if (LIKELY(it != m_dynamic_functions.end()))
	{
		it->second.func(*this);
	}
	else
	{
		auto name = machine().memory.memstring(straddr);
		strf::to(stdout)(
			"Unable to find dynamic function '", name, "' with hash ",
			strf::hex(hash), "\n");
		throw std::runtime_error("Unable to find dynamic function: " + name);
	}
}

void Script::dynamic_call_array(uint32_t idx)
{
	while (true) {
		try {
			this->m_dyncall_array.at(idx)(*this);
			return;
		} catch (const std::exception& e) {
			// This will re-throw unless a new dynamic call is discovered
			this->dynamic_call_error(idx, e);
		}
	}
}

void Script::dynamic_call_error(uint32_t idx, const std::exception& e)
{
	const uint32_t entries = machine().memory.read<uint32_t> (m_g_dyncall_table);
	if (idx < entries) {
		DyncallDesc entry;
		const auto offset = 0x4 + idx * sizeof(DyncallDesc);
		machine().copy_from_guest(&entry, m_g_dyncall_table + offset, sizeof(DyncallDesc));

		// Try to resolve it again
		if (e.what() == std::string("Unimplemented-trap"))
		{
			auto it = m_dynamic_functions.find(entry.hash);
			if (LIKELY(it != m_dynamic_functions.end()))
			{
				// Resolved, return directly
				this->m_dyncall_array.at(idx) = it->second.func;
				return;
			}
			const auto dname = machine().memory.memstring(entry.strname);
			strf::to(stdout)(
				"ERROR: Exception in '", this->name(),"', dynamic function '", dname, "' with hash ",
				strf::hex(entry.hash), " and table index ", idx, "\n"
				"ERROR: Not installed in the host game engine. Forgot to call set_dynamic_handler(...)?\n");
		} else {
			const auto dname = machine().memory.memstring(entry.strname);
			strf::to(stdout)(
				"ERROR: Exception in '", this->name(),"', dynamic function '", dname, "' with hash ",
				strf::hex(entry.hash), " and table index ", idx, "\n");
		}

	} else {
		strf::to(stdout)(
			"ERROR: Exception in '", this->name(),"', dynamic function table index ",
			idx, " out of range\n");
	}
	fflush(stdout);
	throw;
}

void Script::resolve_dynamic_calls(bool initialization, bool client_side, bool verbose)
{
	this->m_g_dyncall_table = machine().address_of("dyncall_table");
	if (m_g_dyncall_table == 0x0)
		throw std::runtime_error(this->name() + ": Unable to find dynamic call table");
	// Table header contains the number of entries
	const uint32_t entries = machine().memory.read<uint32_t> (m_g_dyncall_table);
	if (entries > 512)
		throw std::runtime_error(this->name() + ": Too many dynamic call table entries (bogus value)");
	// Skip past header
	const auto g_table = m_g_dyncall_table + 0x4;

	// Reserve space for host-side dynamic call handlers
	this->m_dyncall_array.reserve(entries);
	this->m_dyncall_array.clear();
	unsigned unimplemented = 0;

	// Copy whole table into vector
	std::vector<DyncallDesc> table (entries);
	machine().copy_from_guest(table.data(), g_table, entries * sizeof(DyncallDesc));

	for (unsigned i = 0; i < entries; i++) {
		auto& entry = table.at(i);
		if (entry.initialization_only && !initialization) {
			if (verbose) strf::to(stdout)(
				"Skipping initialization-only dynamic call '",
				this->machine().memory.memstring(entry.strname), "'\n");
			this->m_dyncall_array.push_back(
			[] (auto&) {
				throw std::runtime_error("Initialization-only dynamic call triggered");
			});
			continue;
		}
		if (entry.client_side_only && !client_side) {
			if (verbose) strf::to(stdout)(
				"Skipping client-side-only dynamic call '",
				machine().memory.memstring(entry.strname), "'\n");
			this->m_dyncall_array.push_back(
			[] (auto&) {
				throw std::runtime_error("Clientside-only dynamic call triggered");
			});
			continue;
		}
		if (entry.server_side_only && client_side) {
			if (verbose) strf::to(stdout)(
				"Skipping server-side-only dynamic call '",
				machine().memory.memstring(entry.strname), "'\n");
			this->m_dyncall_array.push_back(
			[] (auto&) {
				throw std::runtime_error("Serverside-only dynamic call triggered");
			});
			continue;
		}

		auto it = m_dynamic_functions.find(entry.hash);
		if (LIKELY(it != m_dynamic_functions.end()))
		{
			this->m_dyncall_array.push_back(it->second.func);
		} else {
			this->m_dyncall_array.push_back(
			[] (auto&) {
				throw std::runtime_error("Unimplemented dynamic call triggered");
			});
			if (verbose) {
			const std::string name = machine().memory.memstring(entry.strname);
			strf::to(stderr)(
				"WARNING: Unimplemented dynamic function '", name, "' with hash ",
				strf::hex(entry.hash), " and program table index ", i, " (total: ",
				m_dyncall_array.size(), ")\n");
			}
			unimplemented++;
		}
	}
	if (m_dyncall_array.size() != entries)
		throw std::runtime_error("Mismatching number of dynamic call array entries");
	strf::to(stdout)(
		"* Resolved dynamic calls for '", name(), "' with ", entries, " entries, ",
		unimplemented, " unimplemented\n");
}

void Script::set_global_setting(std::string_view setting, gaddr_t value)
{
	m_runtime_settings.insert_or_assign(std::string(setting), value);
}

std::optional<gaddr_t> Script::get_global_setting(std::string_view setting)
{
	auto it = m_runtime_settings.find(setting);
	if (it != m_runtime_settings.end())
		return it->second;
	return std::nullopt;
}

gaddr_t Script::guest_alloc(gaddr_t bytes)
{
	return machine().arena().malloc(bytes);
}

gaddr_t Script::guest_alloc_sequential(gaddr_t bytes)
{
	return machine().arena().seq_alloc_aligned(bytes, 8);
}

bool Script::guest_free(gaddr_t addr)
{
	return machine().arena().free(addr) == 0x0;
}

#include <unistd.h>

std::vector<uint8_t> load_file(const std::string& filename)
{
	size_t size = 0;
	FILE* f		= fopen(filename.c_str(), "rb");
	if (f == NULL)
		throw std::runtime_error("Could not open file: " + filename);

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	std::vector<uint8_t> result(size);
	if (size != fread(result.data(), 1, size, f))
	{
		fclose(f);
		throw std::runtime_error("Error when reading from file: " + filename);
	}
	fclose(f);
	return result;
}
