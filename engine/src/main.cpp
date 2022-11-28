#include "manage_scripts.hpp"
#include <script/event.hpp>
#include <fmt/core.h>

int main()
{
	const bool debug = getenv("DEBUG") != nullptr;
	Script::setup_syscall_interface();
	// Dynamically extend the functionality available
	// See: timers_setup.cpp
	extern void setup_timer_system();
	setup_timer_system();
	// See: debugging.cpp
	extern void setup_debugging_system();
	setup_debugging_system();

	/* A single program that will be used as shared mutable
	   storage among all the level programs. */
	Scripts::load_binary("gameplay",
		"mods/hello_world/scripts/gameplay.elf", "../programs/symbols.map");
	Scripts::create("gameplay", "gameplay", debug);

	/* A few levels. */
	Scripts::load_binary("level1",
		"mods/hello_world/scripts/level1.elf", "../programs/symbols.map");
	Scripts::create("level1", "level1", debug);

	Scripts::load_binary("level2",
		"mods/hello_world/scripts/level2.elf", "../programs/symbols.map");
	Scripts::create("level2", "level2", debug);

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto& events = Scripts::create("events", "gameplay");
	/* A VM function call. The function must be public (listed in the symbols file). */
	events.call("event_loop");

	/* Get the gameplay machine */
	auto& gameplay = SCRIPT("gameplay");

	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in mods/hello_world/scripts/src/gameplay.cpp. */
	auto& level1 = SCRIPT("level1");
	level1.setup_remote_calls_to(gameplay);

	level1.call("start");

	auto& level2 = SCRIPT("level2");
	level2.setup_remote_calls_to(gameplay);

	level2.call("start");

	fmt::print("...\n");
	/* Simulate some physics ticks */
	for (int n = 0; n < 3; n++)
		level1.each_tick_event();

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

	/* Pass some non-trivial parameters to a VM function call. */
	struct C {
		char c = 'C';
	};
	gameplay.call("cpp_function", "Hello", C{}, "World");

	fmt::print("...\n");

	/* Call events of a shared game object */
	struct GameObject {
		bool alive = false;
		std::array<char, 30> name {};

		Event onDeath;
		Event onAction;
	};
	GameObject objects[200]; // Page worth of objects

	/* Insert objects into memory.
	   This allows zero-copy sharing of game state. */
	static constexpr Script::gaddr_t OBJECT_AREA = 0xC000000;
	gameplay.machine().memory.insert_non_owned_memory(
		OBJECT_AREA, objects, sizeof(objects) & ~4095UL);

	/* Initialize object */
	auto& obj = objects[0];
	obj.alive = true;
	fmt::format_to(obj.name.data(), "{}", "myobject");
	obj.onDeath = Event(gameplay, "myobject_death");

	/* Simulate object dying */
	fmt::print("Calling '{}' in '{}'\n",
		obj.onDeath.function(), obj.onDeath.script().name());
	assert(obj.alive == true);
	obj.onDeath.call(OBJECT_AREA + 0 * sizeof(GameObject));
	assert(obj.alive == false);

	fmt::print("...\n");

	/* Test dynamic functions */
	int called = 0x0;
	gameplay.set_dynamic_call("testing",
		[&] (auto&) {
			called |= 0x1;
		});
	gameplay.set_dynamic_call("testing123",
		[&] (auto& s) {
			const auto [arg1, arg2, arg3] =
				s.machine().template sysargs <int, int, int> ();
			if (arg1 == 5 && arg2 == 6 && arg3 == 7) {
				called |= 0x2;
			}
		});
	gameplay.call("test_dynamic_functions");
	// All the functions should have been called
	if (called != 0x3) exit(1);
	// INVALID (Duplicate hash):
	//gameplay.set_dynamic_call("empty", [] (auto&) {});
	// This will replace the function:
	gameplay.reset_dynamic_call("empty", [] (auto&) {});
	// This will remove the function:
	gameplay.reset_dynamic_call("empty");

	/* Create an dynamic function for benchmarking */
	gameplay.set_dynamic_call("empty", [](auto &) {});

	// Benchmarks of various features
	gameplay.call("benchmarks");

	// (Full) Fork benchmark
	fmt::print("Benchmarking full fork:\n");
	Script::benchmark(
		[&gameplay] {
			Script {gameplay.machine(), nullptr, "name", "file"};
		});
	// Reset benchmark
	fmt::print("Benchmarking reset:\n");
	Script uhh {gameplay.machine(), nullptr, "name", "file"};
	Script::benchmark(
		[&uhh] {
			uhh.reset();
		});

	void do_nim_testing(bool debug);
	do_nim_testing(debug);

	void do_nelua_testing(bool debug);
	do_nelua_testing(debug);

	return 0;
}

#include <unistd.h>
static Script* do_nim_load_and_create(const std::string& name, bool debug)
{
	/* If the nim program exists, load it */
#if RISCV_ARCH == 32
#   define NIMPATH  "../programs/micronim/riscv32-unknown-elf/"
#else
#   define NIMPATH  "../programs/micronim/riscv64-unknown-elf/"
#endif
	const std::string filename = NIMPATH + name;
	if (access(filename.c_str(), F_OK) == 0)
	{
		const std::string binary = name + ".nim.elf";
		Scripts::load_binary(binary,
			filename,
			NIMPATH "../src/default.symbols");
		return &Scripts::create("nim." + name, binary, debug);
	}
	return nullptr;
}
void do_nim_testing(bool debug)
{
	auto* nim1 = do_nim_load_and_create("hello", debug);
	if (nim1 && nim1->address_of("hello_nim")) {
		fmt::print("...\n");
		/* This call initiates a breakpoint, which you can connect to
			remotely with GDB. Once disconnected, execution continues. */
		nim1->call("hello_nim");
	}

	auto* nim2 = do_nim_load_and_create("bench", debug);
	if (nim2) {
		nim2->call("start");
	}
}

void do_nelua_testing(bool debug)
{
	/* If the nelua program was built, we can run nelua_sum */
#if RISCV_ARCH == 32
#   define NELUAPATH  "../programs/nelua/riscv32-unknown-elf"
#else
#   define NELUAPATH  "../programs/nelua/riscv64-unknown-elf"
#endif
	const char* filename = NELUAPATH "/hello_nelua";
	if (access(filename, F_OK) == 0)
	{
		Scripts::load_binary("hello_nelua",
			filename,
			NELUAPATH "/../src/default.symbols");
		auto& machine = Scripts::create("nelua", "hello_nelua", debug);
		if (machine.address_of("hello_nelua")) {
			fmt::print("...\n");
			long result = machine.call("hello_nelua");
			fmt::print("> nelua returned {}\n", result);
		}
	}
}
