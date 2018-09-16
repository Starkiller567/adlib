#ifndef __mbuf_include__
#define __mbuf_include__

#include "cbuf.h"

struct mbuf {
	struct cbuf cbuf;
};

typedef struct {
	size_t size;
} __mbuf_msg_hdr;

static inline void mbuf_init(struct mbuf *mbuf, void *mem, size_t capacity)
{
	cbuf_init(&mbuf->cbuf, mem, capacity);
}

static inline void *mbuf_buffer(struct mbuf *mbuf)
{
	return cbuf_buffer(&mbuf->cbuf);
}

static inline size_t mbuf_capacity(struct mbuf *mbuf)
{
	return cbuf_capacity(&mbuf->cbuf);
}

static inline void mbuf_flush(struct mbuf *mbuf)
{
	cbuf_flush(&mbuf->cbuf);
}

static inline size_t mbuf_avail_size(struct mbuf *mbuf)
{
	size_t avail = cbuf_avail_size(&mbuf->cbuf);
	size_t hdr_size = sizeof(__mbuf_msg_hdr);
	return avail > hdr_size ? avail - hdr_size : 0;
}

static size_t mbuf_push(struct mbuf *mbuf, const void *buf, size_t count)
{
	if (count == 0) {
		// accepting empty messages would complicate the interface due
		// to ambiguous return values
		// (was the header pushed or not?, empty message or no message?)
		return 0;
	}
	size_t n;
	__mbuf_msg_hdr hdr;
	if (cbuf_avail_size(&mbuf->cbuf) < sizeof(hdr) + count) {
		// TODO optional overwrite?
		return 0;
	}
	hdr.size = count;
	n = cbuf_push(&mbuf->cbuf, &hdr, sizeof(hdr), false);
	assert(n == sizeof(hdr));
	n = cbuf_push(&mbuf->cbuf, buf, count, false);
	assert(n == count);
	return count;
}

// Returns the size of the next message if the buffer is too small
static size_t mbuf_pop(struct mbuf *mbuf, void *buf, size_t count)
{
	size_t n;
	__mbuf_msg_hdr hdr;
	n = cbuf_peek(&mbuf->cbuf, &hdr, sizeof(hdr));
	if (n == 0) {
		return 0;
	}
	assert(n == sizeof(hdr));
	size_t size = hdr.size;
	assert(0 < size && size <= cbuf_size(&mbuf->cbuf) - sizeof(hdr));
	if (size > count) {
		return size;
	}
	bool b = cbuf_skip(&mbuf->cbuf, sizeof(hdr));
	assert(b);
	n = cbuf_pop(&mbuf->cbuf, buf, size);
	assert(n == size);
	return n;
}

#endif
