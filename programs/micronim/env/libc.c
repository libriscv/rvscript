#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>
#define OVERRIDE()   __attribute__((used))

OVERRIDE()
void* memset(void* vdest, int ch, size_t size)
{
	return (void*) syscall3(SYSCALL_MEMSET, (long) vdest, ch, size);
}
OVERRIDE()
void* memcpy(void* vdest, const void* vsrc, size_t size)
{
	return (void*) syscall3(SYSCALL_MEMCPY, (long) vdest, (long) vsrc, size);
}
OVERRIDE()
void* memmove(void* vdest, const void* vsrc, size_t size)
{
	return (void*) syscall3(SYSCALL_MEMMOVE, (long) vdest, (long) vsrc, size);
}
OVERRIDE()
int memcmp(const void* ptr1, const void* ptr2, size_t n)
{
	return syscall3(SYSCALL_MEMCMP, (long) ptr1, (long) ptr2, n);
}

OVERRIDE()
size_t strlen(const char* str)
{
	return syscall1(SYSCALL_STRLEN, (long) str);
}
OVERRIDE()
int strcmp(const char* str1, const char* str2)
{
	return syscall3(SYSCALL_STRCMP, (long) str1, (long) str2, 4096);
}
OVERRIDE()
int strncmp(const char* s1, const char* s2, size_t n)
{
	return syscall3(SYSCALL_STRCMP, (long) s1, (long) s2, n);
}

OVERRIDE()
wchar_t* wmemcpy(wchar_t* wto, const wchar_t* wfrom, size_t size)
{
	return (wchar_t *) memcpy (wto, wfrom, size * sizeof (wchar_t));
}

OVERRIDE()
__attribute__((noreturn))
void _exit(int code)
{
	register long a0 __asm__("a0") = code;

	__asm__(".insn i SYSTEM, 0, %0, x0, 0x7ff" :: "r"(a0));
	__builtin_unreachable();
}

OVERRIDE()
__attribute__((noreturn))
void exit(int code)
{
	_exit(code);
}
