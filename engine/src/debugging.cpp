#include <script/script.hpp>

void setup_debugging_system(Script& script)
{
	script.set_dynamic_call(
	"remote_gdb", [] (Script& script) {
		auto& machine = script.machine();
		const auto [port] = machine.sysargs<uint16_t> ();
		// We have to pre-emptively return to escape
		// looping back into the dynamic call handler.
		machine.cpu.jump(machine.cpu.reg(riscv::RISCV::REG_RA));
		// Wait for someone to connect:
		script.gdb_listen(port);
	});
}
