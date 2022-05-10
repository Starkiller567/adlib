#include "utils.h"
#include <limits.h>
#include <stdint.h>

#ifndef HAVE_BUILTIN_CLZ
__AD_LINKAGE unsigned int _clz(unsigned int x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}

__AD_LINKAGE unsigned int _clzl(unsigned long x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}

__AD_LINKAGE unsigned int _clzll(unsigned long long x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}
#endif

#ifndef HAVE_BUILTIN_CTZ
__AD_LINKAGE unsigned int _ctz(unsigned int x)
{
	unsigned int n;
	x = ~x & (x - 1);
	for (n = 0; x; n++, x >>= 1);
	return n;
}

__AD_LINKAGE unsigned int _ctzl(unsigned long x)
{
	unsigned int n;
	x = ~x & (x - 1);
	for (n = 0; x; n++, x >>= 1);
	return n;
}

__AD_LINKAGE unsigned int _ctzll(unsigned long long x)
{
	unsigned int n;
	x = ~x & (x - 1);
	for (n = 0; x; n++, x >>= 1);
	return n;
}
#endif

#ifndef HAVE_BUILTIN_POPCOUNT
__AD_LINKAGE unsigned int _popcount(unsigned int x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}

__AD_LINKAGE unsigned int _popcountl(unsigned long x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}

__AD_LINKAGE unsigned int _popcountll(unsigned long long x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}
#endif

static unsigned int _ilog10_32(uint32_t x)
{
	// https://lemire.me/blog/2021/06/03/computing-the-number-of-digits-of-an-integer-even-faster/
	// the constants are different because the original algorithm returns ceil(log10(x)) but we want
	// floor(log10(x)) so that ilog10 matches ilog2
	// the constants are therefore the original constants minus (1 << 32)
	static const uint64_t table[] = {
		UINT64_C(0),
		UINT64_C(4294967286),
		UINT64_C(4294967286),
		UINT64_C(4294967286),
		UINT64_C(8589934492),
		UINT64_C(8589934492),
		UINT64_C(8589934492),
		UINT64_C(12884900888),
		UINT64_C(12884900888),
		UINT64_C(12884900888),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(21474736480),
		UINT64_C(21474736480),
		UINT64_C(21474736480),
		UINT64_C(25768803776),
		UINT64_C(25768803776),
		UINT64_C(25768803776),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(34259738368),
		UINT64_C(34259738368),
		UINT64_C(34259738368),
		UINT64_C(37654705664),
		UINT64_C(37654705664),
		UINT64_C(37654705664),
		UINT64_C(38654705664),
		UINT64_C(38654705664),
	};
	return (x + table[ilog2(x)]) >> 32;
}

static unsigned int _ilog10_64(uint64_t x)
{
	// Hacker's Delight
	static const uint64_t table[] = {
		UINT64_C(9),
		UINT64_C(99),
		UINT64_C(999),
		UINT64_C(9999),
		UINT64_C(99999),
		UINT64_C(999999),
		UINT64_C(9999999),
		UINT64_C(99999999),
		UINT64_C(999999999),
		UINT64_C(9999999999),
		UINT64_C(99999999999),
		UINT64_C(999999999999),
		UINT64_C(9999999999999),
		UINT64_C(99999999999999),
		UINT64_C(999999999999999),
		UINT64_C(9999999999999999),
		UINT64_C(99999999999999999),
		UINT64_C(999999999999999999),
		UINT64_C(9999999999999999999),
	};
	unsigned int y = (19 * ilog2(x)) >> 6;
	y += x > table[y];
	return y;
}

__AD_LINKAGE unsigned int _ilog10(unsigned int x)
{
#if UINT_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}

__AD_LINKAGE unsigned int _ilog10l(unsigned long x)
{
#if ULONG_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}

__AD_LINKAGE unsigned int _ilog10ll(unsigned long long x)
{
#if ULLONG_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}
