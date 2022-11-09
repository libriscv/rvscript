#include "script.hpp"
using gaddr_t = Script::gaddr_t;

#include <libriscv/native_heap.hpp>
#include <libriscv/threads.hpp>
#include <libriscv/util/crc32.hpp>
#include <fstream> // Windows doesn't implement C getline()
#include <sstream>
#include "machine/include_api.hpp"
// the shared area is read-write for the guest
static constexpr gaddr_t STACK_BASE = (gaddr_t)0xffffffffc0000000;
static constexpr gaddr_t SHM_BASE   = 0x2000;
static constexpr gaddr_t SHM_SIZE   = 2 * riscv::Page::size();
static const int HEAP_SYSCALLS_BASE   = 570;
static const int MEMORY_SYSCALLS_BASE = 575;
static const int THREADS_SYSCALL_BASE = 590;
// Memory area used by remote function calls and such
static std::array<uint8_t, SHM_SIZE> shared_memory {};
std::map<uint32_t, Script::ghandler_t> Script::m_dynamic_functions {};
using riscv::crc32;

Script::Script(const machine_t& smach, void* userptr, const std::string& name,
	const std::string& filename, bool debug)
	: m_source_machine(smach), m_userptr(userptr), m_name(name), m_filename(filename),
	  m_hash(crc32(name.c_str())), m_is_debug(debug)
{
	this->reset();
}
Script::~Script() {}

bool Script::reset()
{
	try {
		// Fork the source machine into m_machine */
		riscv::MachineOptions<MARCH> options {
			.memory_max = MAX_MEMORY,
		};
		m_machine.reset(new machine_t(m_source_machine, options));

		// setup system calls and traps
		this->machine_setup();
		// ensures new stack pointer is set
		machine().cpu.reset_stack_pointer();
		// setup program argv *after* setting new stack pointer
		machine().setup_argv({name()});
	} catch (std::exception& e) {
		fmt::print(">>> Exception during initialization: {}\n", e.what());
		throw;
	}

	return true;
}

void Script::add_shared_memory()
{
	auto& mem = machine().memory;
	const auto stack_pageno = (mem.stack_initial() / riscv::Page::size()) - 1;
	const auto stack_baseno = STACK_BASE / riscv::Page::size();

	const auto stack_addr = stack_pageno * riscv::Page::size();
	mem.set_stack_initial(stack_addr);

	// Each thread should have its own stack, but the boundaries are
	// usually not known. To help out things like forked multiprocessing
	// we will add the boundaries for the main thread manually.
	auto* main_thread = machine().threads().get_thread();
	main_thread->stack_size = stack_addr - STACK_BASE;
	main_thread->stack_base = STACK_BASE;

	// Shared memory area between all programs
	mem.insert_non_owned_memory(SHM_BASE, &shared_memory[0], SHM_SIZE);

	// This separates the program, stack and shared memory areas
	mem.install_shared_page(stack_pageno,   riscv::Page::guard_page());
	mem.install_shared_page(stack_baseno-1, riscv::Page::guard_page());
}

