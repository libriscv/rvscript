#include "script.hpp"
#include <libriscv/native_heap.hpp>

using gaddr_t = Script::gaddr_t;
static constexpr gaddr_t REMOTE_IMG_BASE = 0x50000000;

void Script::machine_remote_setup()
{
	if (heap_area() < REMOTE_IMG_BASE)
	{
		constexpr gaddr_t pages_max = MAX_MEMORY / riscv::Page::size();
		machine().memory.set_page_fault_handler(
		[this, pages_max] (auto& mem, const auto page, bool init) -> riscv::Page&
		{
			if (page * riscv::Page::size() < REMOTE_IMG_BASE)
			{
				// Normal path: Create and insert new page
				if (mem.pages_active() < pages_max)
				{
					return mem.allocate_page(page,
						init ? riscv::PageData::INITIALIZED : riscv::PageData::UNINITIALIZED);
				}
				throw riscv::MachineException(riscv::OUT_OF_MEMORY, "Out of memory", pages_max);
			}
			else if (this->m_call_dest != nullptr) {
				// Remote path: Create page on remote and use it here
				return this->m_call_dest->machine().memory.create_writable_pageno(page);
			}
			throw std::runtime_error("No script for remote page fault");
		});
		machine().memory.set_page_readf_handler(
		[&] (auto&, auto pageno) -> const riscv::Page&
		{
			// In order to differentiate between normal
			// execution and remote calls we will compare
			// PC against REMOTE_IMG_BASE here:
			const auto pc = this->machine().cpu.pc();

			if (this->m_call_dest != nullptr &&
				pc < REMOTE_IMG_BASE &&
				pageno * riscv::Page::size() >= REMOTE_IMG_BASE)
			{
				return m_call_dest->machine().memory.get_pageno(pageno);
			}

			return riscv::Page::cow_page();
		});

		// ** Trap failed calls to free **
		// It is OK to free pointers from the remote machine
		// because the remote exists at all times, and there
		// is no conflict here. It is not possible the other
		// way around, because levels come and go.
		auto& arena = machine().arena();
		arena.on_unknown_free(
		[this] (auto address, auto*) {
			if (this->m_call_dest != nullptr &&
				address >= REMOTE_IMG_BASE)
			{
				//fmt::print("freeing remote pointer: {:x}\n", address);
				return m_call_dest->machine().arena().free(address);
			}
			return -1;
		});
	}
} // Script::machine_remote_setup()

void Script::setup_remote_calls_to(Script& dest)
{
	// Allow calling another pre-determined machine
	// by jumping directly to its functions.
	this->m_call_dest = &dest;

	machine().cpu.set_fault_handler(
	[] (auto& cpu, auto&)
	{
		auto* this_script = cpu.machine().template get_userdata<Script>();
		auto* dest_script = this_script->m_call_dest;

		// Check if jump is inside the remote machines space
		if (dest_script != nullptr && cpu.pc() >= REMOTE_IMG_BASE)
		{
			auto& m = dest_script->machine();

		#if 0
			// Copy all registers to destination
			m.cpu.registers().copy_from(
				riscv::Registers<MARCH>::Options::NoVectors,
				cpu.registers());
		#else
			// Copy only argument registers to destination
			for (int i = 0; i < 8; i++) {
				m.cpu.reg(10 + i) = cpu.reg(10 + i);
			}
			for (int i = 0; i < 4; i++) {
				m.cpu.registers().getfl(10 + i) = cpu.registers().getfl(10 + i);
			}
		#endif

			// Read faults happen when there is a read on a missing
			// page. It returns a zero CoW page normally.
			riscv::Memory<MARCH>::page_readf_cb_t old_read_handler;
			old_read_handler = m.memory.set_page_readf_handler(
			[&] (auto&, auto pageno) -> const riscv::Page& {
				if (pageno * riscv::Page::size() < REMOTE_IMG_BASE) {
					// The page is in the callers image space
					// so get the page from the caller:
					return cpu.machine().memory.get_pageno(pageno);
				}
				return riscv::Page::cow_page();
			});

			riscv::Memory<MARCH>::page_fault_cb_t old_fault_handler;
			// This handler makes writes to below the remote machines
			// image base become writes to this Script machine instead.
			// If they are larger than its base, they become normal pages.
			old_fault_handler = m.memory.set_page_fault_handler(
			[&] (auto& mem, const auto pageno, bool init) -> riscv::Page& {
				if (pageno * riscv::Page::size() < REMOTE_IMG_BASE) {
					return cpu.machine().memory.create_writable_pageno(pageno, init);
				}
				return old_fault_handler(mem, pageno, init);
			});

			// Start executing (on the remote)
			dest_script->call(cpu.pc());

			// Restore read/fault handlers
			m.memory.set_page_readf_handler(std::move(old_read_handler));
			m.memory.set_page_fault_handler(std::move(old_fault_handler));
			// Invalidate all cached pages, just in case any
			// of them belong to the caller Script.
			m.memory.invalidate_reset_cache();

			// Penalize by reducing max instructions
			cpu.machine().penalize(m.instruction_counter());
			// Return to caller, with return regs 0 and 1
			cpu.reg(riscv::REG_ARG0) = m.cpu.reg(riscv::REG_ARG0);
			cpu.reg(riscv::REG_ARG1) = m.cpu.reg(riscv::REG_ARG1);
			cpu.jump(cpu.reg(riscv::REG_RA));
			return;
		}
		cpu.trigger_exception(riscv::EXECUTION_SPACE_PROTECTION_FAULT, cpu.pc());
	});
} // Script::setup_remote_calls_to()
