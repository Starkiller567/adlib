// Copyright 2020 Fabian Hügel

// inspired by https://github.com/nothings/stb/blob/master/stb.h

// declare an (empty) array like so: array(int) my_array = NULL;
// (or so: int *my_array = NULL;)
// and use it like so: array_add(my_array, 1)
// or so:
//         void add_a_one(array(int) *some_array)
//         {
//             array_add(*some_array, 1);
//         }
//         ...
//             add_a_one(&my_array);
//             add_a_one(&my_array);
//             add_a_one(&my_array);
//             array_fori(my_array, current_index) {
//                 int current_element = my_array[current_index];
//                 ...
//             }
//             array_free(my_array);
//

#ifndef __array_include__
#define __array_include__

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARRAY_SAFETY_CHECKS
# ifdef NDEBUG
#  define ARRAY_SAFETY_CHECKS 0
# else
#  define ARRAY_SAFETY_CHECKS 1
# endif
#endif

#ifndef ARRAY_INITIAL_SIZE
# define ARRAY_INITIAL_SIZE 8
#endif

#ifndef ARRAY_NO_TYPEOF
# ifndef __ARRAY_TYPEOF
#  define __ARRAY_TYPEOF(x) __typeof__(x)
# endif
#else
# undef __ARRAY_TYPEOF
# define __ARRAY_TYPEOF(x) void *
#endif

// this macro provides a more explicit way of declaring a dynamic array
// (array(int) arr vs int *arr; the second could be a simple pointer or static array or dynamic array)
#define array(T) T *

// get number of elements in array (as size_t)
#define array_len(a)                    _arr_len(a)

// is array empty? (empty arrays are not always NULL due to array_reserve and array_reset)
#define array_empty(a)                  (_arr_len(a) == 0)

#define array_lasti(a)                  _arr_lasti(a)

// get last element
// (Warning: may evaluate 'a' multiple times! TODO is that necessary?)
#define array_last(a)                   _arr_last(a)

// get allocated capacity in elements (as size_t)
#define array_capacity(a)               _arr_capacity(a)

// release all resources of the array (will be set to NULL)
#define array_free(a)                   _arr_free((void **)&(a))

// make an exact copy of the array
#define array_copy(a)	                ((__ARRAY_TYPEOF(a))_arr_copy((a), sizeof((a)[0])))

// TODO array_move

// add n unitialized elements to the end of the array and return a (void) pointer to the first of those
#define array_addn(a, n)                ((__ARRAY_TYPEOF(a))_arr_addn((void **)&(a), sizeof((a)[0]), (n)))

// TODO array_addn_zero ?
// TODO array_add1 ?

// add value v to the end of the array (v must be an r-value of array element type)
// (Warning: evaluates 'a' multiple times! TODO is that necessary?)
#define array_add(a, v)                 _arr_add(a, v)

// insert n unitialized elements at index i and return a pointer to the first of those
// (i must be less than or equal to array_len(a))
#define array_insertn(a, i, n)          (__ARRAY_TYPEOF(a))_arr_insertn((void **)&(a), sizeof((a)[0]), (i), (n))

// TODO array_insertn_zero ?
// TODO array_insert1 ?

// insert value v at index i
// (v must be an r-value of array element type and i must be less than or equal to array_len(a))
// (Warning: evaluates 'a' multiple times! TODO is that necessary?)
#define array_insert(a, i, v)           _arr_insert(a, i, v)

// set the length of the array to zero (but keep the allocated memory)
#define array_reset(a)	                _arr_reset(a)

// set the capacity of the array (truncates the array length if necessary)
#define array_resize(a, capacity)       _arr_resize((void **)&(a), sizeof((a)[0]), (capacity))

// reduce the length of the array (does nothing if newlen >= len)
#define array_truncate(a, newlen)       _arr_truncate((a), (newlen))

// allocate enough space for n additional elements (does not change length, only capacity)
#define array_reserve(a, n)             _arr_reserve((void **)&(a), sizeof((a)[0]), (n))

