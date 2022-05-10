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

RANGE_TEST(clz64, 0, UINT32_MAX)
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

RANDOM_TEST(clz64_rand, 1u << 30, (uint64_t)UINT32_MAX + 1, UINT64_MAX)
{
	uint64_t x = random;
	unsigned int reference = hackers_delight_clz64(x);
	if (clz((uint64_t)x) != reference ||
	    clz((int64_t)x) != reference) {
		return false;

	}
	return true;
}

static unsigned int hackers_delight_ctz32(uint32_t x)
{
	static const char table[64] = {
#define u 0
		32, 0, 1, 12, 2, 6, u, 13, 3, u, 7, u, u, u, u, 14,
		10, 4, u, u, 8, u, u, 25, u, u, u, u, u, 21, 27, 15,
		31, 11, 5, u, u, u, u, u, 9, u, u, 24, u, u, 20, 26,
		30, u, u, u, u, 23, u, 19, 29, u, 22, 18, 28, 17, 16, u
#undef u
	};
	x = (x & -x) * 0x0450FBAF;
	return table[x >> 26];
}

static unsigned int hackers_delight_ctz64(uint64_t x)
{
	unsigned int n = hackers_delight_ctz32(x);
	if (n == 32) {
		n += hackers_delight_ctz32(x >> 32);
	}
	return n;
}

RANGE_TEST(ctz32, 0, UINT32_MAX)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_ctz32(x);
		if (ctz((uint32_t)x) != reference ||
		    ctz((int32_t)x) != reference) {
			return false;
		}
	}
	return true;
}

RANGE_TEST(ctz64, 0, UINT32_MAX)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_ctz64(x);
		if (ctz((uint64_t)x) != reference ||
		    ctz((int64_t)x) != reference) {
			return false;
		}
	}
	return true;
}

RANDOM_TEST(ctz64_rand, 1u << 30, (uint64_t)UINT32_MAX + 1, UINT64_MAX)
{
	uint64_t x = random;
	unsigned int reference = hackers_delight_ctz64(x);
	if (ctz((uint64_t)x) != reference ||
	    ctz((int64_t)x) != reference) {
		return false;
	}
	return true;
}

static unsigned int hackers_delight_popcount32(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);
	return x & 0x0000003F;
}

static unsigned int hackers_delight_popcount64(uint64_t x)
{
	return hackers_delight_popcount32(x) + hackers_delight_popcount32(x >> 32);
}

RANGE_TEST(popcount32, 0, UINT32_MAX)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_popcount32(x);
		if (popcount((uint32_t)x) != reference ||
		    popcount((int32_t)x) != reference) {
			return false;
		}
	}
	return true;
}

RANGE_TEST(popcount64, 0, UINT32_MAX)
{
	for (uint64_t x = start; x <= end; x++) {
		unsigned int reference = hackers_delight_popcount64(x);
		if (popcount((uint64_t)x) != reference ||
		    popcount((int64_t)x) != reference) {
			return false;
		}
	}
	return true;
}

