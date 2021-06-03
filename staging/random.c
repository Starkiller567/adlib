#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>

#include "random.h"

static struct random_state g_random_state;

static uint64_t random_u64(void)
{
	return random_next_u64(&g_random_state);
}

static uint32_t random_u32(void)
{
	return random_next_u32(&g_random_state);
}

static uint64_t random_u64_in_range(uint64_t min, uint64_t max)
{
	return random_next_u64_in_range(&g_random_state, min, max);
}

static uint32_t random_u32_in_range(uint32_t min, uint32_t max)
{
	return random_next_u32_in_range(&g_random_state, min, max);
}

static double random_uniform_double(void)
{
	return random_next_uniform_double(&g_random_state);
}

static float random_uniform_float(void)
{
	return random_next_uniform_float(&g_random_state);
}

static double random_double_in_range(double min, double max)
{
	return random_next_double_in_range(&g_random_state, min, max);
}

static float random_float_in_range(float min, float max)
{
	return random_next_float_in_range(&g_random_state, min, max);
}

static bool random_bool(void)
{
	return random_next_bool(&g_random_state);
}

int main(int argc, char **argv)
{
	// random_state_init(&g_random_state, 12345);
	if (getrandom(&g_random_state, sizeof(g_random_state), 0) == -1) {
		perror("getrandom");
		return 1;
	}
#define N (1024 * 1024 * 1024)
	double *numbers = calloc(N, sizeof(numbers[0]));
	for (size_t i = 0; i < N; i++) {
		// numbers[i] = random_u64();
		numbers[i] = random_u64_in_range(0, 100);
		// numbers[i] = random_u64() % 101;
		// numbers[i] = random_u32();
		// numbers[i] = random_u32_in_range(0, 100);
		// numbers[i] = random_bool();
		// numbers[i] = random_uniform_double();
		// numbers[i] = random_uniform_float();
		// numbers[i] = random_double_in_range(0, 100);
		// numbers[i] = random_float_in_range(0, 100);
	}
	double mean = 0;
	for (size_t i = 0; i < N; i++) {
		mean = (i * mean + numbers[i]) / (i + 1);
	}
	double stddev = 0;
	for (size_t i = 0; i < N; i++) {
		double x = numbers[i] - mean;
		stddev += x * x;
	}
	stddev = sqrt(stddev / N);

	printf("mean: %g, stddev: %g\n", mean, stddev);
}
