#ifndef __SORT_INCLUDE__
#define __SORT_INCLUDE__

struct __quicksort_partition {
	size_t left;
	size_t right;
};

#define DEFINE_SORTFUNC(name, type, compare, threshold)			\
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
									\
		for (;;) {						\
			while (left < right) {				\
				if (right - left < threshold) {		\
					for (size_t i = left + 1; i <= right; i++) { \
						for (size_t j = i;	\
						     j > left && compare(&arr[j - 1], &arr[j]) > 0; \
						     j--) {		\
							__##name##_swap(&arr[j - 1], &arr[j]); \
						}			\
					}				\
					break;				\
				}					\
									\
				size_t mid = (left + right) / 2;	\
				if (compare(&arr[mid], &arr[left]) < 0) { \
					__##name##_swap(&arr[left], &arr[mid]); \
				}					\
				if (compare(&arr[right], &arr[left]) < 0) { \
					__##name##_swap(&arr[left], &arr[right]); \
				}					\
				if (compare(&arr[mid], &arr[right]) < 0) { \
					__##name##_swap(&arr[mid], &arr[right]); \
				}					\
				type pivot = arr[right];		\
				size_t i = left - 1;			\
				size_t j = right + 1;			\
				for (;;) {				\
					do {				\
						i++;			\
					} while (i <= right && compare(&arr[i], &pivot) < 0); \
									\
					do {				\
						j--;			\
					} while (j >= left && compare(&arr[j], &pivot) > 0); \
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
