#include "script.hpp"
#include <libriscv/rsp_server.hpp>

void Script::gdb_remote_finish()
{
	// resume until stopped
	const uint64_t max = MAX_INSTRUCTIONS;
	while (!machine().stopped() && machine().instruction_counter() < max) {
		machine().cpu.simulate();
		machine().increment_counter(1);
	}
}

void Script::gdb_remote_begin(const std::string& entry, uint16_t port)
{
	auto addr = resolve_address(entry);
	if (addr != 0x0) {
		machine().setup_call(addr);
		machine().stop(false);
		gdb_listen(port);
		return;
	}
	throw std::runtime_error("Could not find entry function: " + entry);
}
void Script::gdb_listen(uint16_t port)
{
	using namespace riscv;
	RSP<Script::MARCH> server { machine(), port };
	RSPClient<Script::MARCH>* client = nullptr;
	try {
		client = server.accept();
		//client->set_verbose(true);
		while (client->process_one());
	} catch (...) {
		delete client;
		throw;
	}
	delete client;
	gdb_remote_finish();
}
