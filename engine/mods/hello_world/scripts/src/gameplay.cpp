#include <api.h>
#include "events.hpp"
using namespace api;
struct SomeStruct {
	std::string string;
	int value;
};

int main()
{
	/* This gets called before anything else, on each machine */
	return 0;
}

/* These are used for benchmarking */
static void empty_function() {
}
static void full_thread_function() {
	microthread::create([] (int, int, int) { /* ... */ }, 1, 2, 3);
}
static void oneshot_thread_function() {
	microthread::oneshot([] (int, int, int) { /* ... */ }, 1, 2, 3);
}
static void direct_thread_function() {
	microthread::direct([] { /* ... */ });
}
static void dyncall_handler() {
	sys_empty();
}
PUBLIC(void public_donothing()) {
	/* nothing */
}
static void direct_farcall_testcall() {
	/* A direct farcall takes a function as argument, and can only
	   be used when calling into a remote machine that is running
	   the same executable as this machine. If it's using a different
	   executable you can use a regular FARCALL instead. */
	ExecuteRemotely fc("gameplay2", public_donothing);
	fc();
}
static void farcall_lookup_testcall() {
	/* A regular farcall will have to lookup the function in the remote
	   machine. We also need to specify the function type as a
	   template argument, to help with type safety. */
	FarCall<void()> fc("gameplay2", "public_donothing");
	fc();
}
template <size_t SIZE>
struct alignas(32) MultiprocessWork {
	std::array<float, SIZE> data_a;
	std::array<float, SIZE> data_b;
	float result[16] = {0};
	unsigned workers = 1;
	int counter = 0;

	inline size_t work_size() const noexcept {
		return SIZE / workers;
	}
	float final_sum() const noexcept {
		float sum = 0.0f;
		for (size_t i = 0; i < this->workers; i++) sum += this->result[i];
		return sum;
	}
};
static constexpr size_t WORK_SIZE = 8192;
static constexpr size_t MP_WORKERS = 4;
static bool work_output = false;
static MultiprocessWork<WORK_SIZE> mp_work;
template <size_t SIZE>
static void initialize_work(MultiprocessWork<SIZE>& work) {
	for (size_t i = 0; i < SIZE; i++) {
		work.data_a[i] = 1.0;
		work.data_b[i] = 1.0;
	}
}

static void multiprocessing_function(int cpu, void* vdata)
{
	auto& work = *(MultiprocessWork<WORK_SIZE> *)vdata;
	const size_t start = (cpu + 0) * work.work_size();
	const size_t end   = (cpu + 1) * work.work_size();

	float sum = 0.0f;
	for (size_t i = start; i < end; i++) {
		sum += work.data_a[i] * work.data_b[i];
	}

	work.result[cpu] = sum;
	__sync_fetch_and_add(&work.counter, 1);
}
static void vectorized_multiprocessing_function(int cpu, MultiprocessWork<WORK_SIZE>& work)
{
	union alignas(32) v256 {
		float  f[8];

		__attribute__((naked))
		static inline void zero_v1()
		{
			asm("vmv.v.i v1, 0");
			asm("ret");
		}
		__attribute__((naked))
		inline float sum_v1() {
			asm("vfredusum.vs v1, v0, v1");
			asm("vfmv.f.s     fa0, v1");
			asm("ret");
		}
	};

	const size_t start = (cpu + 0) * work.work_size();
	const size_t end   = (cpu + 1) * work.work_size();

	v256::zero_v1();
	// Vectorized dot product
	for (size_t i = start; i < end; i += 16) {
		v256 *a = (v256 *)&work.data_a[i];
		v256 *b = (v256 *)&work.data_b[i];
		v256 *c = (v256 *)&work.data_a[i + 8];
		v256 *d = (v256 *)&work.data_b[i + 8];

		asm("vle32.v v2, %1"
			:
			: "r"(a->f), "m"(a->f[0]));
		asm("vle32.v v3, %1"
			:
			: "r"(b->f), "m"(b->f[0]));

		asm("vfmadd.vv v1, v2, v3");

		asm("vle32.v v2, %1"
			:
			: "r"(c->f), "m"(c->f[0]));
		asm("vle32.v v3, %1"
			:
			: "r"(d->f), "m"(d->f[0]));

		asm("vfmadd.vv v1, v2, v3");
	}
	// Sum elements
	v256 vsum;
	const float sum = vsum.sum_v1();

	work.result[cpu] = sum;
	__sync_fetch_and_add(&work.counter, 1);
}

