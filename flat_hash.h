#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, THRESHOLD, ...)

#define THRESHOLD 8

typedef int key_type;
typedef int item_type;

struct itable_bucket {
	unsigned int hash;
	key_type key;
};

struct itable {
	unsigned int num_items;
	unsigned int num_tombstones;
	unsigned int size;
	struct itable_bucket *buckets;
	item_type *items;
};

static inline unsigned int __itable_hash_to_idx(unsigned int hash, unsigned int table_size)
{
	return hash & (table_size - 1);
}

static inline unsigned int __itable_get_hash(struct itable *table, unsigned int index)
{
	return table->buckets[index].hash;
}

static inline void __itable_set_hash(struct itable *table, unsigned int index, unsigned int hash)
{
	table->buckets[index].hash = hash;
}

static inline key_type *__itable_key(struct itable *table, unsigned int index)
{
	return &table->buckets[index].key;
}

static inline item_type *__itable_item(struct itable *table, unsigned int index)
{
	return &table->items[index];
}

static inline unsigned int __itable_probe_index(struct itable *table, unsigned int start, unsigned int i)
{
	return (start + i) & (table->size - 1);
}

static void itable_init(struct itable *table, unsigned int size)
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
	table->buckets = (struct itable_bucket *)mem;
	mem += buckets_size;
	table->items = (item_type *)mem;
	for (unsigned int i = 0; i < size; i++) {
		__itable_set_hash(table, i, 0);
	}
	table->num_items = 0;
	table->num_tombstones = 0;
	table->size = size;
}

static void itable_destroy(struct itable *table)
{
	free(table->buckets);
	table->size = 0;
}

static unsigned int __itable_lookup_internal(struct itable *table, key_type key, unsigned int hash)
{
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;

	unsigned int start = __itable_hash_to_idx(hash, table->size);
	for (unsigned int i = 0;; i++) {
		unsigned int index = __itable_probe_index(table, start, i);
		if (__itable_get_hash(table, index) == 0) {
			break;
		}
		key_type const *a = &key;
		key_type const *b = __itable_key(table, index);
		if (hash == __itable_get_hash(table, index) && (*a == *b)) {
			return index;
		}
	}
	return -1;
}

static item_type *itable_lookup(struct itable *table, key_type key, unsigned int hash)
{
	unsigned int index = __itable_lookup_internal(table, key, hash);
	if (index == -1) {
		return NULL;
	}
	return __itable_item(table, index);
}

static item_type *__itable_insert_internal(struct itable *table, key_type *key, unsigned int hash)
{
	unsigned int start = __itable_hash_to_idx(hash, table->size);
	unsigned int index;
	for (unsigned int i = 0; ; i++) {
		assert(i < table->size);
		index = __itable_probe_index(table, start, i);
		if (__itable_get_hash(table, index) == 0) {
			break;
		}

		if (__itable_get_hash(table, index) == 1) {
			table->num_tombstones--;
			break;
		}
	}

	__itable_set_hash(table, index, hash);
	*__itable_key(table, index) = *key;

	return __itable_item(table, index);
}

static void itable_resize(struct itable *table, unsigned int size)
{
	struct itable new_table;
	itable_init(&new_table, size);

	for (unsigned int index = 0; index < table->size; index++) {
		unsigned int hash = __itable_get_hash(table, index);
		if (hash != 0 && hash != 1) {
			item_type *item = __itable_insert_internal(&new_table, __itable_key(table, index), hash);
			*item = *__itable_item(table, index);
		}
	}
	new_table.num_items = table->num_items;
	itable_destroy(table);
	*table = new_table;
}

static item_type *itable_insert(struct itable *table, key_type key, unsigned int hash)
{
	table->num_items++;
	if ((table->num_items + table->num_tombstones) * 10 > THRESHOLD * table->size) {
		itable_resize(table, 2 * table->size);
	}
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;
	return __itable_insert_internal(table, &key, hash);
}

static bool itable_remove(struct itable *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item)
{
	unsigned int index = __itable_lookup_internal(table, key, hash);
	if (index == -1) {
		return false;
	}

	if (ret_key) {
		*ret_key = *__itable_key(table, index);
	}
	if (ret_item) {
		*ret_item = *__itable_item(table, index);
	}
	__itable_set_hash(table, index, 1);
	table->num_items--;
	table->num_tombstones++;
	// if (table->num_tombstones > table->size / 2) {
	// 	itable_resize(table, table->size);
	// }
	return true;
}

#endif
