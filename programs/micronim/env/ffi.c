#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"
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
