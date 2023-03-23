#include "local.hpp"
Local local_struct;

void local_function()
{
}
void Local::local_cpp_function()
{
}
void local_function_ptr(void(*func)())
{
	func();
}
void local_std_function(std::function<void()> callback)
{
	callback();
}
