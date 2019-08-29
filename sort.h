#ifndef __SORT_INCLUDE__
#define __SORT_INCLUDE__

struct __quicksort_partition {
	size_t left;
	size_t right;
};

// 'name' is the identifier of the sort function.
// 'type' is the element type of the array to sort.
// 'threshold' is the minimum number of elements for which quicksort will be used,
// below this number insertion sort will be used.
// The last argument should be a code expression that compares two elements:
// The expression receives two pointers to array elements called 'a' and 'b' and
// "must return an integer less than, equal to, or greater than zero if the first argument (a)
// is considered to be respectively less than, equal to, or greater than the second (b)" (see qsort manpage).
// Examples:
//   DEFINE_SORTFUNC(integer_sort, int, 16, *a - *b)
//   DEFINE_SORTFUNC(string_sort, char *, 16, strcmp(*a, *b))
// To sort an array with the defined functions do:
//   int array[N] = {1, 3, 2, ...};
//   integer_sort(array, N);
// or
//   char **array[N] = {"abc", "ghi", "def", ...}
//   string_sort(array, n);

#define DEFINE_SORTFUNC(name, type, threshold, ...)			\
									\
	static inline void __##name##_swap(type *a, type *b)		\
	{								\
		type tmp = *a;						\
		*a = *b;						\
		*b = tmp;						\
	}								\
									\
	static void name(type *arr, size_t n)				\
	{								\
		struct __quicksort_partition stack[128];		\
		size_t sp = 0;						\
		size_t left = 0;					\
		size_t right = n - 1;					\
		type const *a;						\
		type const *b;						\
									\
		for (;;) {						\
			while (left < right) {				\
				if (right - left < threshold) {		\
					for (size_t i = left + 1; i <= right; i++) { \
						size_t j = i;		\
						a = &arr[j - 1];	\
						b = &arr[j];		\
						while (j > left && (__VA_ARGS__) > 0) { \
							__##name##_swap(&arr[j - 1], &arr[j]); \
							j--;		\
							a = &arr[j - 1]; \
							b = &arr[j];	\
						}			\
					}				\
					break;				\
				}					\
									\
				size_t mid = (left + right) / 2;	\
				a = &arr[mid];				\
				b = &arr[left];				\
				if ((__VA_ARGS__) < 0) {		\
					__##name##_swap(&arr[left], &arr[mid]); \
				}					\
				a = &arr[right];			\
				b = &arr[left];				\
				if ((__VA_ARGS__) < 0) {		\
					__##name##_swap(&arr[left], &arr[right]); \
				}					\
				a = &arr[mid];				\
				b = &arr[right];			\
				if ((__VA_ARGS__) < 0) {		\
					__##name##_swap(&arr[mid], &arr[right]); \
				}					\
				type pivot = arr[right];		\
				b = &pivot;				\
				size_t i = left - 1;			\
				size_t j = right + 1;			\
				for (;;) {				\
					do {				\
						i++;			\
						a = &arr[i];		\
					} while (i <= right && (__VA_ARGS__) < 0); \
									\
					do {				\
						j--;			\
						a = &arr[j];		\
					} while (j >= left && (__VA_ARGS__) > 0); \
									\
					if (i >= j) {			\
						/* assert(sp < 128); */	\
						if (j - left < right - j) { \
							stack[sp].left = j + 1;	\
							stack[sp].right = right; \
							right = j;	\
						} else {		\
							stack[sp].left = left; \
							stack[sp].right = j; \
							left = j + 1;	\
						}			\
						sp++;			\
						break;			\
					}				\
									\
					__##name##_swap(&arr[i], &arr[j]); \
				}					\
			}						\
									\
			if (sp == 0) {					\
				break;					\
			}						\
									\
			sp--;						\
			left = stack[sp].left;				\
			right = stack[sp].right;			\
									\
		}							\
	}


#endif
