#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#define swap(a, b) do { int tmp = a; a = b; b = tmp; } while(0)

struct partition {
	size_t left;
	size_t right;
};

static void insertion_sort(int *arr, size_t left, size_t right)
{
	for (size_t i = left + 1; i <= right; i++) {
		for (size_t j = i; j > left && arr[j - 1] > arr[j]; j--) {
			swap(arr[j - 1], arr[j]);
		}
	}
}

static void quicksort(int *arr, size_t left, size_t right)
{
	struct partition stack[128];
	size_t sp = 0;

	for (;;) {
		while (left < right) {
			if (right - left < 16) {
				insertion_sort(arr, left, right);
				break;
			}

			size_t mid = (left + right) / 2;
			if (arr[mid] < arr[left]) {
				swap(arr[left], arr[mid]);
			}
			if (arr[right] < arr[left]) {
				swap(arr[left], arr[right]);
			}
			if (arr[mid] < arr[right]) {
				swap(arr[mid], arr[right]);
			}
			int pivot = arr[right];
			size_t i = left - 1;
			size_t j = right + 1;
			for (;;) {
				do {
					i++;
				} while (i <= right && arr[i] < pivot);

				do {
					j--;
				} while (j >= left && arr[j] > pivot);

				if (i >= j) {
					assert(sp < 128);
					if (j - left < right - j) {
						stack[sp].left = j + 1;
						stack[sp].right = right;
						right = j;

					} else {
						stack[sp].left = left;
						stack[sp].right = j;
						left = j + 1;
					}
					sp++;
					break;
				}

				swap(arr[i], arr[j]);
			}
		}

		if (sp == 0) {
			break;
		}

		sp--;
		left = stack[sp].left;
		right = stack[sp].right;

	}
}

static void sort(int *arr, size_t n)
{
	quicksort(arr, 0, n - 1);
}

static int is_sorted(int *arr, size_t n)
{
	for (size_t i = 1; i < n; i++) {
		if (arr[i] < arr[i - 1]) {
			return 0;
		}
	}
	return 1;
}

static int int_cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

int main(int argc, char **argv)
{
	size_t n = 32 * 1024 * 1024;
	int *arr = malloc(n * sizeof(arr[0]));

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}

	struct timespec start, end;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	sort(arr, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(is_sorted(arr, n));

	long sec = end.tv_sec - start.tv_sec;
	long nsec = end.tv_nsec - start.tv_nsec;
	if (nsec < 0) {
		sec--;
		nsec += 1000000000;
	}
	printf("%ld.%03ld\n", sec, nsec / 1000000);

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	qsort(arr, n, sizeof(arr[0]), int_cmp);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	assert(is_sorted(arr, n));

	sec = end.tv_sec - start.tv_sec;
	nsec = end.tv_nsec - start.tv_nsec;
	if (nsec < 0) {
		sec--;
		nsec += 1000000000;
	}
	printf("%ld.%ld\n", sec, nsec / 1000000);

	srand(1234);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}
}
