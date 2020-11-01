#include <script/script.hpp>
#include <script/event.hpp>
#include <script/util/crc32.hpp>
#include <fmt/core.h>
#include <unistd.h> /* usleep */

#include "timers.hpp"
Timers timers; // put this in a level structure

#include <script/machine/blackbox.hpp>
static Blackbox<Script::MARCH> blackbox;

/* List of initialized machines ready to go. The Script class
   is a fully initialized machine with a hashed public API and
   a lot of helper functions to simplify usage. */
static std::map<uint32_t, Script> scripts;

static Script& create_script(const std::string& name, const std::string& bbname)
{
	const auto& box = blackbox.get(bbname);
	auto it = scripts.emplace(std::piecewise_construct,
		std::forward_as_tuple(crc32(name.c_str())),
		std::forward_as_tuple(box.machine, name));
	auto& script = it.first->second;
	/* When embedding a program, the public API symbols are stored in memory */
	if (!box.symbols.empty())
		script.hash_public_api_symbols(box.symbols);
	else
		script.hash_public_api_symbols_file(box.sympath);
	return script;
}
/* Retrieve machines based on name (hashed), used by system calls.
   With this all our APIs use the same source for machines. */
Script* get_script(uint32_t machine_hash)
{
	auto it = scripts.find(machine_hash);
	if (it != scripts.end()) {
		return &it->second;
	}
	return nullptr;
}
/* Retrieve machines based on hash and name, throws on failure */
Script& get_script(uint32_t machine_hash, const char* name)
{
	auto it = scripts.find(machine_hash);
	if (it != scripts.end()) {
		return it->second;
	}
	throw std::runtime_error(
		"Unable to find machine: " + std::string(name));
}
#define SCRIPT(x) get_script(crc32(#x), #x)

int main()
{
#ifndef EMBEDDED_MODE
	/* A single program that will be shared among all the machines, for convenience */
	blackbox.insert_binary("gameplay",
		"mods/hello_world/scripts/gameplay.elf",
		"mods/hello_world/scripts/src/gameplay.symbols");
#else
	/* Program embedded into the engine */
	extern char _binary_gameplay_elf_start;
	extern char _binary_gameplay_elf_end;
	extern char _binary_gameplay_symbols_start;
	extern char _binary_gameplay_symbols_end;
	blackbox.insert_embedded_binary("gameplay",
		&_binary_gameplay_elf_start, &_binary_gameplay_elf_end,
		&_binary_gameplay_symbols_start, &_binary_gameplay_symbols_end);
#endif

	/* Naming the machines allows us to call into one machine from another
	   using this name (hashed). These machines will be fully intialized. */
	for (int n = 0; n < 100; n++)
		create_script("gameplay" + std::to_string(n), "gameplay");

	/* The event_loop function can be resumed later, and can execute work
	   that has been preemptively handed to it from other machines. */
	auto& events = create_script("events", "gameplay");
	/* A VM function call. The function must be public (listed in the symbols file). */
	events.call("event_loop");

	/* Get one of our gameplay machines */
	auto& gameplay1 = SCRIPT(gameplay1);
	/* Create an empty group function for benchmarking */
	gameplay1.set_dynamic_function(1, 0, [] (auto&) {});
#ifdef RISCV_DEBUG
	gameplay1.enable_debugging();
#endif
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
	while (timers.active() > 0) {
		timers.handle_events();
		usleep(timers.next() * 1e6);
		/* This guy should run each engine tick instead. We are passing
		   the maximum number of instructions that we allow it to use.
		   This can work well as a substitute for time spent, provided
		   each complex system call increments the counter sufficiently. */
		events.resume(5'000);
	}

	/* Pass some non-trivial parameters to a VM function call.
	   Also call gameplay2 instead of gameplay1. */
	struct C {
		char c = 'C';
	};
	auto& another_machine = SCRIPT(gameplay2);
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

	/* Insert objects into memory, and as this is just example
	   code we won't try to align anything to page sizes.
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

	/* If nim is enabled, we can run the nim test */
	if (another_machine.resolve_address("nim_test")) {
		fmt::print("...\n");
		another_machine.call("nim_test");
	}

	fmt::print("...\n");

	/* Test dynamic functions */
	const int GROUP = 1;
	int called = 0x0;
	gameplay1.set_dynamic_functions(GROUP, {
		{1, [&] (auto& script) {
			const auto& m = script.machine();
			auto [i, str] = m.template sysargs<int, std::string> ();
			fmt::print("{} from a function group handler (also {})\n",
				str, i);
			assert(gameplay1.current_group() == GROUP);
			assert(gameplay1.current_group_index() == 1);
			called |= 0x1;
		}},
		{2, [&] (auto&) {
			assert(gameplay1.current_group() == GROUP);
			assert(gameplay1.current_group_index() == 2);
			called |= 0x2;
		}},
		{33, [&] (auto&) {
			assert(gameplay1.current_group() == GROUP);
			assert(gameplay1.current_group_index() == 33);
			called |= 0x4;
		}},
		{63, [&] (auto&) {
			assert(gameplay1.current_group() == GROUP);
			assert(gameplay1.current_group_index() == 63);
			called |= 0x8;
		}}
	});
	gameplay1.call("test_function_groups");
	// All the functions should have been called
	if (called != 0xF) exit(1);

	return 0;
}