bool Script::initialize()
{
	// clear some state belonging to previous initialization
	this->m_tick_event = 0;
	// run through the initialization
	try {
#ifdef RISCV_DEBUG
		// Verbose debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			machine().verbose_instructions = true;
#endif
		machine().simulate(MAX_INSTRUCTIONS);

		if (UNLIKELY(machine().instruction_counter() >= MAX_INSTRUCTIONS)) {
			fmt::print(stderr, ">>> Exception: Ran out of instructions\n");
			return false;
		}
	} catch (riscv::MachineException& me) {
		fmt::print(stderr,
			">>> Machine exception {}: {} (data: {:#x})\n",
			me.type(), me.what(), me.data());
#ifdef RISCV_DEBUG
		m_machine->print_and_pause();
#else
		// Remote debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			gdb_remote_debugging("", false);
#endif
		return false;
	} catch (std::exception& e) {
		fmt::print(stderr, ">>> Exception: {}\n", e.what());
		return false;
	}
    return true;
}
void Script::machine_setup()
{
	machine().set_userdata<Script>(this);
	machine().set_printer((machine_t::printer_func)
		[] (const machine_t&, const char* p, size_t len) {
			fmt::print(stderr, "{}", std::string_view{p, len});
		});
	machine().set_debug_printer(machine().get_printer());
	machine().set_stdin((machine_t::stdin_func)
		[](const machine_t &, char *, size_t)->long { return 0; });
	machine().on_unhandled_csr =
		[] (machine_t& machine, int csr, int, int) {
			auto& script = *machine.template get_userdata<Script> ();
			fmt::print(stderr, "{}: Unhandled CSR: {}\n", script.name(), csr);
		};
	// Add native system call interfaces
	machine().setup_native_heap(HEAP_SYSCALLS_BASE, heap_area(), MAX_HEAP);
	machine().setup_native_memory(MEMORY_SYSCALLS_BASE);
	machine().setup_native_threads(THREADS_SYSCALL_BASE);

	// Install shared memory area and guard pages
	this->add_shared_memory();
}
void Script::handle_exception(gaddr_t address)
{
	try {
		throw; // re-throw
	}
	catch (const riscv::MachineTimeoutException& e) {
		this->handle_timeout(address);
		return; // NOTE: might wanna stay
	}
	catch (const riscv::MachineException& e) {
		fmt::print(stderr, "Script::call exception: {} (data: {:#x})\n",
			e.what(), e.data());
		const auto instruction = machine().cpu.current_instruction_to_string();
		fmt::print(stderr, ">>> {}\n", instruction);
		fmt::print(stderr, ">>> Machine registers:\n[PC\t{:08x}] {}\n",
			(long) machine().cpu.pc(),
			machine().cpu.registers().to_string());
		// Remote debugging with DEBUG=1 ./engine
		if (getenv("DEBUG"))
			gdb_remote_debugging("", false);
	}
	catch (const std::exception& e) {
		fmt::print(stderr, "Script::call exception: {}\n",
			e.what());
	}
	fmt::print(stderr, "Program page: {}\n",
		machine().memory.get_page_info(machine().cpu.pc()));
	fmt::print(stderr, "Stack page: {}\n",
		machine().memory.get_page_info(machine().cpu.reg(2)));
	// Close active non-main thread (XXX: Probably not what we want)
	auto& mt = machine().threads();
	while (mt.get_tid() != 0) {
		auto* thread = mt.get_thread();
		fmt::print(stderr, "Script::call Closing running thread: {}\n",
			thread->tid);
		thread->exit();
	}

	auto callsite = machine().memory.lookup(address);
	fmt::print(stderr, "Function call: {}\n", callsite.name);
	this->print_backtrace(address);
}
void Script::handle_timeout(gaddr_t address)
{
	this->m_budget_overruns ++;
	auto callsite = machine().memory.lookup(address);
	fmt::print(stderr, "Script::call hit max instructions for: {}"
		" (Overruns: {})\n", callsite.name, m_budget_overruns);
	// Check if we need to suspend a thread
	auto& mt = machine().threads();
	auto* thread = mt.get_thread();
	if (thread->tid != 0) {
		// try to do the right thing here
		if (thread->block_reason != 0) {
			thread->block(thread->block_reason);
		} else {
			thread->suspend();
		}
		// resume some other thread
		mt.wakeup_next();
	}
}
void Script::print_backtrace(const gaddr_t addr)
{
	machine().memory.print_backtrace(
		[] (std::string_view line) {
			fmt::print("-> {}\n", line);
		});
	auto origin = machine().memory.lookup(addr);
	fmt::print("-> [-] 0x{:016x} + 0x{:03x}: {}\n",
			origin.address, origin.offset, origin.name);
}

void Script::print(std::string_view text)
{
	if (this->m_last_newline) {
		fmt::print(">>> [{}] says: {}", this->name(), text);
	} else {
		fmt::print("{}", text);
	}
	this->m_last_newline = (text.back() == '\n');
}

void Script::hash_public_api_symbols(std::string_view contents)
{
	std::stringstream infile {std::string(contents)};

	for (std::string line; infile >> line; )
	{
		m_public_api.insert({
			crc32(line.c_str()), machine().address_of(line.c_str())
		});
	}
}
void Script::hash_public_api_symbols_file(const std::string& file)
{
	/* Ignore symbol file when not specified */
	if (file.empty()) return;

	std::ifstream infile(file);
	if (!infile) {
		fmt::print(stderr,
			">>> Could not find symbols file: {}, ignoring...\n",
			file);
		return;
	}

	std::string str((std::istreambuf_iterator<char>(infile)),
	                 std::istreambuf_iterator<char>());

	this->hash_public_api_symbols(str);
}

gaddr_t Script::address_of(const std::string& name) const {
	return machine().address_of(name.c_str());
}
std::string Script::symbol_name(gaddr_t address) const
{
	auto callsite = machine().memory.lookup(address);
	return callsite.name;
}
gaddr_t Script::api_function_from_hash(uint32_t hash) {
	auto it = m_public_api.find(hash);
	if (it != m_public_api.end()) return it->second;
	return 0;
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
		if (thread->block_reason == this->m_tick_block_reason)
			count++;
	}
	this->preempt(this->m_tick_event, (int) count, (int) this->m_tick_block_reason);
	assert(mt.get_thread()->tid == 0 && "Avoid clobbering regs");
}

