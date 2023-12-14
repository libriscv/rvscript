#include "script_syscalls.hpp"

#include <cmath>
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
	const uint32_t len_g = std::min(1024u, (uint32_t)len);

	auto& scr = script(machine);
	if (scr.machine().is_multiprocessing())
	{
		machine.set_result(-1);
		return;
	}
	if (scr.stdout_enabled() == false)
	{
		machine.set_result(len_g);
		return;
	}

	machine.memory.foreach (
		address, len_g,
		[&scr](auto&, auto, const uint8_t* data, size_t len)
		{
			if (data == nullptr)
			{
				strf::to(stderr)("[", scr.name(), "] had an illegal write\n");
				return;
			}
			scr.print(std::string_view((const char*)data, len));
		});
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

template <bool Preempt = false>
inline void do_farcall(machine_t& machine, Script& dest, gaddr_t addr)
{
	// copy argument registers (1 less integer register)
	const auto& current = machine.cpu.registers();
	auto& regs			= dest.machine().cpu.registers();
	for (int i = 0; i < 6; i++)
	{
		regs.get(10 + i) = current.get(12 + i);
	}
	for (int i = 0; i < 8; i++)
	{
		regs.getfl(10 + i) = current.getfl(10 + i);
	}

	// vmcall with no arguments to avoid clobbering registers
	if constexpr (!Preempt)
	{
		machine.set_result(dest.call(addr));
	}
	else
	{
		machine.set_result(dest.preempt(addr));
	}
	// we short-circuit the ret pseudo-instruction:
	machine.cpu.jump(machine.cpu.reg(riscv::REG_RA) - 4);
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

APICALL(api_farcall)
{
	machine.set_result(-1);
}

APICALL(api_farcall_direct)
{
	machine.set_result(-1);
}

APICALL(api_interrupt)
{
	machine.set_result(-1);
}

APICALL(api_machine_hash)
{
	machine.set_result(script(machine).hash());
}

APICALL(api_each_frame)
{
	auto [addr, reason] = machine.sysargs<gaddr_t, int>();
	script(machine).set_tick_event(addr, reason);
	machine.set_result(0);
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
		{ECALL_FARCALL, api_farcall},
		{ECALL_FARCALL_DIRECT, api_farcall_direct},
		{ECALL_INTERRUPT, api_interrupt},
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
	using namespace riscv;
	static const Instruction<MARCH> dynargs_instruction_handler {
		[](CPU<MARCH>& cpu, rv32i_instruction instr)
		{
			auto& scr = script(cpu.machine());
			// Select type and retrieve value from argument registers
			switch (instr.Itype.funct3)
			{
			case 0b000: // 32-bit signed integer immediate
				scr.dynargs().emplace_back((int64_t)instr.Itype.signed_imm());
				break;
			case 0b001: // 64-bit signed integer
				scr.dynargs().emplace_back((int64_t)cpu.reg(riscv::REG_ARG0));
				break;
			case 0b010: // 32-bit floating point
				scr.dynargs().emplace_back(
					cpu.registers().getfl(riscv::REG_FA0).f32[0]);
				break;
			case 0b111: // std::string
				scr.dynargs().emplace_back(
					cpu.machine().memory.memstring(cpu.reg(riscv::REG_ARG0)));
				break;
			default: throw "Implement me";
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
