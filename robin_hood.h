#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#ifndef MAX_DIST
# define MAX_DIST 3
#endif

#define THRESHOLD 8
#define key_type char *
#define item_type char *
#define compute_hash string_hash
#define keys_equal strings_equal
static bool strings_equal(const char *s1, const char *s2);
static unsigned long string_hash(const void *string);

#define DEFINE_HASHTABLE(name, key_type, item_type, compute_hash, keys_equal, THRESHOLD)

struct stable_bucket {
	unsigned int hash;
	key_type key;
};

struct stable {
	unsigned int num_items;
	unsigned int size;
	struct stable_bucket *buckets;
	item_type *items;
};

static inline unsigned int __stable_hash_to_idx(unsigned int hash, unsigned int table_size)
{
	return hash & (table_size - 1);
}

static void stable_init(struct stable *table, unsigned int size)
{
	if ((size & (size - 1)) != 0) {
		/* round to next power of 2 */
		size--;
		size |= size >> 1;
		size |= size >> 2;
		size |= size >> 4;
		size |= size >> 8;
		size |= size >> 16;
		/* size |= size >> 32; */
		size++;
	}
	size_t buckets_size = size * sizeof(*table->buckets);
	size_t items_size = size * sizeof(*table->items);
	char *mem = malloc(buckets_size + items_size);
	table->buckets = (struct stable_bucket *)mem;
	mem += buckets_size;
	table->items = (item_type *)mem;
	memset(table->buckets, 0, buckets_size);
	table->num_items = 0;
	table->size = size;
}

static void stable_destroy(struct stable *table)
{
	free(table->buckets);
	table->size = 0;
}

static inline unsigned int __stable_compute_dist(struct stable *table, unsigned int index,
                                                 unsigned int hash)
{
	unsigned int pref_index = __stable_hash_to_idx(hash, table->size);
	unsigned int dist = (index - pref_index + table->size) & (table->size - 1);
	assert(dist <= MAX_DIST);
	return dist;
}

static item_type *stable_lookup(struct stable *table, key_type key)
{
	unsigned int hash = compute_hash(key);
	hash = hash == 0 ? 1 : hash;
	unsigned int start_index = __stable_hash_to_idx(hash, table->size);
	unsigned int index = start_index;
	for (unsigned int dist = 0; dist <= MAX_DIST; dist++) {
		struct stable_bucket *bucket = &table->buckets[index];
		if (hash == bucket->hash && keys_equal(key, bucket->key)) {
			return &table->items[index];
		}
		if (hash != 0) __stable_compute_dist(table, index, bucket->hash);
		index = (index + 1) & (table->size - 1);
	}
	return NULL;
}

static bool stable_remove(struct stable *table, key_type key, key_type *pkey, item_type *pitem)
{
	unsigned int hash = compute_hash(key);
	hash = hash == 0 ? 1 : hash;
	unsigned int index = __stable_hash_to_idx(hash, table->size);
	struct stable_bucket *bucket;
	for (unsigned int dist = 0; dist <= MAX_DIST; dist++) {
		bucket = &table->buckets[index];
		if (hash != 0) __stable_compute_dist(table, index, bucket->hash);
		if (hash == bucket->hash && keys_equal(key, bucket->key)) {
			if (pkey) {
				*pkey = bucket->key;
			}
			if (pitem) {
				*pitem = table->items[index];
			}
			bucket->hash = 0;
			table->num_items--;
			return true;
		}
		index = (index + 1) & (table->size - 1);
	}
	return false;
}

static item_type *__stable_insert_internal(struct stable *table, key_type key, unsigned int hash);

static void stable_resize(struct stable *table, unsigned int size)
{
	unsigned int min_size = (table->num_items * 10 + THRESHOLD - 1) / THRESHOLD;
	if (size < min_size) {
		size = min_size;
	}
	if (size == table->size) {
		return;
	}
	struct stable new_table;
	stable_init(&new_table, size);

	for (unsigned int i = 0; i < table->size; i++) {
		struct stable_bucket *bucket = &table->buckets[i];
		if (bucket->hash != 0) {
			item_type *item = __stable_insert_internal(&new_table, bucket->key,
			                                           bucket->hash);
			*item = table->items[i];
		}
	}
	new_table.num_items = table->num_items;
	stable_destroy(table);
	*table = new_table;
}

static void __stable_move(struct stable *table, unsigned int index)
{
	item_type swap_item = table->items[index];
	struct stable_bucket swap_bucket = table->buckets[index];
retry:
	index = __stable_hash_to_idx(swap_bucket.hash, table->size);
	unsigned int dist = 0;
	for (;;) {
		struct stable_bucket *bucket = &table->buckets[index];
		if (bucket->hash == 0) {
			break;
		}
		unsigned int bucket_dist = __stable_compute_dist(table, index, bucket->hash);
		if (bucket_dist < dist) {
			item_type tmp_item = swap_item;
			swap_item = table->items[index];
			table->items[index] = tmp_item;

			struct stable_bucket tmp_bucket = swap_bucket;
			swap_bucket = *bucket;
			*bucket = tmp_bucket;
			goto retry;
		} else if (dist >= MAX_DIST) {
			stable_resize(table, 2 * table->size);
			goto retry;
		}

		index = (index + 1) & (table->size - 1);
		dist++;
	}

	table->items[index] = swap_item;
	table->buckets[index] = swap_bucket;
}

static item_type *__stable_insert_internal(struct stable *table, key_type key, unsigned int hash)
{
	assert(table->num_items * 10 <= THRESHOLD * table->size);

retry:;
	unsigned int index = __stable_hash_to_idx(hash, table->size);
	unsigned int dist = 0;

	struct stable_bucket *bucket;
	for (;;) {
		bucket = &table->buckets[index];
		if (bucket->hash == 0) {
			break;
		}
		unsigned int bucket_dist = __stable_compute_dist(table, index, bucket->hash);
		if (bucket_dist < dist) {
			__stable_move(table, index);
			break;
		} else if (dist >= MAX_DIST) {
			stable_resize(table, 2 * table->size);
			goto retry;
		}
		index = (index + 1) & (table->size - 1);
		dist++;
	}

	bucket->hash = hash;
	bucket->key = key;
	__stable_compute_dist(table, index, bucket->hash);

	return &table->items[index];
}

static item_type *stable_insert(struct stable *table, key_type key)
{
	table->num_items++;
	if (table->num_items * 10 > THRESHOLD * table->size) {
		stable_resize(table, 2 * table->size);
	}

	unsigned int hash = compute_hash(key);
	hash = hash == 0 ? 1 : hash;
	return __stable_insert_internal(table, key, hash);
}

#endif