// increase capacity by atleast n elements
#define array_grow(a, n)                _arr_grow((void **)&(a), sizeof((a)[0]), (n))

// make capacity equal to length
#define array_shrink_to_fit(a)          _arr_shrink_to_fit((void **)&(a), sizeof((a)[0]))

// ensure that i is a valid element index in the array
// (by increasing length and capacity as appropriate, any new elements created by this are uninitialized)
#define array_make_valid(a, i)          _arr_make_valid((void **)&(a), sizeof((a)[0]), (i));

// alias for array_add
#define array_push(a, v)                array_add(a, v)

// remove and return the last element
// (Warning: may evaluate 'a' multiple times! TODO is that necessary?)
#define array_pop(a)                    _arr_pop(a)

// remove the last n elements
#define array_popn(a, n)                _arr_popn((a), (n))

// get the index of the element pointed to by ptr (must be a valid array element!)
#define array_index_of(a, element_ptr)  _arr_index_of((1 ? (a) : (element_ptr)), sizeof((a)[0]), element_ptr)

// delete n elements starting at index i WITHOUT keeping the original order of elements
// (swap with the last n elements and decrease length by n)
#define array_fast_deleten(a, i, n)     _arr_fast_deleten((a), sizeof((a)[0]), (i), (n))

// delete the element at index i WITHOUT keeping the original order of elements
// (swap with the last element and decrement length)
#define array_fast_delete(a, i)         array_fast_deleten(a, i, 1)

// delete n elements starting at index i keeping the original order of elements
#define array_ordered_deleten(a, i, n)  _arr_ordered_deleten((a), sizeof((a)[0]), (i), (n))

// delete the element at index i keeping the original order of elements
#define array_ordered_delete(a, i)      array_ordered_deleten(a, i, 1)

// add the n first elements of array b to a (a and b must have the same type, but b can be a static array)

#define array_add_arrayn(a, b, n)       _arr_add_arrayn((void **)&(a), sizeof((a)[0]), 1 ? (b) : (a), (n))

// add all elements of (dynamic) array b to a (a and b should have the same type)
#define array_add_array(a, b)           _arr_add_array((void **)&(a), sizeof((a)[0]), 1 ? (b) : (a))

// sort array with qsort using compare function (see qsort documentation)
#define array_sort(a, compare)          _arr_sort((a), sizeof((a)[0]), compare)

// are arrays a and b equal in content? (byte-wise equality)
#define array_equal(a, b)               _arr_equal(1 ? (a) : (b), sizeof((a)[0]), (b))

// swap elements at index idx1 and idx2
// (Warning: uses at least as much stack space as the size of one element)
#define array_swap(a, idx1, idx2)       _arr_swap(a, idx1, idx2)

// reverse the order of elements in the array
// (Warning: uses at least as much stack space as the size of one element)
#define array_reverse(a)                _arr_reverse(a)

// shuffle the elements in the array with the provided random function of type size_t (*)(void)
// (creates a random permutation of the elements using the Fisher-Yates shuffle)
// (Warning: uses at least as much stack space as the size of one element)
#define array_shuffle(a, random)        _arr_shuffle(a, random)

// Iterators:
// (Warning: do not modify the array inside these loops,
//  use a custom fori loop that does the necessary index adjustments for this purpose instead)

// iterate over all array indices
// (a variable named 'itername' of type size_t will contain the current index)
#define array_fori(a, itername)         _arr_fori(a, itername)

// iterate over all array indices in reverse
// (a variable named 'itername' of type size_t will contain the current index)
#define array_fori_reverse(a, itername) _arr_fori_reverse(a, itername)

// iterate over all array elements
// #ifndef ARRAY_NO_TYPEOF: a variable named 'itername' will contain a pointer to the current element
// #else: 'itername' must be a variable of type 'pointer to array element' (same as the array type)
//        and will contain the current element
// (Warning: evaluates 'a' multiple times! TODO is that necessary?)
#define array_foreach(a, itername)               _arr_foreach(a, itername)

