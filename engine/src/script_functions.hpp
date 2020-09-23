#pragma once
#include "script.hpp"
using gaddr_t = Script::gaddr_t;
using machine_t = Script::machine_t;

extern Script* get_script(uint32_t machine_hash);
