#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// TODO add ordered hashmap/hashset implementation? (insertion order chaining)
// TODO try hopscotch hashing
typedef unsigned int _hashtable_hash_t;
typedef size_t _hashtable_uint_t;
typedef _hashtable_uint_t _hashtable_idx_t;

struct _hashtable_info {
	_hashtable_uint_t key_size;
	_hashtable_uint_t value_size;
	_hashtable_uint_t threshold;
	bool (*keys_match)(const void *a, const void *b);
	unsigned char f1;
	unsigned char f2;
	unsigned char f3;
};


static _hashtable_uint_t _hashtable_round_capacity(_hashtable_uint_t capacity)
{
	/* round to next power of 2 */
	capacity--;
	capacity |= capacity >> 1;
	capacity |= capacity >> 2;
	capacity |= capacity >> 4;
	capacity |= capacity >> 8;
	capacity |= capacity >> 16;
	capacity |= capacity >> (sizeof(capacity) * 4);
	capacity++;
	return capacity;
}

static _hashtable_uint_t _hashtable_min_capacity(_hashtable_uint_t num_items,
						 const struct _hashtable_info *info)
{
	// threshold=5 -> f1=2, f2=0, f2/threshold=0
	// threshold=6 -> f1=1, f2=4, f2/threshold=4/6=2/3
	// threshold=7 -> f1=1, f2=3, f2/threshold=3/7
	// threshold=8 -> f1=1, f2=2, f2/threshold=2/8=1/4
	// threshold=9 -> f1=1, f2=1, f2/threshold=1/9

	_hashtable_uint_t f1 = info->f1;
	_hashtable_uint_t f2 = info->f2;
	_hashtable_uint_t f3 = info->f3;
	_hashtable_uint_t capacity = num_items * f1 + (num_items * f2 + f3 - 1) / f3;
	// assert(capacity > num_items); resize_unchecked does this already
	return capacity;
}

// #define _HASHTABLE_ROBIN_HOOD 1

#ifndef _HASHTABLE_ROBIN_HOOD

struct _hashtable_bucket {
	_hashtable_hash_t hash;
	unsigned char data[];
};

struct _hashtable {
	_hashtable_uint_t num_items;
	_hashtable_uint_t num_tombstones;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	// TODO add generation and check it during iteration?
};

static _hashtable_idx_t _hashtable_hash_to_index(_hashtable_hash_t hash, _hashtable_uint_t capacity)
{
	return hash & (capacity - 1);
}

static _hashtable_idx_t _hashtable_probe_index(_hashtable_idx_t start, _hashtable_uint_t i,
					       _hashtable_uint_t capacity)
{
	return (start + i) & (capacity - 1);
}

static struct _hashtable_bucket *_hashtable_get_bucket(struct _hashtable *table, _hashtable_idx_t index,
						       const struct _hashtable_info *info)
{
	size_t offset = index * (sizeof(struct _hashtable_bucket) + info->key_size);
	return (struct _hashtable_bucket *)(table->storage + offset);
}

static _hashtable_hash_t _hashtable_get_hash(struct _hashtable *table, _hashtable_idx_t index,
					     const struct _hashtable_info *info)
{
	return _hashtable_get_bucket(table, index, info)->hash;
}

static void _hashtable_set_hash(struct _hashtable *table, _hashtable_idx_t index, _hashtable_hash_t hash,
				const struct _hashtable_info *info)
{
	_hashtable_get_bucket(table, index, info)->hash = hash;
}

static void *_hashtable_key(struct _hashtable *table, _hashtable_idx_t index,
			    const struct _hashtable_info *info)
{
	return &_hashtable_get_bucket(table, index, info)->data[0];
}

static void *_hashtable_value(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	size_t offset = table->capacity * (sizeof(struct _hashtable_bucket) + info->key_size);
	return table->storage + offset + index * info->value_size;
}

static void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
			    const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	assert((capacity & (capacity - 1)) == 0);
	const _hashtable_uint_t alignment = 64;
	_hashtable_uint_t size = sizeof(struct _hashtable_bucket) + info->key_size + info->value_size;
	assert(((_hashtable_uint_t)-alignment) / size >= capacity);
	size *= capacity;
	size = (size + alignment - 1) & ~(alignment - 1);
	table->storage = aligned_alloc(alignment, size);
	table->num_items = 0;
	table->num_tombstones = 0;
	table->capacity = capacity;
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_set_hash(table, i, 0, info);
	}
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
			      _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;

	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_probe_index(start, i, table->capacity);
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

