#include "interface.hpp"
#include <api.h>
using namespace api;

PUBLIC(void start())
{
	print("Hello from Level 2!\n");
	int value = gameplay_allowed_function(123);
	print("Back in Level2! Got result value = ", value, "\n");

	// Create a simple event loop
	while (true) {
		// Wait for a frame to complete
		struct {
			int frame = 0;
		} frame_data;
		Game::wait(frame_data);

		// Do some work here, like updating game state, rendering, etc.
		print("Frame ", frame_data.frame, " in Level2!\n");
	}
}

int main() {}
