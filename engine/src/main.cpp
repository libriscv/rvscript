#include "script.hpp"
static std::shared_ptr<std::vector<uint8_t>> load_file(const std::string& filename);
static std::unique_ptr<Script> gameplay1 = nullptr;
static std::unique_ptr<Script> gameplay2 = nullptr;
static std::unique_ptr<Script> events = nullptr;

#include "timers.hpp"
Timers timers; // put this in a level structure

Script* get_script(uint32_t machine_hash)
{
	if (machine_hash == gameplay1->hash())
		return gameplay1.get();
	if (machine_hash == gameplay2->hash())
		return gameplay2.get();
	if (machine_hash == events->hash())
		return events.get();
	return nullptr;
}

#include <unistd.h> /* sleep */
int main()
{
	auto binary = load_file("mods/hello_world/scripts/gameplay.elf");
	Script::init();
	gameplay1.reset(new Script(binary, "gameplay1"));
	gameplay1->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	gameplay2.reset(new Script(binary, "gameplay2"));
	gameplay2->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	events.reset(new Script(binary, "events"));
	events->hash_public_api_symbols("mods/hello_world/scripts/src/gameplay.symbols");

	events->call("event_loop");

	gameplay1->call("start");

	printf("...\n");
	while (timers.active() > 0) {
		timers.handle_events();
		usleep(timers.next() * 1e6);
		// this guy should probably run each engine tick
		events->resume(5'000);
	}

	return 0;
}


#include <unistd.h>
std::shared_ptr<std::vector<uint8_t>> load_file(const std::string& filename)
{
    size_t size = 0;
    FILE* f = fopen(filename.c_str(), "rb");
    if (f == NULL) throw std::runtime_error("Could not open file: " + filename);

    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);

    auto result = std::make_shared<std::vector<uint8_t>> (size);
    if (size != fread(result->data(), 1, size, f))
    {
        fclose(f);
        throw std::runtime_error("Error when reading from file: " + filename);
    }
    fclose(f);
    return result;
}