// iterate over all array elements in reverse
// #ifndef ARRAY_NO_TYPEOF (a variable named 'itername' will contain a pointer to the current element)
// #else: 'itername' must be a variable of type 'pointer to array element' (same as the array type)
//        and will contain the current element
#define array_foreach_reverse(a, itername)       _arr_foreach_reverse(a, itername)

// iterate over all array elements
// #ifndef ARRAY_NO_TYPEOF: a variable named 'itername' will contain the current element _by value_
// #else: 'itername' must be a variable of the same type as an array element
//        and will contain the current element _by value_
#define array_foreach_value(a, itername)         _arr_foreach_value(a, itername)

// iterate over all array elements in reverse
// #ifndef ARRAY_NO_TYPEOF (a variable named 'itername' will contain the current element _by value_)
// #else: 'itername' must be a variable of the same type as an array element
//        and will contain the current element _by value_
#define array_foreach_value_reverse(a, itername) _arr_foreach_value_reverse(a, itername)

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 3)
# define _arr_nonnull(...)                  __attribute__((nonnull (__VA_ARGS__)))
# define _arr_maybe_unused                  __attribute__((unused))
# define _arr_warn_unused_result            __attribute__((warn_unused_result))
#else
# define _arr_nonnull(...)
# define _arr_maybe_unused
# define _arr_warn_unused_result
#endif

#ifndef ARRAY_MAGIC1
#define ARRAY_MAGIC1 0xdeadbabe
#define ARRAY_MAGIC2 0xbeefcafe
#endif

typedef struct {
	size_t len;
	size_t capacity;
#if ARRAY_SAFETY_CHECKS
	size_t magic1;
	size_t magic2;
#endif
} _arr;

#if ARRAY_SAFETY_CHECKS
# include <assert.h>
# define _arrhead_unchecked(a) ((const _arr *)(a) - 1)
# define _arrhead_const(a) (assert((a) &&				\
				   _arrhead_unchecked(a)->magic1 == ARRAY_MAGIC1 && \
				   _arrhead_unchecked(a)->magic2 == ARRAY_MAGIC2), \
			    _arrhead_unchecked(a))
#else
# define _arrhead_const(a) ((const _arr *)(a) - 1)
#endif

#define _arrhead(a) ((_arr *)_arrhead_const(a))

static _arr_maybe_unused size_t _arr_len(const void *arr)
{
	return arr ? _arrhead_const(arr)->len : 0;
}

static _arr_nonnull(1) _arr_maybe_unused size_t _arr_lasti(const void *arr)
{
	size_t len = _arr_len(arr);
#if ARRAY_SAFETY_CHECKS
	assert(len != 0);
#endif
	return len - 1;
}

static _arr_maybe_unused size_t _arr_capacity(const void *arr)
{
	return arr ? _arrhead_const(arr)->capacity : 0;
}

static _arr_maybe_unused void _arr_reset(void *arr)
{
	if (arr) {
		_arrhead(arr)->len = 0;
	}
}

static _arr_maybe_unused void _arr_truncate(void *arr, size_t newlen)
{
	if (newlen < _arr_len(arr)) {
		_arrhead(arr)->len = newlen;
	}
}

static _arr_maybe_unused _arr_warn_unused_result
void *_arr_resize_internal(void *arr, size_t elem_size, size_t capacity)
{
	if (capacity == 0) {
		if (arr) {
			free(_arrhead(arr));
		}
		return NULL;
	}
	size_t new_size = sizeof(_arr) + (capacity * elem_size);
#if ARRAY_SAFETY_CHECKS
	assert(((capacity * elem_size) / elem_size == capacity) && new_size > sizeof(_arr));
#endif
	_arr *head;
	if (!arr) {
		head = malloc(new_size);
		head->len = 0;
#if ARRAY_SAFETY_CHECKS
		head->magic1 = ARRAY_MAGIC1;
		head->magic2 = ARRAY_MAGIC2;
#endif
	} else {
		head = realloc(_arrhead(arr), new_size);
		if (head->len > capacity) {
			head->len = capacity;
		}
	}
	head->capacity = capacity;
	return head + 1;
}

