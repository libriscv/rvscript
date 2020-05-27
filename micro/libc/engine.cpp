#include "engine.hpp"

__attribute__((noinline)) void halt()
{
	asm("ebreak" ::: "memory");
}
