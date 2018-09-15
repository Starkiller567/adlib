#include <stdio.h>
#include "array.h"

static void print_array(int *arr)
{
	unsigned int len = array_len(arr);
	unsigned int limit = array_limit(arr);
	printf("{\nlen = %u\nlimit = %u\nval = {", len, limit);
	int *cur;
	unsigned int i = 0;
	array_foreach(arr, cur) {
		printf("%i", *cur);
		if (i != len - 1) {
			printf(", ");
		}
		i++;
	}
	printf("}\n}\n");
}

int main(void)
{
	int *arr1 = NULL;
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

	array_pop(arr1);
	array_pop(arr1);
	array_pop(arr1);
	array_pop(arr1);
	array_pop(arr1);
	array_pop(arr1);

	array_make_valid(arr1, 1);
	array_make_valid(arr1, 7);
	arr1[6] = 6;
	arr1[7] = 7;

	array_addn(arr2, 15);
	array_popn(arr2, 10);

	array_clear(arr2);
	array_shrink_to_fit(arr2);

	print_array(arr1);
	array_ordered_deleten(arr1, 2, 1);
	array_fast_deleten(arr1, 2, 1);

	array_ordered_delete(arr1, 0);
	array_fast_delete(arr1, 0);

	array_resize(arr1, 4);

	print_array(arr1);
	print_array(arr2);

	array_free(arr1);
	array_insertn(arr1, 0, 10);
	array_free(arr1);
}
