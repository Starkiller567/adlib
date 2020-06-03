#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, THRESHOLD, ...)		\
									\
	struct __##name##_bucket {					\
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
	static void name##_resize(struct name *table, unsigned int size); \
									\
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{								\
		return hash & (table_size - 1);				\
	}								\
									\
	static inline unsigned int __##name##_get_hash(struct name *table, unsigned int index) \
	{								\
		return ((struct __##name##_bucket *)table->mem)[index].hash; \
	}								\
									\
	static inline void __##name##_set_hash(struct name *table, unsigned int index, unsigned int hash) \
	{								\
		((struct __##name##_bucket *)table->mem)[index].hash = hash; \
	}								\
									\
	static inline key_type *__##name##_key(struct name *table, unsigned int index) \
	{								\
		return &((struct __##name##_bucket *)table->mem)[index].key; \
	}								\
									\
	static inline unsigned int __##name##_probe_index(struct name *table, unsigned int start, unsigned int i) \
	{								\
		return (start + i) & (table->size - 1);			\
	}								\
									\
	static void *__##name##_table_memory(struct name *table)	\
	{								\
		return table->mem;					\
	}								\
									\
	static size_t __##name##_table_memory_bytes(unsigned int size)	\
	{								\
		return size * sizeof(struct __##name##_bucket);		\
	}								\
									\
	static void __##name##_table_init(struct name *table, void *mem, unsigned int size) \
	{								\
		table->mem = mem;					\
		for (unsigned int i = 0; i < size; i++) {		\
			__##name##_set_hash(table, i, 0);		\
		}							\
		table->num_items = 0;					\
		table->num_tombstones = 0;				\
		table->size = size;					\
	}								\
									\
	static void __##name##_table_destroy(struct name *table)	\
	{								\
		table->mem = NULL;					\
	}								\
									\
	static unsigned int __##name##_table_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		hash = hash == 0 || hash == 1 ? hash - 2 : hash;	\
									\
		unsigned int start = __##name##_hash_to_idx(hash, table->size); \
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
	static unsigned int __##name##_get_next(struct name *table, size_t start) \
	{								\
		for (unsigned int index = start; index < table->size; index++) { \
			unsigned int hash = __##name##_get_hash(table, index); \
			if (hash != 0 && hash != 1) {			\
				return index;				\
			}						\
		}							\
		return table->size;					\
	}								\
									\
	static unsigned int __##name##_table_do_insert(struct name *table, key_type *key, unsigned int hash) \
	{								\
		unsigned int start = __##name##_hash_to_idx(hash, table->size); \
		unsigned int index;					\
		for (unsigned int i = 0; ; i++) {			\
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
		return index;						\
	}								\
									\
	static unsigned int __##name##_table_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		table->num_items++;					\
		if ((table->num_items + table->num_tombstones) * 10 > THRESHOLD * table->size) { \
			name##_resize(table, 2 * table->size);		\
		}							\
		hash = hash == 0 || hash == 1 ? hash - 2 : hash;	\
		return __##name##_table_do_insert(table, &key, hash);	\
	}								\
									\
	static bool __##name##_table_remove(struct name *table, unsigned int index) \
	{								\
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
									\
	static void __##name##_table_clear(struct name *table)		\
	{								\
		for (unsigned int i = 0; i < table->size; i++) {	\
			__##name##_set_hash(table, i, 0);		\
		}							\
		table->num_items = 0;					\
		table->num_tombstones = 0;				\
	}								\



#define DEFINE_HASHMAP(name, key_type, item_type, THRESHOLD, ...)	\
									\
	DEFINE_HASHTABLE(name, key_type, THRESHOLD, __VA_ARGS__)	\
									\
	typedef struct name##_iterator {				\
		key_type *key;						\
		item_type *value;					\
		unsigned int _index;					\
		struct name *_map;					\
	} name##_iter_t;						\
									\
	static inline item_type *__##name##_item(struct name *table, unsigned int index) \
	{								\
		return &((item_type *)((char *)__##name##_table_memory(table) \
				       + __##name##_table_memory_bytes(table->size)))[index]; \
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
		size_t buckets_size = __##name##_table_memory_bytes(size); \
		size_t items_size = size * sizeof(item_type);		\
		void *mem = aligned_alloc(64, buckets_size + items_size); \
		__##name##_table_init(table, mem, size);		\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(__##name##_table_memory(table));			\
		__##name##_table_destroy(table);			\
	}								\
									\
	static void name##_clear(struct name *table)			\
	{								\
		__##name##_table_clear(table);				\
	}								\
									\
	static struct name##_iterator name##_iter_start(struct name *table) \
	{								\
		struct name##_iterator iter;				\
		iter._map = table;					\
		iter._index = __##name##_get_next(iter._map, 0);	\
		iter.key = NULL;					\
		iter.value = NULL;					\
		if (iter._index < iter._map->size) {			\
			iter.key = __##name##_key(iter._map, iter._index); \
			iter.value = __##name##_item(iter._map, iter._index); \
		}							\
		return iter;						\
	}								\
									\
	static bool name##_iter_finished(struct name##_iterator *iter)	\
	{								\
		return iter->_index >= iter->_map->size;		\
	}								\
									\
	static void name##_iter_advance(struct name##_iterator *iter)	\
	{								\
		iter->key = NULL;					\
		iter->value = NULL;					\
		if (iter->_index >= iter->_map->size) {			\
			return;						\
		}							\
		iter->_index = __##name##_get_next(iter->_map, iter->_index + 1); \
		if (iter->_index < iter->_map->size) {			\
			iter->key = __##name##_key(iter->_map, iter->_index); \
			iter->value = __##name##_item(iter->_map, iter->_index); \
		}							\
	}								\
									\
	static void name##_resize(struct name *table, unsigned int size) \
	{								\
		struct name new_table;					\
		name##_init(&new_table, size);				\
									\
		/* rewrite this with get_next() */			\
		for (unsigned int index = 0; index < table->size; index++) { \
			unsigned int hash = __##name##_get_hash(table, index); \
			if (hash != 0 && hash != 1) {			\
				unsigned int new_index = __##name##_table_do_insert(&new_table, __##name##_key(table, index), hash); \
				*__##name##_item(&new_table, new_index) = *__##name##_item(table, index); \
			}						\
		}							\
		new_table.num_items = table->num_items;			\
		name##_destroy(table);					\
		*table = new_table;					\
	}								\
									\
	static item_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_table_lookup(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return __##name##_item(table, index);			\
	}								\
									\
	static item_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_table_insert(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return __##name##_item(table, index);			\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item) \
	{								\
		unsigned int index = __##name##_table_lookup(table, key, hash); \
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
		__##name##_table_remove(table, index);			\
		return true;						\
	}								\

#define DEFINE_HASHSET(name, key_type, THRESHOLD, ...)			\
									\
	DEFINE_HASHTABLE(name, key_type, THRESHOLD, __VA_ARGS__)	\
									\
	typedef struct name##_iterator {				\
		key_type *key;						\
		unsigned int _index;					\
		struct name *_map;					\
	} name##_iter_t;						\
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
		void *mem = aligned_alloc(64, __##name##_table_memory_bytes(size)); \
		__##name##_table_init(table, mem, size);		\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(__##name##_table_memory(table));			\
		__##name##_table_destroy(table);			\
	}								\
									\
	static void name##_clear(struct name *table)			\
	{								\
		__##name##_table_clear(table);				\
	}								\
									\
	static struct name##_iterator name##_iter_start(struct name *table) \
	{								\
		struct name##_iterator iter;				\
		iter._map = table;					\
		iter._index = __##name##_get_next(iter._map, 0);	\
		iter.key = NULL;					\
		if (iter._index < iter._map->size) {			\
			iter.key = __##name##_key(iter._map, iter._index); \
		}							\
		return iter;						\
	}								\
									\
	static bool name##_iter_finished(struct name##_iterator *iter)	\
	{								\
		return iter->_index >= iter->_map->size;		\
	}								\
									\
	static void name##_iter_advance(struct name##_iterator *iter)	\
	{								\
		iter->key = NULL;					\
		if (iter->_index >= iter->_map->size) {			\
			return;						\
		}							\
		iter->_index = __##name##_get_next(iter->_map, iter->_index + 1); \
		if (iter->_index < iter->_map->size) {			\
			iter->key = __##name##_key(iter->_map, iter->_index); \
		}							\
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
				__##name##_table_do_insert(&new_table, __##name##_key(table, index), hash); \
			}						\
		}							\
		new_table.num_items = table->num_items;			\
		name##_destroy(table);					\
		*table = new_table;					\
	}								\
									\
	static key_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_table_lookup(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return __##name##_key(table, index);			\
	}								\
									\
	static key_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_table_insert(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return __##name##_key(table, index);			\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key) \
	{								\
		unsigned int index = __##name##_table_lookup(table, key, hash); \
		if (index == -1) {					\
			return false;					\
		}							\
									\
		if (ret_key) {						\
			*ret_key = *__##name##_key(table, index);	\
		}							\
		__##name##_table_remove(table, index);			\
		return true;						\
	}								\

#endif
