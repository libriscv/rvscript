#include "script.hpp"

#include <libriscv/native_heap.hpp>
#include <stdexcept>

using gaddr_t							 = Script::gaddr_t;
static constexpr gaddr_t REMOTE_IMG_BASE = 0x50000000;

void Script::machine_remote_setup()
{
	if (heap_area() < REMOTE_IMG_BASE)
	{
		constexpr gaddr_t pages_max = MAX_MEMORY / riscv::Page::size();
		machine().memory.set_page_fault_handler(
			[this,
			 pages_max](auto& mem, const auto page, bool init) -> riscv::Page&
			{
				const auto addr = page * riscv::Page::size();
				if (addr < REMOTE_IMG_BASE)
				{
					// Normal path: Create and insert new page
					if (addr < mem.memory_arena_size())
					{
						const riscv::PageAttributes attr {
							.read  = true,
							.write = true,
							.non_owning = true
						};
						auto* pdata = (riscv::PageData *)mem.memory_arena_ptr();
						return mem.allocate_page(page, attr, &pdata[page]);
					}
					else if (mem.pages_active() < pages_max)
					{
						return mem.allocate_page(
							page, init ? riscv::PageData::INITIALIZED
									   : riscv::PageData::UNINITIALIZED);
					}
					throw riscv::MachineException(
						riscv::OUT_OF_MEMORY, "Out of memory", pages_max);
				}
				else if (this->m_remote_script != nullptr)
				{
					// Remote path: Create page on remote and use it here
					return this->m_remote_script->machine()
						.memory.create_writable_pageno(page);
				}
				throw std::runtime_error("No script for remote page fault");
			});
		machine().memory.set_page_readf_handler(
			[&](auto& mem, auto pageno) -> const riscv::Page&
			{
				// In order to differentiate between normal
				// execution and remote calls we will compare
				// PC against REMOTE_IMG_BASE here:
				const auto pc = this->machine().cpu.pc();

				if (this->m_remote_script != nullptr && pc < REMOTE_IMG_BASE
					&& pageno * riscv::Page::size() >= REMOTE_IMG_BASE)
				{
					return m_remote_script->machine().memory.get_pageno(
						pageno);
				}

				return riscv::Memory<MARCH>::default_page_read(mem, pageno);
			});
	} // level machine
	else
	{ // shared machine
		// Override the way we build new execute segments
		// on this machine.
		// If the segment is obviously outside of the image
		// space we can look for an existing alternative in
		// a potential connected remote script.
		this->machine().cpu.set_override_new_execute_segment(
			[](auto& cpu) -> riscv::DecodedExecuteSegment<MARCH>&
			{
				auto* this_script
					= cpu.machine().template get_userdata<Script>();
				auto* remote_script = this_script->m_remote_script;

				if (remote_script != nullptr && cpu.pc() < REMOTE_IMG_BASE)
				{
					auto& remote_machine = remote_script->machine();
					return *remote_machine.memory.exec_segment_for(cpu.pc());
				}

				return *cpu.empty_execute_segment();
			});
	}

	// ** Trap failed calls to free and realloc **
	auto& arena = machine().arena();
	arena.on_unknown_free(
		[this](auto address, auto*)
		{
			// We don't care who we are connected to, as long
			// as there is someone. Since we already failed to
			// deallocate locally (as the image spaces are
			// completely separate), we can just try again on
			// the remote, without worries.
			if (this->m_remote_script != nullptr)
			{
				return m_remote_script->machine().arena().free(address);
			}
			return -1;
		});
	arena.on_unknown_realloc(
		[this](auto address, size_t newsize)
		{
			if (this->m_remote_script != nullptr)
			{
				return m_remote_script->machine().arena().realloc(
					address, newsize);
			}
			return riscv::Arena::ReallocResult {0, 0};
		});

} // Script::machine_remote_setup()

