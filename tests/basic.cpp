#include "codebuilder.hpp"

TEST_CASE("Instantiate machine", "[Basic]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>

	int main() {
		return 666;
	}

	extern "C" long MyFunc() {
		return 0xDEADBEEF;
	})M");

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(script.machine().return_value() == 666);

	REQUIRE(script.call("MyFunc") == 0xDEADBEEF);
}

TEST_CASE("Exit game", "[Basic]")
{
	const auto program = build_and_load(R"M(
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

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(exit_called);
	REQUIRE(script.machine().return_value() == 123);
}

TEST_CASE("Verify dynamic calls work", "[Basic]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>

	int main() {
		sys_empty(); /* Opaque dynamic call */

		isys_empty(); /* Inlined dynamic call */

		return 666;
	})M");

	int count = 0;
	Script::set_dynamic_call("void sys_empty ()", [&] (Script&) {
		count += 1;
	});

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(script.machine().return_value() == 666);
	REQUIRE(count == 2);
}

TEST_CASE("Verify late dynamic calls work", "[Basic]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>

	extern "C" int my_func() {
		sys_empty(); /* Opaque dynamic call */

		isys_empty(); /* Inlined dynamic call */

		return 666;
	}

	int main() {
	})M");

	// Unset the previous 'empty'
	Script::set_dynamic_call("void sys_empty ()", nullptr);

	Script script {program, "MyScript", "/tmp/myscript"};

	// Add new 'empty' after initialization
	int count = 0;
	Script::set_dynamic_call("void sys_empty ()", [&] (Script&) {
		count += 1;
	});

	auto ret = script.call("my_func");

	REQUIRE(ret == 666);
	REQUIRE(count == 2);
}
