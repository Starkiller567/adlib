#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

struct _hashtable_info {
	size_t key_size;
	size_t value_size;
	bool (*keys_match)(const void *a, const void *b);
	unsigned int threshold;
};

struct _hashtable_bucket {
	unsigned int hash;
	unsigned char item[];
};

struct _hashtable {
	unsigned int num_items;
	unsigned int num_tombstones;
	unsigned int capacity;
	unsigned char *storage;
};

static inline unsigned int _hashtable_hash_to_idx(unsigned int hash, unsigned int capacity)
{
	return hash & (capacity - 1);
}

static inline unsigned int _hashtable_probe_index(unsigned int start, unsigned int i, unsigned int capacity)
{
	return (start + i) & (capacity - 1);
}

static inline struct _hashtable_bucket *_hashtable_get_bucket(struct _hashtable *table, unsigned int index,
							      const struct _hashtable_info *info)
{
	size_t offset = index * (sizeof(struct _hashtable_bucket) + info->key_size);
	return (struct _hashtable_bucket *)(table->storage + offset);
}

static inline unsigned int _hashtable_get_hash(struct _hashtable *table, unsigned int index,
					       const struct _hashtable_info *info)
{
	return _hashtable_get_bucket(table, index, info)->hash;
}

static inline void _hashtable_set_hash(struct _hashtable *table, unsigned int index, unsigned int hash,
				       const struct _hashtable_info *info)
{
	_hashtable_get_bucket(table, index, info)->hash = hash;
}

static inline void *_hashtable_key(struct _hashtable *table, unsigned int index,
				   const struct _hashtable_info *info)
{
	return &_hashtable_get_bucket(table, index, info)->item[0];
}

static inline void *_hashtable_value(struct _hashtable *table, unsigned int index,
				     const struct _hashtable_info *info)
{
	size_t offset = table->capacity * (sizeof(struct _hashtable_bucket) + info->key_size);
	return table->storage + offset + index * info->value_size;
}

static void _hashtable_init(struct _hashtable *table, unsigned int capacity, const struct _hashtable_info *info)
{
	if (capacity == 0) {
		capacity = 8;
	} else if ((capacity & (capacity - 1)) != 0) {
		/* round to next power of 2 */
		capacity--;
		capacity |= capacity >> 1;
		capacity |= capacity >> 2;
		capacity |= capacity >> 4;
		capacity |= capacity >> 8;
		capacity |= capacity >> 16;
		capacity |= capacity >> (sizeof(capacity) * 4);
		capacity++;
	}
	size_t total_size = capacity * (sizeof(struct _hashtable_bucket) + info->key_size + info->value_size);
	const size_t alignment = 64;
	if (total_size % alignment != 0) {
		total_size += alignment - (total_size % alignment);
	}
	table->storage = aligned_alloc(alignment, total_size);
	for (unsigned int i = 0; i < capacity; i++) {
		_hashtable_set_hash(table, i, 0, info);
	}
	table->num_items = 0;
	table->num_tombstones = 0;
	table->capacity = capacity;
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	table->storage = NULL;
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, unsigned int hash, unsigned int *ret_index,
			      const struct _hashtable_info *info)
{
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;

	unsigned int start = _hashtable_hash_to_idx(hash, table->capacity);
	for (unsigned int i = 0; /* i < table->capacity */; i++) {
		unsigned int index = _hashtable_probe_index(start, i, table->capacity);
		if (_hashtable_get_hash(table, index, info) == 0) {
			break;
		}
		if (hash == _hashtable_get_hash(table, index, info) &&
		    info->keys_match(key, _hashtable_key(table, index, info))) {
			*ret_index = index;
			return true;
		}
	}
	return false;
}

static unsigned int _hashtable_get_next(struct _hashtable *table, size_t start,
					const struct _hashtable_info *info)
{
	for (unsigned int index = start; index < table->capacity; index++) {
		unsigned int hash = _hashtable_get_hash(table, index, info);
		if (hash != 0 && hash != 1) {
			return index;
		}
	}
	return table->capacity;
}

