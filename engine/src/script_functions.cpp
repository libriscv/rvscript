#include "script_functions.hpp"
#include "machine/include_api.hpp"
#include "timers.hpp"
extern Timers timers;
static_assert(ECALL_LAST - GAME_API_BASE <= 100, "Room for system calls");

#define APICALL(func) static long func(machine_t& machine [[maybe_unused]])

static Script& gscript() { return Script::current_script(); }

APICALL(api_self_test)
{
	auto [i32, i64_1, i64_2, str] = 
		machine.template sysargs <int, uint64_t, uint64_t, std::string> ();
	assert(i32 == 44 && "Self-test 32-bit integer");
	assert(i64_1 == 0x5678000012340000 && "Self-test 64-bit integer (1)");
	assert(i64_2 == 0x8800440022001100 && "Self-test 64-bit integer (2)");
	assert(std::string(str) == "This is a test" && "Self-test string");
	return 0;
}

/** Game Engine **/

APICALL(assert_fail)
{
	auto [expr, file, line, func] =
		machine.sysargs<std::string, std::string, int, std::string> ();
	printf(">>> [%s] assertion failed: %s in %s:%d, function %s\n",
		gscript().name().c_str(), expr.c_str(), file.c_str(), line, func.c_str());
	machine.stop();
	return -1;
}

APICALL(api_write)
{
	auto [address, len] = machine.sysargs<uint32_t, uint32_t> ();
	const uint32_t len_g = std::min(1024u, len);
	machine.memory.memview(address, len_g,
		[] (const uint8_t* data, size_t len) {
			printf(">>> [%s] says: %.*s",
				gscript().name().c_str(), (int) len, data);
		});
	return len_g;
}

APICALL(api_measure)
{
	auto [test, address] = machine.template sysargs <std::string, uint32_t> ();
	auto time_ns = gscript().measure(address);
	printf(">>> Measurement \"%s\" average: %ld nanos\n",
			test.c_str(), time_ns);
	return time_ns;
}

APICALL(api_farcall)
{
	const uint32_t mhash = machine.template sysarg <uint32_t> (0);
	const uint32_t fhash = machine.template sysarg <uint32_t> (1);
	auto* script = get_script(mhash);
	if (script == nullptr) return -1;
	// copy argument registers (1 less integer register)
	const auto& current = gscript().machine().cpu.registers();
	auto& regs = script->machine().cpu.registers();
	for (int i = 0; i < 6; i++) {
		regs.get(10 + i) = current.get(12 + i);
	}
	for (int i = 0; i < 8; i++) {
		regs.getfl(10 + i) = current.getfl(10 + i);
	}

	// vmcall with no arguments to avoid clobbering registers
	const auto addr = script->api_function_from_hash(fhash);
	if (LIKELY(addr != 0)) {
		// normal machine call
		return script->call(addr);
	}
	fprintf(stderr, "Unable to find public API function from hash: %#x\n",
		fhash); /** NOTE: we can turn this back into a string using reverse dictionary **/
	return -1;
}

APICALL(api_interrupt)
{
	const uint32_t mhash = machine.template sysarg <uint32_t> (0);
	const uint32_t fhash = machine.template sysarg <uint32_t> (1);
	const uint32_t data = machine.template sysarg <uint32_t> (2);
	auto* script = get_script(mhash);
	if (script == nullptr) return -1;
	// vmcall with no arguments to avoid clobbering registers
	const auto addr = script->api_function_from_hash(fhash);
	if (LIKELY(addr != 0)) {
		// interrupt the machine
		return script->preempt(addr, (uint32_t) data);
	}
	fprintf(stderr, "Unable to find public API function from hash: %#x\n",
		fhash); /** NOTE: we can turn this back into a string using reverse dictionary **/
	return -1;
}

APICALL(api_machine_hash)
{
	return gscript().hash();
}

APICALL(api_each_frame)
{
	auto [addr, reason] = machine.template sysargs <uint32_t, int> ();
	gscript().set_tick_event((uint32_t) addr, (int) reason);
	return 0;
}

APICALL(api_game_exit)
{
	printf("Game::exit() called from script!\n");
	exit(0);
}

/** Timers **/

