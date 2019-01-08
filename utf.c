#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "utf.h"

int
main(int argc, char **argv)
{
	char *str8 = u8"öäüßá 汉字 ↔æ█├𐀀";
	uint16_t *str16 = u"öäüßá 汉字 ↔æ█├𐀀";
	char buf8[256];
	uint16_t buf16[256];
	size_t num_chars1;
	size_t num_chars2;
	size_t num_chars3;
	size_t i1 = utf8_to_utf16(str8, buf16, 256, &num_chars1);
	size_t i2 = utf16_to_utf8(str16, buf8, 256, &num_chars2);
	assert(check_utf8(buf8, &num_chars3));
	assert(strcmp(str8, buf8) == 0);
	assert(memcmp(str16, buf16, i1) == 0);
	assert(num_chars1 == num_chars2);
	assert(num_chars1 == num_chars3);
	for (char *s = str8; *s; s = advance(s, 1)) {
		puts(s);
	}
	return 0;
}
