#include "script_syscalls.hpp"

#include <cmath>
#include <future>
#include <libriscv/rv32i_instr.hpp>
#include <libriscv/threads.hpp>
#include <strf/to_cfile.hpp>
static_assert(ECALL_LAST - GAME_API_BASE <= 100, "Room for system calls");

#define APICALL(func) static void func(machine_t& machine [[maybe_unused]])

inline Script& script(machine_t& m)
{
	return *m.get_userdata<Script>();
}

APICALL(api_self_test)
{
	machine.set_result(0);
}

/** Game Engine **/

APICALL(assert_fail)
{
	auto [expr, file, line, func]
		= machine.sysargs<std::string, std::string, int, std::string>();
	strf::to(stderr)(
		"[", script(machine).name(), "] assertion failed: ", expr, " in ",
		file, ":", line, ", function: ", func, "\n");
	machine.stop();
}

APICALL(api_write)
{
	auto [address, len]	 = machine.sysargs<gaddr_t, uint32_t>();
	const uint32_t len_g = std::min(4096u, (uint32_t)len);

	auto& scr = script(machine);
	if (scr.machine().is_multiprocessing())
	{
		machine.set_result(-1);
		return;
	}

	if (scr.stdout_enabled())
	{
		std::array<riscv::vBuffer, 16> buffers;
		const size_t cnt =
			machine.memory.gather_buffers_from_range(buffers.size(), buffers.data(), address, len_g);
		for (size_t i = 0; i < cnt; i++)
			scr.print(std::string_view{(const char *)buffers[i].ptr, buffers[i].len});
	}

	machine.set_result(len_g);
}

APICALL(api_measure)
{
	const auto [test, address]
		= machine.template sysargs<std::string, gaddr_t>();
	auto& scr	 = script(machine);
	auto time_ns = scr.vmbench(address);
	strf::to(stdout)(
		"[", scr.name(), "] Measurement \"", test, "\" median: ", time_ns,
		"\n\n");
	machine.set_result(time_ns);
}

APICALL(api_dyncall)
{
	auto& regs = machine.cpu.registers();
	// call the handler with register t0 as hash
	script(machine).dynamic_call_hash(
		regs.get(riscv::REG_T0), regs.get(riscv::REG_T1));
}

APICALL(api_dyncall_args)
{
	const auto [hash, g_name] = machine.sysargs<uint32_t, gaddr_t>();

	auto& scr = script(machine);
	// Perform a dynamic call, which takes no arguments
	// Instead, the caller must check the dynargs() vector.
	scr.dynamic_call_hash(hash, g_name);
	// After the call we can clear dynargs.
	scr.dynargs().clear();
}

APICALL(api_machine_hash)
{
	machine.set_result(script(machine).hash());
}

APICALL(api_each_frame)
{
	machine.set_result(-1);
}

APICALL(api_game_setting)
{
	auto [setting] = machine.sysargs<std::string_view>();

	auto value = Script::get_global_setting(setting);
	if (!value.has_value()) {
		strf::to(stdout)("[", script(machine).name(), "] Warning: Could not find", setting, "\n");
	}
	machine.set_result(value.has_value(), value.value_or(0x0));
}

APICALL(api_game_exit)
{
	strf::to(stdout)("[", script(machine).name(), "] Exit called\n");
	script(machine).exit();
}

/** Math **/

APICALL(api_math_sinf)
{
	auto [x] = machine.sysargs<float>();
	machine.set_result(std::sin(x));
}

APICALL(api_math_smoothstep)
{
	auto [edge0, edge1, x] = machine.sysargs<float, float, float>();
	x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	machine.set_result(x * x * (3 - 2 * x));
}

APICALL(api_math_randf)
{
	auto [edge0, edge1] = machine.sysargs<float, float>();
	float random = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	float r		 = random * (edge1 - edge0);
	machine.set_result(edge0 + r);
}

APICALL(api_vector_length)
{
	auto [dx, dy]	   = machine.sysargs<float, float>();
	const float length = std::sqrt(dx * dx + dy * dy);
	machine.set_result(length);
}

APICALL(api_vector_rotate_around)
{
	auto [dx, dy, angle] = machine.sysargs<float, float, float>();
	const float x		 = std::cos(angle) * dx - std::sin(angle) * dy;
	const float y		 = std::sin(angle) * dx + std::cos(angle) * dy;
	machine.set_result(x, y);
}

APICALL(api_vector_normalize)
{
	auto [dx, dy]	   = machine.sysargs<float, float>();
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length > 0.0001f)
	{
		dx /= length;
		dy /= length;
	}
	machine.set_result(dx, dy);
}

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <unistd.h>
#include <mutex>
static constexpr bool DIRECT_RPC = false;
static bool ready = false;
std::mutex m;
std::condition_variable cv;

