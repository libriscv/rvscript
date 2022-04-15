#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"
#include <dyncall_api.h>

void console_print(const char* text, size_t tlen)
{
	syscall2(ECALL_WRITE, (long) text, tlen);
}

void breakpoint(const char* msg)
{
	// Dynamic call: "Debug::breakpoint"
	sys_breakpoint(0, msg);
}
