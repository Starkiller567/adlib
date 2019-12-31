#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, THRESHOLD, ...)

#define THRESHOLD 8

typedef int key_type;
typedef int item_type;

#define BLOCK_SIZE 128
static const unsigned char itable_INVALID_INDEX = -1;

struct itable_block {
	unsigned char first[BLOCK_SIZE];
	unsigned char next[BLOCK_SIZE];
	unsigned int  hash[BLOCK_SIZE];
	key_type      key[BLOCK_SIZE];
};

struct itable {
	unsigned int num_items;
	unsigned int size;
	struct itable_block *blocks;
	item_type *items;
};

static inline unsigned int __itable_hash_to_idx(unsigned int hash, unsigned int table_size)
{
	return hash & (table_size - 1);
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
	size_t num_blocks = ((size + BLOCK_SIZE - 1) / BLOCK_SIZE);
	size = num_blocks * BLOCK_SIZE;
	size_t blocks_size = num_blocks * sizeof(*table->blocks);
	size_t items_size = size * sizeof(*table->items);
	char *mem = malloc(blocks_size + items_size);
	table->blocks = (struct itable_block *)mem;
	mem += blocks_size;
	table->items = (item_type *)mem;
	for (unsigned int i = 0; i < num_blocks; i++) {
		for (unsigned int j = 0; j < BLOCK_SIZE; j++) {
			table->blocks[i].first[j] = itable_INVALID_INDEX;
			table->blocks[i].hash[j] = 0;
		}
	}
	table->num_items = 0;
	table->size = size;
}

static void itable_destroy(struct itable *table)
{
	free(table->blocks);
	table->size = 0;
}

static item_type *itable_lookup(struct itable *table, key_type key, unsigned int hash)
{
	hash = hash == 0 ? -1 : hash;
	unsigned int global_index = __itable_hash_to_idx(hash, table->size);
	unsigned int block_index = global_index / BLOCK_SIZE;
	unsigned int index = global_index % BLOCK_SIZE;
	struct itable_block *block = &table->blocks[block_index];
	index = block->first[index];
	for (;;) {
		if (index == itable_INVALID_INDEX) {
			return NULL;
		}
		key_type const *a = &key;
		key_type const *b = &block->key[index];
		if (hash == block->hash[index] && (*a == *b)) {
			return &table->items[block_index * BLOCK_SIZE + index];
		}
		index = block->next[index];
	}
}

static bool itable_remove(struct itable *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item)
{
	hash = hash == 0 ? -1 : hash;
	unsigned int global_index = __itable_hash_to_idx(hash, table->size);
	unsigned int block_index = global_index / BLOCK_SIZE;
	unsigned int start = global_index % BLOCK_SIZE;

	struct itable_block *block = &table->blocks[block_index];
	unsigned char *indirect = &block->first[start];
	unsigned char index = *indirect;
	for (;;) {
		if (index == itable_INVALID_INDEX) {
			return false;
		}
		key_type const *a = &key;
		key_type const *b = &block->key[index];
		if (hash == block->hash[index] && (*a == *b)) {
			break;
		}
		indirect = &block->next[index];
		index = *indirect;
	}
	if (ret_key) {
		*ret_key = block->key[index];
	}
	if (ret_item) {
		*ret_item = table->items[block_index * BLOCK_SIZE + index];
	}
	*indirect = block->next[index];
	block->hash[index] = 0;
	table->num_items--;
	return true;
}

static item_type *__itable_insert_internal(struct itable *table, key_type key, unsigned int hash)
{
	unsigned int global_index = __itable_hash_to_idx(hash, table->size);
	unsigned int block_index = global_index / BLOCK_SIZE;
	unsigned int start = global_index % BLOCK_SIZE;

	struct itable_block *block = &table->blocks[block_index];
	unsigned int index = start;
	do {
		if (block->hash[index] == 0) {
			break;
		}
		index = (index + 1) % BLOCK_SIZE;
	} while (index != start);

	if (block->hash[index] != 0) {
		return NULL;
	}


	block->hash[index] = hash;
	block->next[index] = block->first[start];
	block->key[index] = key;
	block->first[start] = index;

	return &table->items[block_index * BLOCK_SIZE + index];
}

static void itable_resize(struct itable *table, unsigned int size)
{
	struct itable new_table;
	itable_init(&new_table, size);

	for (unsigned int i = 0; i < table->size; i++) {
		unsigned int block_index = i / BLOCK_SIZE;
		unsigned int index = i % BLOCK_SIZE;
		struct itable_block *block = &table->blocks[block_index];
		if (block->hash[index] != 0) {
			item_type *item = __itable_insert_internal(&new_table, block->key[index],
								   block->hash[index]);
			*item = table->items[i];
		}
	}
	new_table.num_items = table->num_items;
	itable_destroy(table);
	*table = new_table;
}

static item_type *itable_insert(struct itable *table, key_type key, unsigned int hash)
{
	table->num_items++;
	hash = hash == 0 ? -1 : hash;
	item_type *item = __itable_insert_internal(table, key, hash);
	if (item) {
		return item;
	}
	itable_resize(table, 2 * table->size);
	item = __itable_insert_internal(table, key, hash);
	return item;
}

#endif
