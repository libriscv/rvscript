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
	})M");

	Script script {machine, nullptr, "MyScript", "/tmp/myscript"};
	script.initialize();

	// XXX: It calls exit(0) in the host (here).
	REQUIRE(script.machine().return_value() == 0);
}
