/**
 *  API shared between engine and RISC-V binaries
 *
**/
#pragma once
#ifdef __riscv
#include <engine.hpp>
#endif
#include "api_structs.h"

namespace api {

template <typename... Args> void print(Args&&... args);

/** Game **/

struct Game {
	static void exit();
};

/** Timers **/

struct Timer {
	using Callback = Function<void(Timer)>;
	static Timer oneshot(double time, Callback);
	static Timer periodic(double period, Callback);
	static Timer periodic(double time, double period, Callback);
	void stop() const;
	const int id;
};

long  sleep(double seconds);

/** Math **/

float sin(float);
float rand(float, float);
float smoothstep(float, float, float);

// only see the implementation on RISC-V
#ifdef __riscv
#include "api_impl.h"
#endif
}

/** Startup function and arguments **/
struct MapFile {
#ifdef __riscv
	const char* path;
	const char* file;
	const char* event;
#else
	uint32_t path;
	uint32_t file;
	uint32_t event;
#endif
	int  width;
	int  height;
};

#ifdef __riscv
extern void map_main(MapFile*);

#define PUBLIC_API extern "C" __attribute__((used))

#endif
