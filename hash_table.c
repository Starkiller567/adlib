#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <x86intrin.h>
#include "hash_table.h"

static inline unsigned int integer_hash(unsigned int key)
{
	return key * 2654435761;
}

static inline bool integers_equal(unsigned int key1, unsigned int key2)
{
	return key1 == key2;
}

static unsigned long string_hash(const void *string)
{
	const void *data = string;
	const size_t nbytes = strlen(string);
	if (data == NULL || nbytes == 0) {
		return 0;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *)data;
	const uint8_t *tail = (const uint8_t *)data + (nblocks * 4);

	uint32_t h = 0;

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
	case 3:
		k ^= tail[2] << 16;
	case 2:
		k ^= tail[1] << 8;
	case 1:
		k ^= tail[0];
		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;
		h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static bool strings_equal(const char *s1, const char *s2)
{
	return strcmp(s1, s2) == 0;
}

struct short_string {
	char s[16];
};

HASHTABLE_IMPLEMENTATION(itable, unsigned int, unsigned int, integer_hash, integers_equal, 8)
HASHTABLE_IMPLEMENTATION(stable, char *, char *, string_hash, strings_equal, 8)
HASHTABLE_IMPLEMENTATION(sstable, char *, struct short_string, string_hash, strings_equal, 8)

int main(void)
{
	struct itable itable;
	struct stable stable;
	struct sstable sstable;
	unsigned int i, num_items;
	unsigned long long time;

#if 1
	itable_init(&itable, 64);
	num_items = 100000;

	time = __rdtsc();
	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_insert(&itable, i);
		*item = i;
	}
	assert(itable.num_items == num_items);

	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_lookup(&itable, i);
		assert(*item == i);
		bool success = itable_remove(&itable, i);
		assert(success);
		item = itable_lookup(&itable, i);
		assert(!item);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);

	itable_destroy(&itable);
#endif

#if 1
	stable_init(&stable, 4);

	num_items = 10000;

	time = __rdtsc();
	for (i = 0; i < num_items; i++) {
		char *key = malloc(16);
		char *data = malloc(16);
		sprintf(data, "data%u", i);
		sprintf(key, "key%u", i);
		char **item = stable_insert(&stable, key);
		*item = data;
	}

	for (i = 0; i < num_items; i++) {
		char **s;
		char key[16];
		char data[16];
		sprintf(key, "key%u", i);
		sprintf(data, "data%u", i);
		s = stable_lookup(&stable, key);
		assert(strcmp(*s, data) == 0);
		stable_remove(&stable, key);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);
	stable_destroy(&stable);
#endif

#if 1
	sstable_init(&sstable, 4);

	num_items = 100000;

	time = __rdtsc();
	for (i = 0; i < num_items; i++) {
		char *key = malloc(16);
		sprintf(key, "key%u", i);
		struct short_string *item = sstable_insert(&sstable, key);
		sprintf(item->s, "data%u", i);
	}

	for (i = 0; i < num_items; i++) {
		char key[16];
		char data[16];
		sprintf(key, "key%u", i);
		sprintf(data, "data%u", i);
		struct short_string *item = sstable_lookup(&sstable, key);
		assert(strcmp(item->s, data) == 0);
		sstable_remove(&sstable, key);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);
	sstable_destroy(&sstable);
#endif

#if 1
	itable_init(&itable, 32);
	num_items = 100000;
	time = __rdtsc();
	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_insert(&itable, i);
		*item = i;
	}
	for (i = 0; i < num_items; i++) {
		itable_lookup(&itable, i);
	}
	for (i = 0; i < num_items; i++) {
		itable_remove(&itable, i);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);
	itable_destroy(&itable);
#endif
}