static void test_singleprocessing()
{
	mp_work.workers = 1;

	// Perform part of the work on main vCPU
	multiprocessing_function(0, &mp_work);

	const float sum = mp_work.final_sum();
	if (work_output)
		print("Single-process sum = ", sum, "\n");
}
static void test_multiprocessing()
{
	mp_work.workers = MP_WORKERS;
	mp_work.counter = 0;

	// -- Start N extra vCPUs and execute the function --
	// Fork this machine and wait until multiprocessing
	// ends, by calling multiprocess_wait() on all workers. Each
	// worker uses the current stack, copy-on-write. No need for
	// hand-written assembly to handle this variant.
	const unsigned cpu = multiprocess(mp_work.workers);
	if (cpu != 0) {
		multiprocessing_function(cpu-1, &mp_work);
	}

	// Wait and stop workers here
	const auto result = multiprocess_wait();

	// Sum the work together
	const float sum = mp_work.final_sum();
	if (work_output) {
		print("Multi-process sum = ", sum, " (",
			  (sum == WORK_SIZE) ? "good" : "bad", ")\n");
		print("Multi-process counter = ", mp_work.counter, " (",
			  (mp_work.counter == MP_WORKERS) ? "good" : "bad", ")\n");
		print("Multi-process result = ", strf::bin(result >> 1), " (",
			  (result == 0) ? "good" : "bad", ")\n");
	}
}
static void test_vectorized_singleprocessing()
{
	mp_work.workers = 1;

	vectorized_multiprocessing_function(0, mp_work);

	// Sum the work together
	const float sum = mp_work.final_sum();
	if (work_output) {
		print("Vectorized sum = ", sum, "\n");
	}
}
static void test_vectorized_multiprocessing()
{
	mp_work.workers = MP_WORKERS;
	mp_work.counter = 0;

	// Start N extra vCPUs and execute the function
	const unsigned cpu = multiprocess(mp_work.workers);
	if (cpu != 0) {
		vectorized_multiprocessing_function(cpu-1, mp_work);
	}
	// Wait and stop workers here
	const auto result = multiprocess_wait();

	// Sum the work together
	const float sum = mp_work.final_sum();
	if (work_output) {
		print("Vectorized Multi-process sum = ", sum, "\n");
		print("Vectorized Multi-process counter = ", mp_work.counter, "\n");
		print("Vectorized Multi-process result = ", strf::bin(result >> 1), " (",
			  (result == 0) ? "good" : "bad", ")\n");
	}
}
static void test_multiprocessing_forever()
{
	unsigned cpu = multiprocess(4);
	if (cpu != 0) while (true);
	auto res = multiprocess_wait();
	print("Forever multiprocessing result: ", strf::bin(res >> 1),
		" (", (res == 0) ? "good" : "bad", ")\n");
}
static void multiprocessing_overhead()
{
	mp_work.workers = MP_WORKERS;

	// Start the vCPUs
	unsigned cpu = multiprocess(MP_WORKERS);
	if (cpu != 0) {
		// Finish multiprocessing here
		multiprocess_wait();
	}
}

PUBLIC(void benchmarks())
{
	constexpr bool BENCHMARK_MULTIPROCESSING = false;

	initialize_work(mp_work);
	work_output = true;
	test_multiprocessing();
	test_vectorized_multiprocessing();
	test_multiprocessing_forever();
	test_vectorized_singleprocessing();
	test_vectorized_singleprocessing();
	test_singleprocessing();
	work_output = false;

	try {
		throw GameplayException("This is a test!");
	} catch (const GameplayException& ge) {
		print("Exception caught: ", ge.what(), "!\n");
		print("Exception thrown from: ", ge.location().function_name(), ", line ", ge.location().line(), "\n");
	}

	/* This function makes thousands of calls into this machine,
	   while preserving registers, and then prints some statistics. */
	measure("VM function call overhead", empty_function);
	measure("Full thread creation overhead", full_thread_function);
	measure("Oneshot thread creation overhead", oneshot_thread_function);
	measure("Direct thread creation overhead", direct_thread_function);
	measure("Dynamic call handler", dyncall_handler);
	measure("Farcall lookup", farcall_lookup_testcall);
	measure("Farcall direct", direct_farcall_testcall);

	if (BENCHMARK_MULTIPROCESSING && Game::is_debugging() == false)
	{
	measure("Multi-processing overhead", multiprocessing_overhead);
	measure("Multi-processing dotprod", test_multiprocessing);
	measure("RVV 4x-processing dotprod", test_vectorized_multiprocessing);
	measure("RVV 1x-processing dotprod", test_vectorized_singleprocessing);
	/* Takes a long time. Disabled (for now). */
	measure("Single-processing dotprod", test_singleprocessing);
	}
}

/* We can only call this function remotely if it's added to "gameplay.symbols",
   because the symbol files are used in the build system to preserve certain
   functions that would ordinarily get optimized out. It's name also has to
   unmangled, otherwise we can't find it in the ELF string tables. */
extern __attribute__((used, retain))
long gameplay_function(float value, SomeStruct& some)
{
	print("Hello Remote World! value = ", value, "!\n");
	print("Some struct string: ", some.string, "\n");
	print("Some struct value: ", some.value, "\n");
	some.string = "Hello 456! This string is very long!";
	return value * 2;
}

PUBLIC(void gameplay_empty())
{
	// Do nothing
}

struct C {
	std::string to_string() const {
		return std::string(&c, 1) + "++";
	}
private:
	char c;
};
PUBLIC(void cpp_function(const char* a, const C& c, const char* b))
{
	/* Hello C++ World */
	print(a, " ", c.to_string(), " ", b, "\n");
}

/* Example game object logic */
struct GameObject {
	bool alive;
	char name[30];
};

PUBLIC(void myobject_death(GameObject& object))
{
	EXPECT(object.alive);
	print("Object '", object.name, "' is dying!\n");
	/* SFX: Ugh... */
	object.alive = false;
}

PUBLIC(void test_dynamic_functions())
{
	// See: dyncalls.json
	sys_testing();
	sys_testing123(5, 6, 7);
}
