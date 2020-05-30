#include <api.h>
#include "events.hpp"

__attribute__((constructor))
static void gconstr() {
	/* This gets called before anything else, on each machine */
}

/* These are used for benchmarking */
FAST_API void empty_function() {
	/* The ebreak-unreachable combo is a way to abruptly stop the machine, and
	   we can also use it to make the compiler not emit function epilogues.
	   You don't have to use this for anything other than making benchmark
	   numbers pretty or if you have a very tight VM function call. */
	asm("ebreak");
	__builtin_unreachable();
}
FAST_API void thread_function() {
	microthread::direct([] {
		
	});
	asm("ebreak");
	__builtin_unreachable();
}

/* This is the function that gets called at the start */
/* See engine/src/main.cpp:47 */
PUBLIC_API void start()
{
	/* This function is implemented in api_impl.h, and it makes a
	   system call into the engine, which then writes to the terminal. */
	api::print("Hello world!\n");

#ifdef __EXCEPTIONS
	try {
		throw "";
	} catch (...) {
		api::print("Exception caught!\n");
	}
#endif

	/* This function makes thousands of calls into this machine,
	   while preserving registers, and then prints some statistics. */
	api::measure("VM function call overhead", empty_function);
	api::measure("Thread creation overhead", thread_function);

	/* This function tells the engine to find the "gameplay2" machine,
	   and then make a call into it with the provided function and arguments.
	   NOTE: We have to used the shared area to pass anything that is not
	   passed through normal registers. See events.hpp for an example. */
	long r = FARCALL("gameplay2", "some_function", 1234);
	api::print("Back again in the start() function! Return value: ", r, "\n");

	/* This creates a new threads with no arguments and immediately starts
	   executing it. The sleep() will block the thread until some time has
	   passed, and then resume. At the end we make a remote function call
	   to a long-running process that sits in an event loop waiting for work. */
	microthread::direct([] {
		api::print("Hello Microthread World!\n");
		api::sleep(1.0);
		api::print("Hello Belated Microthread World! 1 second passed.\n");
		/* REMOTE_EXEC is implemented in events.hpp */
		REMOTE_EXEC("events", [] {
			api::print("I am being run on another machine!\n");
		});
	});
	api::print("Back again in the start() function!\n");
}

/* We can only call this function remotely if it's added to "gameplay.symbols",
   because the symbol files are used in the build system to preserve certain
   functions that would ordinarily get optimized out. It's name also has to
   unmangled, otherwise we can't find it in the ELF string tables. */
PUBLIC_API long some_function(int value)
{
	api::print("Hello Remote World! value = ", value, "!\n");
	return value;
}
