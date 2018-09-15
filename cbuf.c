#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "cbuf.h"

int main(void)
{
	struct cbuf cbuf;
	cbuf_init(&cbuf, malloc(16), 16);
	const char *str = "abcdefghijklmnopqrstuvwxyz";
	cbuf_write(&cbuf, 54, str, strlen(str));
	char buf[44];
	cbuf_read(&cbuf, 6, buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;
	puts(buf);
}
