// inspired by https://github.com/nothings/stb/blob/master/stb.h

#ifndef __array_include__
#define __array_include__

#include <stdlib.h>
#include <string.h>

// TODO also add safety guards to the end of the array
#ifndef ARRAY_SAFETY_CHECKS
# ifdef NDEBUG
#  define ARRAY_SAFETY_CHECKS 0
# else
#  define ARRAY_SAFETY_CHECKS 1
# endif
#endif

#define ARRAY_INITIAL_SIZE 8

#define ARRAY_MAGIC1 0xdeadbabe
#define ARRAY_MAGIC2 0xbeefcafe

typedef struct {
	size_t len;
	size_t capacity;
#if ARRAY_SAFETY_CHECKS
	size_t magic1;
	size_t magic2;
#endif
} __arr;

#if ARRAY_SAFETY_CHECKS
# include <assert.h>
# define __arrhead_unchecked(a) ((__arr *)(a) - 1)
# define __arrhead(a) (assert(__arrhead_unchecked(a)->magic1 == ARRAY_MAGIC1), \
                       assert(__arrhead_unchecked(a)->magic1 == ARRAY_MAGIC1), \
                       __arrhead_unchecked(a))
#else
# define __arrhead(a) ((__arr *)(a) - 1)
#endif

// get number of elements in array
#define array_len(a)                    ((a) ? __arrhead(a)->len : 0)
// is array empty?
#define array_empty(a)                  (array_len(a) == 0)
// get last valid index
#define array_lasti(a)                  (array_len(a) - 1)
// get last element
#define array_last(a)                   ((a)[array_lasti(a)])
// get allocated capacity in elements
#define array_capacity(a)               ((a) ? __arrhead(a)->capacity : 0)
// release all resources of the array (will be set to NULL)
#define array_free(a)                   __array_free((void **)&(a))
// make an exact copy of the array
#define array_copy(a)	                __array_copy((a), sizeof((a)[0]))
// add n unitialized elements to the end of the array and return a (void) pointer to the first of those
// TODO figure out how to return a pointer of the original type
#define array_addn(a, n)                (__array_addn((void **)&(a), sizeof((a)[0]), n))
// add the element v to the end of the array
#define array_add(a, v)                 (array_addn(a, 1), ((a)[array_lasti(a)] = v))
// insert n unitialized elements at index i
#define array_insertn(a, i, n)          __array_insertn((void **)&(a), sizeof((a)[0]), i, n)
// insert element v at index i
#define array_insert(a, i, v)           (array_insertn(a, i, 1), ((a)[i] = v))
// make the array empty (but keep the memory)
#define array_reset(a)	                do { if (a) __arrhead(a)->len = 0; } while(0);
// set the capacity of the array
#define array_resize(a, capacity)       __array_resize((void **)&(a), sizeof((a)[0]), capacity)
// reduce the length of the array (noop if newlen >= len)
#define array_truncate(a, newlen)       (newlen < array_len(a) ? (void)(__arrhead(a)->len = newlen) : (void)0)
// allocate enough space for n additional elements (does not change length, only capacity)
#define array_reserve(a, n)             __array_reserve((void **)&(a), sizeof((a)[0]), n)
// increase capacity by atleast n elements
#define array_grow(a, n)                __array_grow((void **)&(a), sizeof((a)[0]), n)
// make capacity equal to length
#define array_shrink_to_fit(a)          array_resize(a, array_len(a));
// make sure the index i is within the allocated space
#define array_make_valid(a, i)          __array_make_valid((void **)&(a), sizeof((a)[0]), i);
// alias for add
#define array_push(a, v)                array_add(a, v)
// remove and return the last element
#define array_pop(a)                    ((a)[--__arrhead(a)->len])
// remove the last n elements (returns the new length)
#define array_popn(a, n)	        (__arrhead(a)->len -= n)
// delete n elements starting at index i WITHOUT keeping the original order of elements
#define array_fast_deleten(a, i, n)     __array_fast_deleten((a), sizeof((a)[0]), i, n)
// delete the element at index i WITHOUT keeping the original order of elements
#define array_fast_delete(a, i)         (((a)[i] = array_last(a)), __arrhead(a)->len--)
// delete n elements starting at index i keeping the original order of elements
#define array_ordered_deleten(a, i, n)  __array_ordered_deleten((a), sizeof((a)[0]), i, n)
// delete the element at index i keeping the original order of elements
#define array_ordered_delete(a, i)      array_ordered_deleten(a, i, 1)
// get the index of the element pointed to by ptr
#define array_index_of(a, ptr)          ((size_t)((ptr) - (a)))
// iterate over all array indices (the variable 'itername' contains the current index)
#define array_fori(a, itername)         for (size_t itername = 0; itername < array_len(a); itername++)
// iterate over all array indices in reverse (the variable 'itername' contains the current index)
#define array_fori_reverse(a, itername) for (size_t itername = array_len(a); \
					     !array_empty(a) && --itername != 0;)
// iterate over all array elements (v must be a variable of type 'pointer to element' and contains the current element)
#define array_foreach(a, v)             for ((v) = (a); (v) < (a) + array_len(a); (v)++)
// iterate over all array elements in reverse (v must be a variable of type 'pointer to element' and contains the current element)
#define array_foreach_reverse(a, v)     for ((v) = (a) + array_len(a);	\
					     !array_empty(a) && --(v) >= (a);)
// add the n first elements of array b to a (a and b should have the same type, but b can be a static array)
#define array_add_arrayn(a, b, n)       (memcpy(array_addn(a, n), 1 ? (b) : (a), \
						(n) * sizeof((a)[0])))
