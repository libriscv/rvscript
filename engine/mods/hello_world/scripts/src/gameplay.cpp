#include <api.h>
#include "events.hpp"
using namespace api;
extern void benchmark_multiprocessing();

int main()
{
	/* This gets called before anything else, on each machine */
	return 0;
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

PUBLIC(void benchmarks())
{
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

	benchmark_multiprocessing();
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
#include <embedded_string.hpp>
struct GameObject {
	EmbeddedString<32> name;
	bool alive;
};

PUBLIC(void myobject_death(GameObject& object))
{
	EXPECT(object.alive);
	print("Object '", object.name.to_string(), "' is dying!\n");
	/* SFX: Ugh... */
	object.alive = false;
}

PUBLIC(void test_dynamic_functions())
{
	// See: dyncalls.json
	sys_testing();
	sys_testing123(5, 6, 7);
}
