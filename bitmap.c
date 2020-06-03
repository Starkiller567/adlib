#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "bitmap.h"

static void print_bitmap(const void *bitmap, size_t nbits)
{
	bm_foreach(bitmap, nbits, it) {
		printf("%zu", it);
	}
	putchar('\n');
}

int main()
{
#if 1
#define NBITS 123456789
	static DECLARE_EMPTY_BITMAP(bm, NBITS);
	size_t i = 0;
	for (size_t bit = bm_find_first_zero(bm, NBITS);
	     bit < NBITS;
	     bit = bm_find_next_zero(bm, bit, NBITS)) {
		assert(bit == i);
		bm_set_bit(bm, bit);
		i++;
	}

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		i++;
	}
	assert(i == 0);

	i = 0;
	for (size_t bit = bm_find_first_set(bm, NBITS);
	     bit < NBITS;
	     bit = bm_find_next_set(bm, bit, NBITS)) {
		assert(bit == i);
		bm_clear_bit(bm, bit);
		i++;
	}

	i = 0;
	bm_foreach_set(bm, NBITS, cur) {
		i++;
	}
	assert(i == 0);

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		assert(cur == i);
		bm_set_bit(bm, cur);
		i++;
	}

	i = 0;
	bm_foreach_set(bm, NBITS, cur) {
		assert(cur == i);
		bm_clear_bit(bm, cur);
		i++;
	}

	srand(NBITS);
	for (i = 0; i < 12345; i++) {
		bm_set_bit(bm, rand() % NBITS);
	}

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		assert(!bm_test_bit(bm, cur));
		i++;
	}

	bm_foreach_set(bm, NBITS, cur) {
		assert(bm_test_bit(bm, cur));
		i++;
	}

	assert(i == NBITS);
#else
#define NBITS 11
	DECLARE_EMPTY_BITMAP(bm1, NBITS);
	DECLARE_EMPTY_BITMAP(bm2, NBITS);

	srand(time(NULL));
	for (size_t i = 0; i < NBITS; i++) {
		if (rand() & 1) {
			bm_set_bit(bm1, i);
		}
		if (rand() & 1) {
			bm_set_bit(bm2, i);
		}
	}

	print_bitmap(bm1, NBITS);
	print_bitmap(bm2, NBITS);

	bm_and(bm1, bm2, NBITS);

	print_bitmap(bm1, NBITS);
#endif
}
