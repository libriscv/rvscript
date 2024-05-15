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
	REQUIRE(script.call("did_yep").value() == false);

	timers_loop([] {});

	REQUIRE(timers.active() == 0);
	REQUIRE(script.call("did_yep").value() == true);

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
	REQUIRE(script.call("yep_count").value() == 0);

	timers_loop([&script] {
		REQUIRE(timers.active() == 1);
		const int yep_count = script.call("yep_count").value();
		if (yep_count >= 3)
			timers.stop(0);
	});

	REQUIRE(timers.active() == 0);
	REQUIRE(script.call("yep_count").value() == 3);
}

TEST_CASE("Verify timer storage", "[Timers]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	#include <vector>
	std::vector<int> values;

	static int yep = 0;
	extern "C" int yep_count() {
		return yep;
	}

	extern "C" int get_value(int index) {
		return values.at(index);
	}

	extern "C" void capturing_timers(int value) {
		api::Timer::oneshot(0.5f, [v = value] (auto) {
			values.push_back(v);
		});
	}

	int main() {
		struct MyData {
			int a;
			int b;
			int c;
			int d;
			int e;
			int f;
		} data { 1, 2, 3, 4, 5, 6 };

		api::Timer::periodic(0.25f,
		[data] (auto) {
			assert(data.a == 1);
			assert(data.b == 2);
			assert(data.c == 3);
			assert(data.d == 4);
			assert(data.e == 5);
			assert(data.f == 6);
			yep += 1;
		});
	})M");

	setup_timer_system();
	REQUIRE(timers.active() == 0);

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(timers.active() == 1);
	REQUIRE(script.call("yep_count").value() == 0);

	timers_loop([&script] {
		REQUIRE(timers.active() == 1);
		const int yep_count = script.call("yep_count").value();
		if (yep_count >= 3)
			timers.stop(0);
	});

	REQUIRE(timers.active() == 0);
	REQUIRE(script.call("yep_count").value() == 3);

	script.call("capturing_timers", 123456789);
	script.call("capturing_timers", 987654321);
	script.call("capturing_timers", 135790);
	REQUIRE(timers.active() == 3);

	timers_loop([] {});

	REQUIRE(timers.active() == 0);
	REQUIRE(*script.call("get_value", 0) == 123456789);
	REQUIRE(*script.call("get_value", 1) == 987654321);
	REQUIRE(*script.call("get_value", 2) == 135790);
}
