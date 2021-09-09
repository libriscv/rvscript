#include <script/script.hpp>
#include <libriscv/rsp_server.hpp>
static const uint16_t RSP_PORT = 2159;

static void gdb_remote_finish(Script& script)
{
	auto& machine = script.machine();
	// resume until stopped
	const uint64_t max = Script::MAX_INSTRUCTIONS;
	machine.cpu.simulate(max);
}
static void gdb_listen(Script& script, uint16_t port)
{
	riscv::RSP<Script::MARCH> server { script.machine(), port };
	auto client = server.accept();
	if (client != nullptr) {
		printf("GDB connected\n");
		//client->set_verbose(true);
		while (client->process_one());
	}
	gdb_remote_finish(script);
}

void setup_debugging_system(Script& script)
{
	script.set_dynamic_call(
	"Debug::breakpoint", [] (Script& script) {
		auto& machine = script.machine();
		if (script.is_debug()) {
			// We have to pre-emptively skip over the breakpoint instruction
			machine.cpu.jump(machine.cpu.pc() + 4);
			// Wait for someone to connect:
			auto [port] = machine.sysargs<uint16_t> ();
			port = (port != 0) ? port : RSP_PORT;
			fmt::print("Breakpoint in {}:0x{:X}. Listening for RSP, port={}\n",
				script.name(), machine.cpu.pc(), port);
			gdb_listen(script, port);
		} else {
			fmt::print("Skipped over breakpoint in {}:0x{:X}. Break here with DEBUG=1.\n",
				script.name(), machine.cpu.pc());
		}
	});
}