void Script::set_dynamic_call(const std::string& name, ghandler_t handler)
{
	const uint32_t hash = crc32(name.c_str(), name.size());
	//fmt::print("{}: {:x}\n", name, hash);
	auto it = m_dynamic_functions.find(hash);
	if (it != m_dynamic_functions.end()) {
		fmt::print("Dynamic function {} with hash {:#08x} already exists\n",
			name, hash);
		throw std::runtime_error("Script::set_dynamic_call failed: Hash collision with " + name);
	}
	m_dynamic_functions.emplace(hash, std::move(handler));
}
void Script::reset_dynamic_call(const std::string& name, ghandler_t handler)
{
	const uint32_t hash = crc32(name.c_str(), name.size());
	m_dynamic_functions.erase(hash);
	if (handler != nullptr) {
		set_dynamic_call(name, std::move(handler));
	}
}
void Script::set_dynamic_calls(std::vector<std::pair<std::string, ghandler_t>> vec)
{
	for (const auto& pair : vec) {
		set_dynamic_call(pair.first, std::move(pair.second));
	}
}

void Script::dynamic_call(uint32_t hash, gaddr_t straddr)
{
	auto it = m_dynamic_functions.find(hash);
	if (LIKELY(it != m_dynamic_functions.end())) {
		it->second(*this);
	} else {
		auto name = machine().memory.memstring(straddr);
		fmt::print("Unable to find dynamic function '{}' with hash: {:#08x}\n",
			name, hash);
		throw std::runtime_error("Unable to find dynamic function: " + name);
	}
}

gaddr_t Script::guest_alloc(gaddr_t bytes) {
	return machine().arena().malloc(bytes);
}
void Script::guest_free(gaddr_t addr) {
	machine().arena().free(addr);
}


inline timespec time_now();
inline long nanodiff(timespec start_time, timespec end_time);

template <int ROUNDS = 2000>
inline long perform_test(Script::machine_t& machine, gaddr_t func)
{
	const auto regs = machine.cpu.registers();
	const auto counter = machine.instruction_counter();
	const auto max_counter = machine.max_instructions();
	// this is a very hacky way of avoiding blowing up the stack
	// because vmcall() resets the stack pointer on each call
	auto old_stack = machine.memory.stack_initial();
	timespec t0 = {}, t1 = {};
	try {
		machine.memory.set_stack_initial(machine.cpu.reg(riscv::REG_SP) - 2048);
		// Warmup once
		machine.vmcall(func);
		asm("" : : : "memory");
		t0 = time_now();
		asm("" : : : "memory");
		for (int i = 0; i < ROUNDS; i++) {
			machine.vmcall(func);
		}
		asm("" : : : "memory");
		t1 = time_now();
		asm("" : : : "memory");
	} catch (...) {}
	machine.cpu.registers() = regs;
	machine.reset_instruction_counter();
	machine.increment_counter(counter);
	machine.set_max_instructions(max_counter);
	machine.memory.set_stack_initial(old_stack);
	return nanodiff(t0, t1);
}

long Script::vmbench(gaddr_t address, size_t ntimes)
{
	static constexpr size_t TIMES = 2000;

	std::vector<long> results;
	for (size_t i = 0; i < ntimes; i++)
	{
		perform_test<1>(*m_machine, address); // warmup
		results.push_back( perform_test<TIMES>(*m_machine, address) / TIMES );
	}
	return finish_benchmark(results);
}

long Script::benchmark(std::function<void()> callback, size_t TIMES)
{
	std::vector<long> results;
	for (int i = 0; i < 30; i++)
	{
		callback();
		asm("" : : : "memory");
		auto t0 = time_now();
		asm("" : : : "memory");
		for (size_t j = 0; j < TIMES; j++) {
			callback();
		}
		asm("" : : : "memory");
		auto t1 = time_now();
		asm("" : : : "memory");
		results.push_back( nanodiff(t0, t1) / TIMES );
	}
	return finish_benchmark(results);
}

long Script::finish_benchmark(std::vector<long>& results)
{
	std::sort(results.begin(), results.end());
	const long median = results[results.size() / 2];
	const long lowest = results[0];
	const long highest = results[results.size()-1];

	fmt::print("> median {}ns  \t\tlowest: {}ns     \thighest: {}ns\n",
			median, lowest, highest);
	return median;
}


timespec time_now()
{
	timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t;
}
long nanodiff(timespec start_time, timespec end_time)
{
	return (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
}
