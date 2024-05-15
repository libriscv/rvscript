#include "codebuilder.hpp"

TEST_CASE("Out of instructions", "[Limits]")
{
	const auto program = build_and_load(R"M(
	__asm__(".global lbl\t\n"
		"lbl: j lbl\t\n");

	extern "C" void lbl();

	int main() {
		lbl();
	})M");

	REQUIRE_THROWS(
	[&] {
		Script script {program, "MyScript", "/tmp/myscript"};
	}());
}

TEST_CASE("Out of instructions VM calls", "[Limits]")
{
	const auto program = build_and_load(R"M(

	extern "C" void infinite_loop() {
		infinite_loop();
	}

	int main() {
	})M");

	Script script {program, "MyScript", "/tmp/myscript"};

	script.call("infinite_loop");
}

TEST_CASE("Recursive", "[Limits]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>
	using namespace api;

	extern "C" void exit_caller() {
		Game::exit();
	}

	int main() {
		return 666;
	})M");

	bool exit_called = false;
	Script::on_exit([&] (Script& script) {
		script.call("exit_caller");
		exit_called = true;
	});

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(!exit_called);

	// This will recursively call exit_caller, but eventually throw an exception
	script.call("exit_caller");

	REQUIRE(exit_called);
}
