#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, compute_hash, keys_equal, MAX_DIST)	\
									\
	struct name##_bucket {						\
		unsigned int hash;					\
		key_type key;						\
	};								\
									\
	struct name {							\
		unsigned int num_items;					\
		unsigned int size;					\
		struct name##_bucket *buckets;				\
		item_type *items;					\
	};								\
									\
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{								\
		return hash & (table_size - 1);				\
	}								\
									\
	static void name##_init(struct name *table, unsigned int size)	\
	{								\
		if ((size & (size - 1)) != 0) {				\
			/* round to next power of 2 */			\
			size--;						\
			size |= size >> 1;				\
			size |= size >> 2;				\
			size |= size >> 4;				\
			size |= size >> 8;				\
			size |= size >> 16;				\
			/* size |= size >> 32; */			\
			size++;						\
		}							\
		/* size_t indices_size = size * sizeof(*table->indices); */ \
		size_t buckets_size = size * sizeof(*table->buckets);	\
		size_t items_size = size * sizeof(*table->items);	\
		char *mem = malloc(buckets_size + items_size);		\
		/* table->indices = (unsigned int *)mem; */		\
		/* mem += indices_size; */				\
		table->buckets = (struct name##_bucket *)mem;		\
		mem += buckets_size;					\
		table->items = (item_type *)mem;			\
		/* memset(table->indices, 0xff, indices_size); */	\
		/* memset(table->buckets, 0, buckets_size); */		\
		for (unsigned int i = 0; i < size; i++) {		\
			table->buckets[i].hash = 0;			\
		}							\
		table->num_items = 0;					\
		table->size = size;					\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(table->buckets);					\
		table->size = 0;					\
	}								\
									\
	static item_type *name##_lookup(struct name *table, key_type key) \
	{								\
		unsigned int hash = compute_hash(key);			\
		hash = hash == 0 ? -1 : hash;				\
		unsigned int index = __##name##_hash_to_idx(hash, table->size);	\
		for (unsigned int i = 0; i < 2 * MAX_DIST; i++) {		\
			struct name##_bucket *bucket = &table->buckets[index]; \
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				return &table->items[index];		\
			}						\
			index = (index + 1) & (table->size - 1);	\
		}							\
		return NULL;						\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, key_type *ret_key, item_type *ret_item) \
	{								\
		unsigned int hash = compute_hash(key);			\
		hash = hash == 0 ? -1 : hash;				\
		unsigned int index = __##name##_hash_to_idx(hash, table->size);	\
		struct name##_bucket *bucket;				\
		for (unsigned int i = 0; i < 2 * MAX_DIST; i++) {		\
			bucket = &table->buckets[index];		\
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				break;					\
			}						\
			index = (index + 1) & (table->size - 1);	\
		}							\
		if (ret_key) {						\
			*ret_key = bucket->key;				\
		}							\
		if (ret_item) {						\
			*ret_item = table->items[index];		\
		}							\
		bucket->hash = 0;					\
		table->num_items--;					\
		return true;						\
	}								\
									\
	static item_type *__##name##_insert_internal(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_hash_to_idx(hash, table->size);	\
		struct name##_bucket *bucket;				\
		unsigned int i;						\
		for (i = 0; i < 2 * MAX_DIST; i++) {			\
			bucket = &table->buckets[index];		\
			if (bucket->hash == 0) {			\
				break;					\
			}						\
			index = (index + 1) & (table->size - 1);	\
		}							\
									\
		if (i == 2 * MAX_DIST) {					\
			return NULL;					\
		}							\
									\
		bucket->hash = hash;					\
		bucket->key = key;					\
									\
		return &table->items[index];				\
	}								\
									\
	static bool name##_resize(struct name *table, unsigned int size) \
	{								\
		if (size == table->size) {				\
			return true;					\
		}							\
		struct name new_table;					\
		name##_init(&new_table, size);				\
									\
		for (unsigned int i = 0; i < table->size; i++) {	\
			struct name##_bucket *bucket = &table->buckets[i]; \
			if (bucket->hash != 0) {			\
				item_type *item = __##name##_insert_internal(&new_table, bucket->key, \
									     bucket->hash); \
				if (!item) {				\
					name##_destroy(&new_table);	\
					return false;			\
				}					\
				*item = table->items[i];		\
			}						\
		}							\
		new_table.num_items = table->num_items;			\
		name##_destroy(table);					\
		*table = new_table;					\
		return true;						\
	}								\
									\
	static item_type *name##_insert(struct name *table, key_type key) \
	{								\
		unsigned int hash = compute_hash(key);			\
		hash = hash == 0 ? -1 : hash;				\
		table->num_items++;					\
		for (;;) {						\
			item_type *item = __##name##_insert_internal(table, key, hash);	\
			if (item) {					\
				return item;				\
			}						\
			name##_resize(table, 2 * table->size);		\
		}							\
	}

#endif
