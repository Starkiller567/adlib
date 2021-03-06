/*
 * Copyright (C) 2020-2022 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __AVL_TREE_INCLUDE__
#define __AVL_TREE_INCLUDE__

// TODO documentation, high-level API (see tests/avl_tree.c)

#include <stdint.h>
#include "config.h"
#include "compiler.h"
#include "macros.h"

enum avl_direction {
// don't change these without changing avl_d2b and avl_b2d and possibly other code
	AVL_LEFT = 0,
	AVL_RIGHT = 1
};

struct avl_node {
	uintptr_t _parent_balance;
	union {
		struct {
			struct avl_node *left;
			struct avl_node *right;
		};
		struct avl_node *children[2];
	};
};

struct avl_root {
	struct avl_node *node;
};

#define AVL_EMPTY_ROOT ((struct avl_root){NULL})

#define avl_foreach(root, itername)					\
	for (struct avl_node *(itername) = avl_first(root); (itername); (itername) = avl_next(cur))

__AD_LINKAGE _attr_unused void avl_insert_node(struct avl_root *root, struct avl_node *node, struct avl_node *parent, enum avl_direction dir);
__AD_LINKAGE _attr_unused void avl_remove_node(struct avl_root *root, struct avl_node *node);
__AD_LINKAGE _attr_unused struct avl_node *avl_parent(const struct avl_node *node);
__AD_LINKAGE _attr_unused _attr_pure struct avl_node *avl_first(const struct avl_root *root);
__AD_LINKAGE _attr_unused _attr_pure struct avl_node *avl_next(const struct avl_node *node);


#endif
