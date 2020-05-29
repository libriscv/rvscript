#include <api.h>

__attribute__((constructor))
static void gconstr() {
	api::print("I am called before everything else (once on each machine)\n");
}

FAST_API void empty_function() {
	asm("ebreak");
	__builtin_unreachable();
}

PUBLIC_API void start()
{
	api::print("Hello world!\n");
	api::measure("Function call overhead", empty_function);
	long r = FARCALL("gameplay2", "some_function", 1234);
	api::print("Back again in the start() function! Return value: ", r, "\n");

	microthread::direct([] {
		api::print("Hello Microthread World!\n");
		api::sleep(1.0);
		api::print("Hello Belated Microthread World! 1 second passed.\n");
	});
	api::print("Back again in the start() function!\n");
}

PUBLIC_API long some_function(int value)
{
	api::print("Hello Remote World! value = ", value, "!\n");
	return value;
}
