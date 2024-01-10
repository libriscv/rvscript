#include "main_screen.hpp"
#include <nanogui/nanogui.h>

MainScreen::MainScreen() : nanogui::Screen({800, 600}, "hubris")
{
	this->set_shutdown_glfw(true);
}

void MainScreen::init()
{
	nanogui::init();
}

void MainScreen::buildInterface()
{
	static const int debug = false;

	/* Let the script build the GUI during initialization. */
	this->m_script.reset(new Script("gui", "scripts/gui.elf", debug));
}

void MainScreen::mainLoop()
{
	this->set_visible(true);
	this->perform_layout();
	nanogui::mainloop();

	nanogui::shutdown();
}
