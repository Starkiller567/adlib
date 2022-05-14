#ifndef __CHARCONV_INCLUDE__
#define __CHARCONV_INCLUDE__

#include <compiler.h>

enum to_chars_flags {
	TO_CHARS_DEFAULT = 0, // equivalent to TO_CHARS_DECIMAL, allows passing literal 0 for convenience

	TO_CHARS_BINARY = 2, // %b
	TO_CHARS_OCTAL = 8, // %o
	TO_CHARS_DECIMAL = 10, // %d, %u
	TO_CHARS_HEXADECIMAL = 16, // %x

	__TO_CHARS_BASE_MASK = 31,

	TO_CHARS_LEADING_ZEROS = 128, // %0n with n=maxlen for the specified conversion
	TO_CHARS_PLUS_SIGN = 256, // %+
	TO_CHARS_UPPERCASE = 512, // %x -> %X
};

__AD_LINKAGE size_t to_chars_char(char *buf, char val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_schar(char *buf, signed char val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_uchar(char *buf, unsigned char val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_short(char *buf, short val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_ushort(char *buf, unsigned short val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_int(char *buf, int val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_uint(char *buf, unsigned int val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_long(char *buf, long val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_ulong(char *buf, unsigned long val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_llong(char *buf, long long val, unsigned int flags) _attr_unused;
__AD_LINKAGE size_t to_chars_ullong(char *buf, unsigned long long val, unsigned int flags) _attr_unused;
#define to_chars(buf, val, flags) _Generic(val,				\
					   char : to_chars_char(buf, (char)val, flags), \
					   unsigned char : to_chars_uchar(buf, (unsigned char)val, flags), \
					   unsigned short : to_chars_ushort(buf, (unsigned short)val, flags), \
					   unsigned int : to_chars_uint(buf, (unsigned int)val, flags), \
					   unsigned long : to_chars_ulong(buf, (unsigned long)val, flags), \
					   unsigned long long : to_chars_ullong(buf, (unsigned long long)val, flags), \
					   signed char : to_chars_schar(buf, (signed char)val, flags), \
					   signed short : to_chars_short(buf, (signed short)val, flags), \
					   signed int : to_chars_int(buf, (signed int)val, flags), \
					   signed long : to_chars_long(buf, (signed long)val, flags), \
					   signed long long : to_chars_llong(buf, (signed long long)val, flags))

#endif
