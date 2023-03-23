#include "interface.hpp"
#include <api.h>
using namespace api;

PUBLIC(void start())
{
	print("Hello from Level 2!\n");
	int value = gameplay_allowed_function(123);
	print("Back in Level2! Got result value = ", value, "\n");
}

int main() {}
