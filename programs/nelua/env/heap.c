/**
 * Accelerated heap using syscalls
 *
**/
#include <stddef.h>
#include <syscall.h>
#define OVERRIDE()   __attribute__((used))
struct _reent;

OVERRIDE()
void* malloc(size_t len)
{
	return (void*) syscall1(SYSCALL_MALLOC, len);
}
OVERRIDE()
void* calloc(size_t count, size_t size)
{
	return (void*) syscall2(SYSCALL_CALLOC, count, size);
}
OVERRIDE()
void* realloc(void* ptr, size_t len)
{
	return (void*) syscall2(SYSCALL_REALLOC, (long) ptr, len);
}
OVERRIDE()
void free(void* ptr)
{
	syscall1(SYSCALL_FREE, (long) ptr);
}

int meminfo(void* ptr, size_t len)
{
	return syscall2(SYSCALL_MEMINFO, (long) ptr, len);
}

/* Newlib internal reentrancy-safe heap functions.
   Our system calls are safe because they are atomic. */
OVERRIDE() void*
_malloc_r(struct _reent* re, size_t size)
{
	(void) re;
	return malloc(size);
}
OVERRIDE() void*
_calloc_r(struct _reent* re, size_t count, size_t size)
{
	(void) re;
	return calloc(count, size);
}
OVERRIDE() void*
_realloc_r(struct _reent* re, void* ptr, size_t newsize)
{
	(void) re;
	return realloc(ptr, newsize);
}
OVERRIDE() void
_free_r(struct _reent* re, void* ptr)
{
	(void) re;
	return free(ptr);
}
