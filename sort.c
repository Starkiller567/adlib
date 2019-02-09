#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "sort.h"

static int int_cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static int string_cmp(const void *a, const void *b)
{
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

DEFINE_SORTFUNC(integer_sort, int, int_cmp, 16)
DEFINE_SORTFUNC(string_sort, char *, string_cmp, 16)

static double ns_elapsed(struct timespec *start, struct timespec *end)
{
	long s = end->tv_sec - start->tv_sec;
	long ns = end->tv_nsec - start->tv_nsec;
	return ns / 1000000000.0 + s;
}

int main(int argc, char **argv)
{
	size_t n = 32 * 1024 * 1024;
	int *arr1 = malloc(n * sizeof(arr1[0]));

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		arr1[i] = rand();
	}

	struct timespec start, end;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	integer_sort(arr1, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(int_is_sorted(arr1, n));

	double s = ns_elapsed(&start, &end);
	printf("%.2f\n", s);

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		arr1[i] = rand();
	}

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr1, n, sizeof(arr1[0]), int_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(int_is_sorted(arr1, n));

	s = ns_elapsed(&start, &end);
	printf("%.2f\n", s);

	free(arr1);

	n = 8 * 1024 * 1024;
	char **arr2 = malloc(n * sizeof(arr2[0]));

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		unsigned int r = rand();
		char *s = malloc(16);
		sprintf(s, "%u", r);
		arr2[i] = s;
	}

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	string_sort(arr2, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(string_is_sorted(arr2, n));

	s = ns_elapsed(&start, &end);
	printf("%.2f\n", s);

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		unsigned int r = rand();
		sprintf(arr2[i], "%u", r);
	}

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr2, n, sizeof(arr2[0]), string_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(string_is_sorted(arr2, n));

	s = ns_elapsed(&start, &end);
	printf("%.2f\n", s);

}
