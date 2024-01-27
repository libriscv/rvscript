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

TEST_CASE("Verify dynamic calls with arguments", "[Basic]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>

	extern "C" void test_strings() {
		sys_test_strings("1234", "45678", 5);
	}
	extern "C" void test_args() {
		sys_test_3i3f(123, 456, 789, 10.0f, 100.0f, 1000.0f);
	}
	extern "C" void test_inlined_args() {
		isys_test_3i3f(123, 456, 789, 10.0f, 100.0f, 1000.0f);
	}

	int main() {
	})M");

	int strings_called = 0;
	int args_called = 0;

	Script::set_dynamic_call("void sys_test_strings (const char*, const char*, size_t)",
	[&] (Script& script) {
		auto [str, view] = script.args<std::string, std::string_view> ();
		REQUIRE(str == "1234");
		REQUIRE(view == "45678");
		strings_called += 1;
	});
	Script::set_dynamic_call("void sys_test_3i3f (int, int, int, float, float, float)",
	[&] (Script& script) {
		auto [i1, i2, i3, f1, f2, f3] = script.args<int, int, int, float, float, float> ();
		REQUIRE(i1 == 123);
		REQUIRE(i2 == 456);
		REQUIRE(i3 == 789);
		REQUIRE(f1 == 10.0f);
		REQUIRE(f2 == 100.0f);
		REQUIRE(f3 == 1000.0f);
		args_called += 1;
	});

	Script script {program, "MyScript", "/tmp/myscript"};

	REQUIRE(strings_called == 0);
	REQUIRE(args_called == 0);

	script.call("test_strings");

	REQUIRE(strings_called == 1);
	REQUIRE(args_called == 0);

	// Clear argument registers
	for (int i = 0; i < 8; i++) {
		script.machine().cpu.reg(10+i) = 0;
		script.machine().cpu.registers().getfl(10+i).load_u64(0);
	}

	script.call("test_args");

	REQUIRE(strings_called == 1);
	REQUIRE(args_called == 1);

	// Clear argument registers
	for (int i = 0; i < 8; i++) {
		script.machine().cpu.reg(10+i) = 0;
		script.machine().cpu.registers().getfl(10+i).load_u64(0);
	}

	script.call("test_inlined_args");

	REQUIRE(strings_called == 1);
	REQUIRE(args_called == 2);
}
