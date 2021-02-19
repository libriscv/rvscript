#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"

_Static_assert(ECALL_DYNCALL == 104,
	"The dynamic call syscall number is hard-coded in assembly");
__asm__(".global dyncall_helper\n"
"dyncall_helper:\n"
"	li a7, 104\n"
"	ecall\n"
"   ret\n");
extern long dyncall_helper(uint32_t, ...);

void console_print(const char* text, size_t tlen)
{
	syscall2(ECALL_WRITE, (long) text, tlen);
}

void remote_breakpoint(int port)
{
	// Dynamic call: "remote_gdb"
	dyncall_helper(0x8c68ab32, port);
}
