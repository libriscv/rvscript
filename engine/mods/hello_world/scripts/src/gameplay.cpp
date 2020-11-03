#include <api.h>
#include "events.hpp"
using namespace api;
using namespace std::string_literals;
extern void NimMain();
struct SomeStruct {
	const char* string;
	int value;
};
static long some_function(int value, SomeStruct&);

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
static void dyncall_handler() {
	constexpr Call<void()> empty("empty");
	empty();
}
PUBLIC(void public_donothing()) {
	/* nothing */
}
static void direct_farcall_testcall() {
	/* A direct farcall takes a function as argument, and can only
	   be used when calling into a remote machine that is running
	   the same executable as this machine. If it's using a different
	   executable you can use a regular FARCALL instead. */
	constexpr ExecuteRemotely fc("gameplay2", public_donothing);
	fc();
}
static void farcall_lookup_testcall() {
	/* A regular farcall will have to lookup the function in the remote
	   machine. We also need to specify the function type as a
	   template argument, to help with type safety. */
	constexpr FarCall<void()> fc("gameplay2", "public_donothing");
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
	measure("Dynamic call handler", dyncall_handler);
	measure("Farcall lookup", farcall_lookup_testcall);
	measure("Farcall direct", direct_farcall_testcall);

	/* This function creates a callable object that when called, tells
	   the engine to find the "gameplay2" machine, and then make a call
	   into it with the provided function and arguments.
	   NOTE: We have to used the shared area to pass anything that is not
	   passed through normal registers. See events.hpp for an example. */
	constexpr ExecuteRemotely somefunc("gameplay2", some_function);
	SomeStruct some {
		.string = "Hello 123!",
		.value  = 42
	};
	int r = somefunc(1234, some);
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

PUBLIC(void test_dynamic_calls())
{
	Call<void()> dyncall("testing");
	dyncall();
}
