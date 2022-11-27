#include <api.h>
#include "interface.hpp"
using namespace api;
static void call_remotely();
static void call_remote_member();
static void call_remote_function();
static void call_remote_std_function();

struct SomeStruct
{
	std::string string;
	int value;
};
extern long gameplay_function(float, SomeStruct&);

void do_remote_stuff()
{
	gameplay_state.set_action(true);
	print("Action: ", gameplay_state.get_action(), "\n");
	gameplay_state.set_action(false);
	print("Action: ", gameplay_state.get_action(), "\n");

	SomeStruct ss {
		.string = "Hello this string avoids SSO",
		.value  = 42
	};
	long r = gameplay_function(1234.5, ss);

	print("Back again in the start() function! Return value: ", r, "\n");
	print("Some struct string: ", ss.string, "\n");

	gameplay_exec([&] {
		gameplay_state.strings[123] =
			"This is a remotely allocated string, "
			"with value = " + std::to_string(ss.value);
	});
	gameplay_state.print_string(123);

	gameplay_exec_ptr([] {
		print("I'm printing this from the remote machine. Why does this work?\n");
	});

	measure("Call remote function", call_remotely);
	measure("Call remote C++ member function", call_remote_member);
	measure("Call remote function ptr", call_remote_function);
	measure("Call remote std::function", call_remote_std_function);
}

void call_remotely()
{
	// A do-nothing function used for benchmarking
	extern void gameplay_empty();
	gameplay_empty();
}
void call_remote_member()
{
	// A do-nothing member function used for benchmarking
	gameplay_state.do_nothing();
}
void call_remote_function()
{
	// A remotely executed lambda used for benchmarking
	gameplay_exec_ptr([] {
	});
}
void call_remote_std_function()
{
	// A remotely executed std::function used for benchmarking
	gameplay_exec([] {
	});
}
