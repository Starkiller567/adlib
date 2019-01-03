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

struct avl_node {
	uintptr_t __parent_balance;
	struct avl_node *left;
	struct avl_node *right;
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
	assert(((uintptr_t)parent & 0x3) == 0);
	node->__parent_balance = (node->__parent_balance & 0x3) | (uintptr_t)parent;
}

static inline void avl_set_balance(struct avl_node *node, int balance)
{
	assert(-1 <= balance && balance <= 1);
	node->__parent_balance = (node->__parent_balance & ~0x3) | (balance + 1);
}

static inline void avl_change_child(struct avl_node *old_child, struct avl_node *new_child,
                                    struct avl_node *parent, struct avl_root *root)
{
	if (parent) {
		if (old_child == parent->left) {
			parent->left = new_child;
		} else {
			assert(old_child == parent->right);
			parent->right = new_child;
		}
	} else {
		root->node = new_child;
	}
}

static int num_nodes;
static int debug_recursive_check_tree(struct avl_node *node)
{
	if (!node) {
		return 0;
	}
	num_nodes++;
	if (node->left) {
		assert(avl_parent(node->left) == node);
		assert(to_thing(node)->key > to_thing(node->left)->key);
	}
	if (node->right) {
		assert(avl_parent(node->right) == node);
		assert(to_thing(node)->key < to_thing(node->right)->key);
	}
	assert(!node->left || node->left != node->right);
	assert(node != node->left);
	assert(node != node->right);
	int ld = debug_recursive_check_tree(node->left);
	int rd = debug_recursive_check_tree(node->right);
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
	struct avl_node *cur_node = root->node;
	while (cur_node) {
		if (key == to_thing(cur_node)->key) {
			return cur_node;
		}
		if (key < to_thing(cur_node)->key) {
			cur_node = cur_node->left;
		} else {
			cur_node = cur_node->right;
		}
	}
	return NULL;
}

static bool avl_insert(struct avl_root *root, struct avl_node *node)
{
	// insert
	struct avl_node *parent = NULL;
	struct avl_node **cur = &root->node;
	while (*cur) {
		parent = *cur;
		if (to_thing(node)->key == to_thing(*cur)->key) {
			return false;
		}
		if (to_thing(node)->key < to_thing(*cur)->key) {
			cur = &(*cur)->left;
		} else {
			cur = &(*cur)->right;
		}
	}
	avl_set_parent(node, parent);
	avl_set_balance(node, 0);
	node->left = NULL;
	node->right = NULL;
	*cur = node;

	// repair (generic)
	for (;;) {
		if (!parent) {
			return true;
		}
		int balance = avl_balance(parent);
		if (node == parent->left) {
			balance--;
		} else {
			balance++;
		}

		struct avl_node *grandparent = avl_parent(parent);
		if (balance < -1) {
			assert(node == parent->left);
			assert(balance == -2);
			if (avl_balance(node) > 0) {
				// left-right rotate
				struct avl_node *x = parent;
				struct avl_node *z = node;
				struct avl_node *y = node->right;
				struct avl_node *t2 = y->right;
				struct avl_node *t3 = y->left;
				avl_change_child(x, y, grandparent, root);
				avl_set_parent(y, grandparent);
				y->right = x;
				avl_set_parent(x, y);
				y->left = z;
				avl_set_parent(z, y);
				x->left = t2;
				if (x->left) {
					avl_set_parent(x->left, x);
				}
				z->right = t3;
				if (z->right) {
					avl_set_parent(z->right, z);
				}
				if (avl_balance(y) < 0) {
					avl_set_balance(x, 1);
					avl_set_balance(z, 0);
				} else if (avl_balance(y) == 0) {
					avl_set_balance(x, 0);
					avl_set_balance(z, 0);
				} else {
					avl_set_balance(x, 0);
					avl_set_balance(z, -1);
				}
				avl_set_balance(y, 0);
				node = y;
			} else {
				avl_change_child(parent, node, grandparent, root);
				avl_set_parent(node, grandparent);
				parent->left = node->right;
				if (parent->left) {
					avl_set_parent(parent->left, parent);
				}
				node->right = parent;
				avl_set_parent(parent, node);
				if (avl_balance(node) == 0) {
					avl_set_balance(node, 1);
					avl_set_balance(parent, -1);
				} else {
					avl_set_balance(node, 0);
					avl_set_balance(parent, 0);
				}
			}
		} else if (balance > 1) {
			assert(node == parent->right);
			assert(balance == 2);
			if (avl_balance(node) < 0) {
				// right-left rotate
				struct avl_node *x = parent;
				struct avl_node *z = node;
				struct avl_node *y = node->left;
				struct avl_node *t2 = y->left;
				struct avl_node *t3 = y->right;
				avl_change_child(x, y, grandparent, root);
				avl_set_parent(y, grandparent);
				y->left = x;
				avl_set_parent(x, y);
				y->right = z;
				avl_set_parent(z, y);
				x->right = t2;
				if (x->right) {
					avl_set_parent(x->right, x);
				}
				z->left = t3;
				if (z->left) {
					avl_set_parent(z->left, z);
				}
				if (avl_balance(y) > 0) {
					avl_set_balance(x, -1);
					avl_set_balance(z, 0);
				} else if (avl_balance(y) == 0) {
					avl_set_balance(x, 0);
					avl_set_balance(z, 0);
				} else {
					avl_set_balance(x, 0);
					avl_set_balance(z, 1);
				}
				avl_set_balance(y, 0);
				node = y;
			} else {
				// rotate left
				avl_change_child(parent, node, grandparent, root);
				avl_set_parent(node, grandparent);
				parent->right = node->left;
				if (parent->right) {
					avl_set_parent(parent->right, parent);
				}
				node->left = parent;
				avl_set_parent(parent, node);
				if (avl_balance(node) == 0) {
					avl_set_balance(node, -1);
					avl_set_balance(parent, 1);
				} else {
					avl_set_balance(node, 0);
					avl_set_balance(parent, 0);
				}
			}
		} else {
			avl_set_balance(parent, balance);
			node = parent;
		}
		if (avl_balance(node) == 0) {
			return true;
		}
		parent = grandparent;
		/* debug_recursive_check_tree(node); */
	}
}

