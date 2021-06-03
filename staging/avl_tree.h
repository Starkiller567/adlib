#ifndef __AVL_TREE_INCLUDE__
#define __AVL_TREE_INCLUDE__

#include <stdint.h>
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

#define AVL_EMPTY_ROOT ((struct avl_root){NULL})

#define avl_foreach(root) for (struct avl_node *cur = avl_first(root); cur; cur = avl_next(cur))

static struct avl_node *avl_first(struct avl_root root);
static struct avl_node *avl_next(struct avl_node *node);
static void avl_insert_node(struct avl_root *root, struct avl_node *node, struct avl_node *parent, int dir);
static void avl_remove_node(struct avl_root *root, struct avl_node *node);
static inline struct avl_node *avl_parent(struct avl_node *node);



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                           IMPLEMENTATION                                             //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

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

static struct avl_node *avl_single_rotate(struct avl_node *node, int dir)
{
	int left_dir = dir;
	int right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	node->children[right_dir] = child->children[left_dir];
	if (node->children[right_dir]) {
		avl_set_parent(node->children[right_dir], node);
	}
	child->children[left_dir] = node;
	avl_set_parent(node, child);

#if 0
	if (avl_balance(child) == 0) {
		avl_set_balance(node, avl_d2b(right_dir));
		avl_set_balance(child, avl_d2b(left_dir));
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(child, 0);
	}
#else
	int balance = (~avl_balance(child) & 1) * avl_d2b(right_dir);
	avl_set_balance(node, balance);
	avl_set_balance(child, -balance);
#endif
	return child;
}

static struct avl_node *avl_double_rotate(struct avl_node *node, int dir)
{
	int left_dir = dir;
	int right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	struct avl_node *left = child->children[left_dir];
	node->children[right_dir] = left->children[left_dir];
	if (node->children[right_dir]) {
		avl_set_parent(node->children[right_dir], node);
	}
	child->children[left_dir] = left->children[right_dir];
	if (child->children[left_dir]) {
		avl_set_parent(child->children[left_dir], child);
	}
	left->children[left_dir] = node;
	avl_set_parent(node, left);
	left->children[right_dir] = child;
	avl_set_parent(child, left);

#if 1
	int d = avl_balance(left) - avl_d2b(right_dir);
	d = (d | (d >> 1)) & 1;
	int x = avl_d2b(d * right_dir + (1 - d) * left_dir);
	int b = avl_balance(left) & 1;
	int node_balance = b * ((1 - d) * x);
	int child_balance = b * (d * x);
	avl_set_balance(node, node_balance);
	avl_set_balance(child, child_balance);
#else
	if (avl_balance(left) == 0) {
		avl_set_balance(node, 0);
		avl_set_balance(child, 0);
	} else if (avl_balance(left) == avl_d2b(right_dir)) {
		avl_set_balance(node, avl_d2b(left_dir));
		avl_set_balance(child, 0);
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(child, avl_d2b(right_dir));
	}
#endif

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

static struct avl_node *avl_first(struct avl_root root)
{
	struct avl_node *node = NULL;
	struct avl_node *cur = root.node;
	while (cur) {
		node = cur;
		cur = cur->children[AVL_LEFT];
	}
	return node;
}

static struct avl_node *avl_next(struct avl_node *node)
{
	if (node->children[AVL_RIGHT]) {
		node = node->children[AVL_RIGHT];
		while (node->children[AVL_LEFT]) {
			node = node->children[AVL_LEFT];
		}
		return node;
	}

	struct avl_node *parent = avl_parent(node);
	while (parent && node == parent->children[AVL_RIGHT]) {
		node = parent;
		parent = avl_parent(node);
	}

	return parent;
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

		parent = grandparent;
		if (!parent) {
			break;
		}
		dir = avl_dir_of_child(node, parent);
	}
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
	}
}

#endif
