/**
 *  API shared between engine and RISC-V binaries
 *
 **/
#pragma once
#include "api_structs.h"
#include <dyncall_api.h>
#include <engine.hpp>
#include <optional>
#include <strf.hpp>

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
		static uint32_t current_machine();

		static std::optional<intptr_t> setting(std::string_view);
	};

	/** GUI **/

	using guidx_t = unsigned int;

	struct GUI
	{
		struct Widget
		{
			guidx_t idx;
			void set_position(int x, int y);
			void set_callback(Function<void()> callback);
		};

		struct Window : public Widget
		{
		};

		struct Button : public Widget
		{
		};

		struct Label : public Widget
		{
		};

		// Find a window based on title
		static guidx_t find(const std::string& title);

		// Create a new window
		static Window window(const std::string& title);
		// Create a new widget
		static Widget widget(Widget& parent);
		// Create a new button
		static Button button(Widget& parent, const std::string& caption);
		// Create a new label
		static Label label(Widget& parent, const std::string& text);
	};

	/** Timers **/

	struct Timer
	{
		using Callback = Function<void(Timer)>;
		static Timer oneshot(float time, Callback);
		static Timer periodic(float period, Callback);
		static Timer periodic(float time, float period, Callback);
		void stop() const;

		static long sleep(float seconds);

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

#define ENABLE_EXPERIMENTAL_FAST_RETURNS 1

#if ENABLE_EXPERIMENTAL_FAST_RETURNS > 0
#define FPUB extern "C" __attribute__((noreturn, used, retain))

inline __attribute__((noreturn)) void return_fast()
{
	asm volatile(".insn i SYSTEM, 0, x0, x0, 0x7ff");
	__builtin_unreachable();
}
template <typename T>
inline __attribute__((noreturn)) void return_fast(T t)
{
	register T a0 asm("a0") = t;
	asm volatile(".insn i SYSTEM, 0, x0, x0, 0x7ff" :: "r"(a0));
	__builtin_unreachable();
}
#else
#define FPUB extern "C" __attribute__((used, retain))
#define return_fast() return
#define return_fast(x) return x
#endif
