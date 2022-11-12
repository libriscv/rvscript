/**
 *  API shared between engine and RISC-V binaries
 *
**/
#pragma once
#include <engine.hpp>
#include <dyncall_api.h>
#include "api_structs.h"

namespace api {

template <typename... Args> void print(Args&&... args);

/** Game **/

struct Game {
	static void exit();
	static void breakpoint(std::source_location sl = std::source_location::current());
	static bool is_debugging();
};
uint32_t current_machine();

/** Events **/
template <typename T, typename... Args>
void each_tick(const T& func, Args&&... args);
void wait_next_tick();

/** Timers **/

struct Timer {
	using Callback = Function<void(Timer)>;
	static Timer oneshot(float time, Callback);
	static Timer periodic(float period, Callback);
	static Timer periodic(float time, float period, Callback);
	void stop() const;
	const int id;
};

long  sleep(float seconds);

/** Math **/

float sin(float);
float rand(float, float);
float smoothstep(float, float, float);

/** Multiprocessing **/

using multiprocess_func_t = void(*)(int, void*);
unsigned multiprocess(unsigned cpus);
unsigned multiprocess(unsigned cpus, multiprocess_func_t func, void* data = nullptr);
uint32_t multiprocess_wait();

// only see the implementation on RISC-V
#include "api_impl.h"
}

#define PUBLIC(x) extern "C" __attribute__((used, retain)) x
