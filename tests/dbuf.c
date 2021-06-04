#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dbuf.h"

#ifdef HAVE_MALLOC_USABLE_SIZE
#include <malloc.h>
#endif

static void check_size_and_capacity(const struct dbuf *dbuf)
{
	assert(dbuf_capacity(dbuf) >= dbuf_size(dbuf));
	assert(dbuf_available_size(dbuf) == dbuf_capacity(dbuf) - dbuf_size(dbuf));
#ifdef HAVE_MALLOC_USABLE_SIZE
	assert(malloc_usable_size(dbuf_buffer(dbuf)) >= dbuf_capacity(dbuf));
#endif
}

int main(void)
{
	struct dbuf dbuf;
	dbuf_init(&dbuf);
	dbuf_addstr(&dbuf, "");
	assert(dbuf_size(&dbuf) == 0);
	const char *string = "agdfhgdsio89th4389fcn82fugu";
	dbuf_addstr(&dbuf, string);
	dbuf_addb(&dbuf, 0);
	assert(strcmp(dbuf_buffer(&dbuf), string) == 0);
	check_size_and_capacity(&dbuf);
	dbuf_destroy(&dbuf);
	dbuf = DBUF_INITIALIZER;
	for (const char *s = string; *s; s++) {
		dbuf_addb(&dbuf, *s);
	}
	dbuf_addb(&dbuf, 0);
	assert(strcmp(dbuf_buffer(&dbuf), string) == 0);
	check_size_and_capacity(&dbuf);
	dbuf_destroy(&dbuf);

	dbuf_init(&dbuf);
	char *p1 = dbuf_add_uninitialized(&dbuf, 123);
	assert(dbuf_size(&dbuf) == 123);
	assert(p1 == dbuf_buffer(&dbuf));
	char *p2 = dbuf_add_uninitialized(&dbuf, 123);
	assert(dbuf_size(&dbuf) == 2 * 123);
	assert(p2 == (char *)dbuf_buffer(&dbuf) + 123);
	char *p3 = dbuf_add_uninitialized(&dbuf, 0);
	assert(dbuf_size(&dbuf) == 2 * 123);
	assert(p3 == (char *)dbuf_buffer(&dbuf) + 2 * 123);
	check_size_and_capacity(&dbuf);
	void *p = dbuf_finalize(&dbuf);
#ifdef HAVE_MALLOC_USABLE_SIZE
	assert(malloc_usable_size(p) >= 2 * 123);
#endif
	memset(p, 0, 2 * 123);
	free(p);
	assert(dbuf_size(&dbuf) == 0);
	assert(dbuf_capacity(&dbuf) == 0);
	assert(dbuf_buffer(&dbuf) == NULL);

	int integers[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	dbuf_addbuf(&dbuf, integers, sizeof(integers));
	assert(dbuf_size(&dbuf) == sizeof(integers));
	check_size_and_capacity(&dbuf);
	struct dbuf dbuf2 = DBUF_INITIALIZER;
	memcpy(dbuf_add_uninitialized(&dbuf2, sizeof(integers)), integers, sizeof(integers));
	assert(dbuf_size(&dbuf2) == dbuf_size(&dbuf));
	check_size_and_capacity(&dbuf2);
	struct dbuf dbuf3 = dbuf_copy(&dbuf);
	assert(dbuf_size(&dbuf3) == dbuf_size(&dbuf));
	assert(dbuf_capacity(&dbuf3) == dbuf_capacity(&dbuf));
	assert(dbuf_buffer(&dbuf3) != dbuf_buffer(&dbuf));
	check_size_and_capacity(&dbuf3);
	assert(memcmp(dbuf_buffer(&dbuf), integers, sizeof(integers)) == 0);
	assert(memcmp(dbuf_buffer(&dbuf2), integers, sizeof(integers)) == 0);
	assert(memcmp(dbuf_buffer(&dbuf3), integers, sizeof(integers)) == 0);
	dbuf_clear(&dbuf);
	assert(dbuf_size(&dbuf) == 0);
	assert(dbuf_capacity(&dbuf) == dbuf_capacity(&dbuf3));
	dbuf_adddbuf(&dbuf, &dbuf2);
	assert(dbuf_size(&dbuf) == dbuf_size(&dbuf2));
	assert(dbuf_size(&dbuf) == dbuf_size(&dbuf3));
	assert(dbuf_capacity(&dbuf) == dbuf_capacity(&dbuf3));
	assert(memcmp(dbuf_buffer(&dbuf), integers, sizeof(integers)) == 0);
	check_size_and_capacity(&dbuf);
	check_size_and_capacity(&dbuf2);
	check_size_and_capacity(&dbuf3);
	dbuf_destroy(&dbuf);
	dbuf_destroy(&dbuf2);
	dbuf_destroy(&dbuf3);

	dbuf_init(&dbuf);
	dbuf_printf(&dbuf, "%d %s %c", 123, "abc", '!');
	check_size_and_capacity(&dbuf);
	dbuf_addb(&dbuf, 0);
	check_size_and_capacity(&dbuf);
	assert(strcmp(dbuf_buffer(&dbuf), "123 abc !") == 0);
	dbuf_destroy(&dbuf);

	dbuf_init(&dbuf);
	dbuf_reserve(&dbuf, 1000);
	assert(dbuf_size(&dbuf) == 0);
	assert(dbuf_available_size(&dbuf) >= 1000);
	check_size_and_capacity(&dbuf);
	dbuf_shrink_to_fit(&dbuf);
	check_size_and_capacity(&dbuf);
	for (size_t i = 0; i < 100; i++) {
		dbuf_reserve(&dbuf, 122);
		check_size_and_capacity(&dbuf);
		void *p4 = dbuf_add_uninitialized(&dbuf, 123);
		memset(p4, 0xab, 123);
		check_size_and_capacity(&dbuf);
		dbuf_shrink_to_fit(&dbuf);
		check_size_and_capacity(&dbuf);
		assert(dbuf_size(&dbuf) == (i + 1) * 123);
		for (size_t i = 0; i < dbuf_size(&dbuf); i++) {
			assert(((unsigned char *)dbuf_buffer(&dbuf))[i] == 0xab);
		}
	}
	size_t capacity = dbuf_capacity(&dbuf);
	size_t size = dbuf_size(&dbuf);
	dbuf_grow(&dbuf, 0);
	assert(dbuf_capacity(&dbuf) == capacity);
	assert(dbuf_size(&dbuf) == size);
	check_size_and_capacity(&dbuf);
	dbuf_grow(&dbuf, 123);
	assert(dbuf_capacity(&dbuf) >= capacity + 123);
	assert(dbuf_size(&dbuf) == size);
	check_size_and_capacity(&dbuf);
	dbuf_resize(&dbuf, capacity);
	assert(dbuf_capacity(&dbuf) >= capacity);
	assert(dbuf_size(&dbuf) == size);
	check_size_and_capacity(&dbuf);
	dbuf_resize(&dbuf, size - 1);
	assert(dbuf_size(&dbuf) == size - 1);
	check_size_and_capacity(&dbuf);
	dbuf_truncate(&dbuf, size);
	assert(dbuf_size(&dbuf) == size - 1);
	check_size_and_capacity(&dbuf);
	dbuf_truncate(&dbuf, 234);
	assert(dbuf_size(&dbuf) == 234);
	check_size_and_capacity(&dbuf);
	dbuf_shrink_to_fit(&dbuf);
	assert(dbuf_size(&dbuf) == 234);
	check_size_and_capacity(&dbuf);
	for (size_t i = 0; i < dbuf_size(&dbuf); i++) {
		assert(((unsigned char *)dbuf_buffer(&dbuf))[i] == 0xab);
	}
	dbuf_destroy(&dbuf);

	return 0;
}
