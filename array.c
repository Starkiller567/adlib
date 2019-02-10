#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "array.h"

static void print_array(int *arr, bool print_reverse)
{
	size_t len = array_len(arr);
	size_t limit = array_limit(arr);
	printf("{\nlen = %zu\nlimit = %zu\nval = {", len, limit);
	int *cur;
	size_t i = 0;
	array_foreach(arr, cur) {
		printf("%i", *cur);
		if (i != len - 1) {
			printf(", ");
		}
		i++;
	}

	if (print_reverse) {
		printf("}\nreverse = {");
		array_foreach_reverse(arr, cur) {
			printf("%i", *cur);
			i--;
			if (i != 0) {
				printf(", ");
			}
		}
	}

	printf("}\n}\n");
}

int main(void)
{
#if 0
	int _x;
	int *x = &_x;
	array_add(x, 1);
#endif

	int *arr1 = NULL;

#if 1
	array_push(arr1, 1);
	array_push(arr1, 2);
	array_push(arr1, 3);
	array_push(arr1, 4);
	array_push(arr1, 5);

	array_insert(arr1, 0, 0);
	array_insert(arr1, 0, 0);
	array_insert(arr1, 1, 1);
	array_insert(arr1, 2, 2);
	array_insert(arr1, 3, 3);
	array_insert(arr1, 4, 4);
	array_insert(arr1, 5, 5);

	int *arr2 = array_copy(arr1);

	int i;
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	printf("%d\n", i);

	array_make_valid(arr1, 1);
	array_make_valid(arr1, 7);
	arr1[6] = 6;
	arr1[7] = 7;

	array_addn(arr2, 15);
	array_popn(arr2, 10);

	array_reset(arr2);
	array_shrink_to_fit(arr2);

	print_array(arr1, true);
	array_ordered_deleten(arr1, 2, 1);
	array_fast_deleten(arr1, 2, 1);

	array_ordered_delete(arr1, 0);
	array_fast_delete(arr1, 0);

	array_resize(arr1, 4);

	print_array(arr1, true);
	print_array(arr2, true);

	array_free(arr1);
	array_insertn(arr1, 0, 10);
	array_free(arr1);

	array_reserve(arr1, 1);
	print_array(arr1, true);
	array_reserve(arr1, 5);
	print_array(arr1, true);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	print_array(arr1, true);
	array_free(arr1);

#else

	int n = 300000;
	for (int i = 0; i < n; i++) {
		array_add(arr1, i);
	}

	array_fori(arr1, i) {
		assert(arr1[i] == i);
	}

	array_fori_reverse(arr1, i) {
		assert(arr1[i] == i);
	}

	array_ordered_deleten(arr1, 0, 2);

	array_fori(arr1, i) {
		for (size_t k = 0; k < (i + 1) / 2; k++) {
			if (arr1[i] % arr1[k] == 0) {
				array_ordered_delete(arr1, i);
				i--;
				break;
			}
		}
	}

	printf("%zu primes between 0 and %d\n", array_len(arr1), n);
	array_free(arr1);
#endif
}
