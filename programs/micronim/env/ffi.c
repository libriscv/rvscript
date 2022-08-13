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

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
__asm__(".section .text\n.align 8\n");

__asm__(".global sys_measure\n"
".func sys_measure\n"
"sys_measure:\n"
"	li a7, " STRINGIFY(ECALL_MEASURE) "\n"
"	ecall\n"
"   ret");
