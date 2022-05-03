#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <dstring.h>
#include <string.h>
#include "random.h"
#include "testing.h"

static bool check_from_number_binary(uint64_t x)
{
	// printf doesn't support binary yet...
	dstr_t dstr = dstr_from_number((uint64_t)x, DSTR_FMT_BINARY);
	for (size_t i = 0; i < dstr_length(dstr); i++) {
		if (dstr[i] != '0' && dstr[i] != '1') {
			return false;
		}
		uint32_t b1 = (x >> i) & 1;
		uint32_t b2 = dstr[dstr_length(dstr) - i - 1] - '0';
		if (b1 ^ b2) {
			return false;
		}
	}
	if (dstr_length(dstr) < 64 && (x >> dstr_length(dstr))) {
		return false;
	}
	dstr_free(&dstr);
	dstr = dstr_from_number((int64_t)x, DSTR_FMT_BINARY | DSTR_FMT_LEADING_ZEROS |
				DSTR_FMT_PLUS_SIGN | DSTR_FMT_UPPERCASE);
	if (dstr_length(dstr) != 64) {
		return false;
	}
	for (size_t i = 0; i < dstr_length(dstr); i++) {
		if (dstr[i] != '0' && dstr[i] != '1') {
			return false;
		}
		uint32_t b1 = (x >> i) & 1;
		uint32_t b2 = dstr[dstr_length(dstr) - i - 1] - '0';
		if (b1 ^ b2) {
			return false;
		}
	}
	dstr_free(&dstr);

	dstr = dstr_from_number((uint32_t)x, DSTR_FMT_BINARY);
	for (size_t i = 0; i < dstr_length(dstr); i++) {
		if (dstr[i] != '0' && dstr[i] != '1') {
			return false;
		}
		uint32_t b1 = (x >> i) & 1;
		uint32_t b2 = dstr[dstr_length(dstr) - i - 1] - '0';
		if (b1 ^ b2) {
			return false;
		}
	}
	if ((x & UINT32_MAX) >> dstr_length(dstr)) {
		return false;
	}
	dstr_free(&dstr);
	dstr = dstr_from_number((int32_t)x, DSTR_FMT_BINARY | DSTR_FMT_LEADING_ZEROS |
				DSTR_FMT_PLUS_SIGN | DSTR_FMT_UPPERCASE);
	if (dstr_length(dstr) != 32) {
		return false;
	}
	for (size_t i = 0; i < dstr_length(dstr); i++) {
		if (dstr[i] != '0' && dstr[i] != '1') {
			return false;
		}
		uint32_t b1 = (x >> i) & 1;
		uint32_t b2 = dstr[dstr_length(dstr) - i - 1] - '0';
		if (b1 ^ b2) {
			return false;
		}
	}
	dstr_free(&dstr);
	return true;
}

