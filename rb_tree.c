#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

/* From Wikipedia:
 * 1) Each node is either red or black.
 * 2) The root is black.
 * 3) All leaves (NIL) are black.
 * 4) If a node is red, then both its children are black.
 * 5) Every path from a given node to any of its descendant NIL nodes contains the same number of
 *    black nodes.
 */

#ifndef container_of
#define container_of(pointer, type, member)				\
	((type *)((char *)(pointer) - (char *)&((type *)0)->member))
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(long) * 8)
#endif

enum rb_colors {
	RB_BLACK = 0,
	RB_RED   = 1
};

struct rb_node {
	unsigned long __parent_color;
	struct rb_node *left;
	struct rb_node *right;
};

struct rb_root {
	struct rb_node *node;
};

#define EMPTY_ROOT {NULL}

struct thing {
	int key;
	struct rb_node rb_node;
};

static inline struct thing *to_thing(struct rb_node *node)
{
	return container_of(node, struct thing, rb_node);
}

static struct rb_node *rb_find(struct rb_root *root, int key)
{
	struct rb_node *cur_node = root->node;
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

static inline struct rb_node *__rb_parent(unsigned long pc)
{
	return (struct rb_node *)(pc & ~1);
}

static inline unsigned long __rb_color(unsigned long pc)
{
	return (pc & 1);
}

static inline struct rb_node *rb_parent(struct rb_node *node)
{
	return __rb_parent(node->__parent_color);
}

static inline unsigned long rb_color(struct rb_node *node)
{
	return __rb_color(node->__parent_color);
}

static inline void rb_set_parent(struct rb_node *node, struct rb_node *parent)
{
	assert(((unsigned long)parent & 1) == 0);
	node->__parent_color = rb_color(node) | (unsigned long)parent;
}

static inline void rb_set_color(struct rb_node *node, unsigned long color)
{
	assert(color == RB_RED || color == RB_BLACK);
	node->__parent_color = (unsigned long)rb_parent(node) | color;
}

static inline bool rb_is_red(struct rb_node *node)
{
	return rb_color(node) == RB_RED;
}

static inline bool rb_is_black(struct rb_node *node)
{
	return rb_color(node) == RB_BLACK;
}

static inline bool rb_is_null_or_black(struct rb_node *node)
{
	return !node || rb_is_black(node);
}

static inline void rb_change_child(struct rb_node *old_child, struct rb_node *new_child,
				   struct rb_node *parent, struct rb_root *root)
{
	if (parent) {
		if (old_child == parent->left) {
			parent->left = new_child;
		} else {
			parent->right = new_child;
		}
	} else {
		root->node = new_child;
	}
}

static inline void rotate_left(struct rb_node *node, struct rb_root *root) {
	struct rb_node* nnew = node->right;
	node->right = nnew->left;
	if (node->right) {
		rb_set_parent(node->right, node);
	}
	nnew->left = node;
	struct rb_node *parent = rb_parent(node);
	rb_change_child(node, nnew, parent, root);
	rb_set_parent(nnew, parent);
	rb_set_parent(node, nnew);
}

static inline void rotate_right(struct rb_node *node, struct rb_root *root) {
	struct rb_node* nnew = node->left;
	node->left = nnew->right;
	if (node->left) {
		rb_set_parent(node->left, node);
	}
	nnew->right = node;
	struct rb_node *parent = rb_parent(node);
	rb_change_child(node, nnew, parent, root);
	rb_set_parent(nnew, parent);
	rb_set_parent(node, nnew);
}

static void rb_remove_repair(struct rb_root *root, struct rb_node *parent)
{
	// we only use node at the start to figure out which child node is,
	// since we are always the left child on the first iteration this is fine
	struct rb_node *node = NULL;
	for (;;) {
		struct rb_node *sibling = parent->right;
		bool node_is_left = true;
		if (node == sibling) {
			sibling = parent->left;
			node_is_left = false;
		}

		if (rb_is_red(sibling)) {
			if (node_is_left) {
				// rotate left at parent
				struct rb_node *tmp = sibling->left;
				parent->right = tmp;
				assert(rb_color(parent->right) == RB_BLACK);
				rb_set_parent(parent->right, parent);
				sibling->left = parent;
				rb_change_child(parent, sibling, rb_parent(parent), root);
				sibling->__parent_color = parent->__parent_color;
				rb_set_parent(parent, sibling);
				rb_set_color(parent, RB_RED);
				sibling = tmp;
			} else {
				// rotate right at parent
				struct rb_node *tmp = sibling->right;
				parent->left = tmp;
				assert(rb_color(parent->left) == RB_BLACK);
				rb_set_parent(parent->left, parent);
				sibling->right = parent;
				rb_change_child(parent, sibling, rb_parent(parent), root);
				sibling->__parent_color = parent->__parent_color;
				rb_set_parent(parent, sibling);
				rb_set_color(parent, RB_RED);
				sibling = tmp;
			}
			rb_set_color(parent, RB_RED);
		}

		if (rb_is_null_or_black(sibling->right) &&
		    rb_is_null_or_black(sibling->left)) {
			assert(rb_parent(sibling) == parent);
			rb_set_color(sibling, RB_RED);
			if (rb_is_red(parent)) {
				rb_set_color(parent, RB_BLACK);
			} else {
				node = parent;
				parent = rb_parent(node);
				if (parent) {
					continue;
				}
			}
			break;
		}

		if (node_is_left) {
			if (rb_is_null_or_black(sibling->right)) {
				// rotate right at sibling
				struct rb_node *tmp = sibling->left;
				sibling->left = tmp->right;
				if (sibling->left) {
					assert(rb_is_black(sibling->left));
					rb_set_parent(sibling->left, sibling);
				}
				tmp->right = sibling;
				parent->right = tmp;
				rb_set_parent(sibling, tmp);
				sibling = tmp;
			}
			// rotate left at parent
			parent->right = sibling->left;
			if (parent->right) {
				rb_set_parent(parent->right, parent);
			}
			sibling->left = parent;
			rb_change_child(parent, sibling, rb_parent(parent), root);
			sibling->__parent_color = parent->__parent_color;
			rb_set_parent(parent, sibling);
			rb_set_color(sibling->right, RB_BLACK);
			assert(rb_parent(sibling->right) == sibling);
		} else {
			if (rb_is_null_or_black(sibling->left)) {
				// rotate left at sibling
				struct rb_node *tmp = sibling->right;
				sibling->right = tmp->left;
				if (sibling->right) {
					assert(rb_is_black(sibling->right));
					rb_set_parent(sibling->right, sibling);
				}
				tmp->left = sibling;
				parent->left = tmp;
				rb_set_parent(sibling, tmp);
				sibling = tmp;
			}
			// rotate right at parent
			parent->left = sibling->right;
			if (parent->left) {
				rb_set_parent(parent->left, parent);
			}
			sibling->right = parent;
			rb_change_child(parent, sibling, rb_parent(parent), root);
			sibling->__parent_color = parent->__parent_color;
			rb_set_parent(parent, sibling);
			rb_set_color(sibling->left, RB_BLACK);
			assert(rb_parent(sibling->left) == sibling);
		}
		rb_set_color(parent, RB_BLACK);

		break;
	}
}

static void rb_remove_node(struct rb_root *root, struct rb_node *node)
{
	struct rb_node *child = node->right;
	struct rb_node *tmp = node->left;
	struct rb_node *rebalance;
	unsigned long pc;

	// removal + trivial repairs
	if (!tmp) {
		pc = node->__parent_color;
		struct rb_node *parent = __rb_parent(pc);
		rb_change_child(node, child, parent, root);
		if (child) {
			child->__parent_color = pc;
			rebalance = NULL;
		} else {
			rebalance = (__rb_color(pc) == RB_BLACK) ? parent : NULL;
		}
	} else if (!child) {
		pc = node->__parent_color;
		tmp->__parent_color = pc;
		struct rb_node *parent = __rb_parent(pc);
		rb_change_child(node, tmp, parent, root);
		rebalance = NULL;
	} else {
		struct rb_node *successor = child, *child2, *parent;

		tmp = child->left;
		if (!tmp) {
			parent = successor;
			child2 = successor->right;
		} else {
			do {
				parent = successor;
				successor = tmp;
				tmp = tmp->left;
			} while (tmp);
			child2 = successor->right;
			parent->left = child2;
			successor->right = child;
			rb_set_parent(child, successor);
		}

		tmp = node->left;
		successor->left = tmp;
		rb_set_parent(tmp, successor);

		pc = node->__parent_color;
		tmp = __rb_parent(pc);
		rb_change_child(node, successor, tmp, root);

		if (child2) {
			successor->__parent_color = pc;
			rb_set_color(child2, RB_BLACK); // not sure about this
			rb_set_parent(child2, parent);
			rebalance = NULL;
		} else {
			rebalance = rb_is_black(successor) ? parent : NULL;
			successor->__parent_color = pc;
		}
	}

	if (rebalance) {
		rb_remove_repair(root, rebalance);
	}
}

static struct rb_node *rb_remove_key(struct rb_root *root, int key)
{
	struct rb_node *node = rb_find(root, key);
	if (node) {
		rb_remove_node(root, node);
	}
	return node;
}

static bool rb_insert(struct rb_root *root, struct rb_node *node)
{
	// insert
	struct rb_node *parent = NULL;
	struct rb_node **cur = &root->node;
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
	rb_set_parent(node, parent);
	rb_set_color(node, RB_RED);
	node->left = NULL;
	node->right = NULL;
	*cur = node;

	// repair (generic)
	for (;;) {
		if (!parent) {
			rb_set_color(node, RB_BLACK);
			break;
		}

		if (rb_is_black(parent)) {
			break;
		}

		struct rb_node *grandparent = rb_parent(parent);
		bool parent_is_left = false;
		struct rb_node *uncle = grandparent->left;
		if (parent == grandparent->left) {
			parent_is_left = true;
			uncle = grandparent->right;
		}

		if (rb_is_null_or_black(uncle)) {
			if (parent_is_left && node == parent->right) {
				// rotate left at parent
				parent->right = node->left;
				if (parent->right) {
					rb_set_parent(parent->right, parent);
				}
				node->left = parent;
				grandparent->left = node;
			} else if (!parent_is_left && node == parent->left) {
				// rotate right at parent
				parent->left = node->right;
				if (parent->left) {
					rb_set_parent(parent->left, parent);
				}
				node->right = parent;
				grandparent->right = node;
			}
			rb_set_parent(node, grandparent); // can remove this?
			rb_set_parent(parent, node);
			parent = node;

			if (parent_is_left) {
				// rotate right at grandparent
				grandparent->left = parent->right;
				if (grandparent->left) {
					rb_set_parent(grandparent->left, grandparent);
				}
				parent->right = grandparent;

			} else {
				// rotate left at grandparent
				grandparent->right = parent->left;
				if (grandparent->right) {
					rb_set_parent(grandparent->right, grandparent);
				}
				parent->left = grandparent;
			}
			struct rb_node *greatgrandparent = rb_parent(grandparent);
			rb_change_child(grandparent, parent, greatgrandparent, root);

			rb_set_parent(parent, greatgrandparent);
			rb_set_color(parent, RB_BLACK);

			rb_set_parent(grandparent, parent);
			rb_set_color(grandparent, RB_RED);

			break;
		}

		rb_set_color(parent, RB_BLACK);
		rb_set_color(uncle, RB_BLACK);
		rb_set_color(grandparent, RB_RED);
		node = grandparent;
		parent = rb_parent(node);
	}
	return true;
}

static int black_depth;
static void debug_recursive_check_tree(struct rb_node *node, int cur_black_depth)
{
	if (!node) {
		if (black_depth == -1) {
			black_depth = cur_black_depth;
		}
		assert(black_depth == cur_black_depth);
		return;
	}
	if (rb_is_black(node)) {
		cur_black_depth++;
	}
	if (node->left) {
		assert(rb_parent(node->left) == node);
		assert(!(rb_is_red(node) && rb_is_red(node->left)));
	}
	if (node->right) {
		assert(rb_parent(node->right) == node);
		assert(!(rb_is_red(node) && rb_is_red(node->right)));
	}
	debug_recursive_check_tree(node->left, cur_black_depth);
	debug_recursive_check_tree(node->right, cur_black_depth);
}

static void debug_check_tree(struct rb_root *root)
{
	black_depth = -1;
	if (!root->node) {
		printf("empty\n");
		return;
	}
	assert(rb_is_black(root->node));
	assert(!rb_parent(root->node));
	debug_recursive_check_tree(root->node, 0);
}

int main(void)
{
	struct rb_root root = EMPTY_ROOT;
#if 0
	struct thing *thing = malloc(sizeof(*thing));
	thing->key = 1;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 2;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 3;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 4;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 5;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 6;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 7;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 8;
	rb_insert(&root, &thing->rb_node);
	thing = malloc(sizeof(*thing));
	thing->key = 9;
	rb_insert(&root, &thing->rb_node);
	debug_check_tree(&root);
	rb_remove_key(&root, 1);
	debug_check_tree(&root);
#endif
#if 0
	char buf[128];
	while (fgets(buf, sizeof(buf), stdin)) {
		int key = atoi(buf + 1);
		if (buf[0] == 'f') {
			struct rb_node *node = rb_find(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
			}
		} else if (buf[0] == 'i') {
			struct thing *thing = malloc(sizeof(*thing));
			thing->key = key;
			rb_insert(&root, &thing->rb_node);
			struct rb_node *node = rb_find(&root, key);
			assert(to_thing(node) == thing);
		} else if (buf[0] == 'r') {
			struct rb_node *node = rb_remove_key(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
				assert(rb_find(&root, key) == NULL);
			}
		}
		debug_check_tree(&root);
	}
#endif
#if 0
	// srand(0);
	srand(time(NULL));
	for (;;) {
		int key = rand();
		struct thing *thing = malloc(sizeof(*thing));
		thing->key = key;
		bool success = rb_insert(&root, &thing->rb_node);
		struct rb_node *node = rb_find(&root, key);
		assert(node);
		assert(!success || to_thing(node) == thing);
		assert(to_thing(node)->key == key);
		debug_check_tree(&root);
		printf("%d\n", black_depth);
		usleep(5);
	}
#endif
#if 1
	// srand(0);
	srand(time(NULL));
	for (unsigned int i = 0; ; i++) {
		{
			int key = rand() % 100;
			struct thing *thing = malloc(sizeof(*thing));
			thing->key = key;
			bool success = rb_insert(&root, &thing->rb_node);
			if (!success) {
				struct rb_node *node = rb_remove_key(&root, key);
				free(to_thing(node));
				success = rb_insert(&root, &thing->rb_node);
				assert(success);
			}
			assert(rb_find(&root, key) == &thing->rb_node);
		}
		{
			int key = rand() % 100;
			struct rb_node *node = rb_find(&root, key);
			if (node) {
				rb_remove_node(&root, node);
				assert(!rb_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % 1024 == 0) {
			debug_check_tree(&root);
			printf("%d\n", black_depth);
		}
	}
#endif
}
