#ifndef __UTILS_INCLUDE__
#define __UTILS_INCLUDE__

#include "compiler.h"
#include "config.h"

#define _bitutils_dispatch(x, f) _Generic((x),				\
					  unsigned int : f(x),		\
					  unsigned long : f##l(x),	\
					  unsigned long long : f##ll(x), \
					  signed int : f(x),		\
					  signed long : f##l(x),	\
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
#define clz(x) _bitutils_dispatch(x, _clz)


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

#define ilog2(x) _bitutils_dispatch(x, _ilog2)

__AD_LINKAGE unsigned int _ilog10(unsigned int x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10l(unsigned long x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10ll(unsigned long long x) _attr_const _attr_unused;

// TODO this is rounded up while ilog2 is rounded down which is confusing...
#define ilog10(x) _bitutils_dispatch(x, _ilog10)

#endif