static _arr_maybe_unused void _arr_free(void **arrp)
{
	*arrp = _arr_resize_internal(*arrp, 0, 0);
}

static _arr_maybe_unused void _arr_resize(void **arrp, size_t elem_size, size_t capacity)
{
	*arrp = _arr_resize_internal(*arrp, elem_size, capacity);
}

static _arr_maybe_unused _arr_warn_unused_result void *_arr_copy(const void *arr, size_t elem_size)
{
	void *new_arr = _arr_resize_internal(NULL, elem_size, _arr_capacity(arr));
	if (!new_arr) {
		return NULL;
	}
	_arrhead(new_arr)->len = _arr_len(arr);
	memcpy(new_arr, arr, elem_size * _arr_len(arr));
	return new_arr;
}

static _arr_maybe_unused void _arr_grow(void **arrp, size_t elem_size, size_t n)
{
	if (n == 0) {
		return;
	}
	size_t capacity = _arr_capacity(*arrp);
	size_t new_capacity = n < capacity ? 2 * capacity : capacity + n;
	if (new_capacity < ARRAY_INITIAL_SIZE) {
		new_capacity = ARRAY_INITIAL_SIZE;
	}
	*arrp = _arr_resize_internal(*arrp, elem_size, new_capacity);
}

static _arr_maybe_unused void _arr_reserve(void **arrp, size_t elem_size, size_t n)
{
	size_t rem = _arr_capacity(*arrp) - _arr_len(*arrp);
	if (n > rem) {
		_arr_grow(arrp, elem_size, n - rem);
	}
}

static _arr_maybe_unused void _arr_make_valid(void **arrp, size_t elem_size, size_t i)
{
	size_t capacity = _arr_capacity(*arrp);
	if (i >= capacity) {
		_arr_grow(arrp, elem_size, i - capacity + 1);
	}
	if (i >= _arr_len(*arrp)) {
		// *arrp cannot be null here
		_arrhead(*arrp)->len = i + 1;
	}
}

static _arr_maybe_unused void *_arr_addn(void **arrp, size_t elem_size, size_t n)
{
	if (n == 0) {
		return NULL;
	}
	if (!(*arrp)) {
		_arr_grow(arrp, elem_size, n);
		_arrhead(*arrp)->len = n;
		return *arrp;
	}
	_arr *head = _arrhead(*arrp);
	size_t old_len = head->len;
	head->len += n;
	if (head->len > head->capacity) {
		_arr_grow(arrp, elem_size, head->len - head->capacity);
	}
	return (char *)(*arrp) + (old_len * elem_size);
}

static _arr_maybe_unused void *_arr_insertn(void **arrp, size_t elem_size, size_t i, size_t n)
{
	void *arr = *arrp;
	size_t len = _arr_len(arr);
#if ARRAY_SAFETY_CHECKS
	assert(i <= len);
#endif
	if (len == 0) {
		return _arr_addn(arrp, elem_size, n);
	}
	_arr_addn(&arr, elem_size, n);
	char *src = (char *)arr + i * elem_size;
	char *dst = (char *)arr + (i + n) * elem_size;
	memmove(dst, src, (len - i) * elem_size);
	*arrp = arr;
	return src;
}

static _arr_maybe_unused void _arr_ordered_deleten(void *arr, size_t elem_size, size_t i, size_t n)
{
	size_t len = _arr_len(arr);
#if ARRAY_SAFETY_CHECKS
	assert(i < len && n <= len && (i + n) <= len);
#endif
	char *src = (char *)arr + (i + n) * elem_size;
	char *dst = (char *)arr + i * elem_size;
	memmove(dst, src, (len - (i + n)) * elem_size);
	_arrhead(arr)->len -= n;
}

static _arr_maybe_unused void _arr_fast_deleten(void *arr, size_t elem_size, size_t i, size_t n)
{
	size_t len = _arr_len(arr);
#if ARRAY_SAFETY_CHECKS
	assert(i < len && n <= len && (i + n) <= len);
#endif
	size_t k = len - (i + n);
	if (k > n) {
		k = n;
	}
	char *src = (char *)arr + (len - k) * elem_size;
	char *dst = (char *)arr + i * elem_size;
	memmove(dst, src, k * elem_size);
	_arrhead(arr)->len -= n;
}

