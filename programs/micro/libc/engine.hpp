#pragma once
#include <exception>
#include <source_location>
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

extern "C" long sys_write(const char*, size_t);
extern "C" void (*farcall_helper) ();
extern "C" void (*direct_farcall_helper) ();
extern "C" void sys_interrupt (uint32_t, uint32_t, const void*, size_t);
extern "C" long sys_multiprocess_fork(unsigned);
extern "C" long sys_multiprocess_wait();