// add all elements of (dynamic) array b to a (a and b should have the same type)
#define array_add_array(a, b)           array_add_arrayn(a, b, array_len(b))
// sort array using comparison function (see qsort documentation)
#define array_sort(a, compare_fn)       qsort((a), array_len(a), sizeof((a)[0]), (compare_fn))
// are arrays a and b equal in content (byte-wise equality)
#define array_equal(a, b)               (array_len(a) != array_len(b) ? 0 : \
					 memcmp(1 ? (a) : (b), (b), array_len(a) * sizeof((a)[0])) == 0)
// swap elements at index idx1 and idx2
#define array_swap(a, idx1, idx2)					\
	do {								\
		if (idx1 != idx2) {					\
			unsigned char buf[sizeof((a)[0])];		\
			__array_swap((a), sizeof((a)[0]), buf, (idx1), (idx2));	\
		}							\
	} while (0)
// reverse the order of elements in the array
#define array_reverse(a)					\
	do {							\
		for (size_t i = 0; i < array_len(a) / 2; i++) {	\
			array_swap(a, i, array_lasti(a) - i);	\
		}						\
	} while (0)
// shuffle the elements in the array (creates a random permutation of the elements, Fisher-Yates shuffle)
#define array_shuffle(a)					\
	do {							\
		for (size_t i = array_lasti(a); i > 0; i--) {	\
			array_swap(a, i, rand() % (i + 1));	\
		}						\
	} while (0)

static void __array_swap(void *arr, size_t elem_size, unsigned char *buf, size_t i, size_t j)
{
	size_t n = elem_size;
	unsigned char *a = arr;
	memcpy(buf,       &a[n * i], n);
	memcpy(&a[n * i], &a[n * j], n);
	memcpy(&a[n * j], buf,       n);
}

static void __array_free(void **arrp)
{
	void *arr = *arrp;
	if (arr) {
		free(__arrhead(arr));
		*arrp = NULL;
	}
}

static void __array_resize(void **arrp, size_t elem_size, size_t capacity)
{
	if (capacity == 0) {
		__array_free(arrp);
		return;
	}
	size_t new_size = sizeof(__arr) + (capacity * elem_size);
#if ARRAY_SAFETY_CHECKS
	assert(((capacity * elem_size) / elem_size == capacity) && new_size > sizeof(__arr));
#endif
	__arr *head;
	if (!(*arrp)) {
		head = malloc(new_size);
		head->len = 0;
#if ARRAY_SAFETY_CHECKS
		head->magic1 = ARRAY_MAGIC1;
		head->magic2 = ARRAY_MAGIC2;
#endif
	} else {
		head = realloc(__arrhead(*arrp), new_size);
		if (head->len > capacity) {
			head->len = capacity;
		}
	}
	head->capacity = capacity;
	*arrp = head + 1;
}

static void *__array_copy(void *arr, size_t elem_size)
{
	void *new_arr = NULL;
	__array_resize(&new_arr, elem_size, array_capacity(arr));
	__arrhead(new_arr)->len = array_len(arr);
	memcpy(new_arr, arr, elem_size * array_len(arr));
	return new_arr;
}

static void __array_grow(void **arrp, size_t elem_size, size_t n)
{
	if (n == 0) {
		return;
	}
	size_t capacity = array_capacity(*arrp);
	size_t new_capacity = n < capacity ? 2 * capacity : capacity + n;
	if (new_capacity < ARRAY_INITIAL_SIZE) {
		new_capacity = ARRAY_INITIAL_SIZE;
	}
	__array_resize(arrp, elem_size, new_capacity);
}

static void __array_reserve(void **arrp, size_t elem_size, size_t n)
{
	// TODO implement this as a macro?
	if (n == 0) {
		return;
	}
	size_t rem = array_capacity(*arrp) - array_len(*arrp);
	if (n > rem) {
		__array_grow(arrp, elem_size, n - rem);
	}
}

static void __array_make_valid(void **arrp, size_t elem_size, size_t i)
{
	size_t capacity = array_capacity(*arrp);
	if (i >= capacity) {
		__array_grow(arrp, elem_size, i - capacity + 1);
	}
	if (i >= array_len(*arrp)) {
		__arrhead(*arrp)->len = i + 1;
	}
}

static void *__array_addn(void **arrp, size_t elem_size, size_t n)
{
	if (!(*arrp)) {
		__array_grow(arrp, elem_size, n);
		__arrhead(*arrp)->len = n;
		return *arrp;
	}
	__arr *head = __arrhead(*arrp);
	size_t old_len = head->len;
	head->len += n;
	if (head->len > head->capacity) {
		__array_grow(arrp, elem_size, head->len - head->capacity);
	}
	return (char *)(*arrp) + (old_len * elem_size);
}

static void __array_insertn(void **arrp, size_t size, size_t i, size_t n)
{
	void *arr = *arrp;
	if (!arr) {
		__array_addn(arrp, size, n);
		return;
	}
	size_t len = array_len(arr);
	__array_addn(&arr, size, n);
	memmove((char *)arr + (i + n) * size, (char *)arr + i * size, (len - i) * size);
	*arrp = arr;
}

static void __array_ordered_deleten(void *arr, size_t size, size_t i, size_t n)
{
	size_t len = array_len(arr);
	memmove((char *)arr + i * size, (char *)arr + (i + n) * size, (len - (i + n)) * size);
	__arrhead(arr)->len -= n;
}

static void __array_fast_deleten(void *arr, size_t size, size_t i, size_t n)
{
	size_t len = array_len(arr);
	size_t k = len - (i + n);
	if (k > n) {
		k = n;
	}
	memmove((char *)arr + i * size, (char *)arr + (len - k) * size, k * size);
	__arrhead(arr)->len -= n;
}

#endif
