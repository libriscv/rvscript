#include <api.h>
#include "events.hpp"
using namespace api;
using namespace std::string_literals;
struct SomeStruct {
	const char* string;
	int value;
};
static long some_function(int value, SomeStruct&);

int main()
{
	/* This gets called before anything else, on each machine */
}

/* These are used for benchmarking */
static void empty_function() {
}
static void full_thread_function() {
	microthread::create([] (int, int, int) { /* ... */ }, 1, 2, 3);
}
static void oneshot_thread_function() {
	microthread::oneshot([] (int, int, int) { /* ... */ }, 1, 2, 3);
}
static void direct_thread_function() {
	microthread::direct([] { /* ... */ });
}
static void dyncall_handler() {
	sys_empty();
}
PUBLIC(void public_donothing()) {
	/* nothing */
}
static void direct_farcall_testcall() {
	/* A direct farcall takes a function as argument, and can only
	   be used when calling into a remote machine that is running
	   the same executable as this machine. If it's using a different
	   executable you can use a regular FARCALL instead. */
	ExecuteRemotely fc("gameplay2", public_donothing);
	fc();
}
static void farcall_lookup_testcall() {
	/* A regular farcall will have to lookup the function in the remote
	   machine. We also need to specify the function type as a
	   template argument, to help with type safety. */
	FarCall<void()> fc("gameplay2", "public_donothing");
	fc();
}
template <size_t SIZE>
struct MultiprocessWork {
	unsigned workers = 1;
	std::array<float, SIZE> data_a;
	std::array<float, SIZE> data_b;
	float result[16] = {0};
	int counter = 0;

	inline size_t work_size() const noexcept {
		return SIZE / workers;
	}
	float final_sum() const noexcept {
		float sum = 0.0f;
		for (size_t i = 0; i < this->workers; i++) sum += this->result[i];
		return sum;
	}
};
static constexpr size_t WORK_SIZE = 8192;
static constexpr size_t MP_WORKERS = 4;
static bool work_output = false;
static MultiprocessWork<WORK_SIZE> mp_work;
template <size_t SIZE>
static void initialize_work(MultiprocessWork<SIZE>& work) {
	for (size_t i = 0; i < SIZE; i++) {
		work.data_a[i] = 1.0;
		work.data_b[i] = 1.0;
	}
}

static void multiprocessing_function(int cpu, void* vdata)
{
	auto& work = *(MultiprocessWork<WORK_SIZE> *)vdata;
	const size_t start = (cpu + 0) * work.work_size();
	const size_t end   = (cpu + 1) * work.work_size();

	float sum = 0.0f;
	for (size_t i = start; i < end; i++) {
		sum += work.data_a[i] * work.data_b[i];
	}

	work.result[cpu] = sum;
	__sync_fetch_and_add(&work.counter, 1);
}
static void multiprocessing_dummy(int, void*)
{
}
static void multiprocessing_forever(int, void*)
{
	while (true);
}

static void test_singleprocessing()
{
	mp_work.workers = 1;

	// Perform part of the work on main vCPU
	multiprocessing_function(0, &mp_work);

	const float sum = mp_work.final_sum();
	if (work_output)
		print("Single-process sum = ", sum, "\n");
}
static void test_multiprocessing()
{
	mp_work.workers = MP_WORKERS;

	// Start N extra vCPUs and execute the function
#define MULTIPROCESS_FORK
#ifndef MULTIPROCESS_FORK
	// Method 1: Start new workers, each with their own stacks
	// then call the given function. Most of this is handled
	// in RISC-V assembly.
	multiprocess(mp_work.workers, multiprocessing_function, &mp_work);
#else
	// Method 2: Fork this machine and wait until multiprocessing
	// ends, by calling multiprocess_wait() on all workers. Each
	// worker uses the current stack, copy-on-write. No need for
	// hand-written assembly to handle this variant.
	const unsigned cpu = multiprocess(mp_work.workers);
	if (cpu != 0) {
		multiprocessing_function(cpu-1, &mp_work);
	}
#endif
	// Wait and stop workers here
	const auto result = multiprocess_wait();

	// Sum the work together
	const float sum = mp_work.final_sum();
	if (work_output) {
		print("Multi-process sum = ", sum, "\n");
		print("Multi-process counter = ", mp_work.counter, "\n");
		print("Multi-process result = ", strf::bin(result >> 1), " (",
			(result == 0) ? "good" : "bad", ")\n");
	}
}
static void test_multiprocessing_forever()
{
	multiprocess(4, multiprocessing_forever);
	auto res = multiprocess_wait();
	print("Forever multiprocessing result: ", strf::bin(res >> 1),
		" (", (res == 0) ? "good" : "bad", ")\n");
}
static void multiprocessing_overhead()
{
	mp_work.workers = MP_WORKERS;

	// Start the vCPUs
#ifndef MULTIPROCESS_FORK
	multiprocess(MP_WORKERS, multiprocessing_dummy, nullptr);
	// Wait for all multiprocessing workers
	multiprocess_wait();
#else
	unsigned cpu = multiprocess(MP_WORKERS);
	if (cpu != 0) {
		multiprocessing_dummy (cpu-1, &mp_work);
		// Finish multiprocessing here
		multiprocess_wait();
	}
#endif
}