static _arr_maybe_unused void _arr_shrink_to_fit(void **arrp, size_t elem_size)
{
	*arrp = _arr_resize_internal(*arrp, elem_size, _arr_len(*arrp));
}

static _arr_nonnull(1, 3) _arr_maybe_unused
size_t _arr_index_of(const void *arr, size_t elem_size, const void *ptr)
{
	size_t diff = (char *)ptr - (char *)arr;
	size_t index = diff / elem_size;
#if ARRAY_SAFETY_CHECKS
	assert(diff % elem_size == 0);
	assert(index < _arr_len(arr));
#endif
	return index;
}

static _arr_nonnull(1) _arr_maybe_unused void _arr_popn(void *arr, size_t n)
{
	assert(n <= _arr_len(arr));
	_arrhead(arr)->len -= n;
}

static _arr_maybe_unused void _arr_add_arrayn(void **arrp, size_t elem_size, const void *arr2, size_t n)
{
	void *dst = _arr_addn(arrp, elem_size, n);
	memcpy(dst, arr2, n * elem_size);
}

static _arr_nonnull(3) _arr_maybe_unused
void _arr_sort(void *arr, size_t elem_size, int (*compare)(const void *, const void *))
{
	qsort(arr, _arr_len(arr), elem_size, compare);
}

static _arr_maybe_unused void _arr_add_array(void **arrp, size_t elem_size, const void *arr2)
{
	_arr_add_arrayn(arrp, elem_size, arr2, _arr_len(arr2));
}

static _arr_maybe_unused _arr_warn_unused_result
_Bool _arr_equal(const void *arr1, size_t elem_size, const void *arr2)
{
	if (arr1 == arr2) {
		// the code below does not cover the case where both are NULL!
		return 1;
	}
	size_t len = _arr_len(arr1);
	if (len != _arr_len(arr2)) {
		return 0;
	}
	return memcmp(arr1, arr2, len * elem_size) == 0;
}

static _arr_nonnull(1, 3) _arr_maybe_unused
void _arr_swap_elements(void *arr, size_t elem_size, unsigned char *buf, size_t i, size_t j)
{
	if (i == j) {
		return;
	}
	size_t n = elem_size;
	unsigned char *a = arr;
	memcpy(buf,       &a[n * i], n);
	memcpy(&a[n * i], &a[n * j], n);
	memcpy(&a[n * j], buf,       n);
}

static _arr_nonnull(3) _arr_maybe_unused void _arr_reverse(void *arr, size_t elem_size, unsigned char *buf)
{
	for (size_t i = 0; i < _arr_len(arr) / 2; i++) {
		_arr_swap_elements(arr, elem_size, buf, i, _arr_lasti(arr) - i);
	}
}

static _arr_nonnull(3, 4) _arr_maybe_unused
void _arr_shuffle_elements(void *arr, size_t elem_size, unsigned char *buf, size_t (*rand_sizet)(void))
{
	for (size_t i = _arr_lasti(arr); i > 0; i--) {
		_arr_swap_elements(arr, elem_size, buf, i, rand_sizet() % (i + 1));
	}
}

// TODO align the buffers?

#define _arr_swap(a, idx1, idx2)					\
	do {								\
		unsigned char buf[sizeof((a)[0])];			\
		_arr_swap_elements((a), sizeof((a)[0]), buf, (idx1), (idx2)); \
	} while (0)

#define _arr_reverse(a)					\
	do {						\
		unsigned char buf[sizeof((a)[0])];	\
		_arr_reverse((a), sizeof((a)[0]), buf);	\
	} while (0)

#define _arr_shuffle(a, random)						\
	do {								\
		unsigned char buf[sizeof((a)[0])];			\
		_arr_shuffle_elements((a), sizeof((a)[0]), buf, (random)); \
	} while (0)

