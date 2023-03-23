#include "main_screen.hpp"
#include "manage_scripts.hpp"
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

	Scripts::load_binary("gui", "scripts/gui.elf");
	Scripts::create("gui", "gui", debug);
}

void MainScreen::mainLoop()
{
	this->set_visible(true);
	this->perform_layout();
	nanogui::mainloop();

	nanogui::shutdown();
}
