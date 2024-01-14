#include "codebuilder.hpp"
#include <script/event.hpp>

TEST_CASE("Simple events", "[Events]")
{
	const auto program = build_and_load(R"M(
	#include <api.h>

	int main() {}

	extern "C" unsigned MyFunc() {
		return 0xDEADBEEF;
	}
	extern "C" int IntFunc(int arg) {
		return arg * 2;
	}
	extern "C" int StrFunc(const char *arg1, int arg2) {
		assert(strcmp(arg1, "Hello World!") == 0);
		assert(arg2 == 123);
		return arg2 * 2;
	}
	struct Data {
		char arg1[32];
		int  arg2;
	};
	extern "C" int DataFunc(Data& data) {
		assert(strcmp(data.arg1, "Hello World!") == 0);
		assert(data.arg2 == 123);
		return data.arg2 * 2;
	}
	extern "C" int WriteDataFunc(Data& data) {
		strcpy(data.arg1, "Hello World!");
		data.arg2 = 123;
		return data.arg2 * 2;
	}
	)M");

	Script script {program, "MyScript", "/tmp/myscript"};

	/* Function that returns a value */
	Event<unsigned()> ev0(script, "MyFunc");
	REQUIRE(ev0.call() == 0xDEADBEEF);

	/* Function that takes an integer */
	Event<int(int)> ev1(script, "IntFunc");
	REQUIRE(ev1.call(123) == 246);

	/* Function that takes a string and an integer */
	Event<int(const std::string&, int)> ev2(script, "StrFunc");
	REQUIRE(ev2.call("Hello World!", 123) == 246);

	/* Function that takes a zero-terminated string and an integer */
	Event<int(const char* str, int)> ev3(script, "StrFunc");
	REQUIRE(ev3.call("Hello World!", 123) == 246);

	/* Function that takes a struct */
	struct Data {
		char arg1[32];
		int  arg2;
	} data {.arg1 = "Hello World!", .arg2 = 123};
	Event<int(const Data&)> ev4(script, "DataFunc");

	REQUIRE(ev4.call(data) == 246);
	REQUIRE(ev4.call(Data{"Hello World!", 123}) == 246);

	/* Function that modifies a struct, which we will read back */
	auto obj = script.guest_alloc<Data>();
	Event<int(Script::gaddr_t)> ev5(script, "WriteDataFunc");

	/* Call function with the address of our shared object */
	REQUIRE(ev5.call(obj.address(0)) == 246);

	/* Verify that the script correctly modified the struct */
	REQUIRE(std::string(obj.at(0).arg1) == "Hello World!");
	REQUIRE(obj.at(0).arg2 == 123);
}
