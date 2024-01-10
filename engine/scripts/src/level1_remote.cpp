#include "interface.hpp"
#include <api.h>
using namespace api;
#define NORET __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))
static __attribute__((noreturn)) void call_remotely();
static __attribute__((noreturn)) void call_remote_member();
static __attribute__((noreturn)) void call_remote_function();
static __attribute__((noreturn)) void call_remote_std_function();

struct SomeStruct
{
	std::string string;
	int value;
};

extern long gameplay_function(float, SomeStruct&);

static int local_value = 5678;

extern void gameplay_arg1(int);
extern void gameplay_arg2(int, int);
extern void gameplay_arg3(int, int, int);
extern void gameplay_arg4(int, int, int, int);
extern void gameplay_arg5(int, int, int, int, int);
extern void gameplay_arg6(int, int, int, int, int, int);
extern void gameplay_arg7(int, int, int, int, int, int, int);
extern void gameplay_arg8(int, int, int, int, int, int, int, int);
static NORET void call_args1()
{
	gameplay_arg1(0);
	return_fast();
}
static NORET void call_args2()
{
	gameplay_arg2(0, 0);
	return_fast();
}
static NORET void call_args3()
{
	gameplay_arg3(0, 0, 0);
	return_fast();
}
static NORET void call_args4()
{
	gameplay_arg4(0, 0, 0, 0);
	return_fast();
}
static NORET void call_args5()
{
	gameplay_arg5(0, 0, 0, 0, 0);
	return_fast();
}
static NORET void call_args6()
{
	gameplay_arg6(0, 0, 0, 0, 0, 0);
	return_fast();
}
static NORET void call_args7()
{
	gameplay_arg7(0, 0, 0, 0, 0, 0, 0);
	return_fast();
}
static NORET void call_args8()
{
	gameplay_arg8(0, 0, 0, 0, 0, 0, 0, 0);
	return_fast();
}

#include "local.hpp"

static void bench_local_func()
{
	local_function();
}
static void bench_local_cpp()
{
	local_struct.local_cpp_function();
}
static void bench_local_func_ptr()
{
	local_function_ptr(local_function);
}
static void bench_local_std_func()
{
	local_std_function(local_function);
}

void do_remote_stuff()
{
	gameplay_state.set_action(true);
	print("Action: ", gameplay_state.get_action(), "\n");
	gameplay_state.set_action(false);
	print("Action: ", gameplay_state.get_action(), "\n");

	SomeStruct ss {.string = "Hello this string avoids SSO", .value = 42};
	long r = gameplay_function(1234.5, ss);

	print("Back again in the start() function! Return value: ", r, "\n");
	print("Some struct string: ", ss.string, "\n");

	gameplay_exec(
		[&]
		{
			gameplay_state.strings[123]
				= "This is a remotely allocated string, "
				  "with value = "
				  + std::to_string(ss.value);
		});

	gameplay_exec(
		[]
		{
			unsigned key = 123;
			print(
				"Key ", key, " has value: ", gameplay_state.strings[key],
				"\n");
			// This value cannot be read because the GP register is changed during the remote call
			print("Local GP-relative value: ", local_value, " (Cannot be read)\n");
		});

	gameplay_exec(
		[]
		{
			gameplay_state.functions.push_back(
				[]
				{
					print("This is a callback function!\n");
				});
		});
	gameplay_exec(
		[]
		{
			auto& func = gameplay_state.functions.at(0);
			func();
		});

	if (Game::setting("benchmarks").value_or(false)) {
		measure("Call remote function", call_remotely);
		measure("Call 1-arg function", call_args1);
		measure("Call 2-arg function", call_args2);
		measure("Call 3-arg function", call_args3);
		measure("Call 4-arg function", call_args4);
		measure("Call 5-arg function", call_args5);
		measure("Call 6-arg function", call_args6);
		measure("Call 7-arg function", call_args7);
		measure("Call 8-arg function", call_args8);
		measure("Call remote C++ member function", call_remote_member);
		measure("Call remote function ptr", call_remote_function);
		measure("Call remote std::function", call_remote_std_function);

		measure("Call local function", bench_local_func);
		measure("Call local C++ member function", bench_local_cpp);
		measure("Call local function ptr", bench_local_func_ptr);
		measure("Call local std::function", bench_local_std_func);
	}
}

void call_remotely()
{
	// A do-nothing function used for benchmarking
	extern void gameplay_empty();
	gameplay_empty();
	return_fast();
}

void call_remote_member()
{
	// A do-nothing member function used for benchmarking
	gameplay_state.do_nothing();
	return_fast();
}

void call_remote_function()
{
	// A remotely executed lambda used for benchmarking
	gameplay_exec_ptr([] {});
	return_fast();
}

void call_remote_std_function()
{
	// A remotely executed std::function used for benchmarking
	gameplay_exec([] {});
	return_fast();
}
