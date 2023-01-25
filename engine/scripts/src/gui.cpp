#include <api.h>
using namespace api;

static std::tuple<uint64_t, uint64_t>
measure_exception_throw()
{
	const auto time0  = rdtime();
	const auto cycle0 = rdcycle();
	try
	{
		throw std::runtime_error("Hello Exceptions!");
	}
	catch (const std::exception& e)
	{
		print("Caught exception: ", e.what(), "\n");
	}
	const auto cycle1 = rdcycle();
	const auto time1  = rdtime();
	return {cycle1-cycle0, time1-time0};
}

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

#if 1
	print("Testing C++ exception handling\n");
	auto [cycles, time] = measure_exception_throw();
	print("It took ", cycles, " instructions to throw, catch and print the exception\n");
	print("It took ", time, " milliseconds\n");
	auto [cycles2, time2] = measure_exception_throw();
	print("It took ", cycles2, " instructions the second time\n");
	print("It took ", time2, " milliseconds the second time\n");
#endif

	return 0;
}