static _hashtable_idx_t _hashtable_get_next(struct _hashtable *table, size_t start,
					    const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		if (hash != 0 && hash != 1) {
			return index;
		}
	}
	return table->capacity;
}

static _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
					     const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	_hashtable_idx_t index;
	for (_hashtable_uint_t i = 0;; i++) {
		index = _hashtable_probe_index(start, i, table->capacity);
		_hashtable_hash_t h = _hashtable_get_hash(table, index, info);
		if (h == 0) {
			break;
		}
		if (h == 1) {
			table->num_tombstones--;
			break;
		}
	}
	_hashtable_set_hash(table, index, hash, info);
	return index;
}

static void _hashtable_resize_unchecked(struct _hashtable *table, _hashtable_uint_t new_capacity,
					const struct _hashtable_info *info)
{
	assert(new_capacity > table->num_items);
	struct _hashtable new_table;
	_hashtable_init(&new_table, new_capacity, info);

	for (_hashtable_idx_t index = 0; index < table->capacity; index++) {
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		if (hash != 0 && hash != 1) {
			_hashtable_idx_t new_index = _hashtable_do_insert(&new_table, hash, info);
			const void *key = _hashtable_key(table, index, info);
			memcpy(_hashtable_key(&new_table, new_index, info), key, info->key_size);
			const void *value = _hashtable_value(table, index, info);
			memcpy(_hashtable_value(&new_table, new_index, info), value, info->value_size);
		}
	}
	new_table.num_items = table->num_items;
	_hashtable_destroy(table);
	*table = new_table;
}

static void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items, info);
	if (new_capacity < min_capacity) {
		new_capacity = min_capacity;
	}
	new_capacity = _hashtable_round_capacity(new_capacity);
	_hashtable_resize_unchecked(table, new_capacity, info);
}

static _hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
					  const struct _hashtable_info *info)
{
	table->num_items++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items + table->num_tombstones, info);
	if (min_capacity > table->capacity) {
		_hashtable_resize_unchecked(table, 2 * table->capacity, info);
	}
	hash = hash == 0 || hash == 1 ? hash - 2 : hash;
	return _hashtable_do_insert(table, hash, info);
}

static void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	_hashtable_set_hash(table, index, 1, info);
	table->num_items--;
	table->num_tombstones++;
	if (table->num_items < table->capacity / 8) {
		_hashtable_resize_unchecked(table, table->capacity / 4, info);
	} else if (table->num_tombstones > table->capacity / 2) {
		// can this even happen?
		_hashtable_resize_unchecked(table, table->capacity, info);
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_set_hash(table, i, 0, info);
	}
	table->num_items = 0;
	table->num_tombstones = 0;
}

#else

/* Memory layout:
 * For in-place resizing the memory layout needs to look like this (k=key, v=value, d=dib):
 * kvkvkvkvkvddddd
 * So that when we double the capacity we get this:
 * kvkvkvkvkvddddd
 * kvkvkvkvkvkvkvkvkvkvdddddddddd
 * Notice how the old buckets remain in the same place!
 * (The old dibs get copied to the back early on, so it's fine to overwrite them.
 *  But we don't want to copy any keys or values since those tend to be bigger.)
 */

typedef uint8_t _hashtable_dib_t;
#define __DIB_OVERFLOW ((_hashtable_dib_t)0xfdU)
#define __DIB_REHASH   ((_hashtable_dib_t)0xfeU)
#define __DIB_FREE     ((_hashtable_dib_t)0xffU)

struct _hashtable_bucket {
	_hashtable_hash_t hash;
	unsigned char data[];
};

struct _hashtable {
	_hashtable_uint_t num_items;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	_hashtable_dib_t *dibs;
	// TODO add generation and check it during iteration?
};

static _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t start, _hashtable_uint_t i,
					       _hashtable_uint_t capacity)
{
	return (start + i) & (capacity - 1);
}

static _hashtable_idx_t _hashtable_hash_to_index(_hashtable_hash_t hash, _hashtable_uint_t capacity)
{
	return _hashtable_wrap_index(hash, 0, capacity);
}

static struct _hashtable_bucket *_hashtable_get_bucket(struct _hashtable *table, _hashtable_idx_t index,
						       const struct _hashtable_info *info)
{
	size_t offset = index * (sizeof(struct _hashtable_bucket) + info->key_size + info->value_size);
	return (struct _hashtable_bucket *)(table->storage + offset);
}

