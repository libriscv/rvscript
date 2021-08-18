#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"

_Static_assert(ECALL_DYNCALL == 104,
	"The dynamic call syscall number is hard-coded in assembly");
__asm__(".global sys_breakpoint\n"
"sys_breakpoint:\n"
"	li a7, 104\n"
"	li t0, 0xcaaff686\n"
"	ecall\n"
"   ret\n");
extern long sys_breakpoint(uint16_t);

void console_print(const char* text, size_t tlen)
{
	syscall2(ECALL_WRITE, (long) text, tlen);
}

void remote_breakpoint(int port)
{
	// Dynamic call: "Debug::breakpoint"
	sys_breakpoint(port);
}
