#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "charconv.h"
#include "utils.h"

// TODO this could be a lot faster (multiple characters at once in the loop, 32-bit version, ...)

static _attr_unused _attr_always_inline
size_t _to_chars_helper(char *buf, uint64_t uval, size_t bits, bool is_signed, unsigned int base,
			bool leading_zeros, bool sign_always, bool uppercase)
{
	char sign_char = 0;
	if (base == 10 && is_signed) {
		int64_t sval = uval;
		if (sval < 0) {
			uval = -sval;
			sign_char = '-';
		} else if (sign_always) {
			sign_char = '+';
		}
	}

	uval &= UINT64_MAX >> (64 - bits);

	size_t n = 0;
	const char *alphabet = "0123456789abcdef";
	uint64_t tmp = uval;
	if (leading_zeros) {
		tmp = UINT64_MAX >> (64 - bits);
		if (base == 10 && is_signed) {
			tmp = tmp / 2 + 1;
		}
	}
	switch (base) {
	case 2:
		n = ilog2(tmp) + 1;
		break;
	case 8:
		n = ilog2(tmp) / 3 + 1;
		break;
	case 10:
		n = ilog10(tmp) + 1;
		break;
	case 16:
		n = ilog2(tmp) / 4 + 1;
		if (uppercase) {
			alphabet = "0123456789ABCDEF";
		}
		break;
	default:
		unreachable();
	}
	size_t total_length = n + (sign_char ? 1 : 0);
	if (!buf) {
		return total_length;
	}
	char *p = buf;
	if (sign_char) {
		p[0] = sign_char;
		p++;
	}
	assert(strlen(alphabet) >= base);
	do {
		p[--n] = alphabet[uval % base];
		uval /= base;
	} while (uval != 0);
	if (leading_zeros) {
		while (n-- > 0) {
			p[n] = '0';
		}
	}
	return total_length;
}

static _attr_unused size_t _to_chars(char *buf, uint64_t uval, size_t bits, unsigned int flags, bool is_signed)
{
	if (likely(flags == TO_CHARS_DEFAULT)) {
		flags |= TO_CHARS_DECIMAL;
	}
	unsigned int base = flags & __TO_CHARS_BASE_MASK;
	bool leading_zeros = unlikely(flags & TO_CHARS_LEADING_ZEROS);
	bool sign_always = unlikely(flags & TO_CHARS_PLUS_SIGN);
	bool uppercase = unlikely(flags & TO_CHARS_UPPERCASE);
	if (likely(base == 10)) {
		return _to_chars_helper(buf, uval, bits, is_signed, 10, leading_zeros, sign_always, uppercase);
	}
	if (likely(base == 16)) {
		return _to_chars_helper(buf, uval, bits, is_signed, 16, leading_zeros, sign_always, uppercase);
	}
	if (base == 2) {
		return _to_chars_helper(buf, uval, bits, is_signed, 2, leading_zeros, sign_always, uppercase);
	}
	if (base == 8) {
		return _to_chars_helper(buf, uval, bits, is_signed, 8, leading_zeros, sign_always, uppercase);
	}
	assert(false); // TODO implement
	return 0;
}

#define __CHARCONV_FOREACH_INTTYPE(f)		\
	f(char, char)				\
	f(schar, signed char)			\
	f(uchar, unsigned char)			\
	f(short, short)				\
	f(ushort, unsigned short)		\
	f(int, int)				\
	f(uint, unsigned int)			\
	f(long, long)				\
	f(ulong, unsigned long)			\
	f(llong, long long)			\
	f(ullong, unsigned long long)
#define __TO_CHARS_FUNC(name, type)					\
	__AD_LINKAGE size_t to_chars_##name(char *buf, type val, unsigned int flags) \
	{								\
		return _to_chars(buf, val, sizeof(type) * 8, flags, (type)-1 < 0); \
	}
__CHARCONV_FOREACH_INTTYPE(__TO_CHARS_FUNC)
#undef __TO_CHARS_FUNC
#undef __CHARCONV_FOREACH_INTTYPE
