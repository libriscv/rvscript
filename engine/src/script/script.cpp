#include "script.hpp"
using gaddr_t = Script::gaddr_t;

#include "../../api/api_structs.h"
#include "../../api/syscalls.h"
#include <fstream> // Windows doesn't implement C getline()
#include <libriscv/native_heap.hpp>
#include <libriscv/threads.hpp>
#include <libriscv/util/crc32.hpp>
#include <strf/to_cfile.hpp>
// the shared area is read-write for the guest
static constexpr size_t STACK_SIZE	  = 2ULL << 20;
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
	const machine_t& smach, void* userptr, const std::string& name,
	const std::string& filename, bool debug)
  : m_source_machine(smach), m_userptr(userptr), m_name(name),
	m_filename(filename), m_hash(crc32(name.c_str(), name.size())), m_is_debug(debug)
{
	static bool init = false;
	if (!init)
	{
		init = true;
		Script::setup_syscall_interface();
	}
	this->reset();
}

Script::~Script() {}

bool Script::reset()
{
	// If the reset fails, this object is still valid:
	// m_machine.reset() will not happen if new machine_t fails
	try
	{
		// Fork the source machine into m_machine */
		riscv::MachineOptions<MARCH> options {
			.memory_max		  = MAX_MEMORY,
			.stack_size		  = STACK_SIZE,
			.use_memory_arena = true
		};
		m_machine.reset(new machine_t(m_source_machine, options));

		// setup system calls and traps
		this->machine_setup();
		// setup program argv *after* setting new stack pointer
		machine().setup_linux({name()}, env);
	}
	catch (std::exception& e)
	{
		strf::to(stderr)(
			">>> Exception during initialization: ", e.what(), "\n");
		throw;
	}

	return true;
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

bool Script::initialize()
{
	// clear some state belonging to previous initialization
	this->m_tick_event = 0;
	// run through the initialization
	try
	{
#ifdef RISCV_DEBUG
		// Verbose debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			machine().verbose_instructions = true;
#endif
		machine().simulate<false>(MAX_INSTRUCTIONS);

		if (UNLIKELY(machine().instruction_limit_reached()))
		{
			strf::to(stderr)(
				">>> Exception: Instruction limit reached on ", name(), "\n",
				"Instruction count: ", machine().instruction_counter(), "/",
				machine().max_instructions(), "\n");
			return false;
		}
	}
	catch (riscv::MachineException& me)
	{
		strf::to(stderr)(
			">>> Machine exception ", me.type(), ": ", me.what(),
			" (data: ", strf::hex(me.data()), "\n");
#ifdef RISCV_DEBUG
		m_machine->print_and_pause();
#else
		// Remote debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			gdb_remote_debugging("", false);
#endif
		return false;
	}
	catch (std::exception& e)
	{
		strf::to(stderr)(">>> Exception: ", e.what(), "\n");
		return false;
	}

	strf::to(stderr)(">>> ", name(), " initialized.\n");
	return true;
}

void Script::machine_setup()
{
	machine().set_userdata<Script>(this);
	machine().set_printer((machine_t::printer_func)[](
		const machine_t&, const char* p, size_t len) {
		strf::to(stdout)(std::string_view {p, len});
	});
	machine().set_debug_printer(machine().get_printer());
	machine().set_stdin(
		(machine_t::stdin_func)[](const machine_t&, char*, size_t)->long {
			return 0;
		});
	machine().on_unhandled_csr = [](machine_t& machine, int csr, int, int)
	{
		auto& script = *machine.template get_userdata<Script>();
		strf::to(stderr)(script.name(), ": Unhandled CSR: ", csr, "\n");
	};
	machine().on_unhandled_syscall = [](machine_t& machine, size_t num)
	{
		auto& script = *machine.get_userdata<Script>();
		if (UNLIKELY(num < 600))
		{
			strf::to(stderr)(script.name(), ": Unhandled system call: ", num, "\n");
		}
		else
		{
			// call the handler with register A7 as hash
			script.dynamic_call_hash(num, 0x0);
		}
	};
	// Allocate heap area using mmap
	if (m_heap_area == 0x0)
	{
		this->m_heap_area = machine().memory.mmap_allocate(MAX_HEAP);
	}

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
	this->resolve_dynamic_calls();
}

void Script::could_not_find(std::string_view func)
{
	strf::to(stderr)(
		"Script::call(): Could not find: '", func, "' in '", name(), "'\n");
}

void Script::handle_exception(gaddr_t address)
{
	auto callsite = machine().memory.lookup(address);
	strf::to(stderr)(
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
		strf::to(stderr)(
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
		strf::to(stderr)("\nMessage: ", e.what(), "\n\n");
	}
	strf::to(stderr)(
		"Program page: ", machine().memory.get_page_info(machine().cpu.pc()),
		"\n");
	strf::to(stderr)(
		"Stack page: ", machine().memory.get_page_info(machine().cpu.reg(2)),
		"\n");
	// Close active non-main thread (XXX: Probably not what we want)
	auto& mt = machine().threads();
	while (mt.get_tid() != 0)
	{
		auto* thread = mt.get_thread();
		strf::to(stderr)(
			"Script::call: Closing running thread: ", thread->tid, "\n");
		thread->exit();
	}
}

void Script::handle_timeout(gaddr_t address)
{
	this->m_budget_overruns++;
	auto callsite = machine().memory.lookup(address);
	strf::to(stderr)(
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

void Script::print_backtrace(const gaddr_t addr)
{
	machine().memory.print_backtrace(
		[](std::string_view line)
		{
			strf::to(stderr)("-> ", line, "\n");
		});
	auto origin = machine().memory.lookup(addr);
	strf::to(stderr)(
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
	return machine().address_of(name.c_str());
}

std::string Script::symbol_name(gaddr_t address) const
{
	auto callsite = machine().memory.lookup(address);
	return callsite.name;
}

void Script::each_tick_event()
{
	if (this->m_tick_event == 0)
		return;
	auto& mt = machine().threads();
	assert(mt.get_tid() == 0 && "Avoid clobbering regs");

	int count = 0;
	for (auto* thread : mt.blocked_threads())
	{
		if (thread->block_word == this->m_tick_block_word)
			count++;
	}
	this->preempt(
		this->m_tick_event, (int)count, (int)this->m_tick_block_word);
	assert(mt.get_thread()->tid == 0 && "Avoid clobbering regs");
}

void Script::set_dynamic_call(const std::string& name, ghandler_t handler)
{
	const uint32_t hash = crc32(name.c_str(), name.size());
	auto it				= m_dynamic_functions.find(hash);
	if (it != m_dynamic_functions.end())
	{
		strf::to(stderr)(
			"Dynamic function ", name, " with hash ", strf::hex(hash),
			" already exists\n");
		throw std::runtime_error(
			"Script::set_dynamic_call failed: Hash collision with " + name);
	}
	m_dynamic_functions.emplace(hash, std::move(handler));
}

void Script::reset_dynamic_call(const std::string& name, ghandler_t handler)
{
	const uint32_t hash = crc32(name.c_str(), name.size());
	m_dynamic_functions.erase(hash);
	if (handler != nullptr)
	{
		set_dynamic_call(name, std::move(handler));
	}
}

void Script::set_dynamic_calls(
	std::vector<std::pair<std::string, ghandler_t>> vec)
{
	for (const auto& pair : vec)
	{
		set_dynamic_call(pair.first, std::move(pair.second));
	}
}

void Script::dynamic_call_hash(uint32_t hash, gaddr_t straddr)
{
	auto it = m_dynamic_functions.find(hash);
	if (LIKELY(it != m_dynamic_functions.end()))
	{
		it->second(*this);
	}
	else if (straddr != 0x0)
	{
		auto name = machine().memory.memstring(straddr);
		strf::to(stderr)(
			"Unable to find dynamic function '", name, "' with hash ",
			strf::hex(hash), "\n");
		throw std::runtime_error("Unable to find dynamic function: " + name);
	}
	else
	{
		strf::to(stderr)(
			"Unable to find dynamic function with hash ",
			strf::hex(hash), " (no name)\n");
		throw std::runtime_error("Unable to find dynamic function");
	}
}

void Script::dynamic_call_array(uint32_t idx)
{
	if (idx < this->m_dyncall_array.size())
		this->m_dyncall_array[idx](*this);
	else
		throw riscv::MachineException(
			riscv::INVALID_PROGRAM, "dynamic_call_array(): Index is out of range", idx);
}

void Script::dynamic_call(const std::string& name)
{
	const uint32_t hash = crc32(name.c_str(), name.size());
	auto it				= m_dynamic_functions.find(hash);
	if (LIKELY(it != m_dynamic_functions.end()))
	{
		it->second(*this);
	}
	else
	{
		strf::to(stderr)(
			"Unable to find dynamic function '", name, "' with hash ",
			strf::hex(hash), "\n");
		throw std::runtime_error("Unable to find dynamic function: " + name);
	}
}

void Script::resolve_dynamic_calls()
{
	auto g_table = machine().address_of("dyncall_table");
	if (g_table == 0x0)
		throw std::runtime_error(this->name() + ": Unable to find dynamic call table");
	// Table header contains the number of entries
	const uint32_t entries = machine().memory.read<uint32_t> (g_table);
	if (entries > 512)
		throw std::runtime_error(this->name() + ": Too many dynamic call table entries (bogus value)");
	// Skip past header
	g_table += 0x4;
	// Copy whole table into vector
	struct DyncallEntry {
		uint32_t hash;
		uint32_t resv;
		uint32_t strname;
	};
	std::vector<DyncallEntry> table (entries);
	machine().copy_from_guest(table.data(), g_table, entries * sizeof(DyncallEntry));

	this->m_dyncall_array.resize(entries);

	for (size_t i = 0; i < entries; i++) {
		auto& entry = table.at(i);

		this->m_dyncall_array.at(i) = [this, i, entry] (auto&) {
			// First try to resolve
			auto it = m_dynamic_functions.find(entry.hash);
			if (LIKELY(it != m_dynamic_functions.end()))
			{
				it->second(*this);
				m_dyncall_array.at(i) = it->second;
				return;
			}
			// Fail with helpful message
			const std::string name = this->machine().memory.memstring(entry.strname);
			strf::to(stderr)(
				"WARNING: Unimplemented dynamic function '", name, "' with hash ", strf::hex(entry.hash), " and program table index ",
				this->m_dyncall_array.size(), "\n");
			throw std::runtime_error("The dynamic call " + name + " has not been implemented");
		};
	}
	if (m_dyncall_array.size() != entries)
		throw std::runtime_error("Mismatching number of dynamic call array entries");
}

void Script::set_global_setting(std::string_view setting, gaddr_t value)
{
	const uint32_t hash = riscv::crc32c(setting.begin(), setting.size());

	m_runtime_settings[hash] = value;
}

std::optional<gaddr_t> Script::get_global_setting(uint32_t hash)
{
	auto it = m_runtime_settings.find(hash);
	if (it != m_runtime_settings.end())
		return it->second;
	return std::nullopt;
}

std::optional<gaddr_t> Script::get_global_setting(std::string_view setting)
{
	const uint32_t hash = riscv::crc32c(setting.begin(), setting.size());
	return get_global_setting(hash);
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
