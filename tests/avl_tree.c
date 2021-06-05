#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include "avl_tree.h"
#include "macros.h"

static inline int avl_balance(const struct avl_node *node)
{
	return (int)(node->_parent_balance & 0x3) - 1;
}

struct thing {
	int key;
	struct avl_node avl_node;
};

#define to_thing(ptr) container_of(ptr, struct thing, avl_node)

static struct avl_node *avl_find(struct avl_root *root, int key)
{
	struct avl_node *cur = root->node;
	while (cur) {
		if (key == to_thing(cur)->key) {
			return cur;
		}
		int dir = AVL_RIGHT;
		if (key < to_thing(cur)->key) {
			dir = AVL_LEFT;
		}
		cur = cur->children[dir];
	}
	return NULL;
}

static struct avl_node *avl_remove_key(struct avl_root *root, int key)
{
	struct avl_node *node = avl_find(root, key);
	if (node) {
		avl_remove_node(root, node);
	}
	return node;
}

static bool avl_insert_key(struct avl_root *root, int key)
{
	struct avl_node *parent = NULL;
	struct avl_node *cur = root->node;
	int dir = AVL_LEFT;
	while (cur) {
		if (key == to_thing(cur)->key) {
			return false;
		}
		dir = AVL_RIGHT;
		if (key < to_thing(cur)->key) {
			dir = AVL_LEFT;
		}
		parent = cur;
		cur = cur->children[dir];
	}

	struct thing *thing = malloc(sizeof(*thing));
	thing->key = key;
	avl_insert_node(root, &thing->avl_node, parent, dir);
	return true;
}

static void _avl_destroy(struct avl_node *node)
{
	if (!node) {
		return;
	}
	_avl_destroy(node->children[AVL_LEFT]);
	_avl_destroy(node->children[AVL_RIGHT]);
	free(to_thing(node));
}

static void avl_destroy_tree(struct avl_root *root)
{
	_avl_destroy(root->node);
	root->node = NULL;
}

static int num_nodes;
static int debug_recursive_check_tree(struct avl_node *node)
{
	if (!node) {
		return 0;
	}
	num_nodes++;
	if (node->children[AVL_LEFT]) {
		assert(avl_parent(node->children[AVL_LEFT]) == node);
		assert(to_thing(node)->key > to_thing(node->children[AVL_LEFT])->key);
	}
	if (node->children[AVL_RIGHT]) {
		assert(avl_parent(node->children[AVL_RIGHT]) == node);
		assert(to_thing(node)->key < to_thing(node->children[AVL_RIGHT])->key);
	}
	assert(!node->children[AVL_LEFT] || node->children[AVL_LEFT] != node->children[AVL_RIGHT]);
	assert(node != node->children[AVL_LEFT]);
	assert(node != node->children[AVL_RIGHT]);
	int ld = debug_recursive_check_tree(node->children[AVL_LEFT]);
	int rd = debug_recursive_check_tree(node->children[AVL_RIGHT]);
	int balance = rd - ld;
	assert(-1 <= balance && balance <= 1);
	assert(balance == avl_balance(node));
	return (rd > ld ? rd : ld) + 1;
}

static int debug_check_tree(struct avl_root *root)
{
	num_nodes = 0;
	return debug_recursive_check_tree(root->node);
}

int main(void)
{
	unsigned int seed = 123456789;
	struct avl_root root = AVL_EMPTY_ROOT;
#if 0
	char buf[128];
	while (fgets(buf, sizeof(buf), stdin)) {
		int key = atoi(buf + 1);
		if (buf[0] == 'f') {
			struct avl_node *node = avl_find(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
			}
		} else if (buf[0] == 'i') {
			avl_insert_key(&root, key);
			struct avl_node *node = avl_find(&root, key);
			assert(to_thing(node)->key == key);
		} else if (buf[0] == 'r') {
			struct avl_node *node = avl_remove_key(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
				assert(avl_find(&root, key) == NULL);
			}
		}
		debug_check_tree(&root);
	}
#endif

#if 1
	srand(seed);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = rand();
		avl_insert_key(&root, key);
	}
	debug_check_tree(&root);
	srand(seed);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = rand();
		struct avl_node *node = avl_find(&root, key);
		assert(node && to_thing(node)->key == key);
	}
	srand(seed);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = rand();
		struct avl_node *node = avl_remove_key(&root, key);
		if (node) {
			assert(to_thing(node)->key == key);
			free(to_thing(node));
		}
		if (i % 1024 == 0) {
			debug_check_tree(&root);
		}
	}
	assert(root.node == NULL);
#endif

#if 1
	srand(seed);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = rand();
		avl_insert_key(&root, key);
	}
	debug_check_tree(&root);
	struct thing *prev = NULL;
	avl_foreach(&root, cur) {
		struct thing *thing = to_thing(cur);
		if (prev) {
			assert(prev->key < thing->key);
		}
		prev = thing;
	}
	avl_destroy_tree(&root);
#endif

#if 1
	srand(seed);
	const int max_key = 1024;
	for (unsigned int i = 0; i < 200000; i++) {
		{
			int key = rand() % max_key;
			bool success = avl_insert_key(&root, key);
			if (!success) {
				struct avl_node *node = avl_remove_key(&root, key);
				free(to_thing(node));
				success = avl_insert_key(&root, key);
				assert(success);
			}
			assert(to_thing(avl_find(&root, key))->key == key);
		}
		{
			int key = rand() % max_key;
			struct avl_node *node = avl_find(&root, key);
			if (node) {
				avl_remove_node(&root, node);
				assert(!avl_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % 1024 == 0) {
			debug_check_tree(&root);
			// printf("max depth: %d\n", depth);
			// printf("num nodes: %d\n", num_nodes);
		}
	}
	avl_destroy_tree(&root);
#endif
}
