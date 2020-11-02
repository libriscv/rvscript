#include "engine.hpp"

__attribute__((noinline)) void halt()
{
	asm("ebreak" ::: "memory");
}

static_assert(ECALL_FARCALL == 104,
	"The farcall syscall number is hard-coded in assembly");
asm(".global farcall_helper\n"
"farcall_helper:\n"
"	li a7, 104\n"
"	ecall\n" // Note how there is no ret here
""); // The system call handler must jump back to caller

static_assert(ECALL_FARCALL_DIRECT == 105,
	"The direct farcall syscall number is hard-coded in assembly");
asm(".global direct_farcall_helper\n"
"direct_farcall_helper:\n"
"	li a7, 105\n"
"	ecall\n" // Note how there is no ret here
""); // The system call handler must jump back to caller
