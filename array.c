#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include "array.h"

static void print_array(int *arr, bool print_reverse)
{
	size_t len = array_len(arr);
	size_t limit = array_capacity(arr);
	printf("{\n\tlen = %zu,\n\tlimit = %zu,\n\tval = {", len, limit);
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
		printf("},\n\treverse = {");
		array_foreach_reverse(arr, cur) {
			printf("%i", *cur);
			i--;
			if (i != 0) {
				printf(", ");
			}
		}
		assert(i == 0);
	}

	printf("}\n}\n");
}

static void assert_array_content(int *arr, ...)
{
	va_list args;
	va_start(args, arr);
	int *cur;
	array_foreach(arr, cur) {
		assert(*cur == va_arg(args, int));
	}
	va_end(args);
}

static int cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
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
	print_array(arr1, true);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5);


	int i;
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	i = array_pop(arr1);
	assert(i == 0);

	array_make_valid(arr1, 1);
	array_make_valid(arr1, 7);
	arr1[6] = 6;
	arr1[7] = 7;

	array_addn(arr2, 15);
	array_popn(arr2, 10);

	array_reset(arr2);
	array_shrink_to_fit(arr2);

	print_array(arr1, true);
	assert_array_content(arr1, 0, 1, 2, 3, 4, 5, 6, 7);
	array_ordered_deleten(arr1, 2, 1);
	array_fast_deleten(arr1, 2, 1);

	array_ordered_delete(arr1, 0);
	array_fast_delete(arr1, 0);

	array_resize(arr1, 4);

	print_array(arr1, true);
	print_array(arr2, true);

	array_free(arr1);
	array_insertn(arr1, 0, 10);
	assert(array_capacity(arr1) >= 10);
	array_free(arr1);

	array_reserve(arr1, 1);
	assert(array_capacity(arr1) >= 1);
	print_array(arr1, true);
	array_reserve(arr1, 5);
	assert(array_capacity(arr1) >= 5);
	print_array(arr1, true);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	print_array(arr1, true);
	assert_array_content(arr1, 0, 0, 0, 0, 0);
	array_free(arr1);

	int digits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	arr2 = array_copy(arr1);
	array_add_array(arr1, arr2);
	print_array(arr1, true);
	assert_array_content(arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_truncate(arr1, 10);
	assert_array_content(arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_free(arr1);

	array_reverse(arr2);
	print_array(arr2, true);
	assert_array_content(arr2, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

	array_shuffle(arr2);
	print_array(arr2, true);
	array_shuffle(arr2);
	print_array(arr2, true);
	array_sort(arr2, cmp);
	print_array(arr2, true);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_free(arr2);

	array_add_arrayn(arr1, digits, 10);
	int *dest = array_addn(arr2, 10);
	memcpy(dest, digits, 10 * sizeof(int));
	bool equal = array_equal(arr1, arr2);
	assert(equal);
	arr2[0] = 1;
	equal = array_equal(arr1, arr2);
	assert(!equal);
	array_ordered_delete(arr2, 0);
	equal = array_equal(arr1, arr2);
	assert(!equal);
	array_insert(arr2, 0, 0);
	equal = array_equal(arr1, arr2);
	assert(equal);
	array_free(arr1);
	array_free(arr2);

	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	for (size_t i = 0; i < 10; i++) {
		array_shuffle(arr1);
		int *cur;
		array_foreach(arr1, cur) {
			printf("%i ", *cur);
		}
		putchar('\r');
		fflush(stdout);
		usleep(300000);
	}
	putchar('\n');
	array_free(arr1);
#else

	int n = 300000;
	for (int i = 0; i < n; i++) {
		array_add(arr1, i);
	}

	array_fori(arr1, i) {
		assert(array_index_of(arr1, &arr1[i]) == i);
		assert(arr1[i] == i);
	}

	array_fori_reverse(arr1, i) {
		assert(array_index_of(arr1, &arr1[i]) == i);
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
