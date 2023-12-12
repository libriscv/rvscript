#include "main_screen.hpp"

#include "manage_scripts.hpp"
#include <api/embedded_string.hpp>
#include <script/event.hpp>
#include <strf/to_cfile.hpp>

int main()
{
	const bool debug = getenv("DEBUG") != nullptr;
	const bool do_benchmarks = getenv("BENCHMARK") != nullptr;

	// Dynamically extend the functionality available
	// See: setup_timers.cpp
	extern void setup_timer_system();
	setup_timer_system();
	// See: script_debug.cpp
	extern void setup_debugging_system();
	setup_debugging_system();
	// See: test_dynamic.cpp
	extern void setup_dynamic_calls();
	setup_dynamic_calls();

	/* What happens when the Script wants to exit the game. */
	Script::on_exit([] (auto& script) {
		strf::to(stdout)(script.name(), " called Game::exit()");
		exit(0);
	});

	Script::set_global_setting("benchmarks", do_benchmarks);
	if (Script::get_global_setting("benchmarks") != do_benchmarks)
		throw std::runtime_error("Insane global settings");

	/* A single program that will be used as shared mutable
		   storage among all the level programs. */
	Scripts::load_binary("gameplay", "scripts/gameplay.elf");
	Scripts::create("gameplay", "gameplay", debug);

	/* A few levels. */
	Scripts::load_binary("level1", "scripts/level1.elf");
	Scripts::create("level1", "level1", debug);

	Scripts::load_binary("level2", "scripts/level2.elf");
	Scripts::create("level2", "level2", debug);

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto& events = Scripts::create("events", "gameplay");
	/* A VM function call. The function must be public (listed in the symbols
	 * file). */
	events.call("event_loop");

	/* Get the gameplay machine */
	auto& gameplay = SCRIPT("gameplay");

	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in scripts/src/gameplay.cpp. */
	auto& level1 = SCRIPT("level1");
	level1.setup_remote_calls_to(gameplay);

	level1.call("start");

	/* Use strict remote calls for level2 */
	auto& level2 = SCRIPT("level2");
	level2.setup_strict_remote_calls_to(gameplay);
	/* Allow calling *only* this function remotely, when in strict mode */
	gameplay.add_allowed_remote_function(
		gameplay.address_of("_Z25gameplay_allowed_functioni"));

	level2.call("start");

	strf::to(stdout)("...\n");
	/* Simulate some physics ticks */
	for (int n = 0; n < 3; n++)
		level1.each_tick_event();

	strf::to(stdout)("...\n");
	/* Ordinarily a game engine has a physics loop that ticks regularly,
	   but we don't in this example. Instead we will just sleep until
	   the next available timer. And resume the event loop in between. */
	extern void timers_loop(std::function<void()>);
	timers_loop(
		[&]
		{
			/* This guy should run each engine tick instead. We are passing
			   the maximum number of instructions that we allow it to use.
			   This can work well as a substitute for time spent, provided
			   each complex system call increments the counter sufficiently. */
			events.resume(5'000);
		});

	/* Pass some non-trivial parameters to a VM function call. */
	struct C
	{
		char c = 'C';
	};

	gameplay.call("cpp_function", "Hello", C {}, "World");

	strf::to(stdout)("...\n");

	/* ** Call events of a shared game object **
	   1. Share memory with the guest
	   2. Set up the objects
	   3. Call with address as offset from shared area
	*/
	static constexpr Script::gaddr_t OBJECT_AREA = 0xC000000;

	struct GameObject
	{
		EmbeddedString<32> name;
		bool alive = false;

		Event onDeath;
		Event onAction;

		static Script::gaddr_t address(size_t i)
		{
			return OBJECT_AREA + i * sizeof(GameObject);
		}
	};

	/** Allocate 16 objects on the guest.
	   This uses RAII to sequentially allocate a range
	   for the objects, which is freed on destruction.
	   If the object range is larger than a page (4k),
	   the guarantee will no longer hold, and things will
	   stop working if the pages are not sequential on the
	   host as well. If the objects are somewhat large or
	   you need many, simply manage 1 or more objects at a
	   time. This is a relatively inexpensive abstraction.

	   Example:
	   gameplay.guest_alloc<GameObject>(16) allocates one
	   single memory range on the managed heap of size:
	    sizeof(GameObject) * 16.

	   The returned object manages all the objects, and
	   on destruction will free all objects. It can be
	   moved.

	   All objects are potentially uninitialized, like all
	   heap allocations, and will need to be individually
	   initialized.
	**/
	auto guest_objs = gameplay.guest_alloc<GameObject>(16);

	/* Initialize object at index 0 */
	auto& obj	= guest_objs.at(0);
	obj.alive	= true;
	obj.name	= "myobject";
	obj.onDeath = Event(gameplay, "myobject_death");

	/* Simulate object dying */
	strf::to(stdout)(
		"Calling '", obj.onDeath.function(), "' in '",
		obj.onDeath.script().name(), "' for object at 0x",
		strf::hex(guest_objs.address(0)), "\n");
	assert(obj.alive == true);
	obj.onDeath.call(guest_objs.address(0));
	assert(obj.alive == false);

	/* Guest-allocated objects can be moved */
	auto other_guest_objs		 = std::move(guest_objs);
	other_guest_objs.at(0).alive = true;
	other_guest_objs.at(0).onDeath.call(other_guest_objs.address(0));
	assert(other_guest_objs.at(0).alive == false);

	strf::to(stdout)("...\n");

	/* Test dynamic functions */
	int called = 0x0;
	gameplay.set_dynamic_call(
		"testing",
		[&](auto&)
		{
			called |= 0x1;
		});
	gameplay.set_dynamic_call(
		"testing_strings",
		[&](auto& s)
		{
			// Argument 1: A heap-allocated string
			// Argument 2: A string-view pointing into guest memory
			const auto [str, view]
				= s.machine().template sysargs<std::string, std::string_view>();
			if (str == "Hello World!" && view == "Hello World!")
			{
				called |= 0x2;
			}
		});
	gameplay.set_dynamic_call(
		"testing123",
		[&](auto& s)
		{
			const auto [arg1, arg2, arg3, arg4, arg5, arg6]
				= s.machine().template sysargs<int, int, int, float, float, float>();
			if (arg1 == 4 && arg2 == 5 && arg3 == 6 && arg4 == 7.0 && arg5 == 8.0 && arg6 == 9.0)
			{
				called |= 0x4;
			}
		});
	gameplay.call("test_dynamic_functions");
	// All the functions should have been called
	if (called != (0x1 | 0x2 | 0x4)) {
		strf::to(stderr)(
			"Error: Dynamic calls not invoked or did not set locals!?\n");
		exit(1);
	}

	// INVALID (Duplicate hash):
	// gameplay.set_dynamic_call("empty", [] (auto&) {});
	// This will replace the function:
	gameplay.reset_dynamic_call("empty", [](auto&) {});
	// This will remove the function:
	gameplay.reset_dynamic_call("empty");

	strf::to(stdout)("* Dynamic call tests passed!\n");

	if (Script::get_global_setting("benchmarks").value_or(false))
	{
		// Create an dynamic function for benchmarking
		gameplay.set_dynamic_call("empty", [](auto&) {});

		// Benchmarks of various features
		gameplay.call("benchmarks");

		// Fork benchmarks
		strf::to(stdout)("Benchmarking RISC-V machine fork:\n");
		Script::benchmark(
			[&gameplay]
			{
				// Forking the underlying virtual machine
				const riscv::MachineOptions<Script::MARCH> options {
					.memory_max		  = 16ULL << 20,
					.stack_size		  = 2ULL << 20,
					.use_memory_arena = true
				};
				riscv::Machine<Script::MARCH> fork { gameplay.machine(), options };
			});
		strf::to(stdout)("Benchmarking Script instance fork:\n");
		Script::benchmark(
			[&gameplay]
			{
				// Create a new Script instance based on the underlying virtual machine
				Script {gameplay.machine(), nullptr, "name", "file"};
			});
	}

	strf::to(stdout)("...\nBringing up the main screen!\n");

	MainScreen::init();
	MainScreen screen;

	// See setup_gui.cpp
	extern void setup_gui_system(MainScreen&);
	setup_gui_system(screen);

	screen.buildInterface();

	// ???
	// screen.set_position({200, 200});
	screen.mainLoop();

	return 0;
}
