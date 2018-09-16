#include <stdlib.h>
#include <stdio.h>
#include "mbuf.h"

int main(void)
{
	srand(1234);
	struct mbuf mbuf;
	size_t n = 4096;
	mbuf_init(&mbuf, malloc(n), n);

	const char *str = "ABCDEFGHIHKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	size_t len = strlen(str);
	size_t sizes[128];
	for (size_t i = 0; i < 128; i++) {
		size_t size = rand() % (len + 1);
		if (size == 0) {
			size = len;
		}
		size_t avail = mbuf_avail_size(&mbuf);
		if (size > avail) {
			size = avail;
		}
		n = mbuf_push(&mbuf, str, size);
		assert(n == size);
		sizes[i] = n;
	}

	char *buf = malloc(len + 1);
	for (size_t i = 0; i < 128; i++) {
		n = mbuf_pop(&mbuf, buf, len);
		assert(n == sizes[i]);
		if (n != 0) {
			buf[n] = 0;
			puts(buf);
		}
	}
	free(buf);

	n = mbuf_pop(&mbuf, buf, len);
	assert(n == 0);

	n = mbuf_push(&mbuf, str, len);
	assert(n == len);
	mbuf_flush(&mbuf);
	n = mbuf_pop(&mbuf, buf, len);
	assert(n == 0);

	free(mbuf_buffer(&mbuf));
	return 0;
}