int main(void)
{
	struct avl_root root = EMPTY_ROOT;
	srand(time(NULL));
	/* srand(2); */
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
			struct thing *thing = malloc(sizeof(*thing));
			thing->key = key;
			avl_insert(&root, &thing->avl_node);
			struct avl_node *node = avl_find(&root, key);
			assert(to_thing(node) == thing);
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
#if 0
	for (unsigned int i = 0; ; i++) {
		int key = rand();
		struct thing *thing = malloc(sizeof(*thing));
		thing->key = key;
		bool success = avl_insert(&root, &thing->avl_node);
		struct avl_node *node = avl_find(&root, key);
		assert(node);
		assert(!success || to_thing(node) == thing);
		assert(to_thing(node)->key == key);
		int depth = debug_check_tree(&root);
		printf("max depth: %d\n", depth);
		printf("num nodes: %d\n", num_nodes);
	}
#endif
#if 1
	for (unsigned int i = 0; ; i++) {
		{
			int key = rand() % 10000;
			struct thing *thing = malloc(sizeof(*thing));
			thing->key = key;
			bool success = avl_insert(&root, &thing->avl_node);
			if (!success) {
				/*
				struct avl_node *node = avl_remove_key(&root, key);
				free(to_thing(node));
				success = avl_insert(&root, &thing->avl_node);
				assert(success);
				*/
			}
			if (success) assert(avl_find(&root, key) == &thing->avl_node);
		}
		{
			int key = rand() % 10000;
			struct avl_node *node = avl_find(&root, key);
			if (node) {
				/*
				avl_remove_node(&root, node);
				assert(!avl_find(&root, key));
				free(to_thing(node));
				*/
			}
		}

		// if (i % (1 << 20) == 0) {
		int depth = debug_check_tree(&root);
		printf("max depth: %d\n", depth);
		printf("num nodes: %d\n", num_nodes);
		// }
	}
#endif
}
