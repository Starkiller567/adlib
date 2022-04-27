#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "random.h"

static struct random_state g_random_state;

static _attr_unused uint64_t random_u64(void)
{
	return random_next_u64(&g_random_state);
}

static _attr_unused uint32_t random_u32(void)
{
	return random_next_u32(&g_random_state);
}

static _attr_unused uint64_t random_u64_in_range(uint64_t min, uint64_t max)
{
	return random_next_u64_in_range(&g_random_state, min, max);
}

static _attr_unused uint32_t random_u32_in_range(uint32_t min, uint32_t max)
{
	return random_next_u32_in_range(&g_random_state, min, max);
}

static _attr_unused double random_uniform_double(void)
{
	return random_next_uniform_double(&g_random_state);
}

static _attr_unused float random_uniform_float(void)
{
	return random_next_uniform_float(&g_random_state);
}

static _attr_unused double random_double_in_range(double min, double max)
{
	return random_next_double_in_range(&g_random_state, min, max);
}

static _attr_unused float random_float_in_range(float min, float max)
{
	return random_next_float_in_range(&g_random_state, min, max);
}

static _attr_unused bool random_bool(void)
{
	return random_next_bool(&g_random_state);
}

static void check(double *numbers, size_t n, double min, double max)
{
	double mean = 0;
	for (size_t i = 0; i < n; i++) {
		mean = (i * mean + numbers[i]) / (i + 1);
	}
	double stddev = 0;
	for (size_t i = 0; i < n; i++) {
		double x = numbers[i] - mean;
		stddev += x * x;
	}
	stddev = sqrt(stddev / n);


	double target_mean = (max - min) / 2.0;
	double target_stddev = sqrt((max - min) * (max - min) / 12.0);

	double dev_mean = fabs(target_mean - mean) / target_mean;
	double dev_stddev = fabs(target_stddev - stddev) / target_stddev;

	printf("target: mean = %g, stddev = %g\n", target_mean, target_stddev);
	printf("actual: mean = %g, stddev = %g\n", mean, stddev);
	// printf("deviation: mean = %g%%, stddev = %g%%\n", dev_mean, dev_stddev);

	assert(dev_mean < 0.001);
	assert(dev_stddev < 0.02);
}

int main(int argc, char **argv)
{
	// random_state_init(&g_random_state, 12345);
	if (getrandom(&g_random_state, sizeof(g_random_state), 0) == -1) {
		perror("getrandom");
		return 1;
	}

#define N (32 * 1024 * 1024)
	double *numbers = calloc(N, sizeof(numbers[0]));

	puts("random_u64_in_range(0, 100)");
	for (size_t i = 0; i < N; i++) {
		uint64_t x = random_u64_in_range(0, 100);
		assert(x <= 100);
		numbers[i] = x;
	}
	check(numbers, N, 0, 100);

	puts("random_u32_in_range(0, 100)");
	for (size_t i = 0; i < N; i++) {
		uint32_t x = random_u32_in_range(0, 100);
		assert(x <= 100);
		numbers[i] = x;
	}
	check(numbers, N, 0, 100);

	puts("random_double_in_range(0, 100)");
	for (size_t i = 0; i < N; i++) {
		double x = random_double_in_range(0, 100);
		assert(x <= 100);
		numbers[i] = x;
	}
	check(numbers, N, 0, 100);

	puts("random_float_in_range(0, 100)");
	for (size_t i = 0; i < N; i++) {
		float x = random_float_in_range(0, 100);
		assert(x <= 100);
		numbers[i] = x;
	}
	check(numbers, N, 0, 100);

	puts("random_u64()");
	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_u64();
	}
	check(numbers, N, 0, (double)UINT64_MAX);

	puts("random_u32()");
	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_u32();
	}
	check(numbers, N, 0, UINT32_MAX);

	puts("random_uniform_double()");
	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_uniform_double();
	}
	check(numbers, N, 0, 1);

	puts("random_uniform_float()");
	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_uniform_float();
	}
	check(numbers, N, 0, 1);

	puts("random_state_init()");
	for (uint64_t i = 0; i < N; i += 4) {
		struct random_state state;
		random_state_init(&state, i);
		numbers[i + 0] = state.s[0];
		numbers[i + 1] = state.s[1];
		numbers[i + 2] = state.s[2];
		numbers[i + 3] = state.s[3];
	}
	check(numbers, N, 0, (double)UINT64_MAX);

	puts("random_bool()");
	size_t nfalse = 0;
	size_t ntrue = 0;
	for (size_t i = 0; i < N; i++) {
		if (random_bool()) {
			ntrue++;
		} else {
			nfalse++;
		}
	}
	printf("ntrue = %zu, nfalse = %zu\n", ntrue, nfalse);
	assert(fabs((double)ntrue - (double)nfalse) / N < 0.001);

	free(numbers);
}