APICALL(api_uds_call)
{
	auto [func, view] = machine.sysargs<gaddr_t, std::string_view> ();

	auto& scr = script(machine);
	ssize_t len = write(scr.m_fds[0], view.begin(), view.size());
	if (len < 0)
		throw std::runtime_error("RPC write failed");

	if constexpr (DIRECT_RPC) {
		auto* remote = scr.remote_script();
		remote->m_recvfd = scr.m_fds[1];
		remote->call(func, len);
	} else {
		ready = false;
		std::unique_lock lk(m);
		cv.wait(lk, []{ return ready; });
	}
}
APICALL(api_uds_recv)
{
	auto [buf, len] = machine.sysargs<gaddr_t, gaddr_t> ();

	auto& scr = script(machine);

	std::array<riscv::vBuffer, 128> buffers;
	const size_t cnt =
		scr.machine().memory.gather_writable_buffers_from_range(
			buffers.size(), buffers.data(), buf, len);
	ssize_t res = readv(scr.m_recvfd, (const iovec *)&buffers[0], cnt);
	if (res < 0)
		throw std::runtime_error("RPC readv failed");

	ready = true;
	cv.notify_one();

	scr.machine().set_result(res);
}

APICALL(api_uds_write)
{
	auto& scr = script(machine);

	scr.machine().set_result(-1);
}

void Script::init_uds(Script& other)
{
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, this->m_fds) < 0)
		throw std::runtime_error("Unable to create socket pair for RPC");

	if constexpr (DIRECT_RPC)
		return;

	other.m_recvfd = this->m_fds[1];
	static std::thread thread;
	new (&thread) std::thread([remote = &other] {
		remote->call("rpc_waitloop");
	});
}

void Script::setup_syscall_interface()
{
	// Implement the most basic functionality here,
	// common to all scripts. The syscall numbers
	//  are stored in syscalls.h
	machine_t::install_syscall_handlers({
		{ECALL_SELF_TEST, api_self_test},
		{ECALL_ASSERT_FAIL, assert_fail},
		{ECALL_WRITE, api_write},
		{ECALL_MEASURE, api_measure},
		{ECALL_DYNCALL, api_dyncall},
		{ECALL_DYNARGS, api_dyncall_args},
		{ECALL_RPC_CALL,  api_uds_call},
		{ECALL_RPC_RECV,  api_uds_recv},
		{ECALL_RPC_WRITE, api_uds_write},
		{ECALL_MACHINE_HASH, api_machine_hash},
		{ECALL_EACH_FRAME, api_each_frame},

		{ECALL_GAME_SETTING, api_game_setting},
		{ECALL_GAME_EXIT, api_game_exit},

		{ECALL_SINF, api_math_sinf},
		{ECALL_RANDF, api_math_randf},
		{ECALL_SMOOTHSTEP, api_math_smoothstep},
		{ECALL_VEC_LENGTH, api_vector_length},
		{ECALL_VEC_ROTATE, api_vector_rotate_around},
		{ECALL_VEC_NORMALIZE, api_vector_normalize},
	});
	// Add a few Newlib system calls (just in case)
	machine_t::setup_newlib_syscalls();

	// A custom intruction used to handle indexed dynamic calls.
	using namespace riscv;
	static const Instruction<MARCH> dyncall_instruction_handler {
		[](CPU<MARCH>& cpu, rv32i_instruction instr)
		{
			auto& scr = script(cpu.machine());
			scr.dynamic_call_array(instr.Itype.imm);
		},
		[](char* buffer, size_t len, auto&, rv32i_instruction instr)
		{
			return snprintf(
				buffer, len, "DYNCALL: 4-byte 0x%X (0x%X)", instr.opcode(),
				instr.whole);
		}};
	// A custom intruction used to handle dynamic arguments
	// to the dynamic system call.
	static const Instruction<MARCH> dynargs_instruction_handler {
		[](CPU<MARCH>& cpu, rv32i_instruction instr)
		{
			auto& scr = script(cpu.machine());
			// Select type and retrieve value from argument registers
			switch (instr.Utype.rd)
			{
			case 0x0: // 64-bit signed integer immediate
				scr.dynargs().emplace_back((int64_t)instr.Itype.signed_imm());
				break;
			case 0x1: // 64-bit signed integer
				scr.dynargs().emplace_back((int64_t)cpu.reg(riscv::REG_ARG0));
				break;
			case 0x3: // 32-bit floating point
				scr.dynargs().emplace_back(
					cpu.registers().getfl(riscv::REG_FA0).f32[0]);
				break;
			case 0x7: // std::string
				scr.dynargs().emplace_back(
					cpu.machine().memory.memstring(cpu.reg(riscv::REG_ARG0)));
				break;
			case 0x1F: { // complete the dynamic argument call
				const auto [hash, g_name] = cpu.machine().sysargs<uint32_t, gaddr_t>();

				auto& scr = script(cpu.machine());
				// Perform a dynamic call, which takes no arguments
				// Instead, the caller must check the dynargs() vector.
				scr.dynamic_call_hash(hash, g_name);
				// After the call we can clear dynargs.
				scr.dynargs().clear();
				return;
			}
			default:
				throw std::runtime_error("Dynamic args: Implement me");
			}
		},
		[](char* buffer, size_t len, auto&, rv32i_instruction instr)
		{
			return snprintf(
				buffer, len, "CUSTOM: 4-byte 0x%X (0x%X)", instr.opcode(),
				instr.whole);
		}};
	// Override the machines unimplemented instruction handling,
	// in order to use the custom instruction instead.
	CPU<MARCH>::on_unimplemented_instruction
		= [](rv32i_instruction instr) -> const Instruction<MARCH>&
	{
		if (instr.opcode() == 0b1011011)
		{
			return dyncall_instruction_handler;
		}
		if (instr.opcode() == 0b0001011)
		{
			return dynargs_instruction_handler;
		}
		return CPU<MARCH>::get_unimplemented_instruction();
	};
}