RANDOM_TEST(popcount64_rand, 1u << 30, (uint64_t)UINT32_MAX + 1, UINT64_MAX)
{
	uint64_t x = random;
	unsigned int reference = hackers_delight_popcount64(x);
	if (popcount((uint64_t)x) != reference ||
	    popcount((int64_t)x) != reference) {
		return false;
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

RANDOM_TEST(bswap, 1u << 16, 0, UINT64_MAX)
{
#define CHECK_BSWAP_SIGNEDNESS(type)					\
	CHECK(bswap((unsigned type)random) == (unsigned type)bswap((signed type)random));

	CHECK_BSWAP_SIGNEDNESS(short);
	CHECK_BSWAP_SIGNEDNESS(int);
	CHECK_BSWAP_SIGNEDNESS(long);
	CHECK_BSWAP_SIGNEDNESS(long long);

#define CHECK_BSWAP_TYPE(type) CHECK(_Generic(bswap((type)0), type: 1))
	CHECK_BSWAP_TYPE(unsigned short);
	CHECK_BSWAP_TYPE(unsigned int);
	CHECK_BSWAP_TYPE(unsigned long);
	CHECK_BSWAP_TYPE(unsigned long long);
	CHECK_BSWAP_TYPE(signed short);
	CHECK_BSWAP_TYPE(signed int);
	CHECK_BSWAP_TYPE(signed long);
	CHECK_BSWAP_TYPE(signed long long);

#define CHECK_BSWAP(type)						\
	do {								\
		union {							\
			type val;					\
			char bytes[sizeof(type)];			\
		} normal = {.val = (type)random};			\
		union {							\
			type val;					\
			char bytes[sizeof(type)];			\
		} reversed = {.val = bswap((type)random)};		\
		for (unsigned int i = 0; i < sizeof(type); i++) {	\
			CHECK(normal.bytes[i] == reversed.bytes[sizeof(type) - i - 1]);	\
		}							\
	} while (0)

	CHECK_BSWAP(unsigned short);
	CHECK_BSWAP(unsigned int);
	CHECK_BSWAP(unsigned long);
	CHECK_BSWAP(unsigned long long);

	return true;
}

RANDOM_TEST(endianness, 1u << 16, 0, UINT64_MAX)
{
#define CHECK_CPU_TO_LE_TYPE(bits) CHECK(_Generic(cpu_to_le((uint##bits##_t)0), le##bits##_t: 1))
	CHECK_CPU_TO_LE_TYPE(16);
	CHECK_CPU_TO_LE_TYPE(32);
	CHECK_CPU_TO_LE_TYPE(64);
#define CHECK_LE_TO_CPU_TYPE(bits) CHECK(_Generic(le_to_cpu((le##bits##_t){0}), uint##bits##_t: 1))
	CHECK_LE_TO_CPU_TYPE(16);
	CHECK_LE_TO_CPU_TYPE(32);
	CHECK_LE_TO_CPU_TYPE(64);
#define CHECK_CPU_TO_BE_TYPE(bits) CHECK(_Generic(cpu_to_be((uint##bits##_t)0), be##bits##_t: 1))
	CHECK_CPU_TO_BE_TYPE(16);
	CHECK_CPU_TO_BE_TYPE(32);
	CHECK_CPU_TO_BE_TYPE(64);
#define CHECK_BE_TO_CPU_TYPE(bits) CHECK(_Generic(be_to_cpu((be##bits##_t){0}), uint##bits##_t: 1))
	CHECK_BE_TO_CPU_TYPE(16);
	CHECK_BE_TO_CPU_TYPE(32);
	CHECK_BE_TO_CPU_TYPE(64);

#define CHECK_BE_ROUNDTRIP(bits) CHECK(be##bits##_to_cpu(cpu_to_be##bits(random)) == (uint##bits##_t)random)
	CHECK_BE_ROUNDTRIP(16);
	CHECK_BE_ROUNDTRIP(32);
	CHECK_BE_ROUNDTRIP(64);
#define CHECK_LE_ROUNDTRIP(bits) CHECK(le##bits##_to_cpu(cpu_to_le##bits(random)) == (uint##bits##_t)random)
	CHECK_LE_ROUNDTRIP(16);
	CHECK_LE_ROUNDTRIP(32);
	CHECK_LE_ROUNDTRIP(64);

	const bool is_little_endian = (const union {uint16_t i; uint8_t c;}){.i = 1}.c == 1;
	if (is_little_endian) {
#define CHECK_LE_IDENTITY(bits) CHECK(cpu_to_le##bits(random).val == (uint##bits##_t)random)
		CHECK_LE_IDENTITY(16);
		CHECK_LE_IDENTITY(32);
		CHECK_LE_IDENTITY(64);
#define CHECK_BE_BSWAP(bits) CHECK(cpu_to_be##bits(random).val == bswap((uint##bits##_t)random))
		CHECK_BE_BSWAP(16);
		CHECK_BE_BSWAP(32);
		CHECK_BE_BSWAP(64);
	} else {
#define CHECK_BE_IDENTITY(bits) CHECK(cpu_to_be##bits(random).val == (uint##bits##_t)random)
		CHECK_BE_IDENTITY(16);
		CHECK_BE_IDENTITY(32);
		CHECK_BE_IDENTITY(64);
#define CHECK_LE_BSWAP(bits) CHECK(cpu_to_le##bits(random).val == bswap((uint##bits##_t)random))
		CHECK_LE_BSWAP(16);
		CHECK_LE_BSWAP(32);
		CHECK_LE_BSWAP(64);
	}

	return true;
}
