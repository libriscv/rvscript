#pragma once
#include "syscall.hpp"
#include <tuple>


inline std::tuple<float, float>
fsyscallff(long n, float farg0, float farg1)
{
	register float fa0 asm("fa0") = farg0;
	register float fa1 asm("fa1") = farg1;
	register long syscall_id asm("a7") = n;

	asm volatile ("scall"
		: "+f"(fa0), "+f"(fa1) : "r"(syscall_id) : "a0");

	return std::make_tuple(float{fa0}, float{fa1});
}

inline std::tuple<float, float>
fsyscallff(long n, float farg0, float farg1, float farg2)
{
	register float fa0 asm("fa0") = farg0;
	register float fa1 asm("fa1") = farg1;
	register float fa2 asm("fa2") = farg2;
	register long syscall_id asm("a7") = n;

	asm volatile ("scall"
		: "+f"(fa0), "+f"(fa1) : "f"(fa2), "r"(syscall_id) : "a0");

	return {float{fa0}, float{fa1}};
}
