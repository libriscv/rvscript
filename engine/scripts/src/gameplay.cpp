#include "events.hpp"
#include <api.h>
using namespace api;
extern void benchmark_multiprocessing();

int main()
{
	/* This gets called before anything else, on each machine */
	return 0;
}

/* These are used for benchmarking */
static void empty_function() {}

static void full_thread_function()
{
	microthread::create([](int, int, int) { /* ... */ }, 1, 2, 3);
}

static void oneshot_thread_function()
{
	microthread::oneshot([](int, int, int) { /* ... */ }, 1, 2, 3);
}

static void direct_thread_function()
{
	microthread::direct([] { /* ... */ });
}

static void opaque_dyncall_handler()
{
	sys_empty();
	sys_empty();
	sys_empty();
	sys_empty();
	return_fast();
}

static void inline_dyncall_handler()
{
	isys_empty();
	isys_empty();
	isys_empty();
	isys_empty();
	return_fast();
}

PUBLIC(void public_donothing())
{
	/* nothing */
}

inline void* sys_memset(void* vdest, const int ch, std::size_t size)
{
	register char*   a0 asm("a0") = (char*)vdest;
	register int     a1 asm("a1") = ch;
	register size_t  a2 asm("a2") = size;
	register long syscall_id asm("a7") = SYSCALL_MEMSET;

	asm volatile ("ecall"
	:	"=m"(*(char(*)[size]) a0)
	:	"r"(a0), "r"(a1), "r"(a2), "r"(syscall_id));
	return vdest;
}

static void bench_alloc_free()
{
	auto x = std::make_unique_for_overwrite<char[]>(1024);
	__asm__("" :: "m"(x[0]) : "memory");
	//sys_memset(x.get(), 0, 1024);
}

PUBLIC(void benchmarks())
{
	try
	{
		throw GameplayException("This is a test!");
	}
	catch (const GameplayException& ge)
	{
		print("Exception caught: ", ge.what(), "!\n");
		print(
			"Exception thrown from: ", ge.location().function_name(),
			", line ", ge.location().line(), "\n");
	}

	/* This function makes thousands of calls into this machine,
	   while preserving registers, and then prints some statistics. */
	measure("VM function call overhead",
		[] {
		});
	measure("Full thread creation overhead", full_thread_function);
	measure("Oneshot thread creation overhead", oneshot_thread_function);
	measure("Direct thread creation overhead", direct_thread_function);
	measure("Dynamic call handler x4 (inline)", inline_dyncall_handler);
	measure("Dynamic call handler x4 (call)", opaque_dyncall_handler);

	measure("Allocate 1024-bytes, and free it", bench_alloc_free);

	//benchmark_multiprocessing();
}

struct C
{
	std::string to_string() const
	{
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

struct GameObject
{
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
