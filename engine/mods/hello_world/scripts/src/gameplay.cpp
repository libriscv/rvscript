#include <api.h>

void map_main(MapFile*)
{
	
}

PUBLIC_API void empty_function() {
	asm("ebreak");
	__builtin_unreachable();
}

PUBLIC_API void start()
{
	api::print("Hello world!\n");
	api::measure("Function call overhead", empty_function);
	FARCALL("gameplay", "some_function", 1234);
}

PUBLIC_API long some_function(int value)
{
	api::print("Hello Remote World! value = ", value, "!\n");
	return value;
}
