#ifndef __UTILS_INCLUDE__
#define __UTILS_INCLUDE__

#include <stdbool.h>
#include "compiler.h"
#include "config.h"

#define _bitutils_dispatch_builtin(x, f) _Generic((x),			\
						  unsigned int : f(x),	\
						  unsigned long : f##l(x), \
						  unsigned long long : f##ll(x), \
						  signed int : f(x),	\
						  signed long : f##l(x), \
						  signed long long : f##ll(x))


#ifdef HAVE_BUILTIN_CLZ
static _attr_always_inline _attr_const _attr_unused unsigned int _clz(unsigned int x)
{
	return unlikely(x == 0) ? (8 * sizeof(x)) : __builtin_clz(x);
}
static _attr_always_inline _attr_const _attr_unused unsigned int _clzl(unsigned long x)
{
	return unlikely(x == 0) ? (8 * sizeof(x)) : __builtin_clzl(x);
}
static _attr_always_inline _attr_const _attr_unused unsigned int _clzll(unsigned long long x)
{
	return unlikely(x == 0) ? (8 * sizeof(x)) : __builtin_clzll(x);
}
#else
__AD_LINKAGE unsigned int _clz(unsigned int x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _clzl(unsigned long x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _clzll(unsigned long long x) _attr_const _attr_unused;
#endif
#define clz(x) _bitutils_dispatch_builtin(x, _clz)


static _attr_always_inline _attr_const _attr_unused unsigned int _ilog2(unsigned int x)
{
	return 8 * sizeof(x) - 1 - _clz(x | 1);
}

static _attr_always_inline _attr_const _attr_unused unsigned int _ilog2l(unsigned long x)
{
	return 8 * sizeof(x) - 1 - _clzl(x | 1);
}

static _attr_always_inline _attr_const _attr_unused unsigned int _ilog2ll(unsigned long long x)
{
	return 8 * sizeof(x) - 1 - _clzll(x | 1);
}

#define ilog2(x) _bitutils_dispatch_builtin(x, _ilog2)

__AD_LINKAGE unsigned int _ilog10(unsigned int x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10l(unsigned long x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10ll(unsigned long long x) _attr_const _attr_unused;

#define ilog10(x) _bitutils_dispatch_builtin(x, _ilog10)

#define _bitutils_foreach_type(f)		\
	f(bool, _Bool)				\
	f(char, char)				\
	f(uchar, unsigned char)			\
	f(ushort, unsigned short)			\
	f(uint, unsigned int)			\
	f(ulong, unsigned long)			\
	f(ullong, unsigned long long)		\
	f(schar, signed char)			\
	f(sshort, signed short)			\
	f(sint, signed int)			\
	f(slong, signed long)			\
	f(sllong, signed long long)		\

#define _bituils_min_function(suffix, type)				\
	static _attr_always_inline _attr_const _attr_unused type min_##suffix(type a, type b) \
	{								\
		return a < b ? a : b;					\
	}								\

#define _bituils_max_function(suffix, type)				\
	static _attr_always_inline _attr_const _attr_unused type max_##suffix(type a, type b) \
	{								\
		return a > b ? a : b;					\
	}								\

_bitutils_foreach_type(_bituils_min_function)
_bitutils_foreach_type(_bituils_max_function)

#undef _bitutils_foreach_type
#undef _bituils_min_function
#undef _bituils_max_function

#define _bitutils_dispatch_min_max(f, a, b, t) _Generic((t)0,		\
							_Bool : f##_bool(a, b), \
							char : f##_char(a, b), \
							unsigned char : f##_uchar(a, b), \
							unsigned short : f##_ushort(a, b), \
							unsigned int : f##_uint(a, b), \
							unsigned long : f##_ulong(a, b), \
							unsigned long long : f##_ullong(a, b), \
							signed char : f##_schar(a, b), \
							signed short : f##_sshort(a, b), \
							signed int : f##_sint(a, b), \
							signed long : f##_slong(a, b), \
							signed long long : f##_sllong(a, b))

#define min_t(type, a, b) _bitutils_dispatch_min_max(min, a, b, type)
#define max_t(type, a, b) _bitutils_dispatch_min_max(max, a, b, type)

#endif
