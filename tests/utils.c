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

SIMPLE_TEST(minmax)
{
#define _CHECK_MINMAX(type, e, c)		\
	CHECK(_Generic(e, type : e) == (type)c)
#define CHECK_MIN(type, a, b, c) _CHECK_MINMAX(type, min_t(type, a, b), c)
#define CHECK_MAX(type, a, b, c) _CHECK_MINMAX(type, max_t(type, a, b), c)

	CHECK_MIN(bool, false, false, false);
	CHECK_MIN(bool, false, true, false);
	CHECK_MIN(bool, true, false, false);
	CHECK_MIN(bool, true, true, true);

	CHECK_MAX(bool, false, false, false);
	CHECK_MAX(bool, false, true, true);
	CHECK_MAX(bool, true, false, true);
	CHECK_MAX(bool, true, true, true);

	CHECK_MIN(signed char, 0, -1, -1);
	CHECK_MIN(signed char, 0, 1, 0);
	CHECK_MIN(signed char, 0, 0, 0);
	CHECK_MIN(signed char, 1, -1, -1);
	CHECK_MIN(signed char, -128, 127, -128);

	CHECK_MAX(signed char, 0, -1, 0);
	CHECK_MAX(signed char, 0, 1, 1);
	CHECK_MAX(signed char, 0, 0, 0);
	CHECK_MAX(signed char, 1, -1, 1);
	CHECK_MAX(signed char, -128, 127, 127);

	CHECK_MIN(unsigned char, 0, -1, 0);
	CHECK_MIN(unsigned char, 0, 1, 0);
	CHECK_MIN(unsigned char, 0, 0, 0);
	CHECK_MIN(unsigned char, 1, -1, 1);
	CHECK_MIN(unsigned char, 255, 0, 0);

	CHECK_MAX(unsigned char, 0, -1, -1);
	CHECK_MAX(unsigned char, 0, 1, 1);
	CHECK_MAX(unsigned char, 0, 0, 0);
	CHECK_MAX(unsigned char, 1, -1, -1);
	CHECK_MAX(unsigned char, 255, 0, 255);

#define MINMAX_TEST(type)						\
	do {								\
		for (int i = -10; i <= 10; i++) {			\
			for (int j = -10; j <= 10; j++) {		\
				type a = (type)i;			\
				type b = (type)j;			\
				type min_val = min_t(type, a, b);	\
				type max_val = max_t(type, a, b);	\
				CHECK(min_val <= a && min_val <= b);	\
				CHECK(max_val >= a && max_val >= b);	\
			}						\
		}							\
	} while (0)

	MINMAX_TEST(char);
	MINMAX_TEST(unsigned char);
	MINMAX_TEST(unsigned short);
	MINMAX_TEST(unsigned int);
	MINMAX_TEST(unsigned long);
	MINMAX_TEST(unsigned long long);
	MINMAX_TEST(signed char);
	MINMAX_TEST(signed short);
	MINMAX_TEST(signed int);
	MINMAX_TEST(signed long);
	MINMAX_TEST(signed long long);

	return true;
}
