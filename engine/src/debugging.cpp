#include <script/script.hpp>
#include <libriscv/rsp_server.hpp>
#include <unistd.h>
static const uint16_t RSP_PORT = 2159;

static const char* getenv_with_default(const char* str, const char* defval)
{
	const char* value = getenv(str);
	if (value) return value;
	return defval;
}
static auto replaceAll(std::string str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
	return str;
}


static void gdb_remote_finish(Script& script)
{
	auto& machine = script.machine();
	if (!machine.stopped()) {
		// resume until stopped
		machine.simulate(machine.max_instructions());
	}
}
void Script::gdb_remote_debugging(std::string message, bool one_up, uint16_t port)
{
	port = (port != 0) ? port : RSP_PORT;

	if (0 == fork()) {
		char scrname[64];
		strncpy(scrname, "/tmp/dbgscript-XXXXXX", sizeof(scrname));
		const int fd = mkstemp(scrname);
		if (fd < 0) {
			throw std::runtime_error("Unable to create script for debugging");
		}

		const std::string debugscript =
			// Delete the script file (after GDB closes it)
			"shell unlink " + std::string(scrname) + "\n"
			// Load the original file used by the script
			"file " + this->filename() + "\n"
			// Connect remotely to the given port @port
			"target remote localhost:" + std::to_string(port) + "\n"
			// Enable the fancy TUI
			"layout next\nlayout next\n"
			// Disable pagination for the message
			"set pagination off\n"
			// Print the message given by the caller
			"echo " + replaceAll(message, "\t", "\\n") + "\n"
			// Go up one step from the syscall wrapper (which can fail)
			 + std::string(one_up ? "up\n" : "");

		ssize_t len = write(fd, debugscript.c_str(), debugscript.size());
		if (len < (ssize_t) debugscript.size()) {
			throw std::runtime_error("Unable to write script file for debugging");
		}
		close(fd);

		const char* argv[] = {
			getenv_with_default("GDB", "/usr/bin/gdb-multiarch"), "-x", scrname, nullptr
		};
		// XXX: This is not kosher, but GDB is open-source, safe and let's not
		// pretend that anyone downloads gdb-multiarch from a website anyway.
		// There is a finite list of things we should pass to GDB to make it
		// behave well, but I haven't been able to find the right combination.
		extern char** environ;
		if (-1 == execve(argv[0], (char *const *)argv , environ)) {
			throw std::runtime_error("Unable to start gdb-multiarch for debugging");
		}
    }

	riscv::RSP<Script::MARCH> server { this->machine(), port };
	auto client = server.accept();
	if (client != nullptr) {
		printf("GDB connected\n");
		//client->set_verbose(true);
		while (client->process_one());
	}
	gdb_remote_finish(*this);
}

void setup_debugging_system()
{
	Script::set_dynamic_call(
		"Debug::breakpoint", [] (Script& script) {
			auto& machine = script.machine();
			if (script.is_debug()) {
				// We have to pre-emptively skip over the breakpoint instruction
				machine.cpu.jump(machine.cpu.pc() + 4);
				// Wait for someone to connect:
				auto [port, info] = machine.sysargs<uint16_t, std::string> ();
				auto message = fmt::format("Breakpoint in {}:0x{:X}.\t{}\t\t",
					script.name(), machine.cpu.pc(), info);
				script.gdb_remote_debugging(message, true, port);
				// XXX: Return back(??)
				if (!machine.stopped())
					machine.cpu.jump(machine.cpu.pc() - 4);
			} else {
				fmt::print("Skipped over breakpoint in {}:0x{:X}. Break here with DEBUG=1.\n",
					script.name(), machine.cpu.pc());
			}
		});
}
