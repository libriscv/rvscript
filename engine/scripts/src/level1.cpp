#include "events.hpp"
#include <api.h>
using namespace api;
extern void do_threads_stuff();
extern void do_remote_stuff();
static void do_benchmarks();

/* This is the function that gets called at the start.
   See: engine/src/main.cpp */
PUBLIC(void start())
{
	/* 'print' is implemented in api_impl.h, and it makes a system
	   call into the engine, which then writes to the terminal. */
	print("Hello world!\n\n");

	print("** Fully dynamic system calls **\n");

	DynamicCall("Test::my_dynamic_call")(1234, 5678.0, "nine-ten-eleven-twelve!");

	if (Game::setting("benchmarks").value_or(false))
		do_benchmarks();

	print("** Threads **\n");

	/* Test-run some micro-threads. */
	do_threads_stuff();
}

static std::unique_ptr<TestData[]> test_vector;
int main()
{
	print("Hello from Level 1 main()\n");
	test_vector = std::make_unique_for_overwrite<TestData[]>(64);
	test_vector[0] = { 1, 2, 3, 4.0f, 5.0f, 6.0f };
}

void do_benchmarks()
{
	measure("Benchmark overhead", [] { });

	measure(
		"Run-time setting",
		[]
		{
			volatile auto val = Game::setting("benchmarks");
			(void)val;
		});

	measure(
		"Dynamic call (inline, no arguments)",
		[]
		{
			isys_empty();
		});

	measure(
		"Dynamic arguments (no arguments)",
		[]
		{
			DynamicCall("Test::void")();
		});

	measure(
		"Dynamic arguments (4x arguments)",
		[]
		{
			DynamicCall("Test::void")(-4096, 5678.0, 524287, 8765.0);
		});

	measure(
		"Dynamic call with std::array",
		[]
		{
			sys_test_array(test_vector.get());
		});


	if (Game::setting("remote").value_or(false))
		do_remote_stuff();
}
