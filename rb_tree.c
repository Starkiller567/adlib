#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

/* From Wikipedia:
 * 1) Each node is either red or black.
 * 2) The root is black.
 * 3) All leaves (NIL) are black.
 * 4) If a node is red, then both its children are black.
 * 5) Every path from a given node to any of its descendant NIL nodes contains the same number of
 *    black nodes.
 */

#ifndef container_of
#include <stddef.h>
// from wikipedia, the ternary operator forces matching types on pointer and member
#define container_of(ptr, type, member)				\
((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#endif

#define RB_LEFT 0
#define RB_RIGHT 1

#define RB_RED 0
#define RB_BLACK 1

struct rb_node {
	uintptr_t __parent_color;
	struct rb_node *children[2];
};

struct rb_root {
	struct rb_node *node;
};

#define EMPTY_ROOT {NULL}

#define rb_foreach(root) for (struct rb_node *cur = rb_first(root); cur; cur = rb_next(cur))

struct thing {
	int key;
	struct rb_node rb_node;
};

#define to_thing(ptr) container_of(ptr, struct thing, rb_node)

static inline struct rb_node *__rb_parent(uintptr_t pc)
{
	return (struct rb_node *)(pc & ~1);
}

static inline uintptr_t __rb_color(uintptr_t pc)
{
	return (pc & 1);
}

static inline struct rb_node *rb_parent(struct rb_node *node)
{
	return __rb_parent(node->__parent_color);
}

static inline uintptr_t rb_color(struct rb_node *node)
{
	return __rb_color(node->__parent_color);
}

static inline void rb_set_parent(struct rb_node *node, struct rb_node *parent)
{
	/* assert(((uintptr_t)parent & 1) == 0); */
	node->__parent_color = rb_color(node) | (uintptr_t)parent;
}

static inline void rb_set_color(struct rb_node *node, uintptr_t color)
{
	/* assert(color == RB_RED || color == RB_BLACK); */
	node->__parent_color = (uintptr_t)rb_parent(node) | color;
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
	// technically null nodes are leaves and therefore black
	return !node || rb_is_black(node);
}

static inline struct rb_node *rb_red_parent(struct rb_node *node)
{
	// somehow neither gcc nor clang can optimize the additional 'and' away
	// even if it knows the bottom bit is zero (red)
	/* assert(rb_is_red(node)); */
	return (struct rb_node *)node->__parent_color;
}

static inline void rb_change_child(struct rb_node *old_child, struct rb_node *new_child,
				   struct rb_node *parent, struct rb_root *root)
{
	if (parent) {
		if (old_child == parent->children[RB_LEFT]) {
			parent->children[RB_LEFT] = new_child;
		} else {
			parent->children[RB_RIGHT] = new_child;
		}
	} else {
		root->node = new_child;
	}
}

static struct rb_node *rb_first(struct rb_root root)
{
	struct rb_node *node = NULL;
	struct rb_node *cur = root.node;
	while (cur) {
		node = cur;
		cur = cur->children[RB_LEFT];
	}
	return node;
}

static struct rb_node *rb_next(struct rb_node *node)
{
	if (node->children[RB_RIGHT]) {
		node = node->children[RB_RIGHT];
		while (node->children[RB_LEFT]) {
			node = node->children[RB_LEFT];
		}
		return node;
	}

	struct rb_node *parent = rb_parent(node);
	while (parent && node == parent->children[RB_RIGHT]) {
		node = parent;
		parent = rb_parent(node);
	}

	return parent;
}

static struct rb_node *rb_find(struct rb_root *root, int key)
{
	struct rb_node *cur = root->node;
	while (cur) {
		if (key == to_thing(cur)->key) {
			return cur;
		}
		int dir = RB_RIGHT;
		if (key < to_thing(cur)->key) {
			dir = RB_LEFT;
		}
		cur = cur->children[dir];
	}
	return NULL;
}

static void rb_remove_repair(struct rb_root *root, struct rb_node *parent)
{
	// we only use node at the start to figure out which child node is,
	// since we are always the left child on the first iteration this is fine
	struct rb_node *node = NULL;
	for (;;) {
		int dir = RB_LEFT;
		struct rb_node *sibling = parent->children[RB_RIGHT];
		if (node == sibling) {
			dir = RB_RIGHT;
			sibling = parent->children[RB_LEFT];
		}

		int left_dir = dir;
		int right_dir = 1 - dir;

		if (rb_is_red(sibling)) {
			// rotate left(/right) at parent
			struct rb_node *tmp = sibling->children[left_dir];
			parent->children[right_dir] = tmp;
			/* assert(rb_color(parent->children[right_dir]) == RB_BLACK); */
			rb_set_parent(parent->children[right_dir], parent);
			sibling->children[left_dir] = parent;
			rb_change_child(parent, sibling, rb_parent(parent), root);
			sibling->__parent_color = parent->__parent_color;
			rb_set_parent(parent, sibling);
			rb_set_color(parent, RB_RED);
			sibling = tmp;
			rb_set_color(parent, RB_RED);
		}

		if (rb_is_null_or_black(sibling->children[RB_RIGHT]) &&
		    rb_is_null_or_black(sibling->children[RB_LEFT])) {
			/* assert(rb_parent(sibling) == parent); */
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

		if (rb_is_null_or_black(sibling->children[right_dir])) {
			// rotate right at sibling
			struct rb_node *tmp = sibling->children[left_dir];
			sibling->children[left_dir] = tmp->children[right_dir];
			if (sibling->children[left_dir]) {
				/* assert(rb_is_black(sibling->children[left_dir])); */
				rb_set_parent(sibling->children[left_dir], sibling);
			}
			tmp->children[right_dir] = sibling;
			parent->children[right_dir] = tmp;
			rb_set_parent(sibling, tmp);
			sibling = tmp;
		}
		// rotate left at parent
		parent->children[right_dir] = sibling->children[left_dir];
		if (parent->children[right_dir]) {
			rb_set_parent(parent->children[right_dir], parent);
		}
		sibling->children[left_dir] = parent;
		rb_change_child(parent, sibling, rb_parent(parent), root);
		sibling->__parent_color = parent->__parent_color;
		rb_set_parent(parent, sibling);
		rb_set_color(sibling->children[right_dir], RB_BLACK);
		/* assert(rb_parent(sibling->children[right_dir]) == sibling); */
		rb_set_color(parent, RB_BLACK);

		break;
	}
}

static void rb_remove_node(struct rb_root *root, struct rb_node *node)
{
	struct rb_node *child = node->children[RB_RIGHT];
	struct rb_node *tmp = node->children[RB_LEFT];
	struct rb_node *rebalance;
	uintptr_t pc;

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

		tmp = child->children[RB_LEFT];
		if (!tmp) {
			parent = successor;
			child2 = successor->children[RB_RIGHT];
		} else {
			do {
				parent = successor;
				successor = tmp;
				tmp = tmp->children[RB_LEFT];
			} while (tmp);
			child2 = successor->children[RB_RIGHT];
			parent->children[RB_LEFT] = child2;
			successor->children[RB_RIGHT] = child;
			rb_set_parent(child, successor);
		}

		tmp = node->children[RB_LEFT];
		successor->children[RB_LEFT] = tmp;
		rb_set_parent(tmp, successor);

		pc = node->__parent_color;
		tmp = __rb_parent(pc);
		rb_change_child(node, successor, tmp, root);

		if (child2) {
			rb_set_color(child2, RB_BLACK);
			rb_set_parent(child2, parent);
			rebalance = NULL;
		} else {
			rebalance = rb_is_black(successor) ? parent : NULL;
		}
		successor->__parent_color = pc;
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

static void rb_insert_node(struct rb_root *root, struct rb_node *node, struct rb_node *parent, int dir)
{
	assert(((uintptr_t)node & 1) == 0);
	node->children[RB_LEFT] = NULL;
	node->children[RB_RIGHT] = NULL;
	rb_set_parent(node, parent);
	if (!parent) {
		rb_set_color(node, RB_BLACK);
		root->node = node;
		return;
	}
	rb_set_color(node, RB_RED);
	parent->children[dir] = node;

	// repair (generic)
	for (;;) {
		if (rb_is_black(parent)) {
			break;
		}

		struct rb_node *grandparent = rb_red_parent(parent);
		dir = RB_RIGHT;
		struct rb_node *uncle = grandparent->children[RB_LEFT];
		if (parent == uncle) {
			dir = RB_LEFT;
			uncle = grandparent->children[RB_RIGHT];
		}

		if (rb_is_null_or_black(uncle)) {
			int left_dir = dir;
			int right_dir = 1 - dir;
			if (node == parent->children[right_dir]) {
				// rotate left(/right) at parent
				parent->children[right_dir] = node->children[left_dir];
				if (parent->children[right_dir]) {
					rb_set_parent(parent->children[right_dir], parent);
				}
				node->children[left_dir] = parent;
				grandparent->children[left_dir] = node;
				// rb_set_parent(node, grandparent); we overwrite this later anyway
				rb_set_parent(parent, node);
				parent = node;
			}

			// rotate right(/left) at grandparent
			grandparent->children[left_dir] = parent->children[right_dir];
			if (grandparent->children[left_dir]) {
				rb_set_parent(grandparent->children[left_dir], grandparent);
			}
			parent->children[right_dir] = grandparent;

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
		parent = rb_red_parent(node);

		if (!parent) {
			rb_set_color(node, RB_BLACK);
			break;
		}
	}
}

static bool rb_insert_key(struct rb_root *root, int key)
{
	// insert
	struct rb_node *parent = NULL;
	struct rb_node *cur = root->node;
	int dir = 0;
	while (cur) {
		if (key == to_thing(cur)->key) {
			return false;
		}
		dir = RB_RIGHT;
		if (key < to_thing(cur)->key) {
			dir = RB_LEFT;
		}
		parent = cur;
		cur = cur->children[dir];
	}

	struct thing *thing = malloc(sizeof(*thing));
	thing->key = key;
	rb_insert_node(root, &thing->rb_node, parent, dir);
	return true;
}

static int black_depth;
static int max_depth;
static int num_nodes;
static void debug_recursive_check_tree(struct rb_node *node, int cur_black_depth, int depth)
{
	if (!node) {
		if (black_depth == -1) {
			black_depth = cur_black_depth;
		}
		if (max_depth < depth) {
			max_depth = depth;
		}
		assert(black_depth == cur_black_depth);
		return;
	}
	num_nodes++;
	if (rb_is_black(node)) {
		cur_black_depth++;
	}
	if (node->children[RB_LEFT]) {
		assert(rb_parent(node->children[RB_LEFT]) == node);
		assert(!(rb_is_red(node) && rb_is_red(node->children[RB_LEFT])));
		assert(to_thing(node->children[RB_LEFT])->key < to_thing(node)->key);
	}
	if (node->children[RB_RIGHT]) {
		assert(rb_parent(node->children[RB_RIGHT]) == node);
		assert(!(rb_is_red(node) && rb_is_red(node->children[RB_RIGHT])));
		assert(to_thing(node->children[RB_RIGHT])->key > to_thing(node)->key);
	}
	debug_recursive_check_tree(node->children[RB_LEFT], cur_black_depth, depth + 1);
	debug_recursive_check_tree(node->children[RB_RIGHT], cur_black_depth, depth + 1);
}

static void debug_check_tree(struct rb_root *root)
{
	black_depth = -1;
	max_depth = -1;
	num_nodes = 0;
	if (!root->node) {
		printf("empty\n");
		return;
	}
	assert(rb_is_black(root->node));
	assert(!rb_parent(root->node));
	debug_recursive_check_tree(root->node, 0, 0);
}

int main(void)
{
	struct rb_root root = EMPTY_ROOT;
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
			rb_insert_key(&root, key);
			struct rb_node *node = rb_find(&root, key);
			assert(to_thing(node)->key == key);
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

#if 1
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		rb_insert_key(&root, key);
	}
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		struct rb_node *node = rb_find(&root, key);
		assert(node && to_thing(node)->key == key);
	}
	srand(0);
	for (unsigned int i = 0; i < 3000000; i++) {
		int key = rand();
		struct rb_node *node = rb_remove_key(&root, key);
		assert(!node || to_thing(node)->key == key);
	}
	assert(root.node == NULL);
	return 0;
#endif

#if 1
	srand(0);
	for (unsigned int i = 0; i < 5000000; i++) {
		int key = rand();
		rb_insert_key(&root, key);
	}
	struct thing *prev = NULL;
	rb_foreach(root) {
		struct thing *thing = to_thing(cur);
		if (prev) {
			assert(prev->key < thing->key);
		}
		prev = thing;
	}
	return 0;
#endif

#if 1
	srand(time(NULL));
	for (unsigned int i = 0; ; i++) {
		{
			int key = rand() % 10000;
			bool success = rb_insert_key(&root, key);
			if (!success) {
				struct rb_node *node = rb_remove_key(&root, key);
				free(to_thing(node));
				success = rb_insert_key(&root, key);
				assert(success);
			}
			assert(to_thing(rb_find(&root, key))->key == key);
		}
		{
			int key = rand() % 10000;
			struct rb_node *node = rb_find(&root, key);
			if (node) {
				rb_remove_node(&root, node);
				assert(!rb_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % (1 << 20) == 0) {
			debug_check_tree(&root);
			printf("black depth: %d\n", black_depth);
			printf("max depth: %d\n", max_depth);
			printf("num nodes: %d\n", num_nodes);
		}
	}
#endif
}
