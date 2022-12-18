#include <api.h>
using namespace api;

void do_threads_stuff()
{
	/* Test micro threads. */
	int a = 1, b = 2, c = 3;
	microthread::oneshot(
		[](int a, int b, int c)
		{
			print(
				"Hello from thread 1! a = ", a, ", b = ", b, ", c = ", c,
				"\n");
			microthread::yield();
			print("And we're back! a = ", a, ", b = ", b, ", c = ", c, "\n");
		},
		a, b, c);

	print("Back in the main thread .. going back!\n");
	a = 2;
	b = 4;
	c = 6;
	microthread::yield();

	auto thread = microthread::create(
		[](int a, int b, int c)
		{
			print(
				"Hello from thread 2! a = ", a, ", b = ", b, ", c = ", c,
				"\n");
			microthread::yield();
			print("Anyone going to join us? Returning 666.\n");
			// Since the main thread is joining this thread,
			// we should be able to spin-yield a bit.
			for (size_t i = 0; i < 100; i++)
				microthread::yield();
			return 666;
		},
		a, b, c);
	print("Joining the thread any time now...\n");
	auto retval = microthread::join(thread);
	print("Full thread exited, return value: ", retval, "\n");

	Game::breakpoint();

	/* Create events that will run each physics tick.
	   We will be waiting immediately, so that we don't run the
	   event code now, but instead when each tick happens. */
	each_tick(
		[]
		{
			while (true)
			{
				wait_next_tick();
				static int i = 0;
				print("Tick ", i++, "!\n");
			}
		});
	int physics_value = 44;
	each_tick(
		[physics_value]
		{
			int v = physics_value;
			while (true)
			{
				wait_next_tick();
				print("I have a ", v++, "!\n");
			}
		});

	/* This creates a new thread with no arguments and immediately starts
	   executing it. The sleep() will block the thread until some time has
	   passed, and then resume. At the end we make a remote function call
	   to a long-running process that sits in an event loop waiting for work.
	 */
	microthread::oneshot(
		[](std::string mt)
		{
			print("Hello ", mt, " World!\n");
			sleep(1.0);
			print("Hello Belated Microthread World! 1 second passed.\n");
			/* add_remote_work is implemented in events.hpp
			   NOTE: We cannot pass "anything" we want here,
			   because the shared pagetables between remote machines
			   are only in effect during a call, and not after.
			   Remote work is something that is executed at a later time.
			   But, we can still pass constant read-only data. */
		},
		"Microthread");

	print("Back again in the start() function!\n");
}
