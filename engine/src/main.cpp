#include "script.hpp"
static std::unique_ptr<Script> gameplay1 = nullptr;
static std::unique_ptr<Script> gameplay2 = nullptr;
static std::unique_ptr<Script> events = nullptr;

#include "timers.hpp"
Timers timers; // put this in a level structure

/* This is the worlds fastest hash map. (C) (R) 2020 */
Script* get_script(uint32_t machine_hash)
{
	if (machine_hash == gameplay1->hash())
		return gameplay1.get();
	if (machine_hash == gameplay2->hash())
		return gameplay2.get();
	if (machine_hash == events->hash())
		return events.get();
	return nullptr;
}

#include "machine/blackbox.hpp"
Blackbox blackbox;

#include <unistd.h> /* usleep */
int main()
{
	/* This binary will be shared among all the machines, for convenience */
	blackbox.insert_binary("gameplay", "mods/hello_world/scripts/gameplay.elf");

	Script::init();
	/* Naming the machines allows us to call into one machine from another
	   using this name (hashed). */
	gameplay1.reset(new Script(*blackbox.get("gameplay"), "gameplay1"));
	/* The symbol file contains public functions that we don't want optimized away. */
	gameplay1->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	gameplay2.reset(new Script(*blackbox.get("gameplay"), "gameplay2"));
	gameplay2->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	events.reset(new Script(*blackbox.get("gameplay"), "events"));
	events->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	events->call("event_loop");

	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in mods/hello_world/scripts/src/gameplay.cpp:28. */
	gameplay1->call("start");

	printf("...\n");
	/* Ordinarily a game engine has a physics loop that ticks regularly,
	   but we don't in this example. Instead we will just sleep until
	   the next available timer. And resume the event loop in between. */
	while (timers.active() > 0) {
		timers.handle_events();
		usleep(timers.next() * 1e6);
		/* This guy should run each engine tick instead. We are passing
		   the maximum number of instructions that we allow it to use.
		   This can work well as a substitute for time spent, provided
		   each complex system call increments the counter sufficiently. */
		events->resume(5'000);
	}

	return 0;
}
