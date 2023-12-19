#include "timers.hpp"
#include <script/helpers.hpp>
#include <script/script.hpp>
#include <unistd.h>	  /* usleep */
static Timers timers; // put this in a level structure

/** Timers **/
void setup_timer_system()
{
	using gaddr_t = Script::gaddr_t;

	// We can extend the functionality available in a script
	// using named functions. They abstract away system calls
	// and other low level things, and gives us a nice API
	// with a std::function to work with.
	Script::set_dynamic_calls({
		{"Timer::stop", "void sys_timer_stop (int)",
		 [](Script& script)
		 {
			 // Stop timer
			 const auto [timer_id] = script.machine().sysargs<int>();
			 timers.stop(timer_id);
		 }},
		{"Timer::periodic", "int sys_timer_periodic (float, float, timer_callback, void*, size_t)",
		 [](Script& script)
		 {
			 // Periodic timer
			 auto& machine = script.machine();
			 const auto [time, peri, addr, data, size]
				 = machine.sysargs<float, float, gaddr_t, gaddr_t, gaddr_t>();

			 auto capture = CaptureStorage::get(machine, data, size);

			 int id = timers.periodic(
				 time, peri,
				 [addr = (gaddr_t)addr, capture, &script](int id)
				 {
					 gaddr_t dst = script.guest_alloc(capture.size());
					 script.machine().copy_to_guest(
						 dst, capture.data(), capture.size());
					 script.call(addr, id, dst);
					 script.guest_free(dst);
				 });
			 machine.set_result(id);
		 }},
	});
}

void timers_loop(std::function<void()> callback)
{
	while (timers.active() > 0)
	{
		timers.handle_events();
		usleep(timers.next() * 1e6);
		// users handler
		callback();
	}
}
