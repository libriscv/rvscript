#include "main_screen.hpp"

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

	/* What happens when the Script calls Game::exit(). */
	Script::on_exit([] (auto& script) {
		strf::to(stdout)(script.name(), " called Game::exit()");
		exit(0);
	});

	Script::set_global_setting("benchmarks", do_benchmarks);
	if (Script::get_global_setting("benchmarks") != do_benchmarks)
		throw std::runtime_error("Insane global settings");

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto events = Script("events", "scripts/gameplay.elf", debug);
	/* A VM function call. The function is looked up in the symbol table
	   of the program binary. Without an entry in the table, we cannot
	   know the address of the function, even if it exists in the code. */
	assert(events.address_of("event_loop") != 0x0);
	events.call("event_loop");

	/* Create the gameplay machine by cloning 'events' (same binary, but new instance) */
	auto gameplay = events.clone("gameplay");

	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in scripts/src/level1.cpp. */
	auto level1 = Script("level1", "scripts/level1.elf", debug);
	/* level1 make remote calls to the gameplay program. */
	level1.setup_remote_calls_to(gameplay);

	level1.call("start");

	/* Use strict remote calls for level2 */
	auto level2 = Script("level2", "scripts/level2.elf", debug);
	/* level2 can make remote calls to the gameplay program. */
	level2.setup_strict_remote_calls_to(gameplay);
	/* Allow calling *only* this function remotely, when in strict mode */
	gameplay.add_allowed_remote_function("_Z25gameplay_allowed_functioni");

	level2.call("start");

	strf::to(stdout)("...\n");
	/* Ordinarily a game engine has a physics loop that ticks regularly,
	   but we don't in this example. Instead we will just sleep until
	   the next available timer. And resume the event loop in between. */
	extern void timers_loop(std::function<void()>);
	timers_loop(
		[&]
		{
			/* This should run each engine tick instead. We are passing
			   the maximum number of instructions that we allow it to use. */
			events.resume(5'000);
		});

	/* Create an event that is callable. */
	struct C
	{
		char c = 'C';
	};
	Event<void(const std::string&, const C&, const std::string&)> my_event(gameplay, "cpp_function");

	/* Pass some non-trivial parameters to the function call. */
	my_event.call("Hello", C {}, "World");


	strf::to(stdout)("...\n");

	/*** Call events of shared game objects ***/

	struct GameObject
	{
		EmbeddedString<32> name;
		bool alive = false;

		// Since this is shared between host and Script,
		// we do not use Event here, as it would contain
		// host pointers, which we don't want to share.
		Script::gaddr_t onDeath = 0x0;
		Script::gaddr_t onAction = 0x0;
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

	   All objects are default-initialized.
	**/
	auto guest_objs = gameplay.guest_alloc<GameObject>(16);

	/* Initialize object at index 0 */
	auto& obj	= guest_objs.at(0);
	obj.alive	= true;
	obj.name	= "myobject";
	obj.onDeath = gameplay.address_of("myobject_death");

	/* Simulate object dying */
	strf::to(stdout)(
		"Calling '", gameplay.symbol_name(obj.onDeath), "' in '",
		gameplay.name(), "' for object at 0x",
		strf::hex(guest_objs.address(0)), "\n");
	assert(obj.alive == true);
	gameplay.call(obj.onDeath, guest_objs.address(0));
	assert(obj.alive == false);

	/* Guest-allocated objects can be moved */
	auto other_guest_objs		 = std::move(guest_objs);
	other_guest_objs.at(0).alive = true;
	gameplay.call(other_guest_objs.at(0).onDeath, other_guest_objs.address(0));
	assert(other_guest_objs.at(0).alive == false);

	strf::to(stdout)("...\n");


	if (Script::get_global_setting("benchmarks").value_or(false))
	{
		// Benchmarks of various features
		gameplay.call("benchmarks");
	}

	strf::to(stdout)("...\nBringing up the main screen!\n");

	MainScreen::init();
	MainScreen screen;

	// See setup_gui.cpp
	extern void setup_gui_system(MainScreen&);
	setup_gui_system(screen);

	screen.buildInterface();

	screen.mainLoop();

	return 0;
}
