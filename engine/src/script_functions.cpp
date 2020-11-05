#include "script_functions.hpp"
#include <script/machine/include_api.hpp>
#include <fmt/core.h>
#include "timers.hpp"
static_assert(ECALL_LAST - GAME_API_BASE <= 100, "Room for system calls");

#define APICALL(func) static void func(machine_t& machine [[maybe_unused]])

inline Script& script(machine_t& m) { return *m.get_userdata<Script> (); }

APICALL(api_self_test)
{
	auto [i32, i64_1, i64_2, str] =
		machine.template sysargs <int, uint64_t, uint64_t, std::string> ();
	assert(i32 == 44 && "Self-test 32-bit integer");
	assert(i64_1 == 0x5678000012340000 && "Self-test 64-bit integer (1)");
	assert(i64_2 == 0x8800440022001100 && "Self-test 64-bit integer (2)");
	assert(str == "This is a test" && "Self-test string");
	machine.set_result(0);
}

/** Game Engine **/

APICALL(assert_fail)
{
	auto [expr, file, line, func] =
		machine.sysargs<std::string, std::string, int, std::string> ();
	fmt::print(stderr,
		">>> [{}] assertion failed: {} in {}:{}, function {}\n",
		script(machine).name(), expr, file, line, func);
	machine.stop();
}

APICALL(api_write)
{
	auto [address, len] = machine.sysargs<gaddr_t, uint32_t> ();
	const uint32_t len_g = std::min(1024u, (uint32_t) len);
	machine.memory.memview(address, len_g,
		[&machine] (const uint8_t* data, size_t len) {
			if (data == nullptr) {
				fmt::print(stderr,
					">>> [{}] had an illegal write\n",
					script(machine).name());
				return;
			}
			fmt::print(">>> [{}] says: {}",
				script(machine).name(),
				std::string_view((const char*) data, len));
		});
	machine.set_result(len_g);
}

APICALL(api_measure)
{
	const auto [test, address] =
		machine.template sysargs <std::string, gaddr_t> ();
	auto time_ns = script(machine).measure(address);
	fmt::print(">>> Measurement \"{}\" median: {} nanos\n\n",
		test, time_ns);
	machine.set_result(time_ns);
}

APICALL(api_dyncall)
{
	auto [hash] = machine.template sysargs <uint32_t> ();
	auto& regs = machine.cpu.registers();
	// move down 6 integer registers
	for (int i = 0; i < 6; i++) {
		regs.get(10 + i) = regs.get(11 + i);
	}
	// call the handler
	script(machine).dynamic_call(hash);
	// we short-circuit the ret pseudo-instruction:
	machine.cpu.jump(regs.get(riscv::RISCV::REG_RA) - 4);
}

template <bool Preempt = false>
inline void do_farcall(machine_t& machine, Script& dest, gaddr_t addr)
{
	// copy argument registers (1 less integer register)
	const auto& current = machine.cpu.registers();
	auto& regs = dest.machine().cpu.registers();
	for (int i = 0; i < 6; i++) {
		regs.get(10 + i) = current.get(12 + i);
	}
	for (int i = 0; i < 8; i++) {
		regs.getfl(10 + i) = current.getfl(10 + i);
	}

	// scan registers to determine if we need to mirror the stack
	bool mirror_stack = false;
	const gaddr_t STACK_BEG = machine.memory.stack_initial();
	const gaddr_t STACK_TOP = current.get(2) & ~(gaddr_t) 0xFFF;

	if (&machine != &dest.machine())
	{
		for (int i = 10; i < 16; i++) {
			if (regs.get(i) >= STACK_TOP && regs.get(i) < STACK_BEG) {
				mirror_stack = true;
				break;
			}
		}
	}
	if (mirror_stack)
	{
		// mount the stack of the calling machine as shared memory
		for (gaddr_t src = STACK_TOP; src < STACK_BEG; src += 0x1000)
		{
			const auto& page = machine.memory.get_page(src);
			dest.machine().memory.install_shared_page(src >> 12, page);
		}
	}
	// vmcall with no arguments to avoid clobbering registers
	if constexpr (!Preempt) {
		machine.set_result(dest.call(addr));
	} else {
		machine.set_result(dest.preempt(addr));
	}
	// we short-circuit the ret pseudo-instruction:
	machine.cpu.jump(machine.cpu.reg(riscv::RISCV::REG_RA) - 4);
}

