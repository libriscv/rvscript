#include "script_functions.hpp"

#include "util/crc32.hpp"
#include <fstream> // Windows doesn't implement C getline()
#include <include/syscall_helpers.hpp>
#include <include/threads.hpp>
#include "machine/include_api.hpp"

static const uint32_t MAX_MEMORY    = 1024*1024 * 2;
static const uint32_t MAX_HEAP      = 1024*1024 * 6;
static const bool     TRUSTED_CALLS = true;
Script* Script::g_current_script = nullptr;
std::array<riscv::Page, 2> Script::g_shared_area;
riscv::Page Script::g_hidden_stack;

void Script::init()
{
	for (auto& shpage : g_shared_area)
		shpage.attr.shared = true;
	// the hidden area is read-only for the guest
	g_hidden_stack.attr.write  = false;
	g_hidden_stack.attr.shared = true;
}

Script::Script(std::shared_ptr<std::vector<uint8_t>>& binary,
	const std::string& name)
	: m_binary(binary), m_name(name), m_hash(crc32(name.c_str()))
{
	this->reset(true);
}

Script::~Script() {}

bool Script::reset(bool shared)
{
	try {
		riscv::MachineOptions options {
			.memory_max = MAX_MEMORY
		};
		riscv::verbose_machine = false;
		m_machine.reset(new riscv::Machine<riscv::RISCV32> (
			*this->m_binary, options));
	} catch (std::exception& e) {
		printf(">>> Exception: %s\n", e.what());
		// TODO: shutdown engine?
		exit(1);
	}
	if (this->machine_initialize(shared)) {
		this->m_crashed = false;
		return true;
	}
	return false;
}

void Script::add_shared_memory()
{
	const int shared_pageno = shared_memory_location() >> riscv::Page::SHIFT;
	const int heap_pageno   = 0x40000000 >> riscv::Page::SHIFT;
	const int stack_pageno  = heap_pageno - 1;
	auto& mem = machine().memory;
	mem.set_stack_initial((uint32_t) stack_pageno << riscv::Page::SHIFT);
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

bool Script::machine_initialize(bool shared)
{
	// setup system calls and traps
	this->machine_setup(machine());
	// install the shared memory area
	if (shared) this->add_shared_memory();
	// clear some state belonging to previous initialization
	this->m_tick_event = 0;
	// run through the initialization
	Script::g_current_script = this;
	try {
		machine().simulate(MAX_INSTRUCTIONS);

		if (UNLIKELY(machine().cpu.instruction_counter() == MAX_INSTRUCTIONS)) {
			printf(">>> Exception: Ran out of instructions\n");
			return false;
		}
	} catch (riscv::MachineException& me) {
		printf(">>> Machine exception %d: %s (data: %d)\n",
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
void Script::machine_setup(riscv::Machine<riscv::RISCV32>& machine)
{
	// add system call interface
	auto* arena = setup_native_heap_syscalls<4>(machine, MAX_HEAP);
	setup_native_memory_syscalls<4>(machine, TRUSTED_CALLS);
	this->m_threads = setup_native_threads<4>(machine, arena);
    setup_syscall_interface(machine);

	// create execute trapping syscall page
	// this is the last page in the 32-bit address space
	auto& page = machine.memory.create_page(0xFFFFF);
	// create an execution trap on the page
	page.set_trap(
		[&machine] (riscv::Page&, uint32_t sysn, int, int64_t) -> int64_t {
			// invoke a system call
			machine.system_call(1024 - sysn / 4);
			// return to caller
#ifndef RISCV_DEBUG
			const auto retaddr = machine.cpu.reg(riscv::RISCV::REG_RA);
			machine.cpu.jump(retaddr);
			return 0;
#else
			// in debug mode we need to return the RET instruction here
			// because we rely on memory reads to trap
			return 0x8067;
#endif
		});

	// we need to pass the .eh_frame location to a supc++ function,
	// if C++ RTTI and Exceptions is enabled
	machine.cpu.reg(11) = machine.memory.resolve_section(".eh_frame");
}
void Script::handle_exception(uint32_t address)
{
	try {
		throw;
	}
	catch (const riscv::MachineException& e) {
		fprintf(stderr, "Script::call exception: %s (data: %d)\n", e.what(), e.data());
		fprintf(stderr, ">>> Machine registers:\n%s\n", machine().cpu.registers().to_string().c_str());
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Script::call exception: %s\n", e.what());
	}
	// close all threads
	auto* mt = (multithreading<4>*) m_threads;
	while (mt->get_thread()->tid != 0) {
		auto* thread = mt->get_thread();
		fprintf(stderr, "Script::call Closing running thread: %d\n", thread->tid);
		thread->exit();
	}

	auto callsite = machine().memory.lookup(address);
	fprintf(stderr, "Function call: %s\n", callsite.name.c_str());
	this->print_backtrace(address);
}
void Script::handle_timeout(uint32_t address)
{
	this->m_budget_overruns ++;
	auto callsite = machine().memory.lookup(address);
	fprintf(stderr, "Script::call hit max instructions for: %s"
		" (Overruns: %d)\n", callsite.name.c_str(), m_budget_overruns);
	// check if we need to suspend a thread
	auto* mt = (multithreading<4>*) m_threads;
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
void Script::print_backtrace(const uint32_t addr)
{
	machine().memory.print_backtrace(
		[] (const char* buffer, size_t len) {
			printf("-> %.*s\n", (int)len, buffer);
		});
	auto origin = machine().memory.lookup(addr);
	printf("-> [-] 0x%08x + 0x%.3x: %s\n", 
			origin.address, origin.offset, origin.name.c_str());
}

void Script::hash_public_api_symbols(const std::string& file)
{
	std::ifstream infile(file);
	if (!infile) {
		printf(">>> Could not find symbols file: %s, ignoring...\n",
				file.c_str());
		return;
	}

	std::string line;
	for (std::string line; std::getline(infile, line); )
	{
		if (!line.empty()) {
			m_public_api.insert({
				crc32(line.c_str()), machine().address_of(line.c_str())
			});
		}
	}
}
uint32_t Script::resolve_address(const std::string& name) const {
	return machine().address_of(name.c_str());
}
std::string Script::symbol_name(uint32_t address) const
{
	auto callsite = machine().memory.lookup(address);
	return callsite.name;
}
uint32_t Script::api_function_from_hash(uint32_t hash) {
	auto it = m_public_api.find(hash);
	if (it != m_public_api.end()) return it->second;
	return 0;
}

void Script::each_tick_event()
{
	if (this->m_tick_event == 0)
		return;
	auto* mt = (multithreading<4>*) this->m_threads;
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

inline timespec time_now();
inline long nanodiff(timespec start_time, timespec end_time);

template <int ROUNDS = 2000>
inline long perform_test(riscv::Machine<4>& machine, uint32_t func)
{
	auto regs = machine.cpu.registers();
	auto counter = machine.cpu.instruction_counter();
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
	machine.stop(false);
	return nanodiff(t0, t1);
}

long Script::measure(uint32_t address)
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
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
	return t;
}
long nanodiff(timespec start_time, timespec end_time)
{
	return (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
}
