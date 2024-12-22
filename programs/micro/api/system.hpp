#pragma once
//#include "override/new"
#include <exception>
#include <source_location>
#include <utility>
#include <syscalls.h>
#include <crc32.hpp>
#include <function.hpp>
#include <microthread.hpp>
#include <shared_memory.h>
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
constexpr double PI = 3.14159265358979323846;
extern void halt();

inline auto hart_id()
{
	uint64_t id;
	asm ("csrr %0, mhartid\n" : "=r"(id));
	return id;
}
inline auto rdcycle()
{
	uint64_t cycles;
	asm ("rdcycle %0" : "=r"(cycles) :: "memory");
	return cycles;
}
inline auto rdtime()
{
	uint64_t val;
	asm ("rdtime %0" : "=r"(val) :: "memory");
	return val;
}

struct HashedValue {
	template <size_t N>
	consteval HashedValue(const char (&str)[N])
		: hash(crc32ct<N>(str))
	{
		static_assert(N > 0, "String must be non-empty");
	}

	const uint32_t hash;
};

extern "C" long sys_write(const void*, size_t);
extern "C" void (*farcall_helper) ();
extern "C" void (*direct_farcall_helper) ();
extern "C" void sys_interrupt (uint32_t, uint32_t, const void*, size_t);
extern "C" void sys_assert_fail(const char*, const char*, int, const char*);
extern "C" long sys_measure(const char*, void (*)());
extern "C" uint32_t sys_machine_hash();
extern "C" void sys_game_exit();
extern "C" intptr_t sys_game_setting(const char*, size_t);
extern "C" int sys_timer_sleep(float);
