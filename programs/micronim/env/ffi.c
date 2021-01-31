#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"

void console_print(const char* text, size_t tlen)
{
	syscall2(ECALL_WRITE, (long) text, tlen);
}

void remote_breakpoint(int port)
{
	// Dynamic call: "remote_gdb"
	syscall2(ECALL_DYNCALL, 0x8c68ab32, port);
}
