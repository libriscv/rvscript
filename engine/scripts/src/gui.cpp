#include <api.h>
using namespace api;

/* This gets called before anything else, on each machine */
int main()
{
	auto window = GUI::window("My New Window");
	print("Window: ", window.idx, "\n");

	for (size_t i = 0; i < 10; i++)
	{
		auto button = GUI::button(
			window, "Hello this is button " + std::to_string(i) + "!");
		button.set_callback(
			[i] {
				print("Click! i = ", i, "\n");
			});
	}

	return 0;
}
