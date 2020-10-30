#include "script.hpp"
using gaddr_t = Script::gaddr_t;

#include "util/crc32.hpp"
#include <fstream> // Windows doesn't implement C getline()
#include <sstream>
#include <include/syscall_helpers.hpp>
#include <include/threads.hpp>
#include "machine/include_api.hpp"

static const gaddr_t MAX_MEMORY    = 1024*1024 * 16;
static const gaddr_t MAX_HEAP      = 1024*1024 * 8;
static const bool    TRUSTED_CALLS = true;
// the shared area is read-write for the guest
std::array<riscv::Page, 2> Script::g_shared_area;
// the hidden area is read-only for the guest
riscv::Page Script::g_hidden_stack {{ .write = false }};

Script::Script(const machine_t& smach, const std::string& name)
	: m_source_machine(smach), m_name(name), m_hash(crc32(name.c_str()))
{
	for (int n = ECALL_LAST; n < RISCV_SYSCALLS_MAX; n++) {
		m_free_sysno.push_back(n);
	}
	this->reset();
}
Script::~Script() {}

bool Script::reset()
{
	try {
		riscv::MachineOptions<MARCH> options {
			.memory_max = MAX_MEMORY,
			.owning_machine = &this->m_source_machine
		};
		m_machine.reset(new machine_t(
			m_source_machine.memory.binary(), options));

		// unfortunately, we need to clear all the groups here
		// as a reset will remove all pages in guest memory,
		// which invalidates all groups. this will also give
		// all the used system call numbers back.
		m_groups.clear();

	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		// TODO: shutdown engine?
		exit(1);
	}
	if (this->machine_initialize()) {
		this->m_crashed = false;
		return true;
	}
	return false;
}

void Script::add_shared_memory()
{
	const int shared_pageno = shared_memory_location() >> riscv::Page::SHIFT;
	const int heap_pageno   = 0x40000000 >> riscv::Page::SHIFT;
	const int stack_pageno  = heap_pageno - 2;
	auto& mem = machine().memory;
	mem.set_stack_initial((gaddr_t) stack_pageno << riscv::Page::SHIFT);
	// Install our shared guard-page around the shared-
	// memory area, put the shared page in the middle.
	auto& guard_page = riscv::Page::guard_page();
	mem.install_shared_page(shared_pageno-1, guard_page);
	for (size_t i = 0; i < g_shared_area.size(); i++)
		mem.install_shared_page(shared_pageno+i,   g_shared_area[i]);
	mem.install_shared_page(shared_pageno+g_shared_area.size(), guard_page);
	// this separates heap and stack
	mem.install_shared_page(stack_pageno,    guard_page);
	// install a hidden area that the internal APIs use
	mem.install_shared_page(HIDDEN_AREA >> riscv::Page::SHIFT, g_hidden_stack);
}

bool Script::machine_initialize()
{
	// setup system calls and traps
	this->machine_setup(machine());
	// install the shared memory area
	this->add_shared_memory();
	// clear some state belonging to previous initialization
	this->m_tick_event = 0;
	// run through the initialization
#ifdef RISCV_DEBUG
	this->enable_debugging();
	//machine().verbose_registers = true;
#endif
	try {
		machine().simulate(MAX_INSTRUCTIONS);

		if (UNLIKELY(machine().cpu.instruction_counter() >= MAX_INSTRUCTIONS)) {
			printf(">>> Exception: Ran out of instructions\n");
			return false;
		}
	} catch (riscv::MachineException& me) {
		printf(">>> Machine exception %d: %s (data: 0x%lX)\n",
				me.type(), me.what(), me.data());
#ifdef RISCV_DEBUG
		m_machine->print_and_pause();
#endif
		return false;
	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		return false;
	}
    return true;
}
void Script::machine_setup(machine_t& machine)
{
	machine.set_userdata<Script>(this);
	machine.memory.set_exit_address(machine.address_of("exit"));
	if (UNLIKELY(machine.memory.exit_address() == 0))
		throw std::runtime_error("Exit function not visible/available in program");
	// add system call interface
	auto* arena = setup_native_heap_syscalls<MARCH>(machine, MAX_HEAP);
	setup_native_memory_syscalls<MARCH>(machine, TRUSTED_CALLS);
	this->m_threads = setup_native_threads<MARCH>(machine, arena);
    setup_syscall_interface(machine);
	machine.on_unhandled_syscall(
		[] (int number) {
			printf("Unhandled system call: %d\n", number);
		});

	// we need to pass the .eh_frame location to a supc++ function,
	// if C++ RTTI and Exceptions is enabled
	machine.cpu.reg(11) = machine.memory.resolve_section(".eh_frame");
}
void Script::handle_exception(gaddr_t address)
{
	try {
		throw; // re-throw
	}
	catch (const riscv::MachineException& e) {
		fprintf(stderr, "Script::call exception: %s (data: 0x%lX)\n", e.what(), e.data());
		fprintf(stderr, ">>> Machine registers:\n[PC\t%08lX] %s\n",
			(long) machine().cpu.pc(),
			machine().cpu.registers().to_string().c_str());
	}
	catch (const riscv::MachineTimeoutException& e) {
		this->handle_timeout(address);
		return; // NOTE: might wanna stay
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Script::call exception: %s\n", e.what());
	}
	printf("Program page: %s\n", machine().memory.get_page_info(machine().cpu.pc()).c_str());
	printf("Stack page: %s\n", machine().memory.get_page_info(machine().cpu.reg(2)).c_str());
	// close all threads
	auto* mt = (multithreading<MARCH>*) m_threads;
	while (mt->get_thread()->tid != 0) {
		auto* thread = mt->get_thread();
		fprintf(stderr, "Script::call Closing running thread: %d\n", thread->tid);
		thread->exit();
	}

	auto callsite = machine().memory.lookup(address);
	fprintf(stderr, "Function call: %s\n", callsite.name.c_str());
	this->print_backtrace(address);
}
void Script::handle_timeout(gaddr_t address)
{
	this->m_budget_overruns ++;
	auto callsite = machine().memory.lookup(address);
	fprintf(stderr, "Script::call hit max instructions for: %s"
		" (Overruns: %d)\n", callsite.name.c_str(), m_budget_overruns);
	// check if we need to suspend a thread
	auto* mt = (multithreading<MARCH>*) m_threads;
	auto* thread = mt->get_thread();
	if (thread->tid != 0) {
		// try to do the right thing here
		if (thread->block_reason != 0) {
			thread->block(thread->block_reason);
		} else {
			thread->suspend();
		}
		// resume some other thread
		mt->wakeup_next();
	}
}
void Script::print_backtrace(const gaddr_t addr)
{
	machine().memory.print_backtrace(
		[] (const char* buffer, size_t len) {
			printf("-> %.*s\n", (int)len, buffer);
		});
	auto origin = machine().memory.lookup(addr);
	printf("-> [-] 0x%08x + 0x%.3x: %s\n",
			origin.address, origin.offset, origin.name.c_str());
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
		printf(">>> Could not find symbols file: %s, ignoring...\n",
				file.c_str());
		return;
	}

	std::string str((std::istreambuf_iterator<char>(infile)),
	                 std::istreambuf_iterator<char>());

	this->hash_public_api_symbols(str);
}

