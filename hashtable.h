#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO add ordered hashmap/hashset implementation? (insertion order chaining)
//      how to implement resize cleanly?
// TODO set needs_rehash to false in _hashtable_clear for better performance
// TODO add generation and check it during iteration?
// TODO test the case where the metadata is bigger than the entries

/* Memory layout:
 * For in-place resizing the memory layout needs to look like this (k=key, v=value, m=metadata):
 * kvkvkvkvkvmmmmm
 * So that when we double the capacity we get this:
 * kvkvkvkvkvmmmmm
 * kvkvkvkvkvkvkvkvkvkvmmmmmmmmmm
 * Notice how the old entries remain in the same place!
 * (The old metadata gets copied to the back early on, so it's fine to overwrite it.
 *  But we don't want to copy any keys or values since those tend to be bigger.)
 */

typedef uint32_t _hashtable_hash_t;
typedef size_t _hashtable_uint_t;
typedef _hashtable_uint_t _hashtable_idx_t;

struct _hashtable_info {
	_hashtable_uint_t entry_size;
	_hashtable_uint_t threshold;
	bool (*keys_match)(const void *key, const void *entry);
	unsigned char f1; // constants for the min capacity calculation
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
#define _HASHTABLE_HOPSCOTCH 1

#if !defined(_HASHTABLE_ROBIN_HOOD) && !defined(_HASHTABLE_HOPSCOTCH)

#define _HASHTABLE_EMPTY_HASH 0
#define _HASHTABLE_TOMBSTONE_HASH 1
#define _HASHTABLE_MIN_VALID_HASH 2

typedef struct {
	// Instead of using one bit of the hash for this flag we could allocate a bitmap in grow/shrink,
	// but that would require some bitmap code...
	_hashtable_hash_t needs_rehash : 1;
	_hashtable_hash_t hash : 8 * sizeof(_hashtable_hash_t) - 1;
} _hashtable_metadata_t;

struct _hashtable {
	_hashtable_uint_t num_items;
	_hashtable_uint_t num_tombstones;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	_hashtable_metadata_t *metadata;
};

static _hashtable_idx_t _hashtable_hash_to_index(_hashtable_hash_t hash, _hashtable_uint_t capacity)
{
	return hash & (capacity - 1);
}

static _hashtable_idx_t _hashtable_probe_index(_hashtable_idx_t start, _hashtable_uint_t i,
					       _hashtable_uint_t capacity)
{
	// http://www.chilton-computing.org.uk/acl/literature/reports/p012.htm
	return (start + ((i + 1) * i) / 2) & (capacity - 1);
}

static void *_hashtable_entry(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	return table->storage + index * info->entry_size;
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
						    const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
}

static void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
			    const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	table->storage = NULL;
	table->capacity = capacity;
	table->num_items = 0;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
	}
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < _HASHTABLE_MIN_VALID_HASH ? hash - _HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
			      _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_probe_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (hash == m->hash && info->keys_match(key, _hashtable_entry(table, index, info))) {
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
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
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
	_hashtable_metadata_t *m;
	for (_hashtable_uint_t i = 0;; i++) {
		index = _hashtable_probe_index(start, i, table->capacity);
		m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (m->hash == _HASHTABLE_TOMBSTONE_HASH) {
			table->num_tombstones--;
			break;
		}
	}
	m->hash = hash;
	return index;
}

static bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
					    void *entry, const struct _hashtable_info *info)
{
	void *tmp_entry = alloca(info->entry_size);
	_hashtable_idx_t start = _hashtable_hash_to_index(*phash, table->capacity);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_probe_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			m->hash = *phash;
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		if (m->needs_rehash) {
			m->needs_rehash = false;
			_hashtable_hash_t tmp_hash = m->hash;
			memcpy(tmp_entry, _hashtable_entry(table, index, info), info->entry_size);

			m->hash = *phash;
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);

			*phash = tmp_hash;
			memcpy(entry, tmp_entry, info->entry_size);

			return true;
		}
	}
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_items);
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->needs_rehash = true;
		} else {
			m->hash = _HASHTABLE_EMPTY_HASH;
		}
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		bool need_rehash;
		do {
			need_rehash = _hashtable_insert_during_resize(table, &hash, entry, info);
			num_rehashed++;
		} while (need_rehash);
	}

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		new_metadata[i] = table->metadata[i];
	}
	_hashtable_realloc_storage(table, info);
}

