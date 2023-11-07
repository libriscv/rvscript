#include "codebuilder.hpp"

TEST_CASE("Instantiate machine", "[Basic]")
{
	const auto machine = build_and_load(R"M(
	#include <api.h>

	int main() {
		return 666;
	}

	extern "C" long MyFunc() {
		return 0xDEADBEEF;
	})M");

	Script script {machine, nullptr, "MyScript", "/tmp/myscript"};
	script.initialize();

	REQUIRE(script.machine().return_value() == 666);

	REQUIRE(script.call("MyFunc") == 0xDEADBEEF);
}

TEST_CASE("Exit game", "[Basic]")
{
	const auto machine = build_and_load(R"M(
	#include <api.h>
	using namespace api;

	int main() {
		Game::exit();

		return 666;
	})M");

	bool exit_called = false;
	Script::on_exit([&] (Script& script) {
		script.machine().stop();
		script.machine().set_result(123);
		exit_called = true;
	});

	Script script {machine, nullptr, "MyScript", "/tmp/myscript"};
	script.initialize();

	REQUIRE(exit_called);
	REQUIRE(script.machine().return_value() == 123);
}

TEST_CASE("Verify dynamic calls work", "[Basic]")
{
	const auto machine = build_and_load(R"M(
	#include <api.h>
	using namespace api;

	int main() {
		sys_empty(); /* Opaque dynamic call */

		isys_empty(); /* Inlined dynamic call */

		return 666;
	})M");

	Script script {machine, nullptr, "MyScript", "/tmp/myscript"};

	int count = 0;
	script.reset_dynamic_call("empty", [&] (Script&) {
		count += 1;
	});

	script.initialize();

	REQUIRE(script.machine().return_value() == 666);
	REQUIRE(count == 2);
}
