#include "interface.hpp"
#include <api.h>
using namespace api;

PUBLIC(void start())
{
	print("Hello from Level 2!\n");
	gameplay_state.print_string(123);
}

int main() {}