void Script::setup_remote_calls_to(Script& dest)
{
	// Allow calling another pre-determined machine
	// by jumping directly to its functions.
	this->m_remote_script = &dest;

	machine().cpu.set_fault_handler(
		[](auto& cpu, auto&)
		{
			auto* this_script = cpu.machine().template get_userdata<Script>();
			auto* dest_script = this_script->m_remote_script;

			// Check if jump is inside the remote machines space
			if (dest_script != nullptr) // && cpu.pc() >= REMOTE_IMG_BASE)
			{
				auto& m = dest_script->machine();

#if 0
				// Copy all registers to destination
				m.cpu.registers().copy_from(
					riscv::Registers<MARCH>::Options::NoVectors,
					cpu.registers());
#else
				// Copy only argument registers to destination
				for (int i = 0; i < 8; i++)
				{
					m.cpu.reg(10 + i) = cpu.reg(10 + i);
				}
				for (int i = 0; i < 4; i++)
				{
					m.cpu.registers().getfl(10 + i)
						= cpu.registers().getfl(10 + i);
				}
#endif

				// Read faults happen when there is a read on a missing
				// page. It returns a zero CoW page normally.
				riscv::Memory<MARCH>::page_readf_cb_t old_read_handler;
				old_read_handler = m.memory.set_page_readf_handler(
					[&](auto&, auto pageno) -> const riscv::Page&
					{
						auto& remote_mem = cpu.machine().memory;

						if (pageno * riscv::Page::size() < REMOTE_IMG_BASE)
						{
							// The page is in the callers image space
							// so get the page from the caller:
							return remote_mem.get_pageno(pageno);
						}

						return remote_mem.default_page_read(remote_mem, pageno);
					});

				riscv::Memory<MARCH>::page_fault_cb_t old_fault_handler;
				// This handler makes writes to below the remote machines
				// image base become writes to this Script machine instead.
				// If they are larger than its base, they become normal pages.
				old_fault_handler = m.memory.set_page_fault_handler(
					[&](auto& mem, const auto pageno, bool init) -> riscv::Page&
					{
						if (pageno * riscv::Page::size() < REMOTE_IMG_BASE)
						{
							return cpu.machine().memory.create_writable_pageno(
								pageno, init);
						}
						return old_fault_handler(mem, pageno, init);
					});

				// Allow remote execution back in the caller
				auto* old_remote_script = dest_script->m_remote_script;
				// dest_script->setup_remote_calls_to(*this_script);
				dest_script->m_remote_script = this_script;

				// Start executing (on the remote)
				dest_script->call(cpu.pc());

				// No longer connected
				dest_script->m_remote_script = old_remote_script;

				// Restore read/fault handlers
				m.memory.set_page_readf_handler(std::move(old_read_handler));
				m.memory.set_page_fault_handler(std::move(old_fault_handler));
				// Invalidate all cached pages, just in case any
				// of them belong to the caller Script.
				m.memory.invalidate_reset_cache();

				// Penalize caller script by reducing max instructions
				cpu.machine().penalize(m.instruction_counter());

				// Return to calling function, with return regs 0 and 1
				cpu.reg(riscv::REG_ARG0) = m.cpu.reg(riscv::REG_ARG0);
				cpu.reg(riscv::REG_ARG1) = m.cpu.reg(riscv::REG_ARG1);
				cpu.jump(cpu.reg(riscv::REG_RA));
				return;
			}
			cpu.trigger_exception(
				riscv::EXECUTION_SPACE_PROTECTION_FAULT, cpu.pc());
		});
} // Script::setup_remote_calls_to()

void Script::setup_strict_remote_calls_to(Script& dest)
{
	// Allow calling another pre-determined machine
	// by jumping directly to its functions. However, access to the other
	// machine is limited to making calls into its public functions. This
	// can be enforced by using the remote machines public symbol list and
	// match it against something like syscall_XXX.
	this->m_remote_script = &dest;

	machine().cpu.set_fault_handler(
		[](auto& cpu, auto&)
		{
			auto* this_script = cpu.machine().template get_userdata<Script>();
			auto* dest_script = this_script->m_remote_script;

			// Check if jump is inside the remote machines space
			if (dest_script != nullptr) // && cpu.pc() >= REMOTE_IMG_BASE)
			{
				auto& m = dest_script->machine();
				// Lookup into unordered_map in order to validate the system call
				if (UNLIKELY(dest_script->m_remote_access.count(cpu.pc()) == 0))
					cpu.trigger_exception(
						riscv::EXECUTION_SPACE_PROTECTION_FAULT, cpu.pc());

				// Copy only argument registers to destination
				for (int i = 0; i < 8; i++)
				{
					m.cpu.reg(10 + i) = cpu.reg(10 + i);
				}
				for (int i = 0; i < 4; i++)
				{
					m.cpu.registers().getfl(10 + i)
						= cpu.registers().getfl(10 + i);
				}

				// Read faults happen when there is a read on a missing
				// page. It returns a zero CoW page normally.
				riscv::Memory<MARCH>::page_readf_cb_t old_read_handler;
				old_read_handler = m.memory.set_page_readf_handler(
					[&](auto&, auto pageno) -> const riscv::Page&
					{
						if (pageno * riscv::Page::size() < REMOTE_IMG_BASE)
						{
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
					[&](auto& mem, const auto pageno,
						bool init) -> riscv::Page&
					{
						if (pageno * riscv::Page::size() < REMOTE_IMG_BASE)
						{
							return cpu.machine().memory.create_writable_pageno(
								pageno, init);
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

				// Penalize caller script by reducing max instructions
				cpu.machine().penalize(m.instruction_counter());

				// Return to calling function, with return regs 0 and 1
				cpu.reg(riscv::REG_ARG0) = m.cpu.reg(riscv::REG_ARG0);
				cpu.reg(riscv::REG_ARG1) = m.cpu.reg(riscv::REG_ARG1);
				cpu.jump(cpu.reg(riscv::REG_RA));
				return;
			}
			cpu.trigger_exception(
				riscv::EXECUTION_SPACE_PROTECTION_FAULT, cpu.pc());
		});
} // Script::setup_remote_calls_to()
