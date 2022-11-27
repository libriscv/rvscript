#include <api.h>
#include "interface.hpp"
using namespace api;

struct SomeStruct {
	std::string string;
	int value;
};
KEEP() Gameplay gameplay_state;

KEEP() void Gameplay::set_action(bool a)
{
	print("Setting action to ", a, "\n");
	this->action = a;
}
KEEP() bool Gameplay::get_action()
{
	return this->action;
}

KEEP() void Gameplay::set_string(unsigned key, const std::string& val)
{
	this->strings[key] = val;
}
KEEP() void Gameplay::print_string(unsigned key)
{
	print("Key ", key, " has value: ", this->strings[key], "\n");
}

KEEP() void Gameplay::do_nothing()
{
}

/* We can only call this function remotely if it's added to "gameplay.symbols",
   because the symbol files are used in the build system to preserve certain
   functions that would ordinarily get optimized out. It's name also has to
   unmangled, otherwise we can't find it in the ELF string tables. */
extern KEEP()
long gameplay_function(float value, SomeStruct& some)
{
	print("Hello Remote World! value = ", value, "!\n");
	print("Some struct string: ", some.string, "\n");
	print("Some struct value: ", some.value, "\n");
	some.string = "Hello 456! This string is very long!";
	return value * 2;
}

extern KEEP() void gameplay_empty()
{
	// Do nothing
}
