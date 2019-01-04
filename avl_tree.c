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

static inline bool avl_is_left_child(struct avl_node *child, struct avl_node *parent)
{
	if (child == parent->left) {
		return true;
	} else {
		assert(child == parent->right);
		return false;
	}
}

static inline void avl_change_child(struct avl_node *old_child, struct avl_node *new_child,
                                    struct avl_node *parent, struct avl_root *root)
{
	if (parent) {
		if (avl_is_left_child(old_child, parent)) {
			parent->left = new_child;
		} else {
			assert(old_child == parent->right);
			parent->right = new_child;
		}
	} else {
		root->node = new_child;
	}
}

static inline struct avl_node *avl_rotate_left(struct avl_root *root, struct avl_node *node)
{
	struct avl_node *right = node->right;
	node->right = right->left;
	if (node->right) {
		avl_set_parent(node->right, node);
	}
	right->left = node;
	avl_set_parent(node, right);
	if (avl_balance(right) == 0) {
		avl_set_balance(node, 1);
		avl_set_balance(right, -1);
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(right, 0);
	}
	return right;
}

static inline struct avl_node *avl_rotate_right(struct avl_root *root, struct avl_node *node)
{
	struct avl_node *left = node->left;
	node->left = left->right;
	if (node->left) {
		avl_set_parent(node->left, node);
	}
	left->right = node;
	avl_set_parent(node, left);
	if (avl_balance(left) == 0) {
		avl_set_balance(node, -1);
		avl_set_balance(left, 1);
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(left, 0);
	}
	return left;
}

static inline struct avl_node *avl_rotate_left_right(struct avl_root *root, struct avl_node *parent)
{
	struct avl_node *node = parent->left;
	struct avl_node *right = node->right;
	parent->left = right->right;
	if (parent->left) {
		avl_set_parent(parent->left, parent);
	}
	node->right = right->left;
	if (node->right) {
		avl_set_parent(node->right, node);
	}
	right->right = parent;
	avl_set_parent(parent, right);
	right->left = node;
	avl_set_parent(node, right);
	if (avl_balance(right) < 0) {
		avl_set_balance(parent, 1);
		avl_set_balance(node, 0);
	} else if (avl_balance(right) == 0) {
		avl_set_balance(parent, 0);
		avl_set_balance(node, 0);
	} else {
		avl_set_balance(parent, 0);
		avl_set_balance(node, -1);
	}
	avl_set_balance(right, 0);
	return right;
}

