#include "codebuilder.hpp"

TEST_CASE("Instantiate machine", "[Basic]")
{
	const auto machine = build_and_load(R"M(
	//#include <api.h>

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