// TODO don't pass the key?
static unsigned int _hashtable_do_insert(struct _hashtable *table, const void *key, unsigned int hash,
					 const struct _hashtable_info *info)
{
	unsigned int start = _hashtable_hash_to_idx(hash, table->capacity);
	unsigned int index;
	for (unsigned int i = 0; /* i < table->capacity */; i++) {
		index = _hashtable_probe_index(start, i, table->capacity);
		unsigned int h = _hashtable_get_hash(table, index, info);
		if (h == 0) {
			break;
		}
		if (h == 1) {
			table->num_tombstones--;
			break;
		}
	}

	_hashtable_set_hash(table, index, hash, info);
	memcpy(_hashtable_key(table, index, info), key, info->key_size);

	return index;
}

static void _hashtable_resize(struct _hashtable *table, unsigned int new_capacity,
			      const struct _hashtable_info *info)
{
	// TODO try in-place implementation
	struct _hashtable new_table;
	_hashtable_init(&new_table, new_capacity, info);

	// TODO rewrite this with get_next()
	for (unsigned int index = 0; index < table->capacity; index++) {
		unsigned int hash = _hashtable_get_hash(table, index, info);
		if (hash != 0 && hash != 1) {
			void *key = _hashtable_key(table, index, info);
			unsigned int new_index = _hashtable_do_insert(&new_table, key, hash, info);
			void *value = _hashtable_value(table, index, info);
			memcpy(_hashtable_value(&new_table, new_index, info), value, info->value_size);
		}
	}
	new_table.num_items = table->num_items;
	_hashtable_destroy(table);
	*table = new_table;
}

static unsigned int _hashtable_insert(struct _hashtable *table, const void *key, unsigned int hash,
				      const struct _hashtable_info *info)
{
	table->num_items++;
	// TODO change this computation
	if ((table->num_items + table->num_tombstones) * 10 > info->threshold * table->capacity) {
		_hashtable_resize(table, 2 * table->capacity, info);
	}
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;
	return _hashtable_do_insert(table, key, hash, info);
}