APICALL(api_farcall)
{
	const auto [mhash, fhash] =
		machine.template sysargs <uint32_t, uint32_t> ();
	auto* script = get_script(mhash);
	if (UNLIKELY(script == nullptr)) {
		machine.set_result(-1);
		return;
	}
	// first check if the function exists
	const auto addr = script->api_function_from_hash(fhash);
	if (LIKELY(addr != 0))
	{
		do_farcall<false>(machine, *script, addr);
		return;
	}
	fmt::print(stderr,
		"Unable to find public API function from hash: {:#08x}\n",
		fhash); /** NOTE: we can turn this back into a string using reverse dictionary **/
	machine.set_result(-1);
}

APICALL(api_farcall_direct)
{
	const auto [mhash, faddr] =
		machine.template sysargs <uint32_t, gaddr_t> ();
	auto* script = get_script(mhash);
	if (UNLIKELY(script == nullptr)) {
		machine.set_result(-1);
		return;
	}
	do_farcall<false>(machine, *script, faddr);
}

APICALL(api_interrupt)
{
	const auto [mhash, fhash] =
		machine.template sysargs <uint32_t, uint32_t> ();
	auto* script = get_script(mhash);
	if (script == nullptr) {
		machine.set_result(-1);
		return;
	}
	// vmcall with no arguments to avoid clobbering registers
	const auto faddr = script->api_function_from_hash(fhash);
	if (LIKELY(faddr != 0)) {
		do_farcall<true> (machine, *script, faddr);
		return;
	}
	fmt::print(stderr,
		"Unable to find public API function from hash: {:#08x}\n",
		fhash); /** NOTE: we can turn this back into a string using reverse dictionary **/
	machine.set_result(-1);
}

APICALL(api_machine_hash)
{
	machine.set_result(script(machine).hash());
}

APICALL(api_each_frame)
{
	auto [addr, reason] = machine.template sysargs <gaddr_t, int> ();
	script(machine).set_tick_event((gaddr_t) addr, (int) reason);
	machine.set_result(0);
}

APICALL(api_game_exit)
{
	fmt::print("Game::exit() called from script!\n");
	exit(0);
}

/** Math **/

APICALL(api_math_sinf)
{
	auto [x] = machine.sysargs <float> ();
	machine.set_result(std::sin(x));
}
APICALL(api_math_smoothstep)
{
	auto [edge0, edge1, x] = machine.sysargs <float, float, float> ();
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	machine.set_result(x * x * (3 - 2 * x));
}
APICALL(api_math_randf)
{
	auto [edge0, edge1] = machine.sysargs <float, float> ();
    float random = static_cast<float> (rand()) / static_cast<float> (RAND_MAX);
    float r = random * (edge1 - edge0);
	machine.set_result(edge0 + r);
}
APICALL(api_vector_length)
{
	auto [dx, dy] = machine.sysargs <float, float> ();
	const float length = std::sqrt(dx * dx + dy * dy);
	machine.set_result(length);
}
APICALL(api_vector_rotate_around)
{
	auto [dx, dy, angle] = machine.sysargs <float, float, float> ();
	const float x = std::cos(angle) * dx - std::sin(angle) * dy;
	const float y = std::sin(angle) * dx + std::cos(angle) * dy;
	machine.set_result(x, y);
}
APICALL(api_vector_normalize)
{
	auto [dx, dy] = machine.sysargs <float, float> ();
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length > 0.0001f) {
		dx /= length;
		dy /= length;
	}
	machine.set_result(dx, dy);
}

void Script::setup_syscall_interface(machine_t& machine)
{
	// Implement the most basic functionality here,
	// common to all scripts. The syscall numbers
	// are stored in syscalls.h
	machine.install_syscall_handlers({
		{ECALL_SELF_TEST,   api_self_test},
		{ECALL_ASSERT_FAIL, assert_fail},
		{ECALL_WRITE,       api_write},
		{ECALL_MEASURE,     api_measure},
		{ECALL_DYNCALL,     api_dyncall},
		{ECALL_FARCALL,     api_farcall},
		{ECALL_FARCALL_DIRECT, api_farcall_direct},
		{ECALL_INTERRUPT,   api_interrupt},
		{ECALL_MACHINE_HASH, api_machine_hash},
		{ECALL_EACH_FRAME,  api_each_frame},

		{ECALL_GAME_EXIT,   api_game_exit},

		{ECALL_SINF,        api_math_sinf},
		{ECALL_RANDF,       api_math_randf},
		{ECALL_SMOOTHSTEP,  api_math_smoothstep},
		{ECALL_VEC_LENGTH,  api_vector_length},
		{ECALL_VEC_ROTATE,  api_vector_rotate_around},
		{ECALL_VEC_NORMALIZE, api_vector_normalize},
	});
}
