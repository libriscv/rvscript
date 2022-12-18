#include <nanogui/screen.h>

struct Renderer : public nanogui::Screen
{
	static void init();
	Renderer();

	void renderLoop();
};
