#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, compute_hash, keys_equal, THRESHOLD) \
	  \
	static const unsigned int name##_INVALID_INDEX = -1; \
	  \
	struct name##_bucket { \
		unsigned int hash; \
		key_type key; \
	}; \
	  \
	struct name { \
		unsigned int num_items; \
		unsigned int size; \
		struct name##_bucket *buckets; \
		item_type *items; \
	}; \
	  \
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{ \
		return hash & (table_size - 1); \
	} \
	  \
	static void name##_init(struct name *table, unsigned int size) \
	{ \
		size_t buckets_size = size * sizeof(*table->buckets); \
		size_t items_size = size * sizeof(*table->items); \
		char *mem = malloc(buckets_size + items_size); \
		table->buckets = (struct name##_bucket *)mem; \
		mem += buckets_size; \
		table->items = (item_type *)mem; \
		memset(table->buckets, 0, buckets_size); \
		table->num_items = 0; \
		table->size = size; \
	} \
	  \
	static void name##_destroy(struct name *table) \
	{ \
		free(table->buckets); \
		table->size = 0; \
	} \
	  \
	static item_type *name##_lookup(struct name *table, key_type key) \
	{ \
		unsigned int hash = compute_hash(key); \
		hash = hash == 0 ? 1 : hash; \
		unsigned int start_index = __##name##_hash_to_idx(hash, table->size);	\
		unsigned int index = start_index; \
		for (;;) { \
			struct name##_bucket *bucket = &table->buckets[index]; \
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				return &table->items[index]; \
			} \
			index = (index + 1) & (table->size - 1); \
			if (index == start_index) { \
				return NULL; \
			} \
		} \
	} \
	  \
	static bool name##_remove(struct name *table, key_type key, key_type *pkey, item_type *pitem) \
	{ \
		unsigned int hash = compute_hash(key); \
		hash = hash == 0 ? 1 : hash; \
		unsigned int index = __##name##_hash_to_idx(hash, table->size); \
		struct name##_bucket *bucket; \
		for (;;) { \
			bucket = &table->buckets[index]; \
			if (bucket->hash == 0) { \
				return false; \
			} \
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				break; \
			} \
			index = (index + 1) & (table->size - 1); \
		} \
		if (pkey) { \
			*pkey = bucket->key; \
		} \
		if (pitem) { \
			*pitem = table->items[index]; \
		} \
		bucket->hash = 0; \
		table->num_items--; \
		return true; \
	} \
	  \
	static inline unsigned int __##name##_compute_dist(struct name *table, unsigned int index, \
	                                                   unsigned int hash)	\
	{ \
		unsigned int pref_index = __##name##_hash_to_idx(hash, table->size); \
		unsigned int dist = pref_index - index; \
		if (index > pref_index) { \
			dist = index - pref_index; \
		} \
		return dist; \
	} \
	  \
	static item_type *__##name##_insert_internal(struct name *table, key_type key, unsigned int hash) \
	{ \
		assert(table->num_items * 10 <= THRESHOLD * table->size); \
		unsigned int index = __##name##_hash_to_idx(hash, table->size); \
		unsigned int dist = 0; \
	  \
		struct name##_bucket *bucket; \
		for (;;) { \
			bucket = &table->buckets[index]; \
			if (bucket->hash == 0) { \
				break; \
			} \
			unsigned int bucket_dist = __##name##_compute_dist(table, index, bucket->hash); \
			if (bucket_dist < dist) { \
				item_type *item = __##name##_insert_internal(table, bucket->key, bucket->hash); \
				*item = table->items[index]; \
				break; \
			} \
			index = (index + 1) & (table->size - 1); \
			dist++; \
		} \
	  \
		bucket->hash = hash; \
		bucket->key = key; \
	  \
		return &table->items[index]; \
	} \
	  \
	static void name##_resize(struct name *table, unsigned int size) \
	{ \
		unsigned int min_size = (table->num_items * 10 + THRESHOLD - 1) / THRESHOLD; \
		if (size < min_size) { \
			size = min_size; \
		} \
		if (size == table->size) { \
			return; \
		} \
		struct name new_table; \
		name##_init(&new_table, size); \
	  \
		for (unsigned int i = 0; i < table->size; i++) { \
			struct name##_bucket *bucket = &table->buckets[i]; \
			if (bucket->hash != 0) { \
				item_type *item = __##name##_insert_internal(&new_table, bucket->key,	\
				                                             bucket->hash); \
				*item = table->items[i]; \
			} \
		} \
		new_table.num_items = table->num_items; \
		name##_destroy(table); \
		*table = new_table; \
	} \
	  \
	static item_type *name##_insert(struct name *table, key_type key) \
	{ \
		table->num_items++; \
		if (table->num_items * 10 > THRESHOLD * table->size) { \
			name##_resize(table, 2 * table->size); \
		} \
	  \
		unsigned int hash = compute_hash(key); \
		hash = hash == 0 ? 1 : hash; \
		return __##name##_insert_internal(table, key, hash); \
	}

#endif
