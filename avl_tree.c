#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#ifndef container_of
#include <stddef.h>
// from wikipedia, the ternary operator forces matching types on pointer and member
#define container_of(ptr, type, member)				\
((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#endif

// don't change these without changing avl_d2b and avl_b2d
#define AVL_LEFT 0
#define AVL_RIGHT 1

struct avl_node {
	uintptr_t __parent_balance;
	struct avl_node *children[2];
};

struct avl_root {
	struct avl_node *node;
};

#define EMPTY_ROOT {NULL}

struct thing {
	int key;
	struct avl_node avl_node;
};

#define to_thing(ptr) container_of(ptr, struct thing, avl_node)

static inline int avl_d2b(int dir)
{
	/* assert(dir == AVL_LEFT || dir == AVL_RIGHT); */
	return 2 * dir - 1;
}

static inline int avl_b2d(int balance)
{
	/* assert(balance == -1 || balance == 1); */
	return (balance + 1) / 2;
}

static inline struct avl_node *avl_parent(struct avl_node *node)
{
	return (struct avl_node *)(node->__parent_balance & ~0x3);
}

static inline int avl_balance(struct avl_node *node)
{
	return (int)(node->__parent_balance & 0x3) - 1;
}

static inline void avl_set_parent(struct avl_node *node, struct avl_node *parent)
{
	/* assert(((uintptr_t)parent & 0x3) == 0); */
	node->__parent_balance = (node->__parent_balance & 0x3) | (uintptr_t)parent;
}

static inline void avl_set_balance(struct avl_node *node, int balance)
{
	/* assert(-1 <= balance && balance <= 1); */
	node->__parent_balance = (node->__parent_balance & ~0x3) | (balance + 1);
}

static inline void avl_change_child(struct avl_node *old_child, struct avl_node *new_child,
                                    struct avl_node *parent, struct avl_root *root)
{
	if (parent) {
		if (old_child == parent->children[AVL_LEFT]) {
			parent->children[AVL_LEFT] = new_child;
		} else {
			/* assert(old_child == parent->children[AVL_RIGHT]); */
			parent->children[AVL_RIGHT] = new_child;
		}
	} else {
		root->node = new_child;
	}
}

static inline int avl_dir_of_child(struct avl_node *child, struct avl_node *parent)
{
	if (child == parent->children[AVL_LEFT]) {
		return AVL_LEFT;
	} else {
		/* assert(child == parent->children[AVL_RIGHT]); */
		return AVL_RIGHT;
	}
}

static inline struct avl_node *avl_single_rotate(struct avl_node *node, int dir)
{
	int left_dir = dir;
	int right_dir = 1 - dir;
	struct avl_node *right = node->children[right_dir];
	node->children[right_dir] = right->children[left_dir];
	if (node->children[right_dir]) {
		avl_set_parent(node->children[right_dir], node);
	}
	right->children[left_dir] = node;
	avl_set_parent(node, right);
	if (avl_balance(right) == 0) {
		avl_set_balance(node, avl_d2b(right_dir));
		avl_set_balance(right, avl_d2b(left_dir));
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(right, 0);
	}
	return right;
}

static inline struct avl_node *avl_double_rotate(struct avl_node *parent, int dir)
{
	int left_dir = dir;
	int right_dir = 1 - dir;
	struct avl_node *node = parent->children[right_dir];
	struct avl_node *left = node->children[left_dir];
	parent->children[right_dir] = left->children[left_dir];
	if (parent->children[right_dir]) {
		avl_set_parent(parent->children[right_dir], parent);
	}
	node->children[left_dir] = left->children[right_dir];
	if (node->children[left_dir]) {
		avl_set_parent(node->children[left_dir], node);
	}
	left->children[left_dir] = parent;
	avl_set_parent(parent, left);
	left->children[right_dir] = node;
	avl_set_parent(node, left);
	if (avl_balance(left) == 0) {
		avl_set_balance(parent, 0);
		avl_set_balance(node, 0);
	} else if (avl_b2d(avl_balance(left)) == right_dir) {
		avl_set_balance(parent, avl_d2b(left_dir));
		avl_set_balance(node, 0);
	} else {
		avl_set_balance(parent, 0);
		avl_set_balance(node, avl_d2b(right_dir));
	}
	avl_set_balance(left, 0);
	return left;
}

static struct avl_node *avl_rotate(struct avl_node *node, int dir)
{
	/* assert(dir == AVL_LEFT || dir == AVL_RIGHT); */
	int left_dir = dir;
	int right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	if (avl_balance(child) == avl_d2b(left_dir)) {
		node = avl_double_rotate(node, left_dir);
	} else {
		node = avl_single_rotate(node, left_dir);
	}
	return node;
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

static void avl_remove_node(struct avl_root *root, struct avl_node *node)
{
	struct avl_node *child;
	struct avl_node *parent;
	int dir;

	if (!node->children[AVL_LEFT]) {
		child = node->children[AVL_RIGHT];
	} else if (!node->children[AVL_RIGHT]) {
		child = node->children[AVL_LEFT];
	} else {
		// replace with the leftmost node in the right subtree
		// or the rightmost node in the left subtree depending
		// on which subtree is higher
		int balance = avl_balance(node);
		dir = balance == 0 ? AVL_RIGHT : avl_b2d(balance);

		child = node->children[dir];
		parent = node;

		if (child->children[1 - dir]) {
			dir = 1 - dir;
			while (child->children[dir]) {
				parent = child;
				child = child->children[dir];
			}
		}

		avl_change_child(node, child, avl_parent(node), root);
		child->__parent_balance = node->__parent_balance;

		int left_dir = dir;
		int right_dir = 1 - dir;

		struct avl_node *right = child->children[right_dir];

		child->children[right_dir] = node->children[right_dir];
		avl_set_parent(child->children[right_dir], child);

		if (node == parent) {
			parent = child;
		} else {
			parent->children[left_dir] = right;
			if (parent->children[left_dir]) {
				avl_set_parent(parent->children[left_dir], parent);
			}

			child->children[left_dir] = node->children[left_dir];
			avl_set_parent(child->children[left_dir], child);
		}
		goto rebalance;
	}

	parent = avl_parent(node);
	if (!parent) {
		root->node = NULL;
		return;
	}
	dir = avl_dir_of_child(node, parent);
	parent->children[dir] = child;
	if (child) {
		avl_set_parent(child, parent);
	}

rebalance:
	for (;;) {
		struct avl_node *grandparent = avl_parent(parent);
		int left_dir = dir;
		int right_dir = 1 - dir;
		int balance = avl_balance(parent);
		if (balance == 0) {
			avl_set_balance(parent, avl_d2b(right_dir));
			break;
		}
		if (balance == avl_d2b(right_dir)) {
			balance = avl_balance(parent->children[right_dir]);
			node = avl_rotate(parent, left_dir);

			avl_change_child(parent, node, grandparent, root);
			avl_set_parent(node, grandparent);
			if (balance == 0) {
				break;
			}
		} else {
			avl_set_balance(parent, 0);
			node = parent;
		}

		/* debug_recursive_check_tree(parent); */
		parent = grandparent;
		if (!parent) {
			break;
		}
		dir = avl_dir_of_child(node, parent);
	}
}

static struct avl_node *avl_remove_key(struct avl_root *root, int key)
{
	struct avl_node *node = avl_find(root, key);
	if (node) {
		avl_remove_node(root, node);
	}
	return node;
}

static void avl_insert_node(struct avl_root *root, struct avl_node *node, struct avl_node *parent, int dir)
{
	assert(((uintptr_t)node & 0x3) == 0);
	avl_set_parent(node, parent);
	avl_set_balance(node, 0);
	node->children[AVL_LEFT] = NULL;
	node->children[AVL_RIGHT] = NULL;

	if (!parent) {
		root->node = node;
		return;
	}
	parent->children[dir] = node;

	for (;;) {
		struct avl_node *grandparent = avl_parent(parent);
		int left_dir = dir;
		int right_dir = 1 - dir;
		int balance = avl_balance(parent);
		if (balance == avl_d2b(right_dir)) {
			avl_set_balance(parent, 0);
			break;
		}
		if (balance == avl_d2b(left_dir)) {
			node = avl_rotate(parent, right_dir);
			avl_change_child(parent, node, grandparent, root);
			avl_set_parent(node, grandparent);
			break;
		}

		avl_set_balance(parent, avl_d2b(left_dir));

		node = parent;
		parent = grandparent;
		if (!parent) {
			break;
		}
		dir = avl_dir_of_child(node, parent);
		/* debug_recursive_check_tree(node); */
	}
}

static bool avl_insert_key(struct avl_root *root, int key)
{
	struct avl_node *parent = NULL;
	struct avl_node *cur = root->node;
	int dir;
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

int main(void)
{
	struct avl_root root = EMPTY_ROOT;
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
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		avl_insert_key(&root, key);
	}
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		struct avl_node *node = avl_find(&root, key);
		assert(node && to_thing(node)->key == key);
	}
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		struct avl_node *node = avl_remove_key(&root, key);
		assert(!node || to_thing(node)->key == key);
	}
	assert(root.node == NULL);
	return 0;
#endif

#if 1
	srand(time(NULL));
	for (unsigned int i = 0; ; i++) {
		{
			int key = rand() % 10000;
			bool success = avl_insert_key(&root, key);
			if (!success) {
				struct avl_node *node = avl_remove_key(&root, key);
				free(to_thing(node));
				success = avl_insert_key(&root, key);
				assert(success);
			}
			assert(to_thing(avl_find(&root, key))->key == key);
		}
		debug_check_tree(&root);
		{
			int key = rand() % 10000;
			struct avl_node *node = avl_find(&root, key);
			if (node) {
				avl_remove_node(&root, node);
				assert(!avl_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % (1 << 10) == 0) {
			int depth = debug_check_tree(&root);
			printf("max depth: %d\n", depth);
			printf("num nodes: %d\n", num_nodes);
		}
	}
#endif
}
