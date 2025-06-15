#include "interface.hpp"
#include <api.h>
using namespace api;
static_assert(sizeof(std::string) == 32,
	"std::string must be 32 bytes in size to be compatible with the guest std::string SSO");

PUBLIC(void start())
{
	print("Hello from Level 2!\n");
	int value = gameplay_allowed_function(123);
	print("Back in Level2! Got result value = ", value, "\n");

	// Create a simple event loop with frame data
	struct {
		std::vector<std::string> strings;
		std::vector<int> integers;
		int frame;
	} frame_data;
	while (true) {
		// Wait for a frame to complete
		Game::wait(frame_data);

		// Do some work here, like updating game state, rendering, etc.
		print("Frame ", frame_data.frame, " in Level2 with strings: ");
		for (const std::string& str : frame_data.strings) {
			print("", str, ", ");
		}
		print(" and integers: ");
		for (int i : frame_data.integers) {
			print("", i, ", ");
		}
		print("\n");
	}
}

int main() {}
