#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "cbuf.h"

int main(void)
{
	struct cbuf cbuf;
	size_t n = 16;
	cbuf_init(&cbuf, malloc(n), n);

	const char *str = "ABCDEFGHIHKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);

	char buf[128];
	n = cbuf_pop(&cbuf, buf, 20);
	printf("%lu\n", n);
	buf[n] = 0;
	puts(buf);
	size_t k = cbuf_pop(&cbuf, buf + n, sizeof(buf) - 1 - n);
	printf("%lu\n", k);
	buf[n + k] = 0;
	puts(buf);

	cbuf_flush(&cbuf);

	n = 0;
	for (size_t i = 0; i < 80; i++) {
		bool b = cbuf_pushb(&cbuf, '0' + i, true);
		if (b) {
			n++;
		}
	}

	printf("%lu\n", n);
	if (n > cbuf.capacity) {
		n = cbuf.capacity;
	}

	for (size_t i = 0; i < n; i++) {
		char c;
		bool b = cbuf_popb(&cbuf, &c);
		assert(b);
		putchar(c);
	}
	putchar('\n');

	free(cbuf.buf);
	return 0;
}
