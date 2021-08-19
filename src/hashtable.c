/*
 * Copyright (C) 2020-2021 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "macros.h"

// TODO add ordered hashmap/hashset implementation? (insertion order chaining)
//      store the entries in a linear array in insertion order and let the hashtable map keys to indices
//      (how to implement remove efficiently? bitmap for deleted entries + compaction during resize?)
// TODO add generation and check it during iteration?
// TODO make the hashtable more robust against bad hash functions (e.g. by using fibonacci hashing)?
// TODO in hash_to_idx xor the hash with a random seed that changes whenever the hashtable gets resized?
// TODO make it possible to choose the implementation for each instance? (probably too slow or messy...)
// TODO make _hashtable_lookup return the first eligable slot for insertion to speed up subsequent insertion
//      and make hashtable_set (lookup + insert) helper

/* Memory layout:
 * For in-place resizing the memory layout needs to look like this (e=entry, m=metadata):
 * eeeeemmmmm
 * So that when we double the capacity we get this:
 * eeeeemmmmm
 * eeeeeeeeeemmmmmmmmmm
 * Notice how the old entries remain in the same place!
 * (The old metadata gets copied to the back early on, so it's fine to overwrite it.
 *  But we don't want to copy any entries since those tend to be bigger.)
 */

static _attr_unused _hashtable_uint_t _hashtable_round_capacity(_hashtable_uint_t capacity)
{
	// round to next power of 2
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

static _attr_unused _hashtable_uint_t _hashtable_min_capacity(_hashtable_uint_t num_entries,
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
	_hashtable_uint_t capacity = num_entries * f1 + (num_entries * f2 + f3 - 1) / f3;
	return capacity;
}

static _attr_unused _hashtable_idx_t _hashtable_hash_to_index(const struct _hashtable *table,
							      _hashtable_hash_t hash)
{
	// TODO store log2(capacity) and use different constant for different sizeof(hash)
	// const size_t shift_amount = __builtin_clzll(table->capacity) + 1;
	// hash ^= hash >> shift_amount;
	// return (2654435769 * hash) >> shift_amount;
	// return (11400714819323198485llu * hash) >> shift_amount;

	return (11 * hash) & (table->capacity - 1);

	// return hash & (table->capacity - 1);
}

#if defined(HASHTABLE_QUADRATIC)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_TOMBSTONE_HASH 1
#define __HASHTABLE_MIN_VALID_HASH 2

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
} _hashtable_metadata_t;

static _attr_unused _hashtable_idx_t _hashtable_probe_index(_hashtable_idx_t start, _hashtable_uint_t i,
							    _hashtable_uint_t capacity)
{
	// http://www.chilton-computing.org.uk/acl/literature/reports/p012.htm
	return (start + ((i + 1) * i) / 2) & (capacity - 1);
}

static _attr_unused _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
								 const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _attr_unused _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
							       const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static _attr_unused void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
}

__AD_LINKAGE void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
				  const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	table->storage = NULL;
	table->capacity = capacity;
	table->num_entries = 0;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
}

__AD_LINKAGE void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _attr_unused _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

__AD_LINKAGE bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
				    _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_probe_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (hash == m->hash && info->keys_match(key, _hashtable_entry(table, index, info))) {
			*ret_index = index;
			return true;
		}
	}
	return false;
}

__AD_LINKAGE _hashtable_idx_t _hashtable_get_next(struct _hashtable *table, size_t start,
						  const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static _attr_unused _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
							  const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index;
	_hashtable_metadata_t *m;
	for (_hashtable_uint_t i = 0;; i++) {
		index = _hashtable_probe_index(start, i, table->capacity);
		m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (m->hash == __HASHTABLE_TOMBSTONE_HASH) {
			table->num_tombstones--;
			break;
		}
	}
	m->hash = hash;
	return index;
}

static _attr_unused bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static _attr_unused void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

static _attr_unused bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
							 void *entry, uint32_t *bitmap,
							 const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(table, *phash);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_probe_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH || m->hash == __HASHTABLE_TOMBSTONE_HASH) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			m->hash = *phash;
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		if (_hashtable_slot_needs_rehash(bitmap, index)) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			// if (_hashtable_hash_to_index(table, m->hash) == index) {
			// 	continue;
			// }
			void *tmp_entry = alloca(info->entry_size);
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

static _attr_unused void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
					   const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;

	size_t bitmap_size = (old_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		bool need_rehash;
		do {
			need_rehash = _hashtable_insert_during_resize(table, &hash, entry, bitmap, info);
			num_rehashed++;
		} while (need_rehash);
	}

	free(bitmap_to_free);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	memmove(new_metadata, table->metadata, old_capacity * sizeof(table->metadata[0]));
	_hashtable_realloc_storage(table, info);
}

