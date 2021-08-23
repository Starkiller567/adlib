#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BTREE_K 1
#define BTREE_2K (2 * BTREE_K)

typedef int key_t;

struct btree_node {
	unsigned char num_keys;
	unsigned char position;
	struct btree_node *parent;
	key_t keys[BTREE_2K];
	struct btree_node *children[BTREE_2K + 1]; // TODO most nodes are leafs and don't habe children
};

struct btree {
	struct btree_node *root;
};

struct btree_position {
	struct btree_node *node;
	size_t index;
};

static int compare(const key_t *key1, const key_t *key2)
{
	return *key1 - *key2;
}

static void btree_init(struct btree *btree)
{
	btree->root = calloc(1, sizeof(*btree->root));
}

static bool btree_bsearch(struct btree_node *node, const key_t *key, size_t *ret_idx)
{
	size_t start = 0;
	size_t end = node->num_keys;
	while (end > start) {
		size_t idx = start + (end - start) / 2;
		int cmp = compare(key, &node->keys[idx]);
		if (cmp < 0) {
			end = idx;
		} else if (cmp > 0) {
			start = idx + 1;
		} else {
			*ret_idx = idx;
			return true;
		}
	}
	*ret_idx = start;
	return false;
}

static bool btree_find(struct btree *tree, key_t key)
{
	struct btree_node *node = tree->root;
	while (node) {
		size_t idx;
		if (btree_bsearch(node, &key, &idx)) {
			return true;
		}
		node = node->children[idx];
	}
	return false;
}

static void btree_swap(key_t *key1, key_t *key2)
{
	key_t tmp;
	memcpy(&tmp, key1, sizeof(*key1));
	memcpy(key1, key2, sizeof(*key1));
	memcpy(key2, &tmp, sizeof(*key1));
}

static void btree_node_insert_key(struct btree_node *node, size_t idx, key_t key)
{
	assert(node->num_keys < BTREE_2K);
	memmove(node->keys + idx + 1, node->keys + idx, (node->num_keys - idx) * sizeof(node->keys[0]));
	node->keys[idx] = key;
	node->num_keys++;
}

static void btree_split(struct btree *btree, struct btree_node *node, size_t idx)
{
	if (node == btree->root) {
		struct btree_node *parent = calloc(1, sizeof(*parent));
		parent->children[0] = node;
		node->parent = parent;
		node->position = 0;
		btree->root = parent;
	}
	struct btree_node *parent = node->parent;
	if (parent->num_keys == BTREE_2K) {
		btree_split(btree, parent, node->position);
		parent = node->parent;
	}

	struct btree_node *new_node = calloc(1, sizeof(*new_node));
	node->num_keys = BTREE_K;
	new_node->num_keys = BTREE_K;
	memcpy(new_node->keys, node->keys + BTREE_K, BTREE_K * sizeof(node->keys[0]));
	for (size_t i = 0; i < BTREE_K + 1; i++) {
		struct btree_node *child = node->children[i + BTREE_K];
		new_node->children[i] = child;
		if (child) {
			child->parent = new_node;
			child->position = i;
			node->children[i + BTREE_K] = NULL;
		}
	}

	btree_node_insert_key(parent, node->position, node->keys[--node->num_keys]);
	for (size_t i = parent->num_keys + 1; i >= node->position + 1; i--) {
		struct btree_node *child = parent->children[i];
		parent->children[i + 1] = child;
		if (child) {
			child->position = i + 1;
		}
	}
	parent->children[node->position + 1] = new_node;
	new_node->parent = parent;
	new_node->position = node->position + 1;
}

static bool btree_insert(struct btree *btree, key_t key)
{
	struct btree_node *node = btree->root;
	size_t idx;
	for (;;) {
		if (btree_bsearch(node, &key, &idx)) {
			return false;
		}
		if (!node->children[idx]) {
			break;
		}
		node = node->children[idx];
	}
	if (node->num_keys == BTREE_2K) {
		btree_split(btree, node, idx);
		if (idx >= BTREE_K) {
			node = node->parent->children[node->position + 1];
			idx -= BTREE_K;
		}
	}
	btree_node_insert_key(node, idx, key);
	return true;
}

int main(int argc, char **argv)
{
	struct btree btree;
	btree_init(&btree);
	for (size_t i = 0; i < 10000; i++) {
		int r = rand() % 1024;
		btree_insert(&btree, r);
		assert(btree_find(&btree, r));
	}
}
