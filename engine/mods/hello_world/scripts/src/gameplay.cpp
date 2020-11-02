#include <api.h>
#include "events.hpp"
using namespace api;
using namespace std::string_literals;
extern void NimMain();

int main()
{
	/* This gets called before anything else, on each machine */
#ifdef HAVE_NIM
	NimMain();
#endif
}

/* These are used for benchmarking */
static void empty_function() {
	/* The ebreak-unreachable combo is a way to abruptly stop the machine, and
	   we can also use it to make the compiler not emit function epilogues.
	   You don't have to use this for anything other than making benchmark
	   numbers pretty or if you have a very tight VM function call. */
	asm("ebreak");
	__builtin_unreachable();
}
static void thread_function() {
	microthread::direct([] { /* ... */ });
}
static void group_handler() {
	groupcall<1, 0> ();
}
static void farcall_testcall() {
	constexpr auto fc = FARCALL("gameplay2", "public_donothing", void());
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
	measure("Function group handler", group_handler);
	measure("Farcall", farcall_testcall);

	/* This function tells the engine to find the "gameplay2" machine,
	   and then make a call into it with the provided function and arguments.
	   NOTE: We have to used the shared area to pass anything that is not
	   passed through normal registers. See events.hpp for an example. */
	constexpr auto somefunc = FARCALL("gameplay2", "some_function", int(int));
	int r = somefunc(1234);
	print("Back again in the start() function! Return value: ", r, "\n");

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

	/* This creates a new threads with no arguments and immediately starts
	   executing it. The sleep() will block the thread until some time has
	   passed, and then resume. At the end we make a remote function call
	   to a long-running process that sits in an event loop waiting for work. */
	microthread::direct([] (std::string mt) {
		print("Hello ", mt, " World!\n");
		sleep(1.0);
		print("Hello Belated Microthread World! 1 second passed.\n");
		/* REMOTE_EXEC is implemented in events.hpp */
		REMOTE_EXEC("events", [] {
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
PUBLIC(long some_function(int value))
{
	print("Hello Remote World! value = ", value, "!\n");
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
	print("Object '", object.name, "' is dying!\n");
	/* SFX: Ugh... */
	object.alive = false;
}

static GroupCall<1, 1, void(int, const char*)> myfunction;

PUBLIC(void test_function_groups())
{
	// Verify that the given indices have handlers
	EXPECT(check_group(1, {1, 2, 33, 63}));

	myfunction(1234, "Hello");

	groupcall<1, 2, void()> ();
	//groupcall<1, 3, void()> (); // illegal
	groupcall<1, 33, void()> ();
	groupcall<1, 63, void()> ();
	//groupcall<1, 64, void()> (); // out of bounds
}

PUBLIC(void public_donothing())
{
	/* empty */
}
