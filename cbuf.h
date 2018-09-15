#ifndef __cbuf_include__
#define __cbuf_include__

#include <string.h>

struct cbuf {
	char *buf;
	size_t size;
	size_t start;
	size_t end;
};

static void cbuf_init(struct cbuf *cbuf, void *mem, size_t size)
{
	assert(size != 0 && (size & (size - 1)) == 0);

	cbuf->buf = mem;
	cbuf->size = size;
	cbuf->start = 0;
	cbuf->end = 0;
}

static void cbuf_write(struct cbuf *cbuf, size_t offset, const void *_buf, size_t count)
{
	size_t skip = count > cbuf->size ? count - cbuf->size : 0;
	const char *buf = (const char *)_buf + skip;
	offset += skip;
	count -= skip;
	offset &= cbuf->size - 1;
	char *start = cbuf->buf + offset;
	if (offset + count > cbuf->size) {
		size_t n = cbuf->size - offset;
		memcpy(start, buf, n);
		count -= n;
		buf += n;
		start = cbuf->buf;
	}
	memcpy(start, buf, count);
}

static void cbuf_read(struct cbuf *cbuf, size_t offset, void *_buf, size_t count)
{
	char *buf = (char *)_buf;
	offset &= cbuf->size - 1;
	size_t n = cbuf->size - offset;
	if (n > count) {
		memcpy(buf, cbuf->buf + offset, count);
		return;
	}
	memcpy(buf, cbuf->buf + offset, n);
	buf += n;
	count -= n;
	while (count > cbuf->size) {
		memcpy(buf, cbuf->buf, cbuf->size);
		buf += cbuf->size;
		count -= cbuf->size;
	}
	memcpy(buf, cbuf->buf, count);
}

#endif
