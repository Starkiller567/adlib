#ifndef __UTILS_INCLUDE__
#define __UTILS_INCLUDE__

#include <endian.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "compiler.h"
#include "config.h"

_Static_assert(CHAR_BIT == 8, "this implementation assumes 8-bit chars");

#define _utils_dispatch_builtin(x, f) _Generic((x),			\
					       unsigned int : f(x),	\
					       unsigned long : f##l(x), \
					       unsigned long long : f##ll(x), \
					       signed int : f(x),	\
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
#define clz(x) _utils_dispatch_builtin(x, _clz)


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

#define ilog2(x) _utils_dispatch_builtin(x, _ilog2)

__AD_LINKAGE unsigned int _ilog10(unsigned int x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10l(unsigned long x) _attr_const _attr_unused;
__AD_LINKAGE unsigned int _ilog10ll(unsigned long long x) _attr_const _attr_unused;

#define ilog10(x) _utils_dispatch_builtin(x, _ilog10)

#if USHRT_MAX == UINT16_MAX
# define _SHORT_BITS 16
#elif USHRT_MAX == UINT32_MAX
# define _SHORT_BITS 32
#elif USHRT_MAX == UINT64_MAX
# define _SHORT_BITS 64
#else
#error "not supported"
#endif

#if UINT_MAX == UINT16_MAX
# define _INT_BITS 16
#elif UINT_MAX == UINT32_MAX
# define _INT_BITS 32
#elif UINT_MAX == UINT64_MAX
# define _INT_BITS 64
#else
#error "not supported"
#endif

#if ULONG_MAX == UINT32_MAX
# define _LONG_BITS 32
#elif ULONG_MAX == UINT64_MAX
# define _LONG_BITS 64
#else
#error "not supported"
#endif

#if ULLONG_MAX == UINT64_MAX
# define _LLONG_BITS 64
#else
#error "not supported"
#endif

#define _utils_concat_helper(x, y) x##y
#define _utils_concat(x, y) _utils_concat_helper(x, y)

#define _utils_foreach_multibyte_type(f, ...)				\
		f(ushort, unsigned short,     _SHORT_BITS, __VA_ARGS__)	\
		f(sshort, signed short,       _SHORT_BITS, __VA_ARGS__)	\
		f(uint,   unsigned int,       _INT_BITS,   __VA_ARGS__)	\
		f(sint,   signed int,         _INT_BITS,   __VA_ARGS__)	\
		f(ulong,  unsigned long,      _LONG_BITS,  __VA_ARGS__)	\
		f(slong,  signed long,        _LONG_BITS,  __VA_ARGS__)	\
		f(ullong, unsigned long long, _LLONG_BITS, __VA_ARGS__)	\
		f(sllong, signed long long,   _LLONG_BITS, __VA_ARGS__)

#define _utils_foreach_type(f, ...)					\
		f(bool,  _Bool,         8, __VA_ARGS__)			\
		f(char,  char,          8, __VA_ARGS__)			\
		f(uchar, unsigned char, 8, __VA_ARGS__)			\
		f(schar, signed char,   8, __VA_ARGS__)			\
		_utils_foreach_multibyte_type(f, __VA_ARGS__)


#define _utils_check_bits(suffix, type, bits, ...)			\
	_Static_assert(sizeof(type) * 8 == bits, "wrong bit size for " #type);
_utils_foreach_type(_utils_check_bits)
#undef _utils_check_bits

#define _utils_check_multibytes(suffix, type, bits, ...)		\
	_Static_assert(sizeof(type) > 1, "expected " #type " to have more than 1 byte");
_utils_foreach_multibyte_type(_utils_check_multibytes)
#undef _utils_check_multibytes

#define _utils_min_function(suffix, type, bits, ...)			\
	static _attr_always_inline _attr_const _attr_unused type _min_##suffix(type a, type b) \
	{								\
		return a < b ? a : b;					\
	}

#define _utils_max_function(suffix, type, bits, ...)			\
	static _attr_always_inline _attr_const _attr_unused type _max_##suffix(type a, type b) \
	{								\
		return a > b ? a : b;					\
	}

_utils_foreach_type(_utils_min_function)
_utils_foreach_type(_utils_max_function)

#undef _utils_min_function
#undef _utils_max_function

#define _utils_dispatch_min(suffix, type, bits, a, b) type : _min_##suffix(a, b),
#define _utils_dispatch_max(suffix, type, bits, a, b) type : _max_##suffix(a, b),

typedef struct { int i; } _utils_dummy_t;

#define min_t(type, a, b) _Generic(*(type*)0, _utils_foreach_type(_utils_dispatch_min, a, b) _utils_dummy_t:0)
#define max_t(type, a, b) _Generic(*(type*)0, _utils_foreach_type(_utils_dispatch_max, a, b) _utils_dummy_t:0)

static _attr_always_inline _attr_const _attr_unused uint16_t _bswap16(uint16_t x)
{
#ifdef HAVE_BUILTIN_BSWAP
	return __builtin_bswap16(x);
#else
	return (x >> 8) | (x << 8);
#endif
}

static _attr_always_inline _attr_const _attr_unused uint32_t _bswap32(uint32_t x)
{
#ifdef HAVE_BUILTIN_BSWAP
	return __builtin_bswap32(x);
#else
	return ((x >> 24) | ((x & 0x00ff0000) >>  8) | ((x & 0x0000ff00) <<  8) | (x << 24));
#endif
}

static _attr_always_inline _attr_const _attr_unused uint64_t _bswap64(uint64_t x)
{
#ifdef HAVE_BUILTIN_BSWAP
	return __builtin_bswap64(x);
#else
	return (x >> 56) | ((x & 0x00ff000000000000) >> 40) | ((x & 0x0000ff0000000000) >> 24) |
		((x & 0x000000ff00000000) >>  8) | ((x & 0x00000000ff000000) <<  8) |
		((x & 0x0000000000ff0000) << 24) | ((x & 0x000000000000ff00) << 40) | (x << 56);
#endif
}

#define _utils_bswap_function(suffix, type, bits, ...)			\
	static _attr_always_inline _attr_const _attr_unused type _bswap_##suffix(type x) \
	{								\
		return _utils_concat(_bswap, bits)(x);			\
	}

_utils_foreach_multibyte_type(_utils_bswap_function)

#undef _utils_bswap_function

#define _utils_bswap_dispatch(suffix, type, bits, x) type : _bswap_##suffix(x),

#define bswap(x) _Generic(x, _utils_foreach_multibyte_type(_utils_bswap_dispatch, x) _utils_dummy_t:0)

#if !defined(BYTE_ORDER_IS_LITTLE_ENDIAN) && (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
# define BYTE_ORDER_IS_LITTLE_ENDIAN 1
#endif
#if !defined(BYTE_ORDER_IS_BIG_ENDIAN) && (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
# define BYTE_ORDER_IS_BIG_ENDIAN 1
#endif

#if defined(BYTE_ORDER_IS_LITTLE_ENDIAN) && defined(BYTE_ORDER_IS_BIG_ENDIAN)
# error "both big and little endian"
#endif

typedef union {
	uint16_t val;
	char bytes[2];
} le16_t;

typedef union {
	uint32_t val;
	char bytes[4];
} le32_t;

typedef union {
	uint64_t val;
	char bytes[8];
} le64_t;

typedef union {
	uint16_t val;
	char bytes[2];
} be16_t;

typedef union {
	uint32_t val;
	char bytes[4];
} be32_t;

typedef union {
	uint64_t val;
	char bytes[8];
} be64_t;

#ifdef BYTE_ORDER_IS_LITTLE_ENDIAN

static _attr_always_inline _attr_const _attr_unused be16_t cpu_to_be16(uint16_t x)
{
	return (be16_t)_bswap16(x);
}

static _attr_always_inline _attr_const _attr_unused be32_t cpu_to_be32(uint32_t x)
{
	return (be32_t)_bswap32(x);
}

static _attr_always_inline _attr_const _attr_unused be64_t cpu_to_be64(uint64_t x)
{
	return (be64_t)_bswap64(x);
}

static _attr_always_inline _attr_const _attr_unused uint16_t be16_to_cpu(be16_t x)
{
	return _bswap16(x.val);
}

static _attr_always_inline _attr_const _attr_unused uint32_t be32_to_cpu(be32_t x)
{
	return _bswap32(x.val);
}

static _attr_always_inline _attr_const _attr_unused uint64_t be64_to_cpu(be64_t x)
{
	return _bswap64(x.val);
}

static _attr_always_inline _attr_const _attr_unused le16_t cpu_to_le16(uint16_t x)
{
	return (le16_t)x;
}

static _attr_always_inline _attr_const _attr_unused le32_t cpu_to_le32(uint32_t x)
{
	return (le32_t)x;
}

static _attr_always_inline _attr_const _attr_unused le64_t cpu_to_le64(uint64_t x)
{
	return (le64_t)x;
}

static _attr_always_inline _attr_const _attr_unused uint16_t le16_to_cpu(le16_t x)
{
	return x.val;
}

static _attr_always_inline _attr_const _attr_unused uint32_t le32_to_cpu(le32_t x)
{
	return x.val;
}

static _attr_always_inline _attr_const _attr_unused uint64_t le64_to_cpu(le64_t x)
{
	return x.val;
}

#endif

#ifdef BYTE_ORDER_IS_BIG_ENDIAN

static _attr_always_inline _attr_const _attr_unused le16_t cpu_to_le16(uint16_t x)
{
	return (le16_t)_bswap16(x);
}

static _attr_always_inline _attr_const _attr_unused le32_t cpu_to_le32(uint32_t x)
{
	return (le32_t)_bswap32(x);
}

static _attr_always_inline _attr_const _attr_unused le64_t cpu_to_le64(uint64_t x)
{
	return (le64_t)_bswap64(x);
}

static _attr_always_inline _attr_const _attr_unused uint16_t le16_to_cpu(le16_t x)
{
	return _bswap16(x.val);
}

static _attr_always_inline _attr_const _attr_unused uint32_t le32_to_cpu(le32_t x)
{
	return _bswap32(x.val);
}

static _attr_always_inline _attr_const _attr_unused uint64_t le64_to_cpu(le64_t x)
{
	return _bswap64(x.val);
}

static _attr_always_inline _attr_const _attr_unused be16_t cpu_to_be16(uint16_t x)
{
	return (be16_t)x;
}

static _attr_always_inline _attr_const _attr_unused be32_t cpu_to_be32(uint32_t x)
{
	return (be32_t)x;
}

static _attr_always_inline _attr_const _attr_unused be64_t cpu_to_be64(uint64_t x)
{
	return (be64_t)x;
}

static _attr_always_inline _attr_const _attr_unused uint16_t be16_to_cpu(be16_t x)
{
	return x.val;
}

static _attr_always_inline _attr_const _attr_unused uint32_t be32_to_cpu(be32_t x)
{
	return x.val;
}

static _attr_always_inline _attr_const _attr_unused uint64_t be64_to_cpu(be64_t x)
{
	return x.val;
}

#endif

#if defined(BYTE_ORDER_IS_BIG_ENDIAN) || defined(BYTE_ORDER_IS_LITTLE_ENDIAN)

#define cpu_to_be(x) _Generic(x, uint16_t : cpu_to_be16(x), uint32_t : cpu_to_be32(x), \
			      uint64_t : cpu_to_be64(x))

#define be_to_cpu(x) _Generic(x, be16_t : be16_to_cpu((be16_t)(uint16_t)x.val), \
			      be32_t : be32_to_cpu((be32_t)(uint32_t)x.val), \
			      be64_t : be64_to_cpu((be64_t)(uint64_t)x.val))

#define cpu_to_le(x) _Generic(x, uint16_t : cpu_to_le16(x), uint32_t : cpu_to_le32(x), \
			      uint64_t : cpu_to_le64(x))

#define le_to_cpu(x) _Generic(x, le16_t : le16_to_cpu((le16_t)(uint16_t)x.val), \
			      le32_t : le32_to_cpu((le32_t)(uint32_t)x.val), \
			      le64_t : le64_to_cpu((le64_t)(uint64_t)x.val))

#endif

#undef _SHORT_BITS
#undef _INT_BITS
#undef _LONG_BITS
#undef _LLONG_BITS

#endif
