#include <syscall.h>
#include <syscalls.h>
#include "ffi.h"

void console_print(const char* text, size_t tlen)
{
	syscall2(ECALL_WRITE, (long) text, tlen);
}
