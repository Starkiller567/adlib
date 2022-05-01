#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "random.h"
#include "testing.h"
#include "utils.h"

RANGE_TEST(ilog2, 0, UINT32_MAX)
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

RANDOM_TEST(ilog2_rand_64, 1u << 30, (uint64_t)UINT32_MAX + 1, UINT64_MAX)
{
	uint64_t x = random;
	double lg2 = log2(x != 0 ? (double)x : 1.0);
	if (ilog2((int64_t)x) != (unsigned int)lg2) {
		return false;
	}
	return true;
}

RANGE_TEST(ilog10, 0, UINT32_MAX)
{
	for (uint64_t x = start; x <= end; x++) {
		double lg10 = log10(x != 0 ? (double)x : 1.0);
		if (ilog10((uint32_t)x) != (unsigned int)lg10 ||
		    ilog10((int32_t)x) != ilog10((uint64_t)x)) {
			return false;
		}
	}
	return true;
}

RANDOM_TEST(ilog10_rand_64, 1u << 30, (uint64_t)UINT32_MAX + 1, UINT64_MAX)
{
	uint64_t x = random;
	double lg10 = log10(x != 0 ? (double)x : 1.0);
	if (ilog10((int64_t)x) != (unsigned int)lg10) {
		return false;
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

RANGE_TEST(clz32, 0, UINT32_MAX)
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

RANGE_TEST(clz64, 0, UINT64_C(2) * UINT32_MAX)
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
