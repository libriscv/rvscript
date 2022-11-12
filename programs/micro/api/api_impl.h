#pragma once

struct GameplayException : public std::exception
{
	explicit GameplayException(const std::string& message, std::source_location sl = std::source_location::current())
		: m_msg(message), m_location(sl) {}
	virtual ~GameplayException() throw() {}

	const auto& location() const throw() {
        return m_location;
    }
    const char* what() const throw() override {
        return m_msg.c_str();
    }
private:
	const std::string m_msg;
	const std::source_location m_location;
};

template <typename Expr>
inline void expect_check(Expr expr, const char* strexpr,
	const char* file, int line, const char* func)
{
	if (UNLIKELY(!expr())) {
		asm ("" ::: "memory"); // prevent dead-store optimization
		syscall(ECALL_ASSERT_FAIL, (long) strexpr, (long) file, (long) line, (long) func);
		__builtin_unreachable();
	}
}
#define EXPECT(expr) \
	api::expect_check([&] { return (expr); }, #expr, __FILE__, __LINE__, __FUNCTION__)

template <typename... Args>
inline void print(Args&&... args)
{
	char buffer[2048];
	auto res = strf::to(buffer) (std::forward<Args> (args)...);
	const size_t size = res.ptr - buffer;

	register const char* a0 asm("a0") = buffer;
	register size_t      a1 asm("a1") = size;
	register long syscall_id asm("a7") = ECALL_WRITE;
	register long        a0_out asm("a0");

	asm volatile ("ecall" : "=r"(a0_out)
		: "r"(a0), "m"(*(const char(*)[size]) a0), "r"(a1), "r"(syscall_id));
}

template <typename T>
inline long measure(const char* testname, T testfunc)
{
	return syscall(ECALL_MEASURE, (long) testname, (long) static_cast<void(*)()>(testfunc));
}

template <typename Func>
struct FarCall {
	const uint32_t mhash;
	const uint32_t fhash;

	constexpr FarCall(const char* m, const char* f)
		: mhash(crc32(m)), fhash(crc32(f)) {}

	template <typename... Args>
	auto operator() (Args&&... args) const {
		static_assert( std::is_invocable_v<Func, Args...> );
		using Ret = typename std::invoke_result<Func, Args...>::type;
		using FCH = Ret(*)(uint32_t, uint32_t, Args... args);

		auto fch = reinterpret_cast<FCH> (&farcall_helper);
		return fch(mhash, fhash, args...);
	}
};

template <typename Func>
struct ExecuteRemotely {
	const uint32_t mhash;
	const Func func;

	constexpr ExecuteRemotely(const char* m, Func f) : mhash(crc32(m)), func(f) {}
	constexpr ExecuteRemotely(uint32_t m, Func f) : mhash(m), func(f) {}

	template <typename... Args>
	auto operator() (Args&&... args) const {
		static_assert( std::is_invocable_v<Func, Args...> );
		using Ret = typename std::invoke_result<Func, Args...>::type;
		using FCH = Ret(uint32_t, Func, Args... args);

		auto* fch = reinterpret_cast<FCH*> (&direct_farcall_helper);
		return fch(mhash, func, args...);
	}
};

inline auto interrupt(uint32_t mhash, uint32_t fhash, const void* data, size_t size)
{
	sys_interrupt(mhash, fhash, data, size);
}
inline long interrupt(uint32_t mhash, uint32_t fhash)
{
	return syscall(ECALL_INTERRUPT, mhash, fhash, 0, 0);
}
#define INTERRUPT(mach, function, ...) \
		api::interrupt(crc32(mach), crc32(function), ## __VA_ARGS__)

inline uint32_t current_machine()
{
	return syscall(ECALL_MACHINE_HASH);
}
#define RUNNING_ON(mach) (api::current_machine() == crc32(mach))

extern "C" long sys_multiprocess(unsigned, void*, size_t, multiprocess_func_t, void*);
inline unsigned multiprocess(unsigned cpus, multiprocess_func_t func, void* data)
{
	constexpr size_t STACK_SIZE = 512 * 1024u;
	static uint64_t* mp_stack = nullptr;
	if (mp_stack  == nullptr) {
		mp_stack = new uint64_t[STACK_SIZE / sizeof(uint64_t)];
	}

	return sys_multiprocess(cpus, mp_stack, STACK_SIZE, func, data);
}
__attribute__((always_inline))
inline unsigned multiprocess(unsigned cpus)
{
	register unsigned a0 asm("a0") = cpus;
	register int     sid asm("a7") = ECALL_MULTIPROCESS_FORK;

	asm volatile ("ecall" : "+r"(a0) : "r"(sid));
	return a0;
}
inline uint32_t multiprocess_wait()
{
	register unsigned a0 asm("a0");
	register int     sid asm("a7") = ECALL_MULTIPROCESS_WAIT;

	asm volatile ("ecall" : "=r"(a0) : "r"(sid) : "memory");
	return a0;
}

inline void each_frame_helper(int count, int reason)
{
	for (int i = 0; i < count; i++)
		microthread::wakeup_one_blocked(reason);
}
inline void wait_next_tick()
{
	microthread::block(REASON_FRAME);
}
template <typename T, typename... Args>
inline void each_tick(const T& func, Args&&... args)
{
	static bool init = false;
	if (!init) {
		init = true;
		(void) syscall(ECALL_EACH_FRAME, (long) each_frame_helper, REASON_FRAME);
	}
	microthread::oneshot(func, std::forward<Args> (args)...);
}

inline void Game::exit()
{
	(void) syscall(ECALL_GAME_EXIT);
}
inline void Game::breakpoint(std::source_location sl)
{
	char buffer[512];
	strf::to(buffer)(sl.file_name(), ", ", sl.function_name(), ", line ", sl.line());
	sys_breakpoint(0, buffer);
}
inline bool Game::is_debugging()
{
	return sys_is_debug();
}

using timer_callback = void (*) (int, void*);
inline Timer timer_periodic(float time, float period, timer_callback callback, void* data, size_t size)
{
	return {sys_timer_periodic(time, period, callback, data, size)};
}
inline Timer timer_periodic(float period, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(period, period, callback, data, size);
}
inline Timer Timer::periodic(float time, float period, Function<void(Timer)> callback)
{
	return timer_periodic(time, period,
		[] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}
inline Timer Timer::periodic(float period, Function<void(Timer)> callback)
{
	return Timer::periodic(period, period, std::move(callback));
}
inline Timer timer_oneshot(float time, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(time, 0.0f, callback, data, size);
}
inline Timer Timer::oneshot(float time, Function<void(Timer)> callback)
{
	return timer_oneshot(time,
		[] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}

inline void Timer::stop() const {
	sys_timer_stop(this->id);
}

inline long sleep(float seconds) {
	const int tid = microthread::gettid();
	Timer::oneshot(seconds, [tid] (auto) {
		microthread::unblock(tid);
	});
	return microthread::block();
}

/** Maffs **/

inline float sin(float x) {
	return fsyscallf(ECALL_SINF, x);
}
inline float cos(float x) {
	return fsyscallf(ECALL_SINF, x + PI/2);
}
inline float rand(float a, float b) {
	return fsyscallf(ECALL_RANDF, a, b);
}
inline float smoothstep(float a, float b, float x) {
	return fsyscallf(ECALL_SMOOTHSTEP, a, b, x);
}
inline float length(float dx, float dy) {
	return fsyscallf(ECALL_VEC_LENGTH, dx, dy);
}
inline vec2 rotate_around(float dx, float dy, float angle) {
	const auto [x, y] = fsyscallff(ECALL_VEC_LENGTH, dx, dy, angle);
	return {x, y};
}
inline vec2 normalize(float dx, float dy) {
	const auto [x, y] = fsyscallff(ECALL_VEC_NORMALIZE, dx, dy);
	return {x, y};
}

inline float vec2::length() const
{
	return api::length(this->x, this->y);
}
inline vec2 vec2::rotate(float angle) const
{
	return rotate_around(this->x, this->y, angle);
}
inline vec2 vec2::normalized() const
{
	return api::normalize(this->x, this->y);
}
inline void vec2::normalize()
{
	*this = api::normalize(this->x, this->y);
}