static _attr_unused void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
					 const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	size_t bitmap_size = (new_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	/* Need to use memmove in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	memmove(table->metadata, old_metadata, old_capacity * sizeof(table->metadata[0]));
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		bool need_rehash;
		do {
			need_rehash = _hashtable_insert_during_resize(table, &hash, entry, bitmap, info);
			num_rehashed++;
		} while (need_rehash);
	}
	free(bitmap_to_free);
}

__AD_LINKAGE void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
				    const struct _hashtable_info *info)
{
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries, info);
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

__AD_LINKAGE _hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
						const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries + table->num_tombstones, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

__AD_LINKAGE void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
				    const struct _hashtable_info *info)
{
	_hashtable_metadata(table, index, info)->hash = __HASHTABLE_TOMBSTONE_HASH;
	table->num_entries--;
	table->num_tombstones++;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	} else if (table->num_tombstones > table->capacity / 2) {
		// can this even happen?
		_hashtable_grow(table, table->capacity, info);
	}
}

__AD_LINKAGE void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
	table->num_entries = 0;
	table->num_tombstones = 0;
}

#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_TOMBSTONE_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#elif defined(HASHTABLE_HOPSCOTCH)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_MIN_VALID_HASH 1

// setting this too small (<~8) may cause issues with shrink failing in remove among other problems
#define __HASHTABLE_NEIGHBORHOOD 32

_Static_assert(8 * sizeof(_hashtable_bitmap_t) >= __HASHTABLE_NEIGHBORHOOD,
	       "hopscotch neighborhood size too big for bitmap");

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
} _hashtable_metadata_t;

static _attr_unused _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t index, _hashtable_uint_t capacity)
{
	return index & (capacity - 1);
}

static _attr_unused _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
								 const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _attr_unused _hashtable_uint_t _hashtable_bitmaps_offset(_hashtable_uint_t capacity,
								const struct _hashtable_info *info)
{
	return capacity * (info->entry_size + sizeof(_hashtable_hash_t));
}

static _attr_unused _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
							       const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static _attr_unused _hashtable_bitmap_t *_hashtable_bitmap(struct _hashtable *table, _hashtable_idx_t index,
							   const struct _hashtable_info *info)
{
	return &table->bitmaps[index];
}

static _attr_unused void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t) + sizeof(_hashtable_bitmap_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
	table->bitmaps = (_hashtable_bitmap_t *)(table->storage +
						 _hashtable_bitmaps_offset(table->capacity, info));
}

__AD_LINKAGE void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
				  const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	table->storage = NULL;
	table->capacity = capacity;
	table->num_entries = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
		*_hashtable_bitmap(table, i, info) = 0;
	}
}

__AD_LINKAGE void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _attr_unused _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

__AD_LINKAGE bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
				    _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_bitmap_t bitmap = *_hashtable_bitmap(table, home, info);
	if (bitmap == 0) {
		return false;
	}
	for (_hashtable_uint_t i = 0; i < __HASHTABLE_NEIGHBORHOOD; i++) {
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

__AD_LINKAGE _hashtable_idx_t _hashtable_get_next(struct _hashtable *table, size_t start,
						  const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static _attr_unused bool _hashtable_move_into_neighborhood(struct _hashtable *table, _hashtable_idx_t *pindex,
							   _hashtable_uint_t *pdistance, const struct _hashtable_info *info)
{
	_hashtable_idx_t index = *pindex;
	_hashtable_uint_t distance = *pdistance;
	while (distance >= __HASHTABLE_NEIGHBORHOOD) {
		_hashtable_idx_t empty_index = index;
		for (_hashtable_uint_t i = 1;; i++) {
			if (i == __HASHTABLE_NEIGHBORHOOD) {
				return false;
			}
			index = _hashtable_wrap_index(index - 1, table->capacity);
			distance--;
			_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
			_hashtable_hash_t hash = m->hash;
			_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
			_hashtable_uint_t old_distance = _hashtable_wrap_index(index - home, table->capacity);
			_hashtable_uint_t new_distance = old_distance + i;
			if (new_distance >= __HASHTABLE_NEIGHBORHOOD) {
				continue;
			}
			m->hash = __HASHTABLE_EMPTY_HASH;
			_hashtable_bitmap_t *bitmap = _hashtable_bitmap(table, home, info);
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

static _attr_unused bool _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
					      _hashtable_idx_t *pindex, const struct _hashtable_info *info)
{
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
	}

	if (distance >= __HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		return false;
	}

	_hashtable_bitmap_t *bitmap = _hashtable_bitmap(table, home, info);
	*bitmap |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	*pindex = index;
	return true;
}

static _attr_unused bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static _attr_unused void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

// if an entry was replaced *phash will be != __HASHTABLE_EMPTY_HASH
static _attr_unused bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
							 void *entry, uint32_t *bitmap,
							 const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = *phash;
	*phash = __HASHTABLE_EMPTY_HASH;
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			break;
		}
		if (_hashtable_slot_needs_rehash(bitmap, index)) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			void *tmp_entry = alloca(info->entry_size);
			memcpy(tmp_entry, entry, info->entry_size);
			*phash = m->hash;
			memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);
			entry = tmp_entry;
			m->hash = __HASHTABLE_EMPTY_HASH;
			break;
		}
	}

	bool success = true;
	if (distance >= __HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		success = false;
		// if we didn't succeed, we insert the entry anyway so it doesn't get lost
	}

	*_hashtable_bitmap(table, home, info) |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
	return success;
}

static _attr_unused void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
					   const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}

	_hashtable_uint_t old_capacity = table->capacity;
	size_t bitmap_size = (old_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

retry:
	table->capacity = new_capacity;
	// TODO is there a (simple) way to avoid this work?
	memset(table->bitmaps, 0, old_capacity * sizeof(table->bitmaps[0]));

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			*_hashtable_bitmap(table, index, info) |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			if (!_hashtable_insert_during_resize(table, &hash, entry, bitmap, info)) {
				table->capacity = old_capacity;
				new_capacity *= 2;
				// insert the currently displaced entry somewhere, so it doesn't get lost
				for (_hashtable_uint_t i = 0; hash != __HASHTABLE_EMPTY_HASH; i++) {
					_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
					if (m->hash != __HASHTABLE_EMPTY_HASH) {
						continue;
					}
					m->hash = hash;
					memcpy(_hashtable_entry(table, i, info), entry, info->entry_size);
					break;
				}
				goto retry;
			}
			num_rehashed++;
		} while (hash != __HASHTABLE_EMPTY_HASH);
	}

	free(bitmap_to_free);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	memmove(new_metadata, table->metadata, old_capacity * sizeof(table->metadata[0]));
	size_t new_bitmaps_offset = _hashtable_bitmaps_offset(table->capacity, info);
	_hashtable_bitmap_t *new_bitmaps = (_hashtable_bitmap_t *)(table->storage + new_bitmaps_offset);
	memmove(new_bitmaps, table->bitmaps, old_capacity * sizeof(table->bitmaps[0]));
	_hashtable_realloc_storage(table, info);
}

static _attr_unused void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
					 const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	size_t bitmap_size = (new_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	/* Need to iterate backwards in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	for (_hashtable_uint_t i = old_capacity; i-- > 0;) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = old_metadata[i].hash;
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
	memset(table->bitmaps, 0, table->capacity * sizeof(table->bitmaps[0]));

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			*_hashtable_bitmap(table, index, info) |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			// can't fail here
			_hashtable_insert_during_resize(table, &hash, entry, bitmap, info);
			num_rehashed++;
		} while (hash != __HASHTABLE_EMPTY_HASH);
	}
	free(bitmap_to_free);
}

__AD_LINKAGE void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
				    const struct _hashtable_info *info)
{
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries, info);
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

__AD_LINKAGE _hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
						const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	_hashtable_idx_t index;
	while (!_hashtable_do_insert(table, hash, &index, info)) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return index;
}

__AD_LINKAGE void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
				    const struct _hashtable_info *info)
{
	_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
	_hashtable_idx_t home = _hashtable_hash_to_index(table, m->hash);
	_hashtable_uint_t distance = _hashtable_wrap_index(index - home, table->capacity);
	*_hashtable_bitmap(table, home, info) &= ~((_hashtable_bitmap_t)1 << distance);
	m->hash = __HASHTABLE_EMPTY_HASH;
	table->num_entries--;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	}
}

__AD_LINKAGE void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
		*_hashtable_bitmap(table, i, info) = 0;
	}
	table->num_entries = 0;
}



#undef __HASHTABLE_NEIGHBORHOOD
#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#elif defined(HASHTABLE_ROBINHOOD)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_MIN_VALID_HASH 1

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
} _hashtable_metadata_t;

static _attr_unused _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t start, _hashtable_uint_t i,
							   _hashtable_uint_t capacity)
{
	return (start + i) & (capacity - 1);
}

static _attr_unused _hashtable_hash_t _hashtable_get_hash(struct _hashtable *table, _hashtable_idx_t index,
							  const struct _hashtable_info *info)
{
	return table->metadata[index].hash;
}

static _attr_unused void _hashtable_set_hash(struct _hashtable *table, _hashtable_idx_t index, _hashtable_hash_t hash,
					     const struct _hashtable_info *info)
{
	table->metadata[index].hash = hash;
}

static _attr_unused _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
								 const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _attr_unused _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
							       const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static _attr_unused _hashtable_uint_t _hashtable_get_distance(struct _hashtable *table, _hashtable_idx_t index,
							      const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
	return (index - _hashtable_hash_to_index(table, hash)) & (table->capacity - 1);
}

static _attr_unused void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
}

__AD_LINKAGE void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
				  const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	assert((capacity & (capacity - 1)) == 0);
	table->storage = NULL;
	table->num_entries = 0;
	table->capacity = capacity;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
}

__AD_LINKAGE void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _attr_unused _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

__AD_LINKAGE bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
				    _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);

		if (m->hash == __HASHTABLE_EMPTY_HASH) {
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

__AD_LINKAGE _hashtable_idx_t _hashtable_get_next(struct _hashtable *table, size_t start,
						  const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static _attr_unused bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static _attr_unused void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

static _attr_unused bool _hashtable_insert_robin_hood(struct _hashtable *table, _hashtable_idx_t start,
						      _hashtable_uint_t distance, _hashtable_hash_t *phash,
						      void *entry, uint32_t *bitmap,
						      const struct _hashtable_info *info)
{
	void *tmp_entry = alloca(info->entry_size);
	for (_hashtable_uint_t i = 0;; i++, distance++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			if (bitmap) {
				_hashtable_slot_clear_needs_rehash(bitmap, index);
			}
			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		_hashtable_uint_t d = 0;
		if ((bitmap && _hashtable_slot_needs_rehash(bitmap, index)) ||
		    (d = _hashtable_get_distance(table, index, info)) < distance) {
			_hashtable_hash_t tmp_hash = _hashtable_get_hash(table, index, info);
			memcpy(tmp_entry, _hashtable_entry(table, index, info), info->entry_size);

			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);

			*phash = tmp_hash;
			memcpy(entry, tmp_entry, info->entry_size);

			if (bitmap && _hashtable_slot_needs_rehash(bitmap, index)) {
				_hashtable_slot_clear_needs_rehash(bitmap, index);
				return true;
			}

			distance = d;
		}
	}
}

static _attr_unused _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
							  const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index;
	_hashtable_uint_t i;
	for (i = 0;; i++) {
		index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
		_hashtable_uint_t d = _hashtable_get_distance(table, index, info);
		if (d < i) {
			_hashtable_hash_t h = _hashtable_get_hash(table, index, info);
			void *entry = _hashtable_entry(table, index, info);
			start = _hashtable_wrap_index(start, i + 1, table->capacity);
			_hashtable_insert_robin_hood(table, start, d + 1, &h, entry, NULL, info);
			break;
		}
	}
	_hashtable_set_hash(table, index, hash, info);
	return index;
}

static _attr_unused void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
					   const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;

	size_t bitmap_size = (old_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, bitmap, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(table, hash);
		}
	}

	free(bitmap_to_free);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	memmove(new_metadata, table->metadata, old_capacity * sizeof(table->metadata[0]));
	_hashtable_realloc_storage(table, info);
}

static _attr_unused void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
					 const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	size_t bitmap_size = (new_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	/* Need to use memmove in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	memmove(table->metadata, old_metadata, old_capacity * sizeof(table->metadata[0]));
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, bitmap, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(table, hash);
		}
	}
	free(bitmap_to_free);
}

__AD_LINKAGE void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
				    const struct _hashtable_info *info)
{
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries, info);
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

__AD_LINKAGE _hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
						const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	_hashtable_uint_t min_capacity = _hashtable_min_capacity(table->num_entries, info);
	if (min_capacity > table->capacity) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

__AD_LINKAGE void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
				    const struct _hashtable_info *info)
{
	_hashtable_metadata(table, index, info)->hash = __HASHTABLE_EMPTY_HASH;
	table->num_entries--;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	} else {
		for (_hashtable_uint_t i = 0;; i++) {
			_hashtable_idx_t current_index = _hashtable_wrap_index(index, i, table->capacity);
			_hashtable_idx_t next_index = _hashtable_wrap_index(index, i + 1, table->capacity);
			_hashtable_metadata_t *m = _hashtable_metadata(table, next_index, info);
			_hashtable_uint_t distance;
			if (m->hash == __HASHTABLE_EMPTY_HASH ||
			    (distance = _hashtable_get_distance(table, next_index, info)) == 0) {
				_hashtable_metadata(table, current_index, info)->hash = __HASHTABLE_EMPTY_HASH;
				break;
			}
			_hashtable_hash_t hash = _hashtable_get_hash(table, next_index, info);
			_hashtable_set_hash(table, current_index, hash, info);
			const void *entry = _hashtable_entry(table, next_index, info);
			memcpy(_hashtable_entry(table, current_index, info), entry, info->entry_size);
		}
	}
}

__AD_LINKAGE void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
	table->num_entries = 0;
}

#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#else
# error "No hashtable implementation selected"
#endif
