#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "bitmap.h"

int main()
{
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
	bm_foreach_zero(bm, NBITS) {
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
	bm_foreach_set(bm, NBITS) {
		i++;
	}
	assert(i == 0);

	i = 0;
	bm_foreach_zero(bm, NBITS) {
		assert(cur == i);
		bm_set_bit(bm, cur);
		i++;
	}

	i = 0;
	bm_foreach_set(bm, NBITS) {
		assert(cur == i);
		bm_clear_bit(bm, cur);
		i++;
	}

	srand(NBITS);
	for (i = 0; i < 12345; i++) {
		bm_set_bit(bm, rand() % NBITS);
	}

	i = 0;
	bm_foreach_zero(bm, NBITS) {
		assert(!bm_test_bit(bm, cur));
		i++;
	}

	bm_foreach_set(bm, NBITS) {
		assert(bm_test_bit(bm, cur));
		i++;
	}

	assert(i == NBITS);
}
