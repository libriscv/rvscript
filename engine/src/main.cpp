#include "manage_scripts.hpp"
#include <script/event.hpp>
#include <fmt/core.h>

int main()
{
	const bool debug = getenv("DEBUG") != nullptr;
#ifndef EMBEDDED_MODE
	/* A single program that will be shared among all the machines, for convenience */
	Scripts::load_binary("gameplay",
		"mods/hello_world/scripts/gameplay.elf",
		"mods/hello_world/scripts/src/gameplay.symbols");
#else
	/* "gameplay.elf" program embedded into the engine (by the build system) */
	extern char _binary_gameplay_elf_start;
	extern char _binary_gameplay_elf_end;
	extern char _binary_gameplay_symbols_start;
	extern char _binary_gameplay_symbols_end;
	const size_t binary_size = &_binary_gameplay_elf_end - &_binary_gameplay_elf_start;
	const size_t symbols_size = &_binary_gameplay_symbols_end - &_binary_gameplay_symbols_start;
	Scripts::embedded_binary("gameplay",
		std::string_view{&_binary_gameplay_elf_start, binary_size},
		std::string_view{&_binary_gameplay_symbols_start, symbols_size});
#endif

	Script::setup_syscall_interface();
	Scripts::load_binary("test",
		"mods/hello_world/scripts/gameplay.elf",
		"mods/hello_world/scripts/src/gameplay.symbols");

	/* Naming the machines allows us to call into one machine from another
	   using this name (hashed). These machines will be fully intialized. */
	for (int n = 1; n <= 100; n++) {
		auto& script = Scripts::create(
			"gameplay" + std::to_string(n), "gameplay", debug);
		// Dynamically extend the functionality available
		// See: timers_setup.cpp
		extern void setup_timer_system(Script&);
		setup_timer_system(script);
		// See: debugging.cpp
		extern void setup_debugging_system(Script&);
		setup_debugging_system(script);
	}

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto& events = Scripts::create("events", "gameplay");
	/* A VM function call. The function must be public (listed in the symbols file). */
	events.call("event_loop");

	/* Get one of our gameplay machines */
	auto& gameplay1 = SCRIPT("gameplay1");
	/* Create an dynamic function for benchmarking */
	gameplay1.set_dynamic_call("empty", [] (auto&) {});

	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in mods/hello_world/scripts/src/gameplay.cpp:28. */
	gameplay1.call("start");

	fmt::print("...\n");
	/* Simulate some physics ticks */
	for (int n = 0; n < 3; n++)
		gameplay1.each_tick_event();

	fmt::print("...\n");
	/* Ordinarily a game engine has a physics loop that ticks regularly,
	   but we don't in this example. Instead we will just sleep until
	   the next available timer. And resume the event loop in between. */
	extern void timers_loop(std::function<void()>);
	timers_loop([&] {
		/* This guy should run each engine tick instead. We are passing
		   the maximum number of instructions that we allow it to use.
		   This can work well as a substitute for time spent, provided
		   each complex system call increments the counter sufficiently. */
		events.resume(5'000);
	});

	/* Pass some non-trivial parameters to a VM function call.
	   Also call gameplay2 instead of gameplay1. */
	struct C {
		char c = 'C';
	};
	auto& another_machine = SCRIPT("gameplay2");
	another_machine.call("cpp_function", "Hello", C{}, "World");

	fmt::print("...\n");

	/* Call events of a shared game object */
	struct GameObject {
		bool alive = false;
		char name[30] = {0};

		Event onDeath;
		Event onAction;
	};
	GameObject objects[200]; // Page worth of objects

	/* Insert objects into memory.
	   This allows zero-copy sharing of game state. */
	static constexpr Script::gaddr_t OBJECT_AREA = 0xC000000;
	another_machine.machine().memory.insert_non_owned_memory(
		OBJECT_AREA, objects, sizeof(objects) & ~4095);

	/* Initialize object */
	auto& obj = objects[0];
	obj.alive = true;
	fmt::format_to(obj.name, "{}", "myobject");
	obj.onDeath = Event(another_machine, "myobject_death");

	/* Simulate object dying */
	fmt::print("Calling '{}' in '{}'\n",
		obj.onDeath.function(), obj.onDeath.script().name());
	assert(obj.alive == true);
	obj.onDeath.call(OBJECT_AREA + 0 * sizeof(GameObject));
	assert(obj.alive == false);

	fmt::print("...\n");

	/* Test dynamic functions */
	int called = 0x0;
	gameplay1.set_dynamic_call("testing",
		[&] (auto&) {
			called |= 0x1;
		});
	gameplay1.set_dynamic_call("testing123",
		[&] (auto& s) {
			const auto [arg1, arg2, arg3] =
				s.machine().template sysargs <int, int, int> ();
			if (arg1 == 5 && arg2 == 6 && arg3 == 7) {
				called |= 0x2;
			}
		});
	gameplay1.call("test_dynamic_functions");
	// All the functions should have been called
	if (called != 0x3) exit(1);
	// Duplicate hash:
	//gameplay1.set_dynamic_call("empty", [] (auto&) {});
	// This will replace the function:
	gameplay1.reset_dynamic_call("empty", [] (auto&) {});
	// This will remove the function:
	gameplay1.reset_dynamic_call("empty");

	// (Full) Fork benchmark
	fmt::print("Benchmarking full fork:\n");
	Script::benchmark(
		[&gameplay1] {
			Script {gameplay1.machine(), nullptr, ""};
		});
	// Reset benchmark
	fmt::print("Benchmarking reset:\n");
	Script uhh {gameplay1.machine(), nullptr, ""};
	Script::benchmark(
		[&uhh] {
			uhh.reset();
		});

	void do_nim_testing(bool debug);
	do_nim_testing(debug);

	return 0;
}

#include <unistd.h>
void do_nim_testing(bool debug)
{
	/* If the nim program was built, we can run hello_nim */
	#define NIMPATH  "../programs/micronim/riscv64-unknown-elf"
	const char* nimfile = NIMPATH "/hello_nim";
	if (access(nimfile, F_OK) == 0)
	{
		Scripts::load_binary("micronim",
			nimfile,
			NIMPATH "/../src/default.symbols");
		auto& nim_machine = Scripts::create("nim", "micronim", debug);
		if (nim_machine.address_of("hello_nim")) {
			fmt::print("...\n");
			extern void setup_debugging_system(Script&);
			setup_debugging_system(nim_machine);
			/* This call sets a breakpoint, which if you don't connect in
			   time will be skipped over, and execution continues. */
			nim_machine.call("hello_nim");

			nim_machine.reset_dynamic_call("remote_gdb", [] (auto&) {});
			nim_machine.stdout_enable(false);
			const auto address = nim_machine.address_of("bench_nim");
			nim_machine.vmbench(address);
			nim_machine.stdout_enable(true);
		}
	}
}
