#include <script/script.hpp>
static const uint16_t RSP_PORT = 2159;

void setup_debugging_system(Script& script)
{
	script.set_dynamic_call(
	"remote_gdb", [] (Script& script) {
		auto& machine = script.machine();
		if (getenv("DEBUG")) {
			// We have to pre-emptively skip over the breakpoint instruction
			machine.cpu.jump(machine.cpu.pc() + 4);
			// Wait for someone to connect:
			auto [port] = machine.sysargs<uint16_t> ();
			port = (port != 0) ? port : RSP_PORT;
			fmt::print("Breakpoint in {}:0x{:X}. Listening for RSP, port={}\n",
				script.name(), machine.cpu.pc(), port);
			script.gdb_listen(port);
		} else {
			fmt::print("Skipped over breakpoint in {}:0x{:X}. Enable with DEBUG=1.\n",
				script.name(), machine.cpu.pc());
		}
	});
}
