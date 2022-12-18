#include <api.h>
using namespace api;

/* This gets called before anything else, on each machine */
int main()
{
	auto window = GUI::window("My New Window");
	window.set_position(0, 100);

	for (size_t i = 0; i < 10; i++)
	{
		GUI::label(window, "Button:");
		auto button = GUI::button(
			window, "Button " + std::to_string(i) + "!");
		button.set_callback(
			[i] {
				print("Hello, this is button ", i, "!!\n");
			});
	}

	auto w2 = GUI::window("Another window");
	w2.set_position(140, 100);

	return 0;
}
