#include <api.h>
#include <include/event_loop.hpp>

PUBLIC(bool add_work(const Events::Work*));

inline void add_remote_work(Function<void()> func)
{
	// send work to another machine
	using AddWorkFunc = bool(const Events::Work*);
	api::interrupt<AddWorkFunc>(crc32("events"), crc32("add_work"), &func);
}