static void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
			    const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_items);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);
	/* Need to iterate backwards in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	for (_hashtable_uint_t j = old_capacity; j > 0; j--) {
		_hashtable_uint_t i = j - 1;
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		if (old_metadata[i].hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->hash = old_metadata[i].hash;
			m->needs_rehash = true;
		} else {
			m->hash = _HASHTABLE_EMPTY_HASH;
			m->needs_rehash = false;
		}
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		bool need_rehash;
		do {
			need_rehash = _hashtable_insert_during_resize(table, &hash, entry, info);
			num_rehashed++;
		} while (need_rehash);
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
	hash = _hashtable_sanitize_hash(hash);
	table->num_items++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items + table->num_tombstones, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

static void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	_hashtable_metadata(table, index, info)->hash = _HASHTABLE_TOMBSTONE_HASH;
	table->num_items--;
	table->num_tombstones++;
	if (table->num_items < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	} else if (table->num_tombstones > table->capacity / 2) {
		// can this even happen?
		_hashtable_grow(table, table->capacity, info);
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = _HASHTABLE_EMPTY_HASH;
	}
	table->num_items = 0;
	table->num_tombstones = 0;
}

#undef _HASHTABLE_EMPTY_HASH
#undef _HASHTABLE_TOMBSTONE_HASH
#undef _HASHTABLE_MIN_VALID_HASH

#elif defined(_HASHTABLE_HOPSCOTCH)

// setting this too small (<~8) may cause issues with shrink failing in remove among other problems
#define _HASHTABLE_NEIGHBORHOOD 32

#define _HASHTABLE_EMPTY_HASH 0
#define _HASHTABLE_MIN_VALID_HASH 1

typedef _hashtable_hash_t _hashtable_bitmap_t;

typedef struct {
	_hashtable_hash_t needs_rehash : 1;
	_hashtable_hash_t hash : 8 * sizeof(_hashtable_hash_t) - 1;
	_hashtable_bitmap_t bitmap;
} _hashtable_metadata_t;

_Static_assert(8 * sizeof(_hashtable_bitmap_t) >= _HASHTABLE_NEIGHBORHOOD,
	       "neighborhood size too big for bitmap");

struct _hashtable {
	_hashtable_uint_t num_items;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	_hashtable_metadata_t *metadata;
};

static _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t index, _hashtable_uint_t capacity)
{
	// http://www.chilton-computing.org.uk/acl/literature/reports/p012.htm
	return index & (capacity - 1);
}

static _hashtable_idx_t _hashtable_hash_to_index(_hashtable_hash_t hash, _hashtable_uint_t capacity)
{
	return _hashtable_wrap_index(hash, capacity);
}

static void *_hashtable_entry(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	return table->storage + index * info->entry_size;
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
						    const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
}

static void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
			    const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	table->storage = NULL;
	table->capacity = capacity;
	table->num_items = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
		m->bitmap = 0;
	}
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < _HASHTABLE_MIN_VALID_HASH ? hash - _HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
			      _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t home = _hashtable_hash_to_index(hash, table->capacity);
	_hashtable_bitmap_t bitmap = _hashtable_metadata(table, home, info)->bitmap;
	if (bitmap == 0) {
		return false;
	}
	for (_hashtable_uint_t i = 0; i < _HASHTABLE_NEIGHBORHOOD; i++) {
		if (!(bitmap & ((_hashtable_bitmap_t)1 << i))) {
			continue;
		}
		_hashtable_idx_t index = _hashtable_wrap_index(home + i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (hash == m->hash &&
		    info->keys_match(key, _hashtable_entry(table, index, info))) {
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
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static bool _hashtable_move_into_neighborhood(struct _hashtable *table, _hashtable_idx_t *pindex,
					      _hashtable_uint_t *pdistance, const struct _hashtable_info *info)
{
	_hashtable_idx_t index = *pindex;
	_hashtable_uint_t distance = *pdistance;
	while (distance >= _HASHTABLE_NEIGHBORHOOD) {
		_hashtable_idx_t empty_index = index;
		// TODO try to go as far as possible in one step to avoid some copies?
		for (_hashtable_uint_t i = 1;; i++) {
			if (i == _HASHTABLE_NEIGHBORHOOD) {
				return false;
			}
			index = _hashtable_wrap_index(index - 1, table->capacity);
			distance--;
			_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
			_hashtable_hash_t hash = m->hash;
			_hashtable_idx_t home = _hashtable_hash_to_index(hash, table->capacity);
			_hashtable_uint_t old_distance = _hashtable_wrap_index(index - home, table->capacity);
			_hashtable_uint_t new_distance = old_distance + i;
			if (new_distance >= _HASHTABLE_NEIGHBORHOOD) {
				continue;
			}
			m->hash = _HASHTABLE_EMPTY_HASH;
			_hashtable_bitmap_t *bitmap = &_hashtable_metadata(table, home, info)->bitmap;
			*bitmap &= ~((_hashtable_bitmap_t)1 << old_distance);
			*bitmap |= (_hashtable_bitmap_t)1 << new_distance;
			memcpy(_hashtable_entry(table, empty_index, info),
			       _hashtable_entry(table, index, info), info->entry_size);
			_hashtable_metadata(table, empty_index, info)->hash = hash;
			*pindex = index;
			*pdistance = distance;
			break;
		}
	}
	return true;
}

static bool _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
				 _hashtable_idx_t *pindex, const struct _hashtable_info *info)
{
	_hashtable_idx_t home = _hashtable_hash_to_index(hash, table->capacity);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}
	}

	if (distance >= _HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		return false;
	}

	_hashtable_bitmap_t *bitmap = &_hashtable_metadata(table, home, info)->bitmap;
	*bitmap |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	*pindex = index;
	return true;
}

// if an entry was replaced *phash will be != _HASHTABLE_EMPTY_HASH
static bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
					    void *entry, const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = *phash;
	*phash = _HASHTABLE_EMPTY_HASH;
	void *tmp_entry = alloca(info->entry_size);
	_hashtable_idx_t home = _hashtable_hash_to_index(hash, table->capacity);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (m->needs_rehash) {
			memcpy(tmp_entry, entry, info->entry_size);
			*phash = m->hash;
			memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);
			entry = tmp_entry;
			m->needs_rehash = false;
			m->hash = _HASHTABLE_EMPTY_HASH;
			break;
		}
	}

	bool success = true;
	if (distance >= _HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		success = false;
		// if we didn't succeed, we insert the entry anyway so it doesn't get lost
	}

	_hashtable_bitmap_t *bitmap = &_hashtable_metadata(table, home, info)->bitmap;
	*bitmap |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
	return success;
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_items);
retry:;
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->bitmap = 0;
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->needs_rehash = true;
		} else {
			m->hash = _HASHTABLE_EMPTY_HASH;
		}
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			m->bitmap |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			if (!_hashtable_insert_during_resize(table, &hash, entry, info)) {
				table->capacity = old_capacity;
				new_capacity *= 2;
				// insert the currently displaced entry somewhere, so it doesn't get lost
				for (_hashtable_uint_t i = 0; hash != _HASHTABLE_EMPTY_HASH; i++) {
					_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
					if (m->hash != _HASHTABLE_EMPTY_HASH) {
						continue;
					}
					m->hash = hash;
					memcpy(_hashtable_entry(table, i, info), entry, info->entry_size);
					break;
				}
				goto retry;
			}
			num_rehashed++;
		} while (hash != _HASHTABLE_EMPTY_HASH);
	}

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		new_metadata[i] = table->metadata[i];
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
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);
	for (_hashtable_uint_t j = old_capacity; j != 0; j--) {
		_hashtable_uint_t i = j - 1;
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->bitmap = 0;
		if (old_metadata[i].hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->hash = old_metadata[i].hash;
			m->needs_rehash = true;
		} else {
			m->hash = _HASHTABLE_EMPTY_HASH;
			m->needs_rehash = false;
		}
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
		m->bitmap = 0;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			m->bitmap |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			// can't fail here
			_hashtable_insert_during_resize(table, &hash, entry, info);
			num_rehashed++;
		} while (hash != _HASHTABLE_EMPTY_HASH);
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
	hash = _hashtable_sanitize_hash(hash);
	table->num_items++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_items, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	_hashtable_idx_t index;
	while (!_hashtable_do_insert(table, hash, &index, info)) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return index;
}

static void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	// TODO move up last entry in neighborhood?
	_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
	_hashtable_idx_t home = _hashtable_hash_to_index(m->hash, table->capacity);
	_hashtable_metadata_t *home_m = _hashtable_metadata(table, home, info);
	_hashtable_uint_t distance = _hashtable_wrap_index(index - home, table->capacity);
	home_m->bitmap &= ~((_hashtable_bitmap_t)1 << distance);
	m->hash = _HASHTABLE_EMPTY_HASH;
	table->num_items--;
	if (table->num_items < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = _HASHTABLE_EMPTY_HASH;
		_hashtable_metadata(table, i, info)->needs_rehash = false;
		_hashtable_metadata(table, i, info)->bitmap = 0;
	}
	table->num_items = 0;
}



#undef _HASHTABLE_NEIGHBORHOOD
#undef _HASHTABLE_EMPTY_HASH
#undef _HASHTABLE_MIN_VALID_HASH

#elif defined(_HASHTABLE_ROBIN_HOOD)

#define _HASHTABLE_EMPTY_HASH 0
#define _HASHTABLE_MIN_VALID_HASH 1

typedef struct {
	// Instead of using one bit of the hash for this flag we could allocate a bitmap in grow/shrink,
	// but that would require some bitmap code...
	_hashtable_hash_t needs_rehash : 1;
	_hashtable_hash_t hash : 8 * sizeof(_hashtable_hash_t) - 1;
} _hashtable_metadata_t;

struct _hashtable {
	_hashtable_uint_t num_items;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	_hashtable_metadata_t *metadata;
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

static void *_hashtable_entry(struct _hashtable *table, _hashtable_idx_t index,
			      const struct _hashtable_info *info)
{
	return table->storage + index * info->entry_size;
}

static _hashtable_hash_t _hashtable_get_hash(struct _hashtable *table, _hashtable_idx_t index,
					     const struct _hashtable_info *info)
{
	return table->metadata[index].hash;
}

static void _hashtable_set_hash(struct _hashtable *table, _hashtable_idx_t index, _hashtable_hash_t hash,
				const struct _hashtable_info *info)
{
	table->metadata[index].hash = hash;
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
					       const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static _hashtable_uint_t _hashtable_get_distance(struct _hashtable *table, _hashtable_idx_t index,
						 const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
	return (index - _hashtable_hash_to_index(hash, table->capacity)) & (table->capacity - 1);
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
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
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
	}
}

static void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < _HASHTABLE_MIN_VALID_HASH ? hash - _HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

static bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
			      _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t start = _hashtable_hash_to_index(hash, table->capacity);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);

		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}

		_hashtable_uint_t dist = _hashtable_get_distance(table, index, info);
		if (dist < i) {
			break;
		}

		if (hash == _hashtable_get_hash(table, index, info) &&
		    info->keys_match(key, _hashtable_entry(table, index, info))) {
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
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static bool _hashtable_insert_robin_hood(struct _hashtable *table, _hashtable_idx_t start,
					 _hashtable_uint_t distance, _hashtable_hash_t *phash,
					 void *entry, const struct _hashtable_info *info)
{
	void *tmp_entry = alloca(info->entry_size);
	for (_hashtable_uint_t i = 0;; i++, distance++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		_hashtable_uint_t d;
		if (m->needs_rehash || (d = _hashtable_get_distance(table, index, info)) < distance) {
			_hashtable_hash_t tmp_hash = _hashtable_get_hash(table, index, info);
			memcpy(tmp_entry, _hashtable_entry(table, index, info), info->entry_size);

			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);

			*phash = tmp_hash;
			memcpy(entry, tmp_entry, info->entry_size);

			if (m->needs_rehash) {
				m->needs_rehash = false;
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
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == _HASHTABLE_EMPTY_HASH) {
			break;
		}
		_hashtable_uint_t d = _hashtable_get_distance(table, index, info);
		if (d < i) {
			_hashtable_hash_t h = _hashtable_get_hash(table, index, info);
			void *entry = _hashtable_entry(table, index, info);
			start = _hashtable_wrap_index(start, i + 1, table->capacity);
			_hashtable_insert_robin_hood(table, start, d + 1, &h, entry, info);
			break;
		}
	}
	_hashtable_set_hash(table, index, hash, info);
	return index;
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_items);

	/*
	 * eeeeeeeeeehhhhhhhhhhdddddddddd
	 * eeeeehhhhhddddd
	 */

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		if (m->hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->needs_rehash = true;
		}
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		}
	}

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		new_metadata[i] = table->metadata[i];
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
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);
	/* Need to iterate backwards in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	for (_hashtable_uint_t j = old_capacity; j > 0; j--) {
		_hashtable_uint_t i = j - 1;
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		if (old_metadata[i].hash >= _HASHTABLE_MIN_VALID_HASH) {
			m->hash = old_metadata[i].hash;
			m->needs_rehash = true;
		} else {
			m->hash = _HASHTABLE_EMPTY_HASH;
			m->needs_rehash = false;
		}
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
		m->needs_rehash = false;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (!m->needs_rehash) {
			continue;
		}
		m->needs_rehash = false;
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(hash, table->capacity);
		if (optimal_index == index) {
			num_rehashed++;
			continue;
		}
		m->hash = _HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, info);
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
	hash = _hashtable_sanitize_hash(hash);
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
	_hashtable_metadata(table, index, info)->hash = _HASHTABLE_EMPTY_HASH;
	table->num_items--;
	if (table->num_items < table->capacity / 8) {
		_hashtable_resize(table, table->capacity / 4, info);
	} else {
		for (_hashtable_uint_t i = 0;; i++) {
			_hashtable_idx_t current_index = _hashtable_wrap_index(index, i, table->capacity);
			_hashtable_idx_t next_index = _hashtable_wrap_index(index, i + 1, table->capacity);
			_hashtable_metadata_t *m = _hashtable_metadata(table, next_index, info);
			_hashtable_uint_t distance;
			if (m->hash == _HASHTABLE_EMPTY_HASH ||
			    (distance = _hashtable_get_distance(table, next_index, info)) == 0) {
				_hashtable_metadata(table, current_index, info)->hash = _HASHTABLE_EMPTY_HASH;
				break;
			}
			_hashtable_hash_t hash = _hashtable_get_hash(table, next_index, info);
			_hashtable_set_hash(table, current_index, hash, info);
			const void *entry = _hashtable_entry(table, next_index, info);
			memcpy(_hashtable_entry(table, current_index, info), entry, info->entry_size);
		}
	}
}

static void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = _HASHTABLE_EMPTY_HASH;
	}
	table->num_items = 0;
}

#undef _HASHTABLE_EMPTY_HASH
#undef _HASHTABLE_MIN_VALID_HASH

#endif

#define _HASHTABLE_DEFINE_COMMON(name, entry_size_, THRESHOLD)		\
									\
	_Static_assert(5 <= THRESHOLD && THRESHOLD <= 9,		\
		       "resize threshold (load factor)  must be an integer in the range of 5 to 9 (50%-90%)"); \
									\
	typedef _hashtable_hash_t name##_hash_t;			\
	typedef _hashtable_uint_t name##_uint_t;			\
									\
	static _Alignas(64) const struct _hashtable_info _##name##_info = { \
		.entry_size = entry_size_,				\
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
	struct _##name##_entry {					\
		key_type key;						\
		value_type value;					\
	};								\
									\
	static bool _##name##_keys_match(const void *_key, const void *_entry) \
	{								\
		key_type const * const a = _key;			\
		key_type const * const b = &((const struct _##name##_entry *)_entry)->key; \
		return (__VA_ARGS__);					\
	}								\
									\
	_HASHTABLE_DEFINE_COMMON(name, sizeof(struct _##name##_entry), (THRESHOLD)) \
									\
		typedef struct name##_iterator {			\
		key_type *key;						\
		value_type *value;					\
		_hashtable_idx_t _index;				\
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
			struct _##name##_entry *entry = _hashtable_entry(&iter->_map->impl, iter->_index, &_##name##_info); \
			iter->key = &entry->key;			\
			iter->value = &entry->value;			\
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
		return &((struct _##name##_entry *)_hashtable_entry(&table->impl, index, &_##name##_info))->value; \
	}								\
									\
	static value_type *name##_insert(struct name *table, key_type key, name##_hash_t hash) \
	{								\
		_hashtable_idx_t index = _hashtable_insert(&table->impl, hash, &_##name##_info); \
		struct _##name##_entry *entry = _hashtable_entry(&table->impl, index, &_##name##_info); \
		entry->key = key;					\
		return &entry->value;					\
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
		struct _##name##_entry *entry = _hashtable_entry(&table->impl, index, &_##name##_info); \
		if (ret_key) {						\
			*ret_key = entry->key;				\
		}							\
		if (ret_value) {					\
			*ret_value = entry->value;			\
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
	static bool _##name##_keys_match(const void *_a, const void *_b) \
	{								\
		key_type const * const a = _a;				\
		key_type const * const b = _b;				\
		return (__VA_ARGS__);					\
	}								\
									\
	_HASHTABLE_DEFINE_COMMON(name, sizeof(key_type), (THRESHOLD))	\
									\
		typedef struct name##_iterator {			\
		key_type *key;						\
		_hashtable_idx_t _index;				\
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
			iter->key = _hashtable_entry(&iter->_map->impl, iter->_index, &_##name##_info); \
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
		return _hashtable_entry(&table->impl, index, &_##name##_info); \
	}								\
									\
	static key_type *name##_insert(struct name *table, key_type key, name##_uint_t hash) \
	{								\
		_hashtable_idx_t index = _hashtable_insert(&table->impl, hash, &_##name##_info); \
		key_type *pkey = _hashtable_entry(&table->impl, index, &_##name##_info); \
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
			*ret_key = *(key_type *)_hashtable_entry(&table->impl, index, &_##name##_info); \
		}							\
		_hashtable_remove(&table->impl, index, &_##name##_info); \
		return true;						\
	}								\

#endif