static void _hashtable_remove(struct _hashtable *table, unsigned int index,
			      const struct _hashtable_info *info)
{
	_hashtable_set_hash(table, index, 1, info);
	table->num_items--;
	table->num_tombstones++;
	if (10 * table->num_items < table->capacity) {
		_hashtable_resize(table, table->capacity / 2, info);
	} else if (table->num_tombstones > table->capacity / 4) {
		_hashtable_resize(table, table->capacity, info);
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (unsigned int i = 0; i < table->capacity; i++) {
		_hashtable_set_hash(table, i, 0, info);
	}
	table->num_items = 0;
	table->num_tombstones = 0;
}

#define DEFINE_HASHTABLE_COMMON(name, key_type, value_size_, THRESHOLD, ...) \
									\
	static bool _##name##_keys_match(const void *_a, const void *_b) \
	{								\
		key_type const * const a = _a;				\
		key_type const * const b = _b;				\
		return (__VA_ARGS__);					\
	}								\
									\
	static const struct _hashtable_info _##name##_info = {		\
		.key_size = sizeof(key_type),				\
		.value_size = (value_size_),				\
		.threshold = (THRESHOLD),				\
		.keys_match = _##name##_keys_match,			\
	};								\
									\
	static void name##_init(struct name *table, unsigned int initial_capacity) \
	{								\
		_hashtable_init(&table->impl, initial_capacity, &_##name##_info); \
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		_hashtable_destroy(&table->impl);			\
	}								\
									\
	static struct name *name##_new(unsigned int initial_capacity)	\
	{								\
		struct name *table = malloc(sizeof(*table));		\
		name##_init(table, initial_capacity);			\
		return table;						\
	}								\
									\
	static void name##_delete(struct name *table)			\
	{								\
		name##_destroy(table);					\
		free(table);						\
	}								\
									\
	static void name##_clear(struct name *table)			\
	{								\
		_hashtable_clear(&table->impl, &_##name##_info);	\
	}								\
									\
	static void name##_resize(struct name *table, unsigned int new_capacity) \
	{								\
		_hashtable_resize(&table->impl, new_capacity, &_##name##_info); \
	}								\


#define DEFINE_HASHMAP(name, key_type, value_type, THRESHOLD, ...)	\
									\
	struct name {							\
		struct _hashtable impl;					\
	};								\
									\
	DEFINE_HASHTABLE_COMMON(name, key_type, sizeof(value_type), (THRESHOLD), (__VA_ARGS__)) \
									\
	typedef struct name##_iterator {				\
		key_type *key;						\
		value_type *value;					\
		unsigned int _index;					\
		struct name *_map;					\
		bool _finished;						\
	} name##_iter_t;						\
									\
	static bool name##_iter_finished(struct name##_iterator *iter)	\
	{								\
		return iter->_finished;					\
	}								\
									\
	static void name##_iter_advance(struct name##_iterator *iter)	\
	{								\
		iter->key = NULL;					\
		iter->value = NULL;					\
		if (iter->_finished) {					\
			return;						\
		}							\
		iter->_index = _hashtable_get_next(&iter->_map->impl, iter->_index + 1, &_##name##_info); \
		if (iter->_index >= iter->_map->impl.capacity) {	\
			iter->_finished = true;				\
		} else {						\
			iter->key = _hashtable_key(&iter->_map->impl, iter->_index, &_##name##_info); \
			iter->value = _hashtable_value(&iter->_map->impl, iter->_index, &_##name##_info); \
		}							\
	}								\
									\
	static struct name##_iterator name##_iter_start(struct name *table) \
	{								\
		struct name##_iterator iter = {0};			\
		iter._map = table;					\
		iter._index = -1; /* iter_advance increments this to 0 */ \
		iter._finished = false;					\
		name##_iter_advance(&iter);				\
		return iter;						\
	}								\
									\
	static value_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
		return _hashtable_value(&table->impl, index, &_##name##_info); \
	}								\
									\
	static value_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = _hashtable_insert(&table->impl, &key, hash, &_##name##_info); \
		return _hashtable_value(&table->impl, index, &_##name##_info); \
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, \
				  key_type *ret_key, value_type *ret_value) \
	{								\
		unsigned int index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
									\
		if (ret_key) {						\
			*ret_key = *(key_type *)_hashtable_key(&table->impl, index, &_##name##_info); \
		}							\
		if (ret_value) {					\
			*ret_value = *(value_type *)_hashtable_value(&table->impl, index, &_##name##_info); \
		}							\
		_hashtable_remove(&table->impl, index, &_##name##_info); \
		return true;						\
	}

#define DEFINE_HASHSET(name, key_type, THRESHOLD, ...)			\
									\
	struct name {							\
		struct _hashtable impl;					\
	};								\
									\
	DEFINE_HASHTABLE_COMMON(name, key_type, 0, (THRESHOLD), (__VA_ARGS__)) \
									\
	typedef struct name##_iterator {				\
		key_type *key;						\
		unsigned int _index;					\
		struct name *_map;					\
		bool _finished;						\
	} name##_iter_t;						\
									\
	static bool name##_iter_finished(struct name##_iterator *iter)	\
	{								\
		return iter->_finished;					\
	}								\
									\
	static void name##_iter_advance(struct name##_iterator *iter)	\
	{								\
		iter->key = NULL;					\
		if (iter->_finished) {					\
			return;						\
		}							\
		iter->_index = _hashtable_get_next(&iter->_map->impl, iter->_index + 1, &_##name##_info); \
		if (iter->_index >= iter->_map->impl.capacity) {	\
			iter->_finished = true;				\
		} else {						\
			iter->key = _hashtable_key(&iter->_map->impl, iter->_index, &_##name##_info); \
		}							\
	}								\
									\
	static struct name##_iterator name##_iter_start(struct name *table) \
	{								\
		struct name##_iterator iter = {0};			\
		iter._map = table;					\
		iter._index = -1; /* iter_advance increments this to 0 */ \
		iter._finished = false;					\
		name##_iter_advance(&iter);				\
		return iter;						\
	}								\
									\
	static key_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
		return _hashtable_key(&table->impl, index, &_##name##_info); \
	}								\
									\
	static key_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = _hashtable_insert(&table->impl, &key, hash, &_##name##_info); \
		return _hashtable_key(&table->impl, index, &_##name##_info);			\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key) \
	{								\
		unsigned int index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
									\
		if (ret_key) {						\
			*ret_key = *(key_type *)_hashtable_key(&table->impl, index, &_##name##_info); \
		}							\
		_hashtable_remove(&table->impl, index, &_##name##_info); \
		return true;						\
	}								\

#endif
