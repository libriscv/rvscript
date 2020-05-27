#include <include/libc.hpp>
#include <cstring>
#include <stdio.h>

extern "C"
int puts(const char* string)
{
	const long len = strlen(string);
	return sys_write(string, len);
}

extern "C"
size_t fwrite(const void*__restrict buffer, size_t, size_t count, FILE*)
{
	return sys_write(buffer, count);
}
