#pragma once
#include "script.hpp"
using machine_t = riscv::Machine<4>;

extern Script* get_script(uint32_t machine_hash);
