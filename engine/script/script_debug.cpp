#include "script.hpp"
#include <libriscv/rsp_server.hpp>

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
}