#ifdef ARRAY_NO_TYPEOF
# define _arr_last(a) ((a)[array_lasti(a)])
#else
# define _arr_last(a) (*(__ARRAY_TYPEOF(a))_arr_last_pointer((a), sizeof((a)[0])))
static _arr_nonnull(1) _arr_maybe_unused void *_arr_last_pointer(void *arr, size_t elem_size)
{
	return (char *)arr + _arr_lasti(arr) * elem_size;
}
#endif

#define _arr_add(a, v) ((void)(array_addn(a, 1), ((a)[array_lasti(a)] = (v))))

#define _arr_insert(a, i, v) ((void)(array_insertn(a, i, 1), ((a)[(i)] = (v)), &((a)[(i)])))

#ifdef ARRAY_NO_TYPEOF
# if ARRAY_SAFETY_CHECKS
#  define _arr_pop(a) (assert(!array_empty(a)), (a)[--_arrhead(a)->len])
# else
#  define _arr_pop(a) ((a)[--_arrhead(a)->len])
# endif
#else
# define _arr_pop(a) (*(__ARRAY_TYPEOF(a))_arr_pop_and_return_pointer((a), sizeof((a)[0])))
static _arr_nonnull(1) _arr_maybe_unused void *_arr_pop_and_return_pointer(void *arr, size_t elem_size)
{
	_arrhead(arr)->len--;
	return (char *)arr + _arr_len(arr) * elem_size;
}
#endif

#define _arr_fori(a, itername) for (size_t (itername) = 0, _arr_iter_end_##__LINE__ = array_len(a); (itername) < _arr_iter_end_##__LINE__; (itername)++)
#define _arr_fori_reverse(a, itername) for (size_t (itername) = array_len(a); (itername)-- > 0;)
#ifndef ARRAY_NO_TYPEOF
# define _arr_foreach(a, itername) for (__ARRAY_TYPEOF((a)[0]) *(itername) = (a), *_arr_iter_end_##__LINE__ = (itername) + array_len(a); (itername) < (_arr_iter_end_##__LINE__); (itername)++)
# define _arr_foreach_reverse(a, itername) for (__ARRAY_TYPEOF((a)[0]) *_arr_iter_end_##__LINE__ = (a), *(itername) = (_arr_iter_end_##__LINE__) + array_len(a); (itername)-- > (a);)
# define _arr_foreach_value(a, itername) for (__ARRAY_TYPEOF((a)[0]) (itername), *iter = (a), *_arr_iter_end_##__LINE__ = (a) + array_len(a); (iter) < (_arr_iter_end_##__LINE__) && ((itername) = *iter, 1); (iter)++)
# define _arr_foreach_value_reverse(a, itername) for (__ARRAY_TYPEOF((a)[0]) (itername), *iter = (a) + array_len(a); iter-- > (a) && ((itername) = *iter, 1);)
#else
# define _arr_foreach(a, v) for (size_t _arr_loop_counter_##__LINE__ = 0, _arr_iter_end_##__LINE__ = array_len(a); _arr_loop_counter_##__LINE__ < _arr_iter_end_##__LINE__ && ((v) = &(a)[_arr_loop_counter_##__LINE__], 1); _arr_loop_counter_##__LINE__++)
# define _arr_foreach_reverse(a, v) for (size_t _arr_loop_counter_##__LINE__ = array_len(a); _arr_loop_counter_##__LINE__-- > 0 && ((v) = &(a)[_arr_loop_counter_##__LINE__], 1);)
# define _arr_foreach_value(a, v) for (size_t _arr_loop_counter_##__LINE__ = 0, _arr_iter_end_##__LINE__ = array_len(a); _arr_loop_counter_##__LINE__ < _arr_iter_end_##__LINE__ && ((v) = (a)[_arr_loop_counter_##__LINE__], 1); _arr_loop_counter_##__LINE__++)
# define _arr_foreach_value_reverse(a, v) for (size_t _arr_loop_counter_##__LINE__ = array_len(a); _arr_loop_counter_##__LINE__-- > 0 && ((v) = (a)[_arr_loop_counter_##__LINE__], 1);)
#endif

#endif
