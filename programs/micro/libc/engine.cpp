#include "engine.hpp"

void halt()
{
	asm (".insn i SYSTEM, 0, x0, x0, 0x7ff");
}

SharedMemoryArea::SharedMemoryArea()
	: m_shm {*(SharedMemoryArray *)(void*)SHM_BASE}
{}
void* SharedMemoryArea::push(const void* data, size_t size, size_t align)
{
	auto* dst = this->realign(size, align);
	return memcpy(dst, data, size);
}

static_assert(ECALL_WRITE == 502,
	"The write syscall number is hard-coded in assembly");
asm(".global sys_write\n"
"sys_write:\n"
"	li a7, 502\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_FARCALL == 505,
	"The farcall syscall number is hard-coded in assembly");
asm(".global farcall_helper\n"
"farcall_helper:\n"
"	li a7, 505\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_FARCALL_DIRECT == 506,
	"The direct farcall syscall number is hard-coded in assembly");
asm(".global direct_farcall_helper\n"
"direct_farcall_helper:\n"
"	li a7, 506\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_INTERRUPT == 507,
	"The interrupt syscall number is hard-coded in assembly");
asm(".global sys_interrupt\n"
"sys_interrupt:\n"
"	li a7, 507\n"
"	ecall\n"
"   ret\n"
""); // The system call handler must jump back to caller

static_assert(ECALL_MULTIPROCESS == 510,
	"The multiprocess syscall number is hard-coded in assembly");
asm(".global sys_multiprocess\n"
"sys_multiprocess:\n"
"	li a7, 510\n"
"	ecall\n"
"   beqz a0, sys_multiprocess_ret\n" // Early return for vCPU 0
// Otherwise, create a function call
"   addi a0, a0, -1\n" // Subtract 1 from vCPU ID, making it 0..N-1
"   mv a1, a4\n"       // Move work data to argument 1
"   jalr zero, a3\n"   // Direct jump to work function
"sys_multiprocess_ret:\n"
"   ret\n");           // Return to caller

static_assert(ECALL_MULTIPROCESS_FORK == 511,
	"The multiprocess_fork syscall number is hard-coded in assembly");
asm(".global sys_multiprocess_fork\n"
"sys_multiprocess_fork:\n"
"	li a7, 511\n"
"	ecall\n"
"   ret\n");           // Return to caller

static_assert(ECALL_MULTIPROCESS_WAIT == 512,
	"The multiprocess_wait syscall number is hard-coded in assembly");
asm(".global sys_multiprocess_wait\n"
"sys_multiprocess_wait:\n"
"	li a7, 512\n"
"	ecall\n"
"   ret\n");
