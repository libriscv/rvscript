#pragma once

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
	char buffer[500];
	auto res = strf::to(buffer) (std::forward<Args> (args)...);
	psyscall(ECALL_WRITE, buffer, res.ptr - buffer);
}

template <typename T>
inline long measure(const char* testname, T testfunc)
{
	return syscall(ECALL_MEASURE, (long) testname, (long) static_cast<void(*)()>(testfunc));
}

extern "C" void (*dyncall_helper) ();
extern "C" void (*farcall_helper) ();
extern "C" void (*direct_farcall_helper) ();
extern "C" void (*interrupt_helper) ();

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
		using FCH = Ret(uint32_t, uint32_t, Args... args);

		auto* fch = reinterpret_cast<FCH*> (&farcall_helper);
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

template <typename Func>
struct Call {
	const uint32_t hash;

	constexpr Call(const char* f) : hash(crc32(f)) {}
	constexpr Call(uint32_t h) : hash(h) {}

	template <typename... Args>
	auto operator() (Args... args) const {
		static_assert( std::is_invocable_v<Func, Args...> );
		//using Ret = decltype(((Func*) 0) (args...));
		using Ret = typename std::invoke_result<Func, Args...>::type;
		using FCH = Ret(*)(uint32_t, Args...);

		auto fch = reinterpret_cast<FCH> (&dyncall_helper);
		return fch(hash, args...);
	}
};
#define DYNCALL(name, type, ...) Call<type> (crc32(name)) (__VA_ARGS__)

template <typename Func, typename... Args>
inline long interrupt(uint32_t mhash, uint32_t fhash, Args... args)
{
	static_assert( std::is_invocable_v<Func, Args...> );
	using Ret = typename std::invoke_result<Func, Args...>::type;
	using FP = Ret(*)(uint32_t, uint32_t, Args...);

	auto fptr = reinterpret_cast<FP> (&interrupt_helper);
	return fptr(mhash, fhash, args...);
}
inline long interrupt(uint32_t mhash, uint32_t fhash)
{
	return syscall(ECALL_INTERRUPT, mhash, fhash);
}
#define INTERRUPT(mach, function, ...) \
		api::interrupt(crc32(mach), crc32(function), ## __VA_ARGS__)

inline uint32_t current_machine()
{
	return syscall(ECALL_MACHINE_HASH);
}
#define RUNNING_ON(mach) (api::current_machine() == crc32(mach))

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

using timer_callback = void (*) (int, void*);
inline Timer timer_periodic(float time, float period, timer_callback callback, void* data, size_t size)
{
	constexpr Call<int(float, float, timer_callback, void*, size_t)> dyncall {"timer_periodic"};

	return {dyncall(time, period, callback, data, size)};
}
inline Timer timer_periodic(float period, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(period, period, callback, data, size);
}
inline Timer Timer::periodic(float time, float period, Function<void(Timer)> callback)
{
	return timer_periodic(time, period,
		(timer_callback) [] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}
inline Timer Timer::periodic(float period, Function<void(Timer)> callback)
{
	return Timer::periodic(period, period, std::move(callback));
}
inline Timer timer_oneshot(float time, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(time, 0.0f, std::move(callback), data, size);
}
inline Timer Timer::oneshot(float time, Function<void(Timer)> callback)
{
	return timer_oneshot(time,
		(timer_callback) [] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}

inline void Timer::stop() const {
	constexpr Call<void(int)> stop_timer {"timer_stop"};
	stop_timer(this->id);
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
