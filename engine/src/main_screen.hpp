#include <map>
#include <nanogui/screen.h>

struct MainScreen : public nanogui::Screen
{
	static void init();
	MainScreen();

	void buildInterface();
	void mainLoop();

	virtual void draw(NVGcontext* ctx)
	{
		// mProgress->setValue(std::fmod((float)glfwGetTime() / 10, 1.0f));

		Screen::draw(ctx);
	}

	// Get a managed widget based on index
	auto* getw(uint32_t idx) const;
	uint32_t getw(nanogui::Widget* w);
	// Remove a widget
	uint32_t remw(nanogui::Widget* w);
	// Get the index for a widget
	uint32_t managew(nanogui::Widget* w);

  private:
	std::map<uint32_t, nanogui::Widget*> idx_to_widget;
	std::map<nanogui::Widget*, uint32_t> widget_to_idx;
	uint32_t counter = 0;
};

inline auto* MainScreen::getw(uint32_t idx) const
{
	return idx_to_widget.at(idx);
}

inline uint32_t MainScreen::getw(nanogui::Widget* w)
{
	return widget_to_idx.at(w);
}

inline uint32_t MainScreen::managew(nanogui::Widget* w)
{
	idx_to_widget[counter] = w;
	widget_to_idx[w]	   = counter;
	return counter++;
}
