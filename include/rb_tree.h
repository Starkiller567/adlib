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

#ifndef __RB_TREE_INCLUDE__
#define __RB_TREE_INCLUDE__

// TODO documentation, high-level API (see tests/rb_tree.c)

#include <stdint.h>
#include "config.h"
#include "compiler.h"

enum rb_direction {
	RB_LEFT = 0,
	RB_RIGHT = 1
};

struct rb_node {
	uintptr_t _parent_color;
	union {
		struct {
			struct rb_node *left;
			struct rb_node *right;
		};
		struct rb_node *children[2];
	};
};

struct rb_root {
	struct rb_node *node;
};

#define RB_EMPTY_ROOT ((struct rb_root){NULL})

#define rb_foreach(rb_root, itername)					\
	for (struct rb_node *(itername) = rb_first(rb_root); (itername); (itername) = rb_next(cur))

__AD_LINKAGE _attr_unused void rb_remove_node(struct rb_root *root, struct rb_node *node);
__AD_LINKAGE _attr_unused void rb_insert_node(struct rb_root *root, struct rb_node *node, struct rb_node *parent, enum rb_direction dir);
__AD_LINKAGE _attr_unused _attr_pure struct rb_node *rb_first(const struct rb_root *root);
__AD_LINKAGE _attr_unused struct rb_node *rb_parent(const struct rb_node *node);
__AD_LINKAGE _attr_unused _attr_pure struct rb_node *rb_next(const struct rb_node *node);

#endif
