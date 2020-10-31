#include "function_group.hpp"

#include "machine/syscalls.h"
#include "script.hpp"

uint64_t FunctionGroup::group_area() noexcept {
	return FUNCTION_GROUP_AREA;
}
size_t FunctionGroup::calculate_group(uint64_t pc) noexcept {
	if (pc >= FunctionGroup::group_area()) {
		pc -= FunctionGroup::group_area();
		return pc / FunctionGroup::GROUP_BYTES;
	}
	return (size_t) -1;
}
size_t FunctionGroup::calculate_group_index(uint64_t pc) noexcept {
	if (pc >= FunctionGroup::group_area()) {
		pc -= FunctionGroup::group_area();
		return (pc / 8) % FunctionGroup::GROUP_SIZE;
	}
	return (size_t) -1;
}

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
	m_sysno = request_number();
	// install our syscall wrapper which calls the real handler
	// and then safely returns back to the guests caller
	m_script.machine().install_syscall_handler(m_sysno,
		[this] (auto& m) {
			auto index = (m.cpu.pc() / 8) % GROUP_SIZE;
			auto& handler = m_syscall_handlers.at(index);
			// call handler (std::function already checks null)
			handler(m_script);
			// return back to caller before returning
			m.cpu.jump(m.cpu.reg(riscv::RISCV::REG_RA) - 4);
			return m.cpu.reg(riscv::RISCV::REG_RETVAL);
		});
}
FunctionGroup::~FunctionGroup() {
	free_number(this->m_sysno);
}

void FunctionGroup::install(unsigned idx, ghandler_t callback)
{
	assert(idx < GROUP_SIZE);
	if (idx >= m_syscall_handlers.size()) {
		m_syscall_handlers.resize(idx+1);
	}
	auto& handler = m_syscall_handlers[idx];
	handler = std::move(callback);

	// Create machine code in guests virtual memory
	if (handler != nullptr) {
#ifdef RISCV_SI_SYSCALLS
		// The first two entries are used by ECALL and EBREAK
		static_assert( ECALL_LAST >= 2 );
		m_data[idx*2+0] = 0x73  | ((m_sysno & 0x7FF) << 20); // ECALL.IMM
#else
		m_data[idx*2+0] = 0x893 | ((m_sysno & 0x7FF) << 20); // LI
		m_data[idx*2+1] = 0x73; // ECALL
#endif
	} else {
		m_data[idx*2+0] = 0x0;
		m_data[idx*2+1] = 0x0;
	}
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
