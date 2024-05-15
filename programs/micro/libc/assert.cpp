#include <include/libc.hpp>
#include <include/syscall.hpp>
#include <cstdarg>
#include <strf.hpp>
#include <syscalls.h>

template <typename... Args>
inline void print(Args&&... args)
{
	char buffer[500];
	auto res = strf::to(buffer) (std::forward<Args> (args)...);
	psyscall(ECALL_WRITE, buffer, res.ptr - buffer);
}

static inline void print_backtrace()
{
	syscall1(SYSCALL_BACKTRACE);
}

extern "C"
__attribute__((noreturn))
void panic(const char* reason)
{
	print("\n\n!!! PANIC !!!\n", reason, '\n');

	syscall(ECALL_ASSERT_FAIL, (long)reason, (long)__FILE__, __LINE__, (long)__func__);

	// the end
	asm volatile("unimp");
	__builtin_unreachable();
}

extern "C" __attribute__((weak))
void abort()
{
	print_backtrace();
	panic("Abort called");
}

extern "C"
void abort_message(const char* text, ...)
{
	print_backtrace();
	panic(text);
}

extern "C"
void __assert_func(
	const char *file,
	int line,
	const char *func,
	const char *failedexpr)
{
	print("assertion ", failedexpr, " failed: \"",
		file, "\", line ", line, func ? ", function: " : "",
		func ? func : "", '\n');
	print_backtrace();
	panic("Assertion failed");
}

extern "C"
{
	__attribute__((used, weak))
	uintptr_t __stack_chk_guard __attribute__((section(".data"))) = 0x0C0A00FF;
	__attribute__((used, weak))
	void __stack_chk_fail()
	{
		print_backtrace();
		panic("Stack protector failed check");
	}
}
