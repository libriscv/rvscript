#pragma once
#define NATIVE_SYSCALLS_BASE   70
#define SYSCALL_MALLOC    (NATIVE_SYSCALLS_BASE+0)
#define SYSCALL_CALLOC    (NATIVE_SYSCALLS_BASE+1)
#define SYSCALL_REALLOC   (NATIVE_SYSCALLS_BASE+2)
#define SYSCALL_FREE      (NATIVE_SYSCALLS_BASE+3)
#define SYSCALL_MEMINFO   (NATIVE_SYSCALLS_BASE+4)

#define SYSCALL_MEMCPY    (NATIVE_SYSCALLS_BASE+5)
#define SYSCALL_MEMSET    (NATIVE_SYSCALLS_BASE+6)
#define SYSCALL_MEMMOVE   (NATIVE_SYSCALLS_BASE+7)
#define SYSCALL_MEMCMP    (NATIVE_SYSCALLS_BASE+8)

#define SYSCALL_STRLEN    (NATIVE_SYSCALLS_BASE+10)
#define SYSCALL_STRCMP    (NATIVE_SYSCALLS_BASE+11)
#define SYSCALL_STRNCMP   (NATIVE_SYSCALLS_BASE+12)

#define SYSCALL_BACKTRACE (NATIVE_SYSCALLS_BASE+19)

/** Integral system calls **/

static inline long syscall0(long n)
{
	register long a0 __asm__("a0");
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall" : "=r"(a0) : "r"(syscall_id));

	return a0;
}

static inline long
syscall1(long n, long arg0)
{
	register long a0 __asm__("a0") = arg0;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall" : "+r"(a0) : "r"(syscall_id));

	return a0;
}

static inline long
syscall2(long n, long arg0, long arg1)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall" : "+r"(a0) : "r"(a1), "r"(syscall_id));

	return a0;
}

static inline long
syscall3(long n, long arg0, long arg1, long arg2)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(syscall_id));

	return a0;
}

static inline long
syscall4(long n, long arg0, long arg1, long arg2, long arg3)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long a3 __asm__("a3") = arg3;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(syscall_id));

	return a0;
}

static inline long
syscall5(long n, long arg0, long arg1, long arg2, long arg3, long arg4)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long a3 __asm__("a3") = arg3;
	register long a4 __asm__("a4") = arg4;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall"
		: "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(syscall_id));

	return a0;
}

static inline long
syscall6(long n, long arg0, long arg1, long arg2, long arg3, long arg4, long arg5)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long a3 __asm__("a3") = arg3;
	register long a4 __asm__("a4") = arg4;
	register long a5 __asm__("a5") = arg5;
	// NOTE: only 16 regs in RV32E instruction set
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall"
		: "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(syscall_id));

	return a0;
}

static inline long
syscall7(long n, long arg0, long arg1, long arg2,
		long arg3, long arg4, long arg5, long arg6)
{
	register long a0 __asm__("a0") = arg0;
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long a3 __asm__("a3") = arg3;
	register long a4 __asm__("a4") = arg4;
	register long a5 __asm__("a5") = arg5;
	register long a6 __asm__("a6") = arg6;
	register long syscall_id __asm__("a7") = n;

	__asm__ volatile ("scall"
		: "+r"(a0) : "r"(a1), "r"(a2), "r"(a3),
			"r"(a4), "r"(a5), "r"(a6), "r"(syscall_id));

	return a0;
}
