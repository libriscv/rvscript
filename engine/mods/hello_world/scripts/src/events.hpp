#include <api.h>
#include <include/event_loop.hpp>

PUBLIC(bool add_work(const Events::Work*));

inline void execute_remotely(uint32_t mhash, Function<void()> func)
{
	SharedMemoryArea shm;
	// copy whole function to shared memory area
	auto* work = shm.push(std::move(func));
	// send work to another machine
	api::interrupt(mhash, crc32("add_work"), work);
}
#define REMOTE_EXEC(mach, ...) execute_remotely(crc32(mach), __VA_ARGS__)
