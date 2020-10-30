#include "function_group.hpp"

#include "machine/syscalls.h"
#include "script.hpp"

FunctionGroup::datatype_t& FunctionGroup::create_dataref(int gid, Script& s)
{
	static_assert(riscv::Page::size() % GROUP_BYTES == 0, "Groups must be PO2 less than page size");
	static constexpr size_t GROUPS_PER_PAGE = riscv::Page::size() / GROUP_BYTES;
	// If the page is new, which it may not be as pages can host several
	// groups depending on their size, then the page is already zeroed.
	auto& mem = s.machine().memory;
	auto& p = mem.create_page(FUNCTION_GROUP_AREA >> riscv::Page::SHIFT);
	p.attr.read  = false;
	p.attr.write = false;
	p.attr.exec  = true;
	assert(p.has_data());

	return *(datatype_t*)
		(p.data() + GROUP_BYTES * (gid % GROUPS_PER_PAGE));
}

FunctionGroup::FunctionGroup(int gid, Script& s)
	: m_script(s), m_data(create_dataref(gid, s))
{
	// NOTE: we may want to write helpful instructions to
	// each entry, as you will get "Illegal opcode" right now.
}
FunctionGroup::~FunctionGroup()
{
	for (auto sysno : m_syscall_numbers)
		if (sysno != 0) free_number(sysno);
}

int FunctionGroup::install(int idx, ghandler_t callback)
{
	auto& handler = m_syscall_handlers.at(idx);
	handler = std::move(callback);
	if (idx >= m_syscall_numbers.size())
		m_syscall_numbers.resize(idx+1);
	// request new system call number if not set
	auto& sysno = m_syscall_numbers[idx];
	if (sysno == 0) sysno = request_number();
	// create machine code in guests virtual memory
	m_data[idx*2+0] = 0x893 | ((sysno & 0x7FF) << 20); // LI
	m_data[idx*2+1] = 0x73; // ECALL
	// install our syscall wrapper which calls the real handler
	// and then safely returns back to the guests caller
	m_script.machine().install_syscall_handler(sysno,
		[&handler] (auto& m) {
			// return back to caller immediately
			m.cpu.jump(m.cpu.reg(riscv::RISCV::REG_RA) - 4);
			// call custom handler
			handler(m);
			return m.cpu.reg(riscv::RISCV::REG_RETVAL);
		});
	return sysno;
}

void FunctionGroup::free_number(int sysno)
{
	m_script.m_free_sysno.push_back(sysno);
}
int FunctionGroup::request_number()
{
	assert(!m_script.m_free_sysno.empty());
	int number = m_script.m_free_sysno.back();
	m_script.m_free_sysno.pop_back();
	return number;
}
