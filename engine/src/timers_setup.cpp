#include <script/script.hpp>
#include <unistd.h> /* usleep */
#include "timers.hpp"
static Timers timers; // put this in a level structure

/** Timers **/
void setup_timer_system(Script& script)
{
	using gaddr_t = Script::gaddr_t;

	// We can extend the functionality available in a script
	// using named functions. They abstract away system calls
	// and other low level things, and gives us a nice API
	// with a std::function to work with.
	script.set_dynamic_functions({
		{"timer_stop", [] (Script& script) {
			// Stop timer
			const auto [timer_id] = script.machine().sysargs<int> ();
			timers.stop(timer_id);
		}},
		{"timer_periodic", [] (Script& script) {
			// Periodic timer
			auto& machine = script.machine();
			const auto [time, peri, addr, data, size] =
				machine.sysargs<float, float, gaddr_t, gaddr_t, size_t> ();
			std::array<uint8_t, 32> capture;
			if (UNLIKELY(size > sizeof(capture))) {
				throw std::runtime_error("Timer data must be 32-bytes or less");
			}
			machine.memory.memcpy_out(capture.data(), data, size);

			int id = timers.periodic(time, peri,
				[addr = (gaddr_t) addr, capture, &script] (int id) {
					std::copy(capture.begin(), capture.end(), Script::hidden_area().data());
					script.call(addr, (int) id, (gaddr_t) Script::HIDDEN_AREA);
		        });
			machine.set_result(id);
		}},
	});
}

void timers_loop(std::function<void()> callback)
{
	while (timers.active() > 0) {
		timers.handle_events();
		usleep(timers.next() * 1e6);
		// users handler
		callback();
	}
}
