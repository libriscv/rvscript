#include <script/script.hpp>
#include <script/util/crc32.hpp>
#include <unistd.h> /* usleep */

#include "timers.hpp"
Timers timers; // put this in a level structure

#include <script/machine/blackbox.hpp>
static Blackbox<Script::MARCH> blackbox;


static std::map<uint32_t, Script> machines;
/* Retrieve machines based on name (hashed), used by system calls.
   With this all our APIs use the same source for machines. */
Script* get_script(uint32_t machine_hash)
{
	auto it = machines.find(machine_hash);
	if (it != machines.end()) {
		return &it->second;
	}
	return nullptr;
}
/* Retrieve machines based on hash and name, throws on failure */
Script& get_script(uint32_t machine_hash, const char* name)
{
	auto it = machines.find(machine_hash);
	if (it != machines.end()) {
		return it->second;
	}
	throw std::runtime_error(
		"Unable to find machine with given hash: " + std::string(name));
}
static Script& create_script(const std::string& name, const std::string& bbname)
{
	const auto& box = blackbox.get(bbname);
	auto it = machines.emplace(std::piecewise_construct,
		std::forward_as_tuple(crc32(name.c_str())),
		std::forward_as_tuple(box.machine, name));
	auto& script = it.first->second;
	script.hash_public_api_symbols(box.sympath);
	return script;
}
#define SCRIPT(x) get_script(crc32(#x), #x)

int main()
{
	/* This binary will be shared among all the machines, for convenience */
	blackbox.insert_binary("gameplay",
		"mods/hello_world/scripts/gameplay.elf",
		"mods/hello_world/scripts/src/gameplay.symbols");

	Script::init();
	/* Naming the machines allows us to call into one machine from another
	   using this name (hashed). These machines will be fully intialized. */
	for (int n = 0; n < 100; n++)
		create_script("gameplay" + std::to_string(n), "gameplay");

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto& events = create_script("events", "gameplay");
	events.call("event_loop");

	/* Get one of our gameplay machines */
	auto& gameplay1 = SCRIPT(gameplay1);
#ifdef RISCV_DEBUG
	gameplay1.enable_debugging();
#endif
	/* This is the main start function, which would be something like the
	   starting function for the current levels script. You can find the
	   implementation in mods/hello_world/scripts/src/gameplay.cpp:28. */
	gameplay1.call("start");

	printf("...\n");
	/* Simulate some physics ticks */
	for (int n = 0; n < 3; n++)
		gameplay1.each_tick_event();

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
		events.resume(5'000);
	}

	/* Pass some non-trivial parameters */
	struct C {
		char c = 'C';
	};
	auto& another_machine = SCRIPT(gameplay2);
	another_machine.call("cpp_function", "Hello", C{}, "World");

	return 0;
}