static _hashtable_hash_t _hashtable_get_hash(struct _hashtable *table, _hashtable_idx_t index,
					     const struct _hashtable_info *info)
{
	return _hashtable_get_bucket(table, index, info)->hash;
}

static void _hashtable_set_hash(struct _hashtable *table, _hashtable_idx_t index, _hashtable_hash_t hash,
				const struct _hashtable_info *info)
{
	_hashtable_get_bucket(table, index, info)->hash = hash;
}

static void *_hashtable_key(struct _hashtable *table, _hashtable_idx_t index,
			    const struct _hashtable_info *info)
{
	return &_hashtable_get_bucket(table, index, info)->data[0];
}

static void *_hashtable_value(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	return &_hashtable_get_bucket(table, index, info)->data[info->key_size];
}

static _hashtable_uint_t _hashtable_dib_offset(_hashtable_uint_t capacity,
					       const struct _hashtable_info *info)
{
	return capacity * (sizeof(struct _hashtable_bucket) + info->key_size + info->value_size);
}

static _hashtable_dib_t _hashtable_dib(struct _hashtable *table, _hashtable_idx_t index,
				       const struct _hashtable_info *info)
{
	return table->dibs[index];
}

static void _hashtable_set_dib(struct _hashtable *table, _hashtable_idx_t index, _hashtable_dib_t dib,
			       const struct _hashtable_info *info)
{
	table->dibs[index] = dib;
}

static void _hashtable_set_distance(struct _hashtable *table, _hashtable_idx_t index, _hashtable_uint_t distance,
				    const struct _hashtable_info *info)
{
	_hashtable_dib_t dib = distance;
	if (distance >= __DIB_OVERFLOW) {
		dib = __DIB_OVERFLOW;
	}
	_hashtable_set_dib(table, index, dib, info);
}

static _hashtable_uint_t _hashtable_distance(struct _hashtable *table, _hashtable_idx_t index,
					     const struct _hashtable_info *info)
{
	_hashtable_dib_t dib = _hashtable_dib(table, index, info);
	if (dib < __DIB_OVERFLOW) {
		return dib;
	}
	_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
	return (index - _hashtable_hash_to_index(hash, table->capacity)) & (table->capacity - 1);
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = sizeof(struct _hashtable_bucket) + info->key_size + info->value_size + 1;
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	table->dibs = table->storage + _hashtable_dib_offset(table->capacity, info);
}

static void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
			    const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	assert((capacity & (capacity - 1)) == 0);
	table->storage = NULL;
	table->num_items = 0;
	table->capacity = capacity;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_set_dib(table, i, __DIB_FREE, info);
	}
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
			      _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);

		if (dib == __DIB_FREE) {
			break;
		}

		_hashtable_uint_t dist = _hashtable_distance(table, index, info);
		if (dist < i) {
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

static _hashtable_idx_t _hashtable_get_next(struct _hashtable *table, size_t start,
					    const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);
		if (dib != __DIB_FREE) {
			return index;
		}
	}
	return table->capacity;
}

static bool _hashtable_insert_robin_hood(struct _hashtable *table, _hashtable_idx_t start,
					 _hashtable_uint_t distance, _hashtable_hash_t *phash,
					 void *key, void *value, const struct _hashtable_info *info)
{
	void *tmp_key = alloca(info->key_size);
	void *tmp_value = alloca(info->value_size);
	for (_hashtable_uint_t i = 0;; i++, distance++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);
		if (dib == __DIB_FREE) {
			_hashtable_set_distance(table, index, distance, info);
			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_key(table, index, info), key, info->key_size);
			memcpy(_hashtable_value(table, index, info), value, info->value_size);
			return false;
		}

		_hashtable_uint_t d;
		if (dib == __DIB_REHASH || (d = _hashtable_distance(table, index, info)) < distance) {
			_hashtable_set_distance(table, index, distance, info);

			_hashtable_hash_t tmp_hash = _hashtable_get_hash(table, index, info);
			memcpy(tmp_key, _hashtable_key(table, index, info), info->key_size);
			memcpy(tmp_value, _hashtable_value(table, index, info), info->value_size);

			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_key(table, index, info), key, info->key_size);
			memcpy(_hashtable_value(table, index, info), value, info->value_size);

			*phash = tmp_hash;
			memcpy(key, tmp_key, info->key_size);
			memcpy(value, tmp_value, info->value_size);

			if (dib == __DIB_REHASH) {
				return true;
			}

			distance = d;
		}
	}
}

