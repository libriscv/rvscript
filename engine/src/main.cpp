#include "script.hpp"
#include "timers.hpp"
Timers timers;
static std::unique_ptr<Script> gameplay1 = nullptr;
static std::unique_ptr<Script> gameplay2 = nullptr;

Script* get_script(uint32_t machine_hash)
{
	if (machine_hash == gameplay1->hash())
		return gameplay1.get();
	if (machine_hash == gameplay2->hash())
		return gameplay2.get();
	return nullptr;
}

#include <unistd.h> /* sleep */
int main()
{
	Script::init();
	gameplay1.reset(new Script("../mods/hello_world/scripts/gameplay.elf", "gameplay1"));
	gameplay1->hash_public_api_symbols("../mods/hello_world/scripts/src/gameplay.symbols");

	gameplay2.reset(new Script("../mods/hello_world/scripts/gameplay.elf", "gameplay2"));
	gameplay2->hash_public_api_symbols("../mods/hello_world/scripts/src/gameplay.symbols");

	gameplay1->call("start");

	printf("...\n");
	while (true) {
		timers.timers_handler();
		if (timers.active() == 0) break;
		sleep(1);
	}

	return 0;
}
