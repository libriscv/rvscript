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
static void thread_function() {
	microthread::oneshot([] { /* ... */ });
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

/* This is the function that gets called at the start */
/* See engine/src/main.cpp:69 */
PUBLIC(void start())
{
	/* This function is implemented in api_impl.h, and it makes a
	   system call into the engine, which then writes to the terminal. */
	print("Hello world!\n");

#ifdef __EXCEPTIONS
	try {
		throw "";
	} catch (...) {
		print("Exception caught!\n");
	}
#endif

	/* This function makes thousands of calls into this machine,
	   while preserving registers, and then prints some statistics. */
	measure("VM function call overhead", empty_function);
	measure("Thread creation overhead", thread_function);
	measure("Dynamic call handler", dyncall_handler);
	measure("Farcall lookup", farcall_lookup_testcall);
	measure("Farcall direct", direct_farcall_testcall);

	/* This incantation creates a callable object that when called, tells
	   the engine to find the "gameplay2" machine, and then make a call
	   into it with the provided function and arguments. */
	ExecuteRemotely somefunc("gameplay2", some_function);
	/* The engine will detect if we are passing a reference to the stack,
	   and then mount this stack into the remote machine. That allows for
	   passing stack data as arguments to remote machines, with no copying. */
	SomeStruct some {
		.string = "Hello 123!",
		.value  = 42
	};
	/* This is the actual remote function call. It has to match the
	   definition of some_function, or you get a compile error. */
	int r = somefunc(1234, some);
	print("Back again in the start() function! Return value: ", r, "\n");

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
