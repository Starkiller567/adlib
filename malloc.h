#ifndef __heap_include__
#define __heap_include__

#include "list.h"

#define HEAP_ALIGNMENT 16 // must be power of 2, >= 8, and <= 4096

struct heap {
	struct list_head free_list;
};

void *heap_malloc(struct heap *heap, size_t size)
	__attribute__((malloc, assume_aligned(HEAP_ALIGNMENT)));
void heap_free(struct heap *heap, void *mem);
void *heap_realloc(struct heap *heap, void *ptr, size_t size)
	__attribute__((assume_aligned(HEAP_ALIGNMENT)));
void heap_init(struct heap *heap);

#endif