/* This is the function that gets called at the start */
/* See engine/src/main.cpp:69 */
PUBLIC(void start())
{
	/* This function is implemented in api_impl.h, and it makes a
	   system call into the engine, which then writes to the terminal. */
	print("Hello world!\n");

	initialize_work(mp_work);
	work_output = true;
	test_multiprocessing();
	test_multiprocessing_forever();
	test_singleprocessing();
	work_output = false;

	try {
		throw GameplayException("This is a test!");
	} catch (const GameplayException& ge) {
		print("Exception caught: ", ge.what(), "!\n");
		print("Exception thrown from: ", ge.location().function_name(), ", line ", ge.location().line(), "\n");
	}

	/* This function makes thousands of calls into this machine,
	   while preserving registers, and then prints some statistics. */
	measure("VM function call overhead", empty_function);
	measure("Full thread creation overhead", full_thread_function);
	measure("Oneshot thread creation overhead", oneshot_thread_function);
	measure("Direct thread creation overhead", direct_thread_function);
	measure("Dynamic call handler", dyncall_handler);
	measure("Farcall lookup", farcall_lookup_testcall);
	measure("Farcall direct", direct_farcall_testcall);

	measure("Multi-processing overhead", multiprocessing_overhead);
	measure("Multi-processing dotprod", test_multiprocessing);
	measure("Single-processing dotprod", test_singleprocessing);

	int a = 1, b = 2, c = 3;
	microthread::oneshot([] (int a, int b, int c) {
		print("Hello from thread 1! a = ", a, ", b = ", b, ", c = ", c, "\n");
		microthread::yield();
		print("And we're back! a = ", a, ", b = ", b, ", c = ", c, "\n");
	}, a, b, c);

	print("Back in the main thread .. going back!\n");
	a = 2; b = 4; c = 6;
	microthread::yield();

	auto thread = microthread::create(
		[] (int a, int b, int c) {
			print("Hello from thread 2! a = ", a, ", b = ", b, ", c = ", c, "\n");
			microthread::yield();
			print("Anyone going to join us? Returning 666.\n");
			// Since the main thread is joining this thread,
			// we should be able to spin-yield a bit.
			for (size_t i = 0; i < 100; i++)
				microthread::yield();
			return 666;
		}, a, b, c);
	print("Joining the thread any time now...\n");
	auto retval = microthread::join(thread);
	print("Full thread exited, return value: ", retval, "\n");

	/* This incantation creates a callable object that when called, tells
	   the engine to find the "gameplay2" machine, and then make a call
	   into it with the provided function and arguments. */
	ExecuteRemotely somefunc("gameplay2", some_function);
	/* We will need to put the struct on a shared memory area, so that it
	   will be visible and writable on both sides. */
	SharedMemoryArea shm;
	auto& some = shm(SomeStruct{
		.string = "Hello 123!",
		.value  = 42
	});
	/* This is the actual remote function call. It has to match the
	   definition of some_function, or you get a compile error. */
	int r = somefunc(1234, some);
	print("Back again in the start() function! Return value: ", r, "\n");
	print("Some struct string: ", some.string, "\n");

	Game::breakpoint();

	/* Create events that will run each physics tick.
	   We will be waiting immediately, so that we don't run the
	   event code now, but instead when each tick happens. */
	each_tick([] {
		while (true) {
			wait_next_tick();
			static int i = 0;
			print("Tick ", i++, "!\n");
		}
	});
	int physics_value = 44;
	each_tick([physics_value] {
		int v = physics_value;
		while (true) {
			wait_next_tick();
			print("I have a ", v++, "!\n");
		}
	});

	/* This creates a new thread with no arguments and immediately starts
	   executing it. The sleep() will block the thread until some time has
	   passed, and then resume. At the end we make a remote function call
	   to a long-running process that sits in an event loop waiting for work. */
	microthread::oneshot([] (std::string mt) {
		print("Hello ", mt, " World!\n");
		sleep(1.0);
		print("Hello Belated Microthread World! 1 second passed.\n");
		/* add_remote_work is implemented in events.hpp
		   NOTE: We cannot pass "anything" we want here,
		   because the shared pagetables between remote machines
		   are only in effect during a call, and not after.
		   Remote work is something that is executed at a later time.
		   But, we can still pass constant read-only data. */
		add_remote_work([] {
			/* This works because gameplay and events are running
			   the same binary, so they share read-only memory,
			   such as strings, constant structs and functions. */
			print("I am being run on another machine!\n");
		});
	}, "Microthread"s);

	print("Back again in the start() function!\n");
	// RA is loaded from stack, and becomes 0x0
}

/* We can only call this function remotely if it's added to "gameplay.symbols",
   because the symbol files are used in the build system to preserve certain
   functions that would ordinarily get optimized out. It's name also has to
   unmangled, otherwise we can't find it in the ELF string tables. */
long some_function(int value, SomeStruct& some)
{
	print("Hello Remote World! value = ", value, "!\n");
	print("Some struct string: ", some.string, "\n");
	print("Some struct value: ", some.value, "\n");
	some.string = "Hello 456!";
	return value;
}

struct C {
	std::string to_string() const {
		return std::string(&c, 1) + "++";
	}
private:
	char c;
};
PUBLIC(void cpp_function(const char* a, const C& c, const char* b))
{
	/* Hello C++ World */
	print(a, " ", c.to_string(), " ", b, "\n");
}

/* Example game object logic */
struct GameObject {
	bool alive;
	char name[30];
};

PUBLIC(void myobject_death(GameObject& object))
{
	EXPECT(object.alive);
	print("Object '", object.name, "' is dying!\n");
	/* SFX: Ugh... */
	object.alive = false;
}

PUBLIC(void test_dynamic_functions())
{
	// See: dyncalls.json
	sys_testing();
	sys_testing123(5, 6, 7);
}