APICALL(api_timer_oneshot)
{
	auto time = machine.sysarg<double> (0);
	auto addr = machine.sysarg<uint32_t> (0);
	auto data = machine.sysarg<uint32_t> (1);
	auto size = machine.sysarg<uint32_t> (2);
	std::array<uint8_t, 16> capture;
	assert(size <= sizeof(capture) && "Must fit inside temp buffer");
	machine.memory.memcpy_out(capture.data(), data, size);

	return timers.oneshot(time,
		[addr = (uint32_t) addr, capture, &script = gscript()] (int id) {
			std::copy(capture.begin(), capture.end(), Script::hidden_area().data());
			script.call(addr, (int) id, (uint32_t) Script::HIDDEN_AREA);
        });
}
APICALL(api_timer_periodic)
{
	auto time = machine.sysarg<double> (0);
	auto peri = machine.sysarg<double> (1);
	auto addr = machine.sysarg<uint32_t> (0);
	auto data = machine.sysarg<uint32_t> (1);
	auto size = machine.sysarg<uint32_t> (2);
	std::array<uint8_t, 16> capture;
	assert(size <= sizeof(capture) && "Must fit inside temp buffer");
	machine.memory.memcpy_out(capture.data(), data, size);

	return timers.periodic(time, peri,
		[addr = (uint32_t) addr, capture, &script = gscript()] (int id) {
			std::copy(capture.begin(), capture.end(), Script::hidden_area().data());
			script.call(addr, (int) id, (uint32_t) Script::HIDDEN_AREA);
        });
}
APICALL(api_timer_stop)
{
	const auto [timer_id] = machine.sysargs<int> ();
	timers.stop(timer_id);
	return 0;
}

/** Math **/

APICALL(api_math_sinf)
{
	auto [x] = machine.sysargs <float> ();
	machine.cpu.registers().getfl(10).set_float(std::sin(x));
	return 0;
}
APICALL(api_math_smoothstep)
{
	auto [edge0, edge1, x] = machine.sysargs <float, float, float> ();
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	machine.cpu.registers().getfl(10).set_float(x * x * (3 - 2 * x));
    return 0;
}
APICALL(api_math_randf)
{
	auto [edge0, edge1] = machine.sysargs <float, float> ();
    float random = static_cast<float> (rand()) / static_cast<float> (RAND_MAX);
    float r = random * (edge1 - edge0);
	machine.cpu.registers().getfl(10).set_float(edge0 + r);
    return 0;
}
APICALL(api_vector_length)
{
	auto [dx, dy] = machine.sysargs <float, float> ();
	const float length = std::sqrt(dx * dx + dy * dy);
	machine.cpu.registers().getfl(10).set_float(length);
	return 0;
}
APICALL(api_vector_rotate_around)
{
	auto [dx, dy, angle] = machine.sysargs <float, float, float> ();
	const float x = std::cos(angle) * dx - std::sin(angle) * dy;
	const float y = std::sin(angle) * dx + std::cos(angle) * dy;
	machine.cpu.registers().getfl(10).set_float(x);
	machine.cpu.registers().getfl(11).set_float(y);
    return 0;
}
APICALL(api_vector_normalize)
{
	auto [dx, dy] = machine.sysargs <float, float> ();
	const float length = std::sqrt(dx * dx + dy * dy);
	if (length > 0.0001f) {
		dx /= length;
		dy /= length;
	}
	machine.cpu.registers().getfl(10).set_float(dx);
	machine.cpu.registers().getfl(11).set_float(dy);
    return 0;
}

void Script::setup_syscall_interface(machine_t& machine)
{
	machine.install_syscall_handlers({
		{ECALL_SELF_TEST,   api_self_test},
		{ECALL_ASSERT_FAIL, assert_fail},
		{ECALL_WRITE,       api_write},
		{ECALL_MEASURE,     api_measure},
		{ECALL_FARCALL,     api_farcall},
		{ECALL_INTERRUPT,   api_interrupt},
		{ECALL_MACHINE_HASH, api_machine_hash},
		{ECALL_EACH_FRAME,  api_each_frame},

		{ECALL_GAME_EXIT,   api_game_exit},

		{ECALL_TIMER_ONESHOT, api_timer_oneshot},
		{ECALL_TIMER_PERIODIC, api_timer_periodic},
		{ECALL_TIMER_STOP,  api_timer_stop},

		{ECALL_SINF,        api_math_sinf},
		{ECALL_RANDF,       api_math_randf},
		{ECALL_SMOOTHSTEP,  api_math_smoothstep},
		{ECALL_VEC_LENGTH,  api_vector_length},
		{ECALL_VEC_ROTATE,  api_vector_rotate_around},
		{ECALL_VEC_NORMALIZE, api_vector_normalize},
	});
}
