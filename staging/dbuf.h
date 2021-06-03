#ifndef __dbuf_include__
#define __dbuf_include__

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct dbuf {
	char *buf;
	size_t size;
	size_t capacity;
};

static void dbuf_init(struct dbuf *dbuf)
{
	dbuf->buf = NULL;
	dbuf->size = 0;
	dbuf->capacity = 0;
}

static void dbuf_free(struct dbuf *dbuf)
{
	free(dbuf->buf);
	dbuf_init(dbuf);
}

static void *dbuf_finalize(struct dbuf *dbuf)
{
	void *buf = dbuf->buf;
	dbuf_init(dbuf);
	return buf;
}

static struct dbuf dbuf_copy(const struct dbuf *dbuf)
{
	struct dbuf copy;
	copy.buf = malloc(dbuf->capacity);
	copy.size = dbuf->size;
	copy.capacity = dbuf->capacity;
	memcpy(copy.buf, dbuf->buf, dbuf->size);
	return copy;
}

static inline void *dbuf_buffer(const struct dbuf *dbuf)
{
	return dbuf->buf;
}

static inline size_t dbuf_size(const struct dbuf *dbuf)
{
	return dbuf->size;
}

static inline size_t dbuf_capacity(const struct dbuf *dbuf)
{
	return dbuf->capacity;
}

static inline size_t dbuf_avail_size(const struct dbuf *dbuf)
{
	return dbuf->capacity - dbuf->size;
}

static void dbuf_truncate(struct dbuf *dbuf, size_t new_size)
{
	if (new_size < dbuf->size) {
		dbuf->size = new_size;
	}
}

static void dbuf_reset(struct dbuf *dbuf)
{
	dbuf_truncate(dbuf, 0);
}

static void dbuf_resize(struct dbuf *dbuf, size_t capacity)
{
	dbuf->buf = realloc(dbuf->buf, capacity);
	if (dbuf->size > capacity) {
		dbuf->size = capacity;
	}
	dbuf->capacity = capacity;
}

static inline void dbuf_shrink_to_fit(struct dbuf *dbuf)
{
	dbuf_resize(dbuf, dbuf->size);
}

// increase capacity by atleast n bytes
static void dbuf_grow(struct dbuf *dbuf, size_t n)
{
	if (n == 0) {
		return;
	}
	unsigned int capacity = dbuf_capacity(dbuf);
	unsigned int new_capacity = n < capacity ? 2 * capacity : capacity + n;
	dbuf_resize(dbuf, new_capacity);
}

static void dbuf_reserve(struct dbuf *dbuf, size_t n)
{
	size_t avail = dbuf_avail_size(dbuf);
	if (n > avail) {
		dbuf_grow(dbuf, n - avail);
	}
}

static void dbuf_addb(struct dbuf *dbuf, unsigned char byte)
{
	dbuf_reserve(dbuf, 1);
	dbuf->buf[dbuf->size] = byte;
	dbuf->size++;
}

static void *dbuf_add_uninitialized(struct dbuf *dbuf, size_t count)
{
	dbuf_reserve(dbuf, count);
	void *p = dbuf->buf + dbuf->size;
	dbuf->size += count;
	return p;
}

static void dbuf_add(struct dbuf *dbuf, const void *buf, size_t count)
{
	void *p = dbuf_add_uninitialized(dbuf, count);
	memcpy(p, buf, count);
}

static inline void dbuf_addbuf(struct dbuf *dbuf, const struct dbuf *buf)
{
	dbuf_add(dbuf, dbuf_buffer(buf), dbuf_size(dbuf));
}

static inline void dbuf_addstr(struct dbuf *dbuf, const char *str)
{
	dbuf_add(dbuf, str, strlen(str));
}

static __attribute__ ((format (printf, 2, 3))) void dbuf_printf(struct dbuf *dbuf, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	dbuf_reserve(dbuf, n + 1);

	va_start(args, fmt);
	vsnprintf(dbuf->buf + dbuf->size, n + 1, fmt, args);
	va_end(args);

	dbuf->size += n;
}

#endif