static inline struct avl_node *avl_rotate_right_left(struct avl_root *root, struct avl_node *parent)
{
	struct avl_node *node = parent->right;
	struct avl_node *left = node->left;
	parent->right = left->left;
	if (parent->right) {
		avl_set_parent(parent->right, parent);
	}
	node->left = left->right;
	if (node->left) {
		avl_set_parent(node->left, node);
	}
	left->left = parent;
	avl_set_parent(parent, left);
	left->right = node;
	avl_set_parent(node, left);
	if (avl_balance(left) > 0) {
		avl_set_balance(parent, -1);
		avl_set_balance(node, 0);
	} else if (avl_balance(left) == 0) {
		avl_set_balance(parent, 0);
		avl_set_balance(node, 0);
	} else {
		avl_set_balance(parent, 0);
		avl_set_balance(node, 1);
	}
	avl_set_balance(left, 0);
	return left;
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

static void avl_remove_node(struct avl_root *root, struct avl_node *node)
{
	struct avl_node *child;
	struct avl_node *parent;
	bool is_left;

	if (!node->left) {
		child = node->right;
	} else if (!node->right) {
		child = node->left;
	} else {
		child = node->right;
		parent = node;
		is_left = false;
		while (child->left) {
			parent = child;
			child = child->left;
			is_left = true;
		}

		assert(is_left == avl_is_left_child(child, parent));

		avl_change_child(node, child, avl_parent(node), root);
		child->__parent_balance = node->__parent_balance;

		if (node == parent) {
			child->left = node->left;
			avl_set_parent(child->left, child);

			parent = child;
		} else {
			parent->left = child->right;
			if (parent->left) {
				avl_set_parent(parent->left, parent);
			}

			child->right = node->right;
			avl_set_parent(child->right, child);

			child->left = node->left;
			avl_set_parent(child->left, child);


		}
		goto rebalance;
	}

	parent = avl_parent(node);
	if (!parent) {
		root->node = NULL;
		return;
	}
	is_left = avl_is_left_child(node, parent);
	if (is_left) {
		parent->left = child;
	} else {
		parent->right = child;
	}
	if (child) {
		avl_set_parent(child, parent);
	}

rebalance:;
	// repair (generic)
	struct avl_node *grandparent;
	for (;;) {
		grandparent = avl_parent(parent);
		int balance = avl_balance(parent);
		if (is_left) {
			if (balance == 0) {
				avl_set_balance(parent, 1);
				break;
			}
			if (balance > 0) {
				balance = avl_balance(parent->right);
				if (balance < 0) {
					node = avl_rotate_right_left(root, parent);
				} else {
					node = avl_rotate_left(root, parent);
				}
			} else {
				avl_set_balance(parent, 0);
				node = parent;
				goto cont;
			}
		} else {
			if (balance == 0) {
				avl_set_balance(parent, -1);
				break;
			}
			if (balance < 0) {
				balance = avl_balance(parent->left);
				if (balance > 0) {
					node = avl_rotate_left_right(root, parent);
				} else {
					node = avl_rotate_right(root, parent);
				}
			} else {
				avl_set_balance(parent, 0);
				node = parent;
				goto cont;
			}
		}
		avl_change_child(parent, node, grandparent, root);
		avl_set_parent(node, grandparent);
		if (balance == 0) {
			break;
		}

	cont:
		debug_recursive_check_tree(parent);
		parent = grandparent;
		if (!parent) {
			break;
		}
		is_left = avl_is_left_child(node, parent);
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
			break;
		}
		struct avl_node *grandparent = avl_parent(parent);
		int balance = avl_balance(parent);
		if (avl_is_left_child(node, parent)) {
			if (balance > 0) {
				avl_set_balance(parent, 0);
				break;
			}
			if (balance < 0) {
				if (avl_balance(node) > 0) {
					node = avl_rotate_left_right(root, parent);
				} else {
					node = avl_rotate_right(root, parent);
				}
				avl_change_child(parent, node, grandparent, root);
				avl_set_parent(node, grandparent);
				break;
			}
			avl_set_balance(parent, -1);
		} else {
			if (balance < 0) {
				avl_set_balance(parent, 0);
				break;
			}
			if (balance > 0) {
				if (avl_balance(node) < 0) {
					node = avl_rotate_right_left(root, parent);
				} else {
					node = avl_rotate_left(root, parent);
				}
				avl_change_child(parent, node, grandparent, root);
				avl_set_parent(node, grandparent);
				break;
			}
			avl_set_balance(parent, 1);

		}
		node = parent;
		parent = avl_parent(parent);
		/* debug_recursive_check_tree(node); */
	}
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
#if 1
	srand(0);
	for (unsigned int i = 0; i < 20000; i++) {
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
	return 0;
#endif
#if 1
	srand(time(NULL));
	for (unsigned int i = 0; ; i++) {
		{
			int key = rand() % 10000;
			struct thing *thing = malloc(sizeof(*thing));
			thing->key = key;
			bool success = avl_insert(&root, &thing->avl_node);
			if (!success) {
				struct avl_node *node = avl_remove_key(&root, key);
				free(to_thing(node));
				success = avl_insert(&root, &thing->avl_node);
				assert(success);
			}
			if (success) assert(avl_find(&root, key) == &thing->avl_node);
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

		if (i % (1 << 20) == 0) {
			int depth = debug_check_tree(&root);
			printf("max depth: %d\n", depth);
			printf("num nodes: %d\n", num_nodes);
		}
	}
#endif
}
