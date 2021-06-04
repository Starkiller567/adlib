#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "array.h"

static void print_array(int *arr, bool print_reverse)
{
	size_t len = array_len(arr);
	size_t limit = array_capacity(arr);
	printf("{\n\tlen = %zu,\n\tlimit = %zu,\n\tval = {", len, limit);
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
	array_foreach(arr, cur) {
		assert(*cur == va_arg(args, int));
	}
	va_end(args);
}

static int cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static void add_a_one(array_t(int) *some_array)
{
	array_t(int) *arr = some_array;
	array_add(*arr, 1);
}

int main(void)
{
	int *arr1 = NULL;
	add_a_one(&arr1);
	// array_push(arr1, 1);
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
	array_shrink_to_fit(arr2);

	assert(arr2[array_lasti(arr2)] == 5);
	assert(arr2[array_lasti(arr2)] == array_last(arr2));

	assert(array_index_of(arr1, &arr1[3]) == 3);

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
	size_t len = array_len(arr2);
	array_popn(arr2, 10);
	assert(array_len(arr2) == len - 10);

	array_clear(arr2);
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

	array_shuffle(arr2, (size_t(*)(void))random);
	print_array(arr2, true);
	array_shuffle(arr2, (size_t(*)(void))random);
	print_array(arr2, true);
	array_sort(arr2, cmp);
	print_array(arr2, true);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_free(arr2);

	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	array_fori(arr1, i) {
		assert(array_index_of(arr1, &arr1[i]) == i);
		assert(arr1[i] == i);
	}
	array_fori_reverse(arr1, i) {
		assert(array_index_of(arr1, &arr1[i]) == i);
		assert(arr1[i] == i);
	}
	array_free(arr1);

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
	array_fori_reverse(arr1, i) {
		array_add(arr2, arr1[i]);
	}
	print_array(arr2, true);
	array_reverse(arr2);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_fori(arr2, i) {
		assert(arr2[i] == (int)i);
	}
	array_clear(arr2);

	{
		array_foreach_reverse(arr1, it) {
			array_add(arr2, *it);
		}
	}

	print_array(arr2, true);
	array_reverse(arr2);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	{
		size_t i = 0;
		array_foreach(arr2, it) {
			assert(*it == (int)i);
			i++;
		}
	}
	array_clear(arr2);

	{
		array_foreach_value(arr1, it) {
			array_add(arr2, it);
		}
		assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
		array_clear(arr2);
		array_foreach_value_reverse(arr1, it) {
			array_add(arr2, it);
		}
		array_reverse(arr1);
		assert(array_equal(arr1, arr2));
		assert_array_content(arr1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
		assert_array_content(arr2, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
		array_clear(arr1);
		array_foreach_value_reverse(arr2, it) {
			array_add(arr1, it);
		}
		int i = 0;
		array_foreach_value(arr1, it) {
			assert(it == i);
			i++;
		}
	}
	array_free(arr2);
	array_free(arr1);

	for (int i = 0; i < 1000; i++) {
		array_add(arr1, i);
	}
	for (int i = 0; i < 1000; i++) {
		int *x = array_bsearch(arr1, &i, cmp);
		assert(x);
		assert(*x == i);
	}
	array_free(arr1);

	*array_add1(arr1) = 1;
	array_add1_zero(arr1);
	array_add1_zero(arr1);
	*array_add1(arr1) = 1;
	array_addn_zero(arr1, 2);
	*array_add1(arr1) = 1;
	assert_array_content(arr1, 1, 0, 0, 1, 0, 0, 1);
	array_free(arr1);
	array_addn_zero(arr1, 2);
	array_add(arr1, 1);
	array_addn_zero(arr1, 2);
	array_add(arr1, 1);
	array_addn_zero(arr1, 2);
	assert_array_content(arr1, 0, 0, 1, 0, 0, 1, 0, 0);
	array_clear(arr1);
	array_addn_zero(arr1, 8);
	assert(array_len(arr1) == 8);
	assert_array_content(arr1, 0, 0, 0, 0, 0, 0, 0, 0);
	for (size_t i = 0; i < 8; i++) {
		array_addn_zero(arr1, 1);
	}
	assert(array_len(arr1) == 16);
	assert_array_content(arr1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	array_free(arr1);
	array_addn_zero(arr1, 123);
	assert(array_len(arr1) == 123);
	array_free(arr1);

	*array_insert1(arr1, 0) = 1;
	*array_insert1(arr1, 0) = 1;
	*array_insert1(arr1, 1) = 1;
	array_insert1_zero(arr1, 1);
	array_insert1_zero(arr1, 1);
	array_insert1_zero(arr1, 4);
	array_insert1_zero(arr1, 5);
	assert_array_content(arr1, 1, 0, 0, 1, 0, 0, 1);
	array_free(arr1);
	array_add(arr1, 1);
	array_add(arr1, 1);
	array_insertn_zero(arr1, 0, 2);
	array_insertn_zero(arr1, 3, 2);
	array_insertn_zero(arr1, 6, 2);
	assert_array_content(arr1, 0, 0, 1, 0, 0, 1, 0, 0);
	array_clear(arr1);
	array_insertn_zero(arr1, 0, 8);
	assert(array_len(arr1) == 8);
	assert_array_content(arr1, 0, 0, 0, 0, 0, 0, 0, 0);
	for (size_t i = 0; i < 8; i++) {
		array_insertn_zero(arr1, i, 1);
	}
	assert(array_len(arr1) == 16);
	assert_array_content(arr1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	array_free(arr1);
	array_insertn_zero(arr1, 0, 123);
	assert(array_len(arr1) == 123);
	array_free(arr1);

	for (int i = 0; i < 10; i++) {
		array_add(arr1, i);
	}
	arr2 = array_move(arr1);
	assert(arr1 == NULL);
	assert_array_content(arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	arr1 = array_move(arr2);
	assert(arr2 == NULL);
	assert_array_content(arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	array_swap(arr1, 1, 3);
	assert_array_content(arr1, 0, 3, 2, 1, 4, 5, 6, 7, 8, 9);
	array_swap(arr1, 6, 2);
	assert_array_content(arr1, 0, 3, 6, 1, 4, 5, 2, 7, 8, 9);
	array_swap(arr1, 9, 8);
	assert_array_content(arr1, 0, 3, 6, 1, 4, 5, 2, 7, 9, 8);
	array_swap(arr1, 8, 0);
	assert_array_content(arr1, 9, 3, 6, 1, 4, 5, 2, 7, 0, 8);
	array_swap(arr1, 9, 0);
	assert_array_content(arr1, 8, 3, 6, 1, 4, 5, 2, 7, 0, 9);
	array_swap(arr1, 5, 9);
	assert_array_content(arr1, 8, 3, 6, 1, 4, 9, 2, 7, 0, 5);
	array_free(arr1);

#if 0
	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	for (size_t i = 0; i < 5; i++) {
		array_shuffle(arr1, (size_t(*)(void))random);
		array_foreach(arr1, cur) {
			printf("%i ", *cur);
		}
		putchar('\r');
		fflush(stdout);
		usleep(300000);
	}
	putchar('\n');
	array_free(arr1);
#endif
}
