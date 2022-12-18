#pragma once

struct GameplayException : public std::exception
{
	explicit GameplayException(
		const std::string& message,
		std::source_location sl = std::source_location::current())
	  : m_msg(message), m_location(sl)
	{
	}

	virtual ~GameplayException() throw() {}

	const auto& location() const throw()
	{
		return m_location;
	}

	const char* what() const throw() override
	{
		return m_msg.c_str();
	}

  private:
	const std::string m_msg;
	const std::source_location m_location;
};

template <typename Expr>
inline void expect_check(
	Expr expr, const char* strexpr, const char* file, int line,
	const char* func)
{
	if (UNLIKELY(!expr()))
	{
		asm("" ::: "memory"); // prevent dead-store optimization
		syscall(
			ECALL_ASSERT_FAIL, (long)strexpr, (long)file, (long)line,
			(long)func);
		__builtin_unreachable();
	}
}

#define EXPECT(expr)                                                          \
	api::expect_check(                                                        \
		[&]                                                                   \
		{                                                                     \
			return (expr);                                                    \
		},                                                                    \
		#expr, __FILE__, __LINE__, __FUNCTION__)

template <typename... Args> inline void print(Args&&... args)
{
	char buffer[2048];
	auto res		  = strf::to(buffer)(std::forward<Args>(args)...);
	const size_t size = res.ptr - buffer;

	register const char* a0 asm("a0")  = buffer;
	register size_t a1 asm("a1")	   = size;
	register long syscall_id asm("a7") = ECALL_WRITE;
	register long a0_out asm("a0");

	asm volatile("ecall"
				 : "=r"(a0_out)
				 : "r"(a0), "m"(*(const char(*)[size])a0), "r"(a1),
				   "r"(syscall_id));
}

template <typename T> inline long measure(const char* testname, T testfunc)
{
	return syscall(
		ECALL_MEASURE, (long)testname,
		(long)static_cast<void (*)()>(testfunc));
}

inline uint32_t Game::current_machine()
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
	if (!init)
	{
		init = true;
		(void)syscall(ECALL_EACH_FRAME, (long)each_frame_helper, REASON_FRAME);
	}
	microthread::oneshot(func, std::forward<Args>(args)...);
}

/** Game **/

inline void Game::exit()
{
	(void)syscall(ECALL_GAME_EXIT);
}

inline void Game::breakpoint(std::source_location sl)
{
	char buffer[512];
	strf::to(buffer)(
		sl.file_name(), ", ", sl.function_name(), ", line ", sl.line());
	sys_breakpoint(0, buffer);
}

inline bool Game::is_debugging()
{
	return sys_is_debug();
}

/** Timers **/

using timer_callback = void (*)(int, void*);

inline Timer timer_periodic(
	float time, float period, timer_callback callback, void* data, size_t size)
{
	return {sys_timer_periodic(time, period, callback, data, size)};
}

inline Timer
timer_periodic(float period, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(period, period, callback, data, size);
}

inline Timer
Timer::periodic(float time, float period, Function<void(Timer)> callback)
{
	return timer_periodic(
		time, period,
		[](int id, void* data)
		{
			(*(decltype(&callback))data)({id});
		},
		&callback, sizeof(callback));
}

inline Timer Timer::periodic(float period, Function<void(Timer)> callback)
{
	return Timer::periodic(period, period, std::move(callback));
}

inline Timer
timer_oneshot(float time, timer_callback callback, void* data, size_t size)
{
	return timer_periodic(time, 0.0f, callback, data, size);
}

inline Timer Timer::oneshot(float time, Function<void(Timer)> callback)
{
	return timer_oneshot(
		time,
		[](int id, void* data)
		{
			(*(decltype(&callback))data)({id});
		},
		&callback, sizeof(callback));
}

inline void Timer::stop() const
{
	sys_timer_stop(this->id);
}

inline long sleep(float seconds)
{
	const int tid = microthread::gettid();
	Timer::oneshot(
		seconds,
		[tid](auto)
		{
			microthread::unblock(tid);
		});
	return microthread::block();
}

/** Maffs **/

inline float sin(float x)
{
	return fsyscallf(ECALL_SINF, x);
}

inline float cos(float x)
{
	return fsyscallf(ECALL_SINF, x + PI / 2);
}

inline float rand(float a, float b)
{
	return fsyscallf(ECALL_RANDF, a, b);
}

inline float smoothstep(float a, float b, float x)
{
	return fsyscallf(ECALL_SMOOTHSTEP, a, b, x);
}

inline float length(float dx, float dy)
{
	return fsyscallf(ECALL_VEC_LENGTH, dx, dy);
}

inline vec2 rotate_around(float dx, float dy, float angle)
{
	const auto [x, y] = fsyscallff(ECALL_VEC_LENGTH, dx, dy, angle);
	return {x, y};
}

inline vec2 normalize(float dx, float dy)
{
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

template <class T>
struct is_stdstring : public std::is_same<T, std::basic_string<char>>
{
};

template <typename T>
struct is_string : public std::disjunction<
					   std::is_same<char*, typename std::decay<T>::type>,
					   std::is_same<const char*, typename std::decay<T>::type>>
{
};

template <typename... Args>
inline constexpr void
dynamic_call(const uint32_t hash, const char* name, Args&&... args)
{
	[[maybe_unused]] unsigned argc = 0;
	std::array<uint8_t, 8> type {};
	std::array<uintptr_t, 8> gpr {};
	std::array<float, 8> fpr {};

	(
		[&]
		{
			if constexpr (std::is_integral_v<std::remove_reference_t<Args>>)
			{
				gpr[argc]  = args;
				type[argc] = 0b001;
				argc++;
			}
			else if constexpr (std::is_floating_point_v<
								   std::remove_reference_t<Args>>)
			{
				fpr[argc]  = args;
				type[argc] = 0b010;
				argc++;
			}
			else if constexpr (is_stdstring<std::remove_cvref_t<Args>>::value)
			{
				gpr[argc]  = (uintptr_t)args.data();
				type[argc] = 0b111;
				argc++;
			}
			else if constexpr (is_string<Args>::value)
			{
				gpr[argc]  = (uintptr_t) const_cast<const char*>(args);
				type[argc] = 0b111;
				argc++;
			}
		}(),
		...);

	register long a0 asm("a0");
	register float fa0 asm("fa0");
	register long syscall_id asm("a7") = ECALL_DYNCALL2;

	for (unsigned i = 0; i < argc; i++)
	{
		if (type[i] == 0b001)
		{
			if ((int64_t)gpr[i] >= -4096 && (int64_t)gpr[i] < 4096)
			{
				asm(".insn i 0b0001011, 0, x0, x0, %0" ::"I"(gpr[i]));
			}
			else
			{
				a0 = gpr[i];
				asm(".word 0b001000000001011" : : "r"(a0));
			}
		}
		else if (type[i] == 0b010)
		{
			fa0 = fpr[i];
			asm(".word 0b010000000001011" : : "f"(fa0));
		}
		else if (type[i] == 0b111)
		{
			a0 = gpr[i];
			asm(".word 0b111000000001011" : : "r"(a0));
		}
	}

	register const size_t name_hash asm("a0") = hash;
	register const char* name_ptr asm("a1")	  = name;
	asm("ecall"
		:
		: "r"(name_hash), "m"(*name_ptr), "r"(name_ptr), "r"(syscall_id));
}

#define DYNCALL(name, ...) dynamic_call(crc32(name), name, ##__VA_ARGS__)
