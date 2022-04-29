#include <alloca.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <threads.h>
#include "random.h"
#include "utils.h"

static bool test_ilog2(uint64_t start, uint64_t end)
{
	for (uint64_t x = start; x <= end; x++) {
		double lg2 = log2(x != 0 ? (double)x : 1.0);
		if (ilog2((uint32_t)x) != (unsigned int)lg2 ||
		    ilog2((int32_t)x) != ilog2((uint64_t)x)) {
			return false;
		}
	}
	return true;
}

static bool test_ilog2_rand_64(uint64_t start, uint64_t end)
{
	struct random_state state;
	random_state_init(&state, start ^ end);
	for (uint64_t i = start; i <= end; i++) {
		uint64_t x = random_next_u64_in_range(&state, (uint64_t)UINT32_MAX + 1, UINT64_MAX);
		double lg2 = log2(x != 0 ? (double)x : 1.0);
		if (ilog2((int64_t)x) != (unsigned int)lg2) {
			return false;
		}
	}
	return true;
}

static bool test_ilog10(uint64_t start, uint64_t end)
{
	for (uint64_t x = start; x <= end; x++) {
		double lg10 = log10(x != 0 ? (double)x : 1.0);
		if (ilog10((uint32_t)x) != (unsigned int)lg10 ||
		    ilog10((int32_t)x) != ilog10((uint64_t)x)) {
			printf("%lu %u %u %u\n",
			       x, ilog10((uint32_t)x), ilog10((uint64_t)x), (unsigned int)lg10);
			return false;
		}
	}
	return true;
}

static bool test_ilog10_rand_64(uint64_t start, uint64_t end)
{
	struct random_state state;
	random_state_init(&state, start ^ end);
	for (uint64_t i = start; i <= end; i++) {
		uint64_t x = random_next_u64_in_range(&state, (uint64_t)UINT32_MAX + 1, UINT64_MAX);
		double lg10 = log10(x != 0 ? (double)x : 1.0);
		if (ilog10((int64_t)x) != (unsigned int)lg10) {
			return false;
		}
	}
	return true;
}

static unsigned int hackers_delight_clz32(uint32_t x)
{
	static const char table[64] = {
#define u 0
		32, 20, 19, u, u, 18, u, 7, 10, 17, u, u, 14, u, 6, u,
		u, 9, u, 16, u, u, 1, 26, u, 13, u, u, 24, 5, u, u,
		u, 21, u, 8, 11, u, 15, u, u, u, u, 2, 27, 0, 25, u,
		22, u, 12, u, u, 3, 28, u, 23, u, 4, 29, u, u, 30, 31
#undef u
	};
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x & ~(x >> 16);
	x = x * 0xFD7049FF;
	return table[x >> 26];
}

static unsigned int hackers_delight_clz64(uint64_t x)
{
	unsigned int n = hackers_delight_clz32(x >> 32);
	if (n == 32) {
		n += hackers_delight_clz32(x);
	}
	return n;
}

static bool test_clz32(uint64_t start, uint64_t end)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_clz32(x);
		if (clz((uint32_t)x) != reference ||
		    clz((int32_t)x) != reference) {
			return false;
		}
	}
	return true;
}

static bool test_clz64(uint64_t start, uint64_t end)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_clz64(x);
		if (clz((uint64_t)x) != reference ||
		    clz((int64_t)x) != reference) {
			return false;
		}
	}
	return true;
}

// TODO factor this out into a helper library and use it for other tests as well

struct range_work {
	thrd_t t;
	unsigned int tid;
	uint64_t start;
	uint64_t end;
	bool (*f)(uint64_t start, uint64_t end);
	bool success;
};

static int range_work_thread(void *arg)
{
	struct range_work *work = arg;
	work->success = work->f(work->start, work->end);
	return 0;
}

static void run_on_range(const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end), unsigned int nthreads)
{
	if (start == end) {
		return;
	}
	if (nthreads == 0) {
		static int nprocs = 0;
		if (nprocs == 0) {
			nprocs = get_nprocs();
			assert(nprocs > 0);
		}
		nthreads = nprocs;
	}
	uint64_t n = end - start;
	if (nthreads - 1 > n) {
		nthreads = n + 1;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads + 1;
	struct range_work *work = alloca(nthreads * sizeof(work[0]));
	uint64_t cur = start - 1;
	for (unsigned int t = 0; t < nthreads; t++) {
		work[t].tid = t;
		work[t].f = f;
		work[t].start = cur + 1;
		cur += per_thread;
		if (rem) {
			cur++;
			rem--;
		}
		work[t].end = cur;
		if (t != nthreads - 1) {
			if (thrd_create(&work[t].t, range_work_thread, &work[t]) != thrd_success) {
				fputs("thrd_create failed\n", stderr);
				exit(EXIT_FAILURE);
			}
		} else {
			range_work_thread(&work[t]);
		}
	}
	assert(cur == end);
	bool passed = true;
	for (unsigned int t = 0; t < nthreads; t++) {
		if (t != nthreads - 1) {
			if (thrd_join(work[t].t, NULL) != thrd_success) {
				fputs("thrd_join failed\n", stderr);
				exit(EXIT_FAILURE);
			}
		}
		if (!work[t].success) {
			passed = false;
			printf("[%s]: failure in range [%" PRIu64 ", %" PRIu64 "]\n",
			       name, work[t].start, work[t].end);
		}
	}
	if (passed) {
		printf("[%s]: passed in range [%" PRIu64 ", %" PRIu64 "]\n",
		       name, start, end);
	}
}

int main(int argc, char **argv)
{
	struct {
		uint64_t start;
		uint64_t end;
		bool (*f)(uint64_t start, uint64_t end);
		const char *name;
	} range_tests[] = {
#define TEST(start, end, f) {start, end, test_##f, #f}
		TEST(0, UINT32_MAX, clz32),
		TEST(0, UINT64_C(2) * UINT32_MAX, clz64),
		TEST(0, UINT32_MAX, ilog2),
		TEST(0, 1u << 30, ilog2_rand_64),
		TEST(0, UINT32_MAX, ilog10),
		TEST(0, 1u << 30, ilog10_rand_64),
#undef TEST
	};

	const char *nthreads_str = getenv("TESTS_NTHREADS");
	unsigned int nthreads = nthreads_str ? atoi(nthreads_str) : 0;
	for (size_t i = 0; i < sizeof(range_tests) / sizeof(range_tests[0]); i++) {
		if (argc > 1 && strcmp(argv[1], range_tests[i].name) != 0) {
			continue;
		}
		run_on_range(range_tests[i].name, range_tests[i].start, range_tests[i].end,
			     range_tests[i].f, nthreads);
	}
}