static bool check_from_number(uint64_t x)
{
	uint32_t u32 = x;
	int32_t  s32 = x;
	uint64_t u64 = x;
	int64_t  s64 = x;
	char buf[4096];
	sprintf(buf,
		"x" // dummy for strtok
		" %" PRId32
		" %" PRIu32
		" %" PRIo32
		" %" PRIx32
		" %" PRIX32
		" %+" PRId32
		" %" PRIu32
		" %" PRIo32
		" %" PRIx32
		// " %010" PRId32
		" %010" PRIu32
		" %011" PRIo32
		" %08" PRIx32
		" %+011" PRId32
		" %010" PRIu32
		" %011" PRIo32
		" %08" PRIx32
		" %" PRId64
		" %" PRIu64
		" %" PRIo64
		" %" PRIx64
		" %" PRIX64
		" %+" PRId64
		" %" PRIu64
		" %" PRIo64
		" %" PRIx64
		// " %019" PRId64
		" %020" PRIu64
		" %022" PRIo64
		" %016" PRIx64
		" %+020" PRId64
		" %020" PRIu64
		" %022" PRIo64
		" %016" PRIx64,
		u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
		u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64);
	char *save_ptr = NULL;
	strtok_r(buf, " ", &save_ptr);

	const struct {
		bool is_64bit;
		bool is_signed;
		unsigned int flags;
	} tests[] = {
		{false, true, 0},
		{false, false, DSTR_FMT_DECIMAL},
		{false, false, DSTR_FMT_OCTAL},
		{false, false, DSTR_FMT_HEXADECIMAL},
		{false, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_UPPERCASE},
		{false, true,  DSTR_FMT_DECIMAL | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_DECIMAL | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_OCTAL | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_PLUS_SIGN},
		// {false, true,  DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS},
		{false, false, DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS},
		{false, false, DSTR_FMT_OCTAL | DSTR_FMT_LEADING_ZEROS},
		{false, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_LEADING_ZEROS},
		{false, true,  DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_OCTAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{false, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{true, true, 0},
		{true, false, DSTR_FMT_DECIMAL},
		{true, false, DSTR_FMT_OCTAL},
		{true, false, DSTR_FMT_HEXADECIMAL},
		{true, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_UPPERCASE},
		{true, true,  DSTR_FMT_DECIMAL | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_DECIMAL | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_OCTAL | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_PLUS_SIGN},
		// {true, true,  DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS},
		{true, false, DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS},
		{true, false, DSTR_FMT_OCTAL | DSTR_FMT_LEADING_ZEROS},
		{true, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_LEADING_ZEROS},
		{true, true,  DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_DECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_OCTAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
		{true, false, DSTR_FMT_HEXADECIMAL | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_PLUS_SIGN},
	};
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		dstr_t dstr;
		if (tests[i].is_64bit) {
			if (tests[i].is_signed) {
				dstr = dstr_from_number(s64, tests[i].flags);
			} else {
				dstr = dstr_from_number(u64, tests[i].flags);
			}
		} else {
			if (tests[i].is_signed) {
				dstr = dstr_from_number(s32, tests[i].flags);
			} else {
				dstr = dstr_from_number(u32, tests[i].flags);
			}
		}
		char *reference = strtok_r(NULL, " ", &save_ptr);
		assert(reference);
		if (!dstr_equal_cstr(dstr, reference)) {
			unsigned long long ull = tests[i].is_64bit ? u64 : u32;
			long long ll = tests[i].is_64bit ? s64 : s32;
			printf("[%zu] %s != %s (%llu %lld %d %d %u)\n", i + 1, dstr, reference,
			       ull, ll, tests[i].is_64bit, tests[i].is_signed, tests[i].flags);
			return false;
		}
		dstr_free(&dstr);
	}
	assert(!strtok_r(NULL, " ", &save_ptr));
	return check_from_number_binary(x);
}

RANGE_TEST(from_number_range, 0, 1u << 22)
{
	for (uint64_t i = start; i <= end; i++) {
		if (!check_from_number(i)) {
			return false;
		}
	}
	return true;
}

RANDOM_TEST(from_number_random, 1u << 22, 1u << 22, UINT64_MAX)
{
	return check_from_number(random);
}

// TODO test smaller types and edge cases
// TODO parallelize
// TODO test these
// puts(dstr_from_int(INT_MIN, DSTR_FMT_DEFAULT));
// puts(dstr_from_int(-1, DSTR_FMT_DEFAULT));
// puts(dstr_from_int(0, DSTR_FMT_DEFAULT));
// puts(dstr_from_int(1, DSTR_FMT_DEFAULT));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_DEFAULT));
// puts(dstr_from_uint(0, DSTR_FMT_DEFAULT));
// puts(dstr_from_uint(10, DSTR_FMT_DEFAULT));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_DEFAULT));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(0, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(1, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_10 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_2));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_2));
// puts(dstr_from_int(0, DSTR_FMT_BASE_2));
// puts(dstr_from_int(1, DSTR_FMT_BASE_2));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_2));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_2));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_2));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_2));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(0, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(1, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_2 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_8));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_8));
// puts(dstr_from_int(0, DSTR_FMT_BASE_8));
// puts(dstr_from_int(1, DSTR_FMT_BASE_8));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_8));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_8));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_8));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_8));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(0, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(1, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_8 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_16));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_16));
// puts(dstr_from_int(0, DSTR_FMT_BASE_16));
// puts(dstr_from_int(1, DSTR_FMT_BASE_16));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_16));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_16));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_16));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_16));
// puts("");
// puts(dstr_from_int(INT_MIN, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(-1, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(0, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(1, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_int(INT_MAX, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(0, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(10, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts(dstr_from_uint(UINT_MAX, DSTR_FMT_BASE_16 | DSTR_FMT_LEADING_ZEROS | DSTR_FMT_SIGN_ALWAYS));
// puts("");
// puts(dstr_from_number((char)-1, DSTR_FMT_DEFAULT));
// puts(dstr_from_number((unsigned char)-1, DSTR_FMT_DEFAULT));
// puts(dstr_from_number((signed char)-1, DSTR_FMT_DEFAULT));
