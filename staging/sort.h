#ifndef __SORT_INCLUDE__
#define __SORT_INCLUDE__

// TODO implement the 3-way optimization?
// TODO implement introsort?
// TODO put comparison into its own function, so it only has access to a and b
// TODO user data pointer for comparison
// TODO glibc places the smallest element first to speed up insertion sort's inner loop (saves the j>left check)
// TODO glibc does the insertion sort over the whole array at the end, is that better?

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
//   char *array[N] = {"abc", "ghi", "def", ...}
//   string_sort(array, N);

#define DEFINE_SORTFUNC(name, type, threshold, ...)			\
									\
	static inline int _##name##_cmp(type const *a, type const *b)	\
	{								\
		return (__VA_ARGS__);					\
	}								\
									\
	static inline void _##name##_swap(type *a, type *b)		\
	{								\
		type tmp = *a;						\
		*a = *b;						\
		*b = tmp;						\
	}								\
									\
	static void name(type *arr, size_t n)				\
	{								\
		const size_t T = (threshold) > 2 ? (threshold) : 2;	\
		if (n >= T) {						\
			struct {					\
				size_t left;				\
				size_t right;				\
			} stack[64];					\
			size_t sp = 0;					\
			size_t left = 0;				\
			size_t right = n - 1;				\
			for (;;) {					\
				size_t mid = left + (right - left) / 2;	\
				if (_##name##_cmp(&arr[mid], &arr[left]) < 0) { \
					_##name##_swap(&arr[mid], &arr[left]); \
				}					\
				if (_##name##_cmp(&arr[right], &arr[mid]) < 0) { \
					_##name##_swap(&arr[right], &arr[mid]); \
					if (_##name##_cmp(&arr[mid], &arr[left]) < 0) { \
						_##name##_swap(&arr[mid], &arr[left]); \
					}				\
				}					\
				size_t i = left + 1;			\
				size_t j = right - 1;			\
				do {					\
					while (_##name##_cmp(&arr[i], &arr[mid]) < 0) { \
						i++;			\
					}				\
									\
					while (_##name##_cmp(&arr[mid], &arr[j]) < 0) { \
						j--;			\
					}				\
									\
					if (i >= j) {			\
						if (i == j) {		\
							i++;		\
							j--;		\
						}			\
						break;			\
					}				\
									\
					_##name##_swap(&arr[i], &arr[j]); \
					if (mid == i) {			\
						mid = j;		\
					} else if (mid == j) {		\
						mid = i;		\
					}				\
				} while (++i <= --j);			\
				size_t ls = j - left + 1;		\
				size_t rs = right - i + 1;		\
				if (ls < T) {				\
					if (rs < T) {			\
						if (sp == 0) {		\
							break;		\
						}			\
						sp--;			\
						left = stack[sp].left;	\
						right = stack[sp].right; \
						continue;		\
					}				\
					left = i;			\
					continue;			\
				}					\
				if (rs < T) {				\
					right = j;			\
					continue;			\
				}					\
				if (ls > rs) {				\
					stack[sp].left = left;		\
					stack[sp].right = j;		\
					left = i;			\
				} else {				\
					stack[sp].left = i;		\
					stack[sp].right = right;	\
					right = j;			\
				}					\
				sp++;					\
			}						\
		}							\
		if (T == 2) {						\
			return;						\
		}							\
		size_t m = 0;						\
		for (size_t i = 1; i < T && i < n; i++) {		\
			if (_##name##_cmp(&arr[i], &arr[m]) < 0) {	\
				m = i;					\
			}						\
		}							\
		if (m != 0) {						\
			_##name##_swap(&arr[0], &arr[m]);		\
		}							\
		for (size_t i = 2; i < n; i++) {			\
			size_t j = i;					\
			while (_##name##_cmp(&arr[j], &arr[j - 1]) < 0) { \
				_##name##_swap(&arr[j - 1], &arr[j]);	\
				j--;					\
			}						\
		}							\
	}


#endif