static _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
					     const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	_hashtable_idx_t index;
	_hashtable_uint_t i;
	for (i = 0;; i++) {
		index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);
		if (dib == __DIB_FREE) {
			break;
		}
		_hashtable_uint_t d = _hashtable_distance(table, index, info);
		if (d < i) {
			// TODO make pointer variants of all getters and setters
			_hashtable_hash_t h = _hashtable_get_hash(table, index, info);
			void *key = _hashtable_key(table, index, info);
			void *value = _hashtable_value(table, index, info);
			start = _hashtable_wrap_index(start, i + 1, table->capacity);
			d++;
			_hashtable_insert_robin_hood(table, start, d, &h, key, value, info);
			break;
		}
	}
	_hashtable_set_distance(table, index, i, info);
	_hashtable_set_hash(table, index, hash, info);
	return index;
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_items);
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		table->dibs[i] = table->dibs[i] == __DIB_FREE ? __DIB_FREE : __DIB_REHASH;
	}

	void *key = alloca(info->key_size);
	void *value = alloca(info->value_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);
		if (dib != __DIB_REHASH) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			_hashtable_set_distance(table, index, 0, info);
			num_rehashed++;
			continue;
		}
		_hashtable_set_dib(table, index, __DIB_FREE, info);
		memcpy(key, _hashtable_key(table, index, info), info->key_size);
		memcpy(value, _hashtable_value(table, index, info), info->value_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, key, value, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		}
	}

	size_t new_dib_offset = _hashtable_dib_offset(table->capacity, info);
	_hashtable_dib_t *new_dibs = (_hashtable_dib_t *)(table->storage + new_dib_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		new_dibs[i] = table->dibs[i];
	}
	_hashtable_realloc_storage(table, info);
}

static void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
			    const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_items);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	_hashtable_realloc_storage(table, info);
	size_t old_dib_offset = _hashtable_dib_offset(old_capacity, info);
	_hashtable_dib_t *old_dibs = (_hashtable_dib_t *)(table->storage + old_dib_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		_hashtable_set_dib(table, i, old_dibs[i] == __DIB_FREE ? __DIB_FREE : __DIB_REHASH, info);
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_set_dib(table, i, __DIB_FREE, info);
	}

	void *key = alloca(info->key_size);
	void *value = alloca(info->value_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_dib_t dib = _hashtable_dib(table, index, info);
		if (dib != __DIB_REHASH) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			_hashtable_set_distance(table, index, 0, info);
			num_rehashed++;
			continue;
		}
		_hashtable_set_dib(table, index, __DIB_FREE, info);
		memcpy(key, _hashtable_key(table, index, info), info->key_size);
		memcpy(value, _hashtable_value(table, index, info), info->value_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, key, value, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		}
	}
}

static void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items, info);
	if (new_capacity < min_capacity) {
		new_capacity = min_capacity;
	}
	new_capacity = _hashtable_round_capacity(new_capacity);
	if (new_capacity < table->capacity) {
		_hashtable_shrink(table, new_capacity, info);
	} else {
		_hashtable_grow(table, new_capacity, info);
	}
}

static _hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
					  const struct _hashtable_info *info)
{
	table->num_items++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

static void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	_hashtable_set_dib(table, index, __DIB_FREE, info);
	table->num_items--;
	if (table->num_items < table->capacity / 8) {
		_hashtable_resize(table, table->capacity / 4, info);
	} else {
		for (_hashtable_uint_t i = 0;; i++) {
			_hashtable_idx_t current_index = _hashtable_wrap_index(index, i, table->capacity);
			_hashtable_idx_t next_index = _hashtable_wrap_index(index, i + 1, table->capacity);
			_hashtable_dib_t dib = _hashtable_dib(table, next_index, info);
			_hashtable_uint_t distance;
			if (dib == __DIB_FREE ||
			    (distance = _hashtable_distance(table, next_index, info)) == 0) {
				_hashtable_set_dib(table, current_index, __DIB_FREE, info);
				break;
			}
			_hashtable_set_distance(table, current_index, distance - 1, info);
			_hashtable_hash_t hash = _hashtable_get_hash(table, next_index, info);
			_hashtable_set_hash(table, current_index, hash, info);
			const void *key = _hashtable_key(table, next_index, info);
			memcpy(_hashtable_key(table, current_index, info), key, info->key_size);
			const void *value = _hashtable_value(table, next_index, info);
			memcpy(_hashtable_value(table, current_index, info), value, info->value_size);
		}
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_set_dib(table, i, __DIB_FREE, info);
	}
	table->num_items = 0;
}

