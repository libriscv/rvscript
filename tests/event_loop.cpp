#include "codebuilder.hpp"

TEST_CASE("Simple event loop", "[EventLoop]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	#include <include/event_loop.hpp>

	static std::array<Events<>, 2> events;

	PUBLIC(void event_loop())
	{
		api::print("Entering event loop...\n");
		while (true)
		{
			for (auto& ev : events)
				ev.consume_work();
			halt();
		}
	}

	PUBLIC(bool add_work(void(*callback)(void*), void* arg))
	{
		EXPECT(callback != nullptr);
		for (auto& ev : events)
			if (ev.add([callback, arg] () {
					callback(arg);
				}))
			{
				return true;
			}
		return false;
	}

	struct WorkObject {
		int work_done = 0;
	};
	PUBLIC(void some_work(WorkObject& obj))
	{
		obj.work_done += 1;
	}

	int main() {
	})M");

	// Load the program and run through main()
	Script script {program, "MyScript", "/tmp/myscript"};

	// Get the address of 'event_loop' and 'add_work'
	const auto event_loop = script.address_of("event_loop");
	const auto add_work = script.address_of("add_work");
	REQUIRE(event_loop != 0x0);
	REQUIRE(add_work != 0x0);

	// Allocate 16 objects on the heap inside the script program
	struct WorkObject {
		int work_done = 0;
	};
	auto shared_objs = script.guest_alloc<WorkObject>(16);

	// Start the event loop
	script.call(event_loop);

	REQUIRE(shared_objs.at(0).work_done == 0);

	// Add some work with the first guest object as argument
	// NOTE: Using preempt() in order to not break the event loop!
	script.preempt(add_work, script.address_of("some_work"), shared_objs.address(0));

	// The work is still not completed
	REQUIRE(shared_objs.at(0).work_done == 0);

	// Let the event loop run for a bit
	script.resume(1000);

	// The work is now completed
	REQUIRE(shared_objs.at(0).work_done == 1);

	// Let the event loop run for a bit
	script.resume(1000);

	// The work is not *more* done
	REQUIRE(shared_objs.at(0).work_done == 1);

	for (size_t i = 0; i < 10; i++) {
		// Add some work with the first guest object as argument
		// NOTE: Using preempt() in order to not break the event loop!
		script.preempt(add_work, script.address_of("some_work"), shared_objs.address(0));
	}

	// The work is *still* not *more* done
	REQUIRE(shared_objs.at(0).work_done == 1);

	// Let the event loop run for a bit
	script.resume(400);

	// More work has been done, but not all of it
	REQUIRE(shared_objs.at(0).work_done >= 2);

	// Let the event loop run until it's done
	script.resume(100'000);

	// All of the work has been done
	REQUIRE(shared_objs.at(0).work_done == 11);
}
