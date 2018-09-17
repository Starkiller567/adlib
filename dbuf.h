#ifndef __dbuf_include__
#define __dbuf_include__

#include <stdlib.h>
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

static void dbuf_addb(struct dbuf *dbuf, char byte)
{
	size_t avail = dbuf_avail_size(dbuf);
	if (avail == 0) {
		dbuf_grow(dbuf, 1);
	}
	dbuf->buf[dbuf->size] = byte;
	dbuf->size++;
}

static void dbuf_add(struct dbuf *dbuf, const void *buf, size_t count)
{
	size_t avail = dbuf_avail_size(dbuf);
	if (count > avail) {
		dbuf_grow(dbuf, count - avail);
	}
	memcpy(dbuf->buf + dbuf->size, buf, count);
	dbuf->size += count;
}

static inline void dbuf_addbuf(struct dbuf *dbuf, const struct dbuf *buf)
{
	dbuf_add(dbuf, dbuf_buffer(buf), dbuf_size(dbuf));
}

static inline void dbuf_addstr(struct dbuf *dbuf, const char *str)
{
	dbuf_add(dbuf, str, strlen(str));
}

#endif
