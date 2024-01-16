#include "codebuilder.hpp"
#include "../engine/src/setup_timers.cpp"
#include "../engine/src/timers.cpp"

TEST_CASE("Verify oneshot timer", "[Timers]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	bool yep = false;

	extern "C" bool did_yep() {
		return yep;
	}

	extern "C" void shortlived_timer() {
		auto t = api::Timer::oneshot(1.0f, [] (auto) {
			assert(0);
		});
		t.stop();
	}

	int main() {
		api::Timer::oneshot(1.0f, [] (auto) {
			yep = true;
		});
	})M");

	setup_timer_system();
	REQUIRE(timers.active() == 0);

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(timers.active() == 1);
	REQUIRE(script.call("did_yep") == false);

	timers_loop([] {});

	REQUIRE(timers.active() == 0);
	REQUIRE(script.call("did_yep") == true);

	script.call("shortlived_timer");
	REQUIRE(timers.active() == 0);
}

TEST_CASE("Verify periodic timer", "[Timers]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	int yep = 0;

	extern "C" int yep_count() {
		return yep;
	}

	int main() {
		api::Timer::periodic(0.25f, [] (auto) {
			yep ++;
		});
	})M");

	setup_timer_system();
	REQUIRE(timers.active() == 0);

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(timers.active() == 1);
	REQUIRE(script.call("yep_count") == 0);

	timers_loop([&script] {
		REQUIRE(timers.active() == 1);
		const int yep_count = script.call("yep_count");
		if (yep_count >= 3)
			timers.stop(0);
	});

	REQUIRE(timers.active() == 0);
	REQUIRE(script.call("yep_count") == 3);
}
