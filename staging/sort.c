#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "sort.h"
#include "heap.h"

static size_t comparisons;

static
__attribute__((noinline, noipa))
int int_cmp(const void *a, const void *b)
{
	comparisons++;
	return *(const int *)a - *(const int *)b;
}

static
__attribute__((noinline, noipa))
int string_cmp(const void *a, const void *b)
{
	comparisons++;
	return strcmp(*(const char **)a, *(const char **)b);
}


static int int_is_sorted(int *arr, size_t n)
{
	for (size_t i = 1; i < n; i++) {
		if (arr[i] < arr[i - 1]) {
			return 0;
		}
	}
	return 1;
}

static int string_is_sorted(char **arr, size_t n)
{
	for (size_t i = 1; i < n; i++) {
		if (strcmp(arr[i], arr[i - 1]) < 0) {
			return 0;
		}
	}
	return 1;
}

static int (*int_cmp_ptr)(const void *a, const void *b) = int_cmp;
static int (*string_cmp_ptr)(const void *a, const void *b) = string_cmp;

DEFINE_SORTFUNC(integer_sort, int, 16, int_cmp_ptr(a, b));
DEFINE_SORTFUNC(string_sort, const char *, 16, string_cmp_ptr(a, b));

DEFINE_MINHEAP(intheap, int, int_cmp_ptr(a, b) > 0);
DEFINE_MINHEAP(stringheap, const char *, string_cmp_ptr(a, b) > 0);

static void fill_int_array(int *arr, size_t n, bool random)
{
	if (random) {
		srand(1234);
	}
	for (size_t i = 0; i < n; i++) {
		int r = (random || (i % 4 == 0)) ? rand() : i;
		arr[i] = r;
	}
}

static void fill_string_array(char **arr, size_t n, bool random)
{
	srand(1234);
	for (size_t i = 0; i < n; i++) {
		unsigned int r = (random || (i % 4 == 0)) ? rand() : i;
		sprintf(arr[i], "%u", r);
	}
}

static double elapsed(struct timespec *start, struct timespec *end)
{
	long s = end->tv_sec - start->tv_sec;
	long ns = end->tv_nsec - start->tv_nsec;
	return ns / 1000000000.0 + s;
}

int main(int argc, char **argv)
{
	struct timespec start, end;

	size_t n = 8 * 1024 * 1024;
	int *arr1 = malloc(n * sizeof(arr1[0]));

	fill_int_array(arr1, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	integer_sort(arr1, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("integer_sort (random) %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	fill_int_array(arr1, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	intheap_heapify(arr1, n);
	intheap_sort(arr1, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("intheapsort (random)  %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	fill_int_array(arr1, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr1, n, sizeof(arr1[0]), int_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("qsort(int) (random)   %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	fill_int_array(arr1, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	integer_sort(arr1, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("integer_sort (presorted) %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	fill_int_array(arr1, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	intheap_heapify(arr1, n);
	intheap_sort(arr1, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("intheapsort (presorted)  %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	fill_int_array(arr1, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr1, n, sizeof(arr1[0]), int_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("qsort(int) (presorted)   %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(int_is_sorted(arr1, n));

	n = 2 * 1024 * 1024;
	char **arr2 = malloc(n * sizeof(arr2[0]));
	for (size_t i = 0; i < n; i++) {
		arr2[i] = malloc(16);
	}

	fill_string_array(arr2, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	string_sort((const char **)arr2, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("string_sort (random)    %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));

	fill_string_array(arr2, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	stringheap_heapify((const char **)arr2, n);
	stringheap_sort((const char **)arr2, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("stringheapsort (random) %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));

	fill_string_array(arr2, n, true);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr2, n, sizeof(arr2[0]), string_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("qsort(string) (random)  %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));

	fill_string_array(arr2, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	string_sort((const char **)arr2, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("string_sort (presorted)    %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));

	fill_string_array(arr2, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	stringheap_heapify((const char **)arr2, n);
	stringheap_sort((const char **)arr2, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("stringheapsort (presorted) %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));

	fill_string_array(arr2, n, false);
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr2, n, sizeof(arr2[0]), string_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("qsort(string) (presorted)  %.3fs (%zu comparisons)\n", elapsed(&start, &end), comparisons);
	assert(string_is_sorted(arr2, n));


	free(arr1);
	for (size_t i = 0; i < n; i++) {
		free(arr2[i]);
	}
	free(arr2);
}
