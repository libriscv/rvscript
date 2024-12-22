#include "microthread.hpp"

extern "C" void microthread_set_tp(void*);

namespace microthread
{
	static Thread main_thread {nullptr};

	void trampoline(Thread* thread)
	{
		thread->startfunc();
	}
	void oneshot_exit()
	{
		auto* thread = self();
		// after this point stack unusable
		free((char *)thread + sizeof(Thread) - Thread::STACK_SIZE);
		sys_microthread_exit(0);
		__builtin_unreachable();
	}

	/* glibc sets up its own main thread, *required* by C++ exceptions */
#if 0
	__attribute__((constructor, used))
	static void init_threads()
	{
		microthread_set_tp(&main_thread);
	}
#endif
}

asm(".section .text\n"
".global microthread_set_tp\n"
".type microthread_set_tp, @function\n"
"microthread_set_tp:\n"
"  mv tp, a0\n"
"  ret\n");

#define STRINGIFY_HELPER(x) #x
#define STR(x) STRINGIFY_HELPER(x)

#define CREATE_SYSCALL(name, syscall_id)      \
	__asm__(".pushsection .text\n"            \
			".global " #name "\n"             \
			".type " #name ", @function\n"    \
			"" #name ":\n"                    \
			"	li a7, " STR(syscall_id) "\n" \
			"	ecall\n" \
			"	ret\n"   \
			".popsection .text\n")

CREATE_SYSCALL(sys_microthread_create,  THREAD_SYSCALLS_BASE+0);
CREATE_SYSCALL(sys_microthread_exit,    THREAD_SYSCALLS_BASE+1);
CREATE_SYSCALL(sys_microthread_yield,   THREAD_SYSCALLS_BASE+2);
CREATE_SYSCALL(sys_microthread_yield_to,THREAD_SYSCALLS_BASE+3);
CREATE_SYSCALL(sys_microthread_block,   THREAD_SYSCALLS_BASE+4);
CREATE_SYSCALL(sys_microthread_unblock, THREAD_SYSCALLS_BASE+6);
CREATE_SYSCALL(sys_microthread_wakeup_one_blocked, THREAD_SYSCALLS_BASE+5);
CREATE_SYSCALL(sys_microthread_direct,  THREAD_SYSCALLS_BASE+8);
CREATE_SYSCALL(threadcall_destructor,   THREAD_SYSCALLS_BASE+9);
