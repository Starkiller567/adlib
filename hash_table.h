#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define HASHTABLE_IMPLEMENTATION(name, key_type, item_type, compute_hash, keys_equal, THRESHOLD) \
	  \
	static const unsigned int name##_INVALID_INDEX = -1; \
	  \
	struct name##_bucket { \
		unsigned int first; \
		unsigned int hash; \
		unsigned int next; \
		key_type key; \
	}; \
	  \
	struct name { \
		unsigned int num_items; \
		unsigned int size; \
		struct name##_bucket *list_entries; \
		item_type *items; \
	}; \
	  \
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{ \
		return hash % table_size; \
	} \
	  \
	static void name##_init(struct name *table, unsigned int size) \
	{ \
		/* size_t indices_size = size * sizeof(*table->indices); */ \
		size_t list_entries_size = size * sizeof(*table->list_entries); \
		size_t items_size = size * sizeof(*table->items); \
		char *mem = malloc(list_entries_size + items_size); \
		/* table->indices = (unsigned int *)mem; */ \
		/* mem += indices_size; */ \
		table->list_entries = (struct name##_bucket *)mem; \
		mem += list_entries_size; \
		table->items = (item_type *)mem; \
		/* memset(table->indices, 0xff, indices_size); */ \
		/* memset(table->list_entries, 0, list_entries_size); */ \
		for (unsigned int i = 0; i < size; i++) { \
			table->list_entries[i].first = name##_INVALID_INDEX; \
			table->list_entries[i].hash = 0; \
		} \
		table->num_items = 0; \
		table->size = size; \
	} \
	  \
	static void name##_destroy(struct name *table) \
	{ \
		free(table->list_entries); \
		table->size = 0; \
	} \
	  \
	static item_type *name##_lookup(struct name *table, key_type key) \
	{ \
		unsigned int hash = compute_hash(key); \
		hash = hash == 0 ? 1 : hash; \
		unsigned int index = table->list_entries[__##name##_hash_to_idx(hash, table->size)].first; \
		for (;;) { \
			if (index == name##_INVALID_INDEX) { \
				return NULL; \
			} \
			struct name##_bucket *bucket = &table->list_entries[index]; \
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				return &table->items[index]; \
			} \
			index = bucket->next; \
		} \
	} \
	  \
	static bool name##_remove(struct name *table, key_type key) \
	{ \
		unsigned int hash = compute_hash(key); \
		hash = hash == 0 ? 1 : hash; \
		unsigned int *indirect = &table->list_entries[__##name##_hash_to_idx(hash, table->size)].first; \
		unsigned int index = *indirect; \
		struct name##_bucket *bucket; \
		for (;;) { \
			if (index == name##_INVALID_INDEX) { \
				return false; \
			} \
			bucket = &table->list_entries[index]; \
			if (hash == bucket->hash && keys_equal(key, bucket->key)) { \
				break; \
			} \
			indirect = &bucket->next; \
			index = *indirect; \
		} \
		*indirect = bucket->next; \
		bucket->hash = 0; \
		table->num_items--; \
		return true; \
	} \
	  \
	static item_type *__##name##_insert_internal(struct name *table, key_type key, unsigned int hash) \
	{ \
		assert(table->num_items * 10 <= THRESHOLD * table->size); \
		unsigned int index = __##name##_hash_to_idx(hash, table->size); \
	  \
		unsigned int *indirect = &table->list_entries[index].first; \
		unsigned int next = name##_INVALID_INDEX; \
		struct name##_bucket *bucket; \
		for (;;) { \
			bucket = &table->list_entries[index]; \
			if (bucket->hash == 0) { \
				break; \
			} \
			index = (index + 1) % table->size; \
			if (index > *indirect) { \
				struct name##_bucket *prev = &table->list_entries[*indirect]; \
				indirect = &prev->next; \
				next = prev->next; \
			} \
		} \
	  \
		bucket->hash = hash; \
		bucket->next = next; \
		bucket->key = key; \
		*indirect = index; \
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
			struct name##_bucket *bucket = &table->list_entries[i]; \
			if (bucket->hash != 0) { \
				item_type *item = __##name##_insert_internal(&new_table, bucket->key, \
				                                             bucket->hash); \
				memcpy(item, &table->items[i], sizeof(item_type)); \
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
