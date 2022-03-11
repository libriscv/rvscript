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
#include <shared_memory.h>
constexpr double PI = 3.14159265358979323846;
extern void halt();
#define PUBLIC(...) extern "C" __attribute__((used)) __VA_ARGS__

inline constexpr uint32_t operator "" _hash (const char* value, std::size_t len) {
	return crc32(value, len);
}

inline auto hart_id()
{
	uint64_t id;
	asm ("csrr %0, mhartid\n" : "=r"(id));
	return id;
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
