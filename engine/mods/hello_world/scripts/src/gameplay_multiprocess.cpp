#include <api.h>
using namespace api;

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

void benchmark_multiprocessing()
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