gaddr_t Script::resolve_address(const std::string& name) const {
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
	auto* mt = (multithreading<MARCH>*) this->m_threads;
	assert(mt->get_thread()->tid == 0 && "Avoid clobbering regs");

	int count = 0;
	for (auto* thread : mt->blocked)
	{
		if (thread->block_reason == this->m_tick_block_reason)
			count++;
	}
	this->preempt(this->m_tick_event, (int) count, (int) this->m_tick_block_reason);
	assert(mt->get_thread()->tid == 0 && "Avoid clobbering regs");
}

int Script::set_dynamic_function(int gid, int index, ghandler_t handler)
{
	auto it = m_groups.find(gid);
	if (it == m_groups.end()) {
		it = m_groups.try_emplace(gid, gid, *this).first;
	}
	auto& group = it->second;
	return group.install(index, std::move(handler));
}

void Script::enable_debugging()
{
#ifdef RISCV_DEBUG
	machine().verbose_instructions = true;
#endif
}

inline timespec time_now();
inline long nanodiff(timespec start_time, timespec end_time);

template <int ROUNDS = 2000>
inline long perform_test(Script::machine_t& machine, gaddr_t func)
{
	auto regs = machine.cpu.registers();
	auto counter = machine.cpu.instruction_counter();
	// this is a very hacky way of avoiding blowing up the stack
	// because vmcall() resets the stack pointer on each call
	auto old_stack = machine.memory.stack_initial();
	machine.memory.set_stack_initial(machine.cpu.reg(riscv::RISCV::REG_SP) & ~0xF);
	asm("" : : : "memory");
	auto t0 = time_now();
	asm("" : : : "memory");
	for (int i = 0; i < ROUNDS; i++) {
		machine.vmcall(func);
	}
	asm("" : : : "memory");
	auto t1 = time_now();
	asm("" : : : "memory");
	machine.cpu.registers() = regs;
	machine.cpu.reset_instruction_counter();
	machine.cpu.increment_counter(counter);
	machine.memory.set_stack_initial(old_stack);
	machine.stop(false);
	return nanodiff(t0, t1);
}

long Script::measure(gaddr_t address)
{
	static constexpr size_t TIMES = 2000;

	std::vector<long> results;
	for (int i = 0; i < 200; i++)
	{
		perform_test<1>(*m_machine, address); // warmup
		results.push_back( perform_test<TIMES>(*m_machine, address) );
	}
	std::sort(results.begin(), results.end());
	long median = results[results.size() / 2] / TIMES;
	long lowest = results[0] / TIMES;
	long highest = results[results.size()-1] / TIMES;

	printf("> median %ldns  \t\tlowest: %ldns     \thighest: %ldns\n",
			median, lowest, highest);
	return median;
}

timespec time_now()
{
	timespec t;
#ifdef __linux__
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
#else
	clock_gettime(CLOCK_MONOTONIC, &t);
#endif
	return t;
}
long nanodiff(timespec start_time, timespec end_time)
{
	return (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
}
