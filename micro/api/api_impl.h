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
	return apicall<long>(ECALL_MEASURE, testname, static_cast<void(*)()>(testfunc));
}

template <typename Ret = long, typename... Args>
inline Ret farcall(uint32_t mhash, uint32_t fhash, Args&&... args)
{
	return apicall<Ret>(ECALL_FARCALL, mhash, fhash, std::forward<Args>(args)...);
}
#define FARCALL(mach, function, ...) \
		api::farcall(crc32(mach), crc32(function), ## __VA_ARGS__)

template <typename T>
inline long interrupt(uint32_t mhash, uint32_t fhash, T argument)
{
	asm ("" ::: "memory"); // avoid dead-store optimzation
	return syscall(ECALL_INTERRUPT, mhash, fhash, (long) argument);
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
inline Timer timer_oneshot(double time, timer_callback callback, void* data, size_t size)
{
	return {apicall(ECALL_TIMER_ONESHOT, time, callback, data, size)};
}
inline Timer Timer::oneshot(double time, Function<void(Timer)> callback)
{
	return timer_oneshot(time,
		(timer_callback) [] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}
inline Timer timer_periodic(double time, double period, timer_callback callback, void* data, size_t size)
{
	return {apicall(ECALL_TIMER_PERIODIC, time, period, callback, data, size)};
}
inline Timer timer_periodic(double period, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(period, period, callback, data, size);
}
inline Timer Timer::periodic(double time, double period, Function<void(Timer)> callback)
{
	return timer_periodic(time, period,
		(timer_callback) [] (int id, void* data) {
			(*(decltype(&callback)) data) ({id});
		}, &callback, sizeof(callback));
}
inline Timer Timer::periodic(double period, Function<void(Timer)> callback)
{
	return Timer::periodic(period, period, std::move(callback));
}

inline void Timer::stop() const {
	(void) syscall(ECALL_TIMER_STOP, this->id);
}

inline long sleep(double seconds) {
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
