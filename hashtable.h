#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, THRESHOLD, ...)	\
									\
	struct name##_bucket {						\
		unsigned int hash;					\
		key_type key;						\
	};								\
									\
	struct name {							\
		unsigned int num_items;					\
		unsigned int num_tombstones;				\
		unsigned int size;					\
		void *mem;						\
	};								\
									\
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{								\
		return hash & (table_size - 1);				\
	}								\
									\
	static inline unsigned int __##name##_get_hash(struct name *table, unsigned int index) \
	{								\
		return ((struct name##_bucket *)table->mem)[index].hash; \
	}								\
									\
	static inline void __##name##_set_hash(struct name *table, unsigned int index, unsigned int hash) \
	{								\
		((struct name##_bucket *)table->mem)[index].hash = hash; \
	}								\
									\
	static inline key_type *__##name##_key(struct name *table, unsigned int index) \
	{								\
		return &((struct name##_bucket *)table->mem)[index].key; \
	}								\
									\
	static inline item_type *__##name##_item(struct name *table, unsigned int index) \
	{								\
		return &((item_type *)((unsigned char *)table->mem + table->size * sizeof(struct name##_bucket)))[index]; \
	}								\
									\
	static inline unsigned int __##name##_probe_index(struct name *table, unsigned int start, unsigned int i) \
	{								\
		return (start + i) & (table->size - 1);			\
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
		size_t buckets_size = size * sizeof(struct name##_bucket); \
		size_t items_size = size * sizeof(item_type);		\
		table->mem = aligned_alloc(64, buckets_size + items_size); \
		for (unsigned int i = 0; i < size; i++) {		\
			__##name##_set_hash(table, i, 0);		\
		}							\
		table->num_items = 0;					\
		table->num_tombstones = 0;				\
		table->size = size;					\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(table->mem);					\
		table->size = 0;					\
	}								\
									\
	static unsigned int __##name##_lookup_internal(struct name *table, key_type key, unsigned int hash) \
	{								\
		hash = hash == 0 || hash == 1 ? hash - 2 : hash;	\
									\
		unsigned int start = __##name##_hash_to_idx(hash, table->size);	\
		for (unsigned int i = 0;; i++) {			\
			unsigned int index = __##name##_probe_index(table, start, i); \
			if (__##name##_get_hash(table, index) == 0) {	\
				break;					\
			}						\
			key_type const *a = &key;			\
			key_type const *b = __##name##_key(table, index); \
			if (hash == __##name##_get_hash(table, index) && (__VA_ARGS__)) { \
				return index;				\
			}						\
		}							\
		return -1;						\
	}								\
									\
	static item_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_lookup_internal(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return __##name##_item(table, index);			\
	}								\
									\
	static item_type *__##name##_insert_internal(struct name *table, key_type *key, unsigned int hash) \
	{								\
		unsigned int start = __##name##_hash_to_idx(hash, table->size);	\
		unsigned int index;					\
		for (unsigned int i = 0; ; i++) {			\
			assert(i < table->size);			\
			index = __##name##_probe_index(table, start, i); \
			if (__##name##_get_hash(table, index) == 0) {	\
				break;					\
			}						\
									\
			if (__##name##_get_hash(table, index) == 1) {	\
				table->num_tombstones--;		\
				break;					\
			}						\
		}							\
									\
		__##name##_set_hash(table, index, hash);		\
		*__##name##_key(table, index) = *key;			\
									\
		return __##name##_item(table, index);			\
	}								\
									\
	static void name##_resize(struct name *table, unsigned int size) \
	{								\
		struct name new_table;					\
		name##_init(&new_table, size);				\
									\
		for (unsigned int index = 0; index < table->size; index++) { \
			unsigned int hash = __##name##_get_hash(table, index); \
			if (hash != 0 && hash != 1) {			\
				item_type *item = __##name##_insert_internal(&new_table, __##name##_key(table, index), hash); \
				*item = *__##name##_item(table, index);	\
			}						\
		}							\
		new_table.num_items = table->num_items;			\
		name##_destroy(table);					\
		*table = new_table;					\
	}								\
									\
	static item_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		table->num_items++;					\
		if ((table->num_items + table->num_tombstones) * 10 > THRESHOLD * table->size) { \
			name##_resize(table, 2 * table->size);		\
		}							\
		hash = hash == 0 || hash == 1 ? hash - 2 : hash;	\
		return __##name##_insert_internal(table, &key, hash);	\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item) \
	{								\
		unsigned int index = __##name##_lookup_internal(table, key, hash); \
		if (index == -1) {					\
			return false;					\
		}							\
									\
		if (ret_key) {						\
			*ret_key = *__##name##_key(table, index);	\
		}							\
		if (ret_item) {						\
			*ret_item = *__##name##_item(table, index);	\
		}							\
		__##name##_set_hash(table, index, 1);			\
		table->num_items--;					\
		table->num_tombstones++;				\
		if (10 * table->num_items < table->size) {		\
			name##_resize(table, table->size / 2);		\
		} else if (table->num_tombstones > table->size / 4) {	\
			name##_resize(table, table->size);		\
		}							\
		return true;						\
	}								\

#endif
