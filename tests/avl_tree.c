#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include "avl_tree.h"
#include "macros.h"
#include "random.h"
#include "testing.h"

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

static bool check_tree_recursive(struct avl_node *node, int *depth)
{
	if (!node) {
		*depth = 0;
		return true;
	}
	if (node->children[AVL_LEFT]) {
		CHECK(avl_parent(node->children[AVL_LEFT]) == node);
		CHECK(to_thing(node)->key > to_thing(node->children[AVL_LEFT])->key);
	}
	if (node->children[AVL_RIGHT]) {
		CHECK(avl_parent(node->children[AVL_RIGHT]) == node);
		CHECK(to_thing(node)->key < to_thing(node->children[AVL_RIGHT])->key);
	}
	CHECK(!node->children[AVL_LEFT] || node->children[AVL_LEFT] != node->children[AVL_RIGHT]);
	CHECK(node != node->children[AVL_LEFT]);
	CHECK(node != node->children[AVL_RIGHT]);
	int ld, rd;
	CHECK(check_tree_recursive(node->children[AVL_LEFT], &ld));
	CHECK(check_tree_recursive(node->children[AVL_RIGHT], &rd));
	int balance = rd - ld;
	CHECK(-1 <= balance && balance <= 1);
	CHECK(balance == avl_balance(node));
	*depth = (rd > ld ? rd : ld) + 1;
	return true;
}

static bool check_tree(struct avl_root *root)
{
	int depth;
	return check_tree_recursive(root->node, &depth);
}

RANDOM_TEST(insert_find_remove, 2, 0, UINT64_MAX)
{
	const unsigned int N = 200000;
	struct avl_root root = AVL_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		avl_insert_key(&root, key);
	}
	CHECK(check_tree(&root));
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		struct avl_node *node = avl_find(&root, key);
		CHECK(node && to_thing(node)->key == key);
	}
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		struct avl_node *node = avl_remove_key(&root, key);
		if (node) {
			CHECK(to_thing(node)->key == key);
			free(to_thing(node));
		}
		if (i % 1024 == 0) {
			CHECK(check_tree(&root));
		}
	}
	CHECK(root.node == NULL);
	return true;
}

RANDOM_TEST(foreach, 2, 0, UINT64_MAX)
{
	struct avl_root root = AVL_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = random_next_u32(&rng);
		avl_insert_key(&root, key);
	}
	CHECK(check_tree(&root));
	struct thing *prev = NULL;
	avl_foreach(&root, cur) {
		struct thing *thing = to_thing(cur);
		if (prev) {
			CHECK(prev->key < thing->key);
		}
		prev = thing;
	}
	avl_destroy_tree(&root);
	return true;
}

RANDOM_TEST(random_insert_find_remove, 2, 0, UINT64_MAX)
{
	struct avl_root root = AVL_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < 200000; i++) {
		const int max_key = 1024;
		{
			int key = random_next_u32(&rng) % max_key;
			bool success = avl_insert_key(&root, key);
			if (!success) {
				struct avl_node *node = avl_remove_key(&root, key);
				CHECK(node);
				free(to_thing(node));
				success = avl_insert_key(&root, key);
				CHECK(success);
			}
			CHECK(to_thing(avl_find(&root, key))->key == key);
		}
		{
			int key = random_next_u32(&rng) % max_key;
			struct avl_node *node = avl_find(&root, key);
			if (node) {
				avl_remove_node(&root, node);
				CHECK(!avl_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % 1024 == 0) {
			CHECK(check_tree(&root));
		}
	}
	CHECK(check_tree(&root));
	avl_destroy_tree(&root);
	return true;
}

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
		assert(check_tree(&root));
	}
#endif
