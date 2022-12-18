#include "main_screen.hpp"
#include <script/script.hpp>

int main()
{
	const bool debug = getenv("DEBUG") != nullptr;
	Script::setup_syscall_interface();
	// Dynamically extend the functionality available
	// See: setup_timers.cpp
	extern void setup_timer_system();
	setup_timer_system();
	// See: script_debug.cpp
	extern void setup_debugging_system();
	setup_debugging_system();

	MainScreen::init();
	MainScreen screen;

	// See setup_gui.cpp
	extern void setup_gui_system(MainScreen&);
	setup_gui_system(screen);


	screen.buildInterface();

	// ???
	// screen.set_position({200, 200});
	screen.mainLoop();

	return 0;
}
