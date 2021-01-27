#include "engine.hpp"

__attribute__((noinline)) void trap()
{
	__builtin_trap();
}

void halt()
{
	asm (".insn i SYSTEM, 0, x0, x0, 0x7ff");
}

static_assert(ECALL_DYNCALL == 104,
	"The dynamic call syscall number is hard-coded in assembly");
asm(".global dyncall_helper\n"
"dyncall_helper:\n"
"	li a7, 104\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_FARCALL == 105,
	"The farcall syscall number is hard-coded in assembly");
asm(".global farcall_helper\n"
"farcall_helper:\n"
"	li a7, 105\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_FARCALL_DIRECT == 106,
	"The direct farcall syscall number is hard-coded in assembly");
asm(".global direct_farcall_helper\n"
"direct_farcall_helper:\n"
"	li a7, 106\n"
"	ecall\n"
"   ret\n");

static_assert(ECALL_INTERRUPT == 107,
	"The interrupt syscall number is hard-coded in assembly");
asm(".global interrupt_helper\n"
"interrupt_helper:\n"
"	li a7, 107\n"
"	ecall\n"
"   ret\n"
""); // The system call handler must jump back to caller
