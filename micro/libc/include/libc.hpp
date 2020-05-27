#pragma once
#include <cstddef>
#include <cstdint>
#include <stdlib.h>
#include <stdio.h>

extern "C" {
__attribute__((noreturn))
void panic(const char* reason);

void* memset(void* dest, int ch, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
void* memmove(void* dest, const void* src, size_t size);
int   memcmp(const void* ptr1, const void* ptr2, size_t n);
char*  strcpy(char* dst, const char* src);
size_t strlen(const char* str);
int    strcmp(const char* str1, const char* str2);
char*  strcat(char* dest, const char* src);

int   sys_write(const void*, size_t);

int   print_backtrace();

void* malloc(size_t) __malloc_like __result_use_check __alloc_size(1) _NOTHROW;
void* calloc(size_t, size_t) __malloc_like __result_use_check _NOTHROW;
void  free(void*) _NOTHROW;

void _exit(int) __attribute__((noreturn));
}

#include "syscall.hpp"
#include <syscalls.h>

inline int sys_write(const void* data, size_t len)
{
	return syscall(ECALL_WRITE, (long) data, len);
}

inline int print_backtrace()
{
	return syscall(SYSCALL_BACKTRACE);
}

inline void print(const char* text)
{
	syscall(ECALL_WRITE, (long) text, __builtin_strlen(text));
}

#include <strf.hpp>

template <typename... Args>
inline void print(Args&&... args)
{
	char buffer[500];
	auto res = strf::to(buffer) (std::forward<Args> (args)...);
	sys_write(buffer, res.ptr - buffer);
}
