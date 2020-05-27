#include "script.hpp"
#include "timers.hpp"
Timers timers;
static std::unique_ptr<Script> gameplay = nullptr;

Script* get_script(uint32_t machine_hash)
{
	if (machine_hash == gameplay->hash())
		return gameplay.get();
	return nullptr;
}

int main()
{
	gameplay.reset(new Script("../mods/hello_world/scripts/gameplay.elf", "gameplay"));
	gameplay->hash_public_api_symbols("../mods/hello_world/scripts/src/gameplay.symbols");

	gameplay->call("start");
	return 0;
}
