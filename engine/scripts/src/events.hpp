#include <api.h>
#include <include/event_loop.hpp>

PUBLIC(bool add_work(const Events<>::Work*));

inline void add_remote_work(Function<void()> func)
{
	// Send work to 'events' machine. The add_work function
	// expects a reference to struct Work as argument.
	// api::interrupt(crc32("events"), crc32("add_work"), &func, sizeof(func));
}
