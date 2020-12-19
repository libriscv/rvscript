#include <syscall.h>
#include <syscalls.h>
#include <stdio.h>

static inline int
sys_write(const char* data, size_t len)
{
	return syscall2(ECALL_WRITE, (long) data, len);
}


ssize_t write(void* f, const void* buffer, size_t len)
{
	(void) f;
	return sys_write((const char*) buffer, len);
}

void _write_r(void* f, int n, const void* buffer, size_t len)
{
	(void) f;
	(void) n;
	sys_write((const char*) buffer, len);
}

size_t fwrite(const void*__restrict buffer, size_t sz, size_t count, FILE* file)
{
	(void) file;
	return sys_write((const char*) buffer, sz * count);
}