#undef __DIB_OVERFLOW
#undef __DIB_REHASH
#undef __DIB_FREE

#endif

#define DEFINE_HASHTABLE_COMMON(name, key_type, value_size_, THRESHOLD, ...) \
									\
	_Static_assert(5 <= THRESHOLD && THRESHOLD <= 9,		\
		       "resize threshold (load factor)  must be an integer in the range of 5 to 9 (50%-90%)"); \
									\
	typedef _hashtable_hash_t name##_hash_t;			\
	typedef _hashtable_uint_t name##_uint_t;			\
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
		.f1 = 10 / (THRESHOLD),					\
		.f2 = (10 % (THRESHOLD)) / ((THRESHOLD) % 2 == 0 ? 2 : 1), \
		.f3 = (THRESHOLD) / ((THRESHOLD) % 2 == 0 ? 2 : 1),	\
	};								\
									\
	static void name##_init(struct name *table, name##_uint_t initial_capacity) \
	{								\
		initial_capacity = _hashtable_round_capacity(initial_capacity);	\
		_hashtable_init(&table->impl, initial_capacity, &_##name##_info); \
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		_hashtable_destroy(&table->impl);			\
	}								\
									\
	static struct name *name##_new(name##_uint_t initial_capacity)	\
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
	static void name##_resize(struct name *table, name##_uint_t new_capacity) \
	{								\
		_hashtable_resize(&table->impl, new_capacity, &_##name##_info); \
	}								\
									\
	static name##_uint_t name##_capacity(struct name *table)	\
	{								\
		return table->impl.capacity;				\
	}								\
									\
	static name##_uint_t name##_num_items(struct name *table)	\
	{								\
		return table->impl.num_items;				\
	}


#define DEFINE_HASHMAP(name, key_type, value_type, THRESHOLD, ...)	\
									\
	struct name {							\
		struct _hashtable impl;					\
		/* TODO small static storage? (simple key-value array) */ \
	};								\
									\
	DEFINE_HASHTABLE_COMMON(name, key_type, sizeof(value_type), (THRESHOLD), (__VA_ARGS__)) \
									\
		typedef struct name##_iterator {			\
		key_type *key;						\
		value_type *value;					\
		_hashtable_idx_t _index;					\
		struct name *_map;					\
		bool _finished;						\
		} name##_iter_t;					\
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
	static value_type *name##_lookup(struct name *table, key_type key, name##_uint_t hash) \
	{								\
		_hashtable_idx_t index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
		return _hashtable_value(&table->impl, index, &_##name##_info); \
	}								\
									\
	static value_type *name##_insert(struct name *table, key_type key, name##_hash_t hash) \
	{								\
		_hashtable_idx_t index = _hashtable_insert(&table->impl, hash, &_##name##_info); \
		*(key_type *)_hashtable_key(&table->impl, index, &_##name##_info) = key; \
		return _hashtable_value(&table->impl, index, &_##name##_info); \
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, name##_uint_t hash, \
				  key_type *ret_key, value_type *ret_value) \
	{								\
		_hashtable_idx_t index;					\
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
		typedef struct name##_iterator {			\
		key_type *key;						\
		_hashtable_idx_t _index;					\
		struct name *_map;					\
		bool _finished;						\
		} name##_iter_t;					\
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
	static key_type *name##_lookup(struct name *table, key_type key, name##_uint_t hash) \
	{								\
		_hashtable_idx_t index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
		return _hashtable_key(&table->impl, index, &_##name##_info); \
	}								\
									\
	static key_type *name##_insert(struct name *table, key_type key, name##_uint_t hash) \
	{								\
		_hashtable_idx_t index = _hashtable_insert(&table->impl, hash, &_##name##_info); \
		key_type *pkey = _hashtable_key(&table->impl, index, &_##name##_info); \
		*pkey = key;						\
		return pkey;						\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, name##_uint_t hash, key_type *ret_key) \
	{								\
		_hashtable_idx_t index;					\
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
