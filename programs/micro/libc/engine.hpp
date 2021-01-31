#pragma once
#include <new>
#include <utility>
#include <syscalls.h>
#include <strf.hpp>
#include <include/crc32.hpp>
#include <include/libc.hpp>
#include <include/function.hpp>
#include <include/tuplecall.hpp>
#include <microthread.hpp>
constexpr double PI = 3.14159265358979323846;
extern "C" void _exit(int) __attribute__((noreturn));
extern void trap();
extern void halt();
#define PUBLIC(...) extern "C" __attribute__((used)) __VA_ARGS__

inline constexpr uint32_t operator "" _hash (const char* value, std::size_t len) {
	return crc32(value, len);
}

inline auto rdcycle()
{
	union {
		uint64_t whole;
		uint32_t word[2];
	};
	asm ("rdcycleh %0\n rdcycle %1\n" : "=r"(word[1]), "=r"(word[0]) :: "memory");
	return whole;
}
inline auto rdtime()
{
	union {
		uint64_t whole;
		uint32_t word[2];
	};
	asm ("rdtimeh %0\n rdtime %1\n" : "=r"(word[1]), "=r"(word[0]) :: "memory");
	return whole;
}

template <typename Expr, typename T>
constexpr inline void check_assertion(
	Expr expr, const T& left, const T& right, const char* exprstring,
	const char* file, const char* func, const int line)
{
	if (UNLIKELY(!expr())) {
		print("assertion (", left, ") ", exprstring, " (", right,
			") failed: \"", file, "\", line ", line,
			func ? ", function: " : "", func ? func : "", '\n');
		syscall(SYSCALL_BACKTRACE);
		_exit(-1);
	}
}
template <typename Expr, typename T>
constexpr inline void check_assertion(
	Expr expr, const T& left, nullptr_t, const char* exprstring,
	const char* file, const char* func, const int line)
{
	if (UNLIKELY(!expr())) {
		print("assertion (", left, ") ", exprstring, " (null",
			") failed: \"", file, "\", line ", line,
			func ? ", function: " : "", func ? func : "", '\n');
		syscall(SYSCALL_BACKTRACE);
		_exit(-1);
	}
}
#define ASSERT(l, op, r) \
	check_assertion([&] { return (l op r); }, l, r, \
	#l " " #op " " #r,\
	__FILE__, __FUNCTION__, __LINE__)
