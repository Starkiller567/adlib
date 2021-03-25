#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "array.h"
// #include "hashtable_linked.h"
// #include "hashtable_chunked.h"
// #include "hashtable_maxdist.h"
// #include "hashtable_robinhood.h"
#include "hashtable.h"

static inline uint32_t integer_hash(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352d;
	x ^= x >> 15;
	x *= 0x846ca68b;
	x ^= x >> 16;
	return x;
}

static int cmp_int(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

DEFINE_HASHSET(itable, int, 8, (*a == *b))

int main(int argc, char **argv)
{
	struct itable itable;
	itable_init(&itable, 16);
	int *arr = NULL;
	unsigned int seed = argc > 1 ? atoi(argv[1]) : time(NULL);
	printf("seed: %u\n", seed);
	srand(seed);

	for (unsigned long counter = 0; counter < 500000; counter++) {
		int r = rand() % 128;
		if (r < 100) {
			int x = rand() % (1 << 20);
			bool found = false;
			int *item;
			array_foreach(arr, item) {
				if (*item == x) {
					found = true;
					break;
				}
			}
			item = itable_lookup(&itable, x, integer_hash(x));
			if (item) {
				assert(found);
				assert(*item == x);
			} else {
				assert(!found);
				item = itable_insert(&itable, x, integer_hash(x));
				// *item = x;
				assert(*item == x);
				array_add(arr, x);
			}
		} else if (array_len(arr) != 0) {
			int key;
			int idx = rand() % array_len(arr);
			int x = arr[idx];
			bool removed = itable_remove(&itable, x, integer_hash(x), &key);
			assert(removed);
			array_fast_delete(arr, idx);
		}

		if (counter % 4096 == 0) {
			int *i;
			array_foreach(arr, i) {
				assert(itable_lookup(&itable, *i, integer_hash(*i)));
			}
			int *arr2 = NULL;
			array_reserve(arr2, array_len(arr));
			for (itable_iter_t iter = itable_iter_start(&itable);
			     !itable_iter_finished(&iter);
			     itable_iter_advance(&iter)) {
				array_add(arr2, *iter.key);
			}
			array_sort(arr, cmp_int);
			array_sort(arr2, cmp_int);
			assert(array_equal(arr, arr2));
			array_free(arr2);
			fprintf(stderr, "%zu %lu\r", array_len(arr), counter);
		}
	}

	itable_destroy(&itable);
	array_free(arr);
}
