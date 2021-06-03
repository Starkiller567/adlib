#include <stdio.h>
#include "dbuf.h"

int main(void)
{
	struct dbuf dbuf;
	dbuf_init(&dbuf);
	const char *str = "ABCDEFGHIHKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	dbuf_addstr(&dbuf, str);
	struct dbuf dbuf2 = dbuf_copy(&dbuf);
	dbuf_addbuf(&dbuf, &dbuf2);
	dbuf_free(&dbuf2);
	dbuf_truncate(&dbuf, dbuf_size(&dbuf) / 2 + 1);
	dbuf_addb(&dbuf, 0);
	dbuf_shrink_to_fit(&dbuf);
	puts(dbuf_buffer(&dbuf));
	dbuf_truncate(&dbuf, 0);
	dbuf_printf(&dbuf, "size = %lu, capacity = %lu\n", dbuf_size(&dbuf), dbuf_capacity(&dbuf));
	dbuf_printf(&dbuf, "size = %lu, capacity = %lu\n", dbuf_size(&dbuf), dbuf_capacity(&dbuf));
	dbuf_printf(&dbuf, "size = %lu, capacity = %lu\n", dbuf_size(&dbuf), dbuf_capacity(&dbuf));
	dbuf_addb(&dbuf, 0);
	dbuf_add_uninitialized(&dbuf, 1000);
	printf("%s", (char *)dbuf_buffer(&dbuf));
	printf("size = %lu, capacity = %lu\n", dbuf_size(&dbuf), dbuf_capacity(&dbuf));
	dbuf_free(&dbuf);
	return 0;
}
