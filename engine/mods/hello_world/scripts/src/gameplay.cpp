#include <api.h>
#include "events.hpp"

__attribute__((constructor))
static void gconstr() {
	/* This get's called on each machine */
}

FAST_API void empty_function() {
	asm("ebreak");
	__builtin_unreachable();
}
FAST_API void thread_function() {
	microthread::direct([] {
		
	});
	asm("ebreak");
	__builtin_unreachable();
}

PUBLIC_API void start()
{
	api::print("Hello world!\n");

	api::measure("VM function call overhead", empty_function);
	api::measure("Thread creation overhead", thread_function);

	long r = FARCALL("gameplay2", "some_function", 1234);
	api::print("Back again in the start() function! Return value: ", r, "\n");

	microthread::direct([] {
		api::print("Hello Microthread World!\n");
		api::sleep(1.0);
		api::print("Hello Belated Microthread World! 1 second passed.\n");
		REMOTE_EXEC("events", [] {
			api::print("I am being run on another machine!\n");
		});
	});
	api::print("Back again in the start() function!\n");
}

PUBLIC_API long some_function(int value)
{
	api::print("Hello Remote World! value = ", value, "!\n");
	return value;
}
