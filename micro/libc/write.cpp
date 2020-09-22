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
ssize_t write(int, const void* buffer, size_t len)
{
	return sys_write(buffer, len);
}

extern "C"
size_t fwrite(const void*__restrict buffer, size_t, size_t count, FILE*)
{
	return sys_write(buffer, count);
}

extern "C"
int fputs (const char* str, FILE*)
{
	return puts(str);
}

extern "C"
int fputc (int c, FILE*)
{
	return sys_write(&c, 1);
}
extern "C"
wint_t fputwc(wchar_t ch, FILE*)
{
	char c = ch;
	sys_write(&c, 1);
	return ch;
}

#ifndef USE_NEWLIB

__attribute__((format (printf, 2, 3)))
int sprintf(char *buffer, const char *format, ...)
{
	const long len = strlen(format);
	memcpy(buffer, format, len);
	return len;
}

#endif
