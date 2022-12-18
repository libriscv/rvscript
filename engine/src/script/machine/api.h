/**
 *  API shared between engine and RISC-V binaries
 *
 **/
#pragma once
#include "api_structs.h"
#include <dyncall_api.h>
#include <engine.hpp>

namespace api
{

	template <typename... Args> void print(Args&&... args);

	/** Game **/

	struct Game
	{
		static void exit();
		static void
		breakpoint(std::source_location sl = std::source_location::current());
		static bool is_debugging();
	};

	uint32_t current_machine();

	/** GUI **/

	using guidx_t = unsigned int;

	struct GUI
	{
		struct Button
		{
			guidx_t idx;
			void on_click(Function<void()> callback);
		};

		// Find a window based on title
		static guidx_t find(const std::string& title);

		// Create a new button
		static Button button(guidx_t parent, const std::string& caption);
	};

	/** Events **/
	template <typename T, typename... Args>
	void each_tick(const T& func, Args&&... args);
	void wait_next_tick();

	/** Timers **/

	struct Timer
	{
		using Callback = Function<void(Timer)>;
		static Timer oneshot(float time, Callback);
		static Timer periodic(float period, Callback);
		static Timer periodic(float time, float period, Callback);
		void stop() const;
		const int id;
	};

	long sleep(float seconds);

	/** Math **/

	float sin(float);
	float rand(float, float);
	float smoothstep(float, float, float);

// only see the implementation on RISC-V
#include "api_gui_impl.h"
#include "api_impl.h"
} // namespace api

#define PUBLIC(x) extern "C" __attribute__((used, retain)) x
#define KEEP() __attribute__((used, retain))
