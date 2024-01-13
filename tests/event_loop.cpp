#include "codebuilder.hpp"

#include "../ext/libriscv/binaries/barebones/libc/include/event_loop.hpp"

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
			if (ev.add([callback, arg] {
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

TEST_CASE("Advanced event loop", "[EventLoop]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	#include <include/event_loop.hpp>

	PUBLIC(void event_loop(SharedEvents<>& ev0, SharedEvents<>& ev1))
	{
		api::print("Entering event loop...\n");
		while (true)
		{
			try {
				while (ev0.has_work() || ev1.has_work()) {
					ev0.consume_work();
					ev1.consume_work();
				}
				halt();
			} catch (const std::exception& e) {
				api::print("Event loop exception: ", e.what(), "\n");
			} catch (...) {
				//api::print("Event loop unknown exception\n");
			}
		}
	}

	struct WorkObject {
		int work_done = 0;
	};
	PUBLIC(void some_work(WorkObject& obj))
	{
		obj.work_done += 1;
	}
	PUBLIC(void some_erroneous_work(WorkObject& obj))
	{
		obj.work_done += 1;
		throw "";
	}

	int main() {
	})M");

	// Load the program and run through main()
	Script script {program, "MyScript", "/tmp/myscript"};

	// Allocate 16 objects on the heap inside the script program
	struct WorkObject {
		int work_done = 0;
	};
	auto shared_objs = script.guest_alloc<WorkObject>(16);

	auto shared_evs = script.guest_alloc<SharedEvents<>>(2);

	// Start the event loop
	script.call("event_loop", shared_evs.address(0), shared_evs.address(1));

	REQUIRE(shared_objs.at(0).work_done == 0);

	// Create a function that picks a free event loop
	auto select_free_ev = [&shared_evs] () -> SharedEvents<>& {
		if (!shared_evs.at(0).in_use)
			return shared_evs.at(0);
		return shared_evs.at(1);
	};

	// Add some work with the first guest object as argument
	auto& ev = select_free_ev();
	ev.host_add(script.address_of("some_work"), shared_objs.address(0));

	// The work is still not completed
	REQUIRE(shared_objs.at(0).work_done == 0);

	// Let the event loop run for a bit
	script.resume(1000);

	// The work is now completed
	REQUIRE(shared_objs.at(0).work_done == 1);

	const auto some_work = script.address_of("some_work");
	shared_objs.at(0).work_done = 0;
	int expected_work = 0;

	// Run event loop 10k times, and keep adding work
	// Remember how much work was actually added
	for (int i = 0; i < 10000; i++) {
		auto& ev = select_free_ev();
		if (ev.host_add(some_work, shared_objs.address(0)))
			expected_work += 1;

		script.resume(100);
	}

	// Finish all work
	script.resume(10'000'000ull);

	REQUIRE(!shared_evs.at(0).has_work());
	REQUIRE(!shared_evs.at(1).has_work());
	REQUIRE(expected_work == shared_objs.at(0).work_done);

	const auto some_erroneous_work = script.address_of("some_erroneous_work");
	REQUIRE(some_erroneous_work != 0x0);

	shared_objs.at(0).work_done = 0;
	expected_work = 0;

	// Run event loop 10k times, and keep adding problematic work
	for (int i = 0; i < 10000; i++) {
		auto& ev = select_free_ev();
		if (ev.host_add(some_erroneous_work, shared_objs.address(0)))
			expected_work += 1;

		script.resume(30'000ull);
	}

	// Finish all work
	script.resume(50'000'000ull);

	REQUIRE(!shared_evs.at(0).has_work());
	REQUIRE(!shared_evs.at(1).has_work());
	REQUIRE(expected_work == shared_objs.at(0).work_done);
}
