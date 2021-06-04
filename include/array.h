/*
 * Copyright (C) 2020-2021 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// inspired by https://github.com/nothings/stb/blob/master/stb.h

// declare an (empty) array like so: array_t(int) my_array = NULL;
// (or so: int *my_array = NULL;)
// and use it like so: array_add(my_array, 1)
// or so:
//         void add_a_one(array_t(int) *some_array)
//         {
//             array_add(*some_array, 1);
//         }
//         ...
//         add_a_one(&my_array);
//         add_a_one(&my_array);
//         add_a_one(&my_array);
//         int sum = 0;
//         array_fori(my_array, current_index) {
//             int current_element = my_array[current_index];
//             sum += current_element;
//         }
//         assert(sum == 3);
//         array_free(my_array);
//

// TODO array_memset, array_set_all, array_push_repeat
// TODO clearly define when reallocation happens
// TODO handle non-standard alignments?

#ifndef __ARRAY_INCLUDE__
#define __ARRAY_INCLUDE__

#include <string.h>
#include "config.h"
#include "macros.h"

#ifdef ARRAY_SAFETY_CHECKS
# include <assert.h>
#endif

_Static_assert(HAVE_TYPEOF, "the array implementation requires typeof");

_Static_assert(ARRAY_GROWTH_FACTOR_NUMERATOR > ARRAY_GROWTH_FACTOR_DENOMINATOR,
	       "array growth factor must be greater than one");

// this macro provides a more explicit way of declaring a dynamic array
// (array_t(int) arr vs int *arr; the second could be a simple pointer or static array or dynamic array)
#define array_t(T) T *

// get number of elements in array (as size_t)
#define array_len(a)                    _arr_len(a)

// is array empty? (empty arrays are not always NULL due to array_reserve and array_clear)
#define array_empty(a)                  (_arr_len(a) == 0)

#define array_lasti(a)                  _arr_lasti(a)

// get last element
#define array_last(a)                   _arr_last(a)

// get allocated capacity in elements (as size_t)
#define array_capacity(a)               _arr_capacity(a)

// release all resources of the array (will be set to NULL)
#define array_free(a)                   _arr_free((void **)&(a))

// make an exact copy of the array
#define array_copy(a)	                ((typeof(a))_arr_copy((a), sizeof((a)[0])))

// b = array_move(a) is equivalent to b = a, a = NULL
#define array_move(a)	                ((typeof(a))_arr_move((void **)&(a)))

// add n unitialized elements to the end of the array and return a pointer to the first of those
// (returns NULL if n is zero)
#define array_addn(a, n)                ((typeof(a))_arr_addn((void **)&(a), sizeof((a)[0]), (n)))

// add one unitialized element to the end of the array and return a pointer to it
#define array_add1(a)                   array_addn((a), 1)

// add n zeroed elements to the end of the array and return a pointer to the first of those
// (returns NULL if n is zero)
#define array_addn_zero(a, n)           ((typeof(a))_arr_addn_zero((void **)&(a), sizeof((a)[0]), (n)))

// add one zeroed element to the end of the array and return a pointer to it
#define array_add1_zero(a)              array_addn_zero((a), 1)

// add value v to the end of the array (v must be an r-value of array element type)
#define array_add(a, v)                 _arr_add(a, v)

// insert n unitialized elements at index i and return a pointer to the first of them
// (i must be less than or equal to array_len(a))
// (returns NULL if n is zero)
#define array_insertn(a, i, n)          (typeof(a))_arr_insertn((void **)&(a), sizeof((a)[0]), (i), (n))

// insert one unitialized element at index i and return a pointer to it
// (i must be less than or equal to array_len(a))
#define array_insert1(a, i)             array_insertn((a), (i), 1)

// insert n zeroed elements at index i and return a pointer to the first of them
// (i must be less than or equal to array_len(a))
// (returns NULL if n is zero)
#define array_insertn_zero(a, i, n)     (typeof(a))_arr_insertn_zero((void **)&(a), sizeof((a)[0]), (i), (n))

// insert one zeroed element at index i and return a pointer to it
// (i must be less than or equal to array_len(a))
#define array_insert1_zero(a, i)        array_insertn_zero((a), (i), 1)

// insert value v at index i
// (v must be an r-value of array element type and i must be less than or equal to array_len(a))
#define array_insert(a, i, v)           _arr_insert(a, i, v)

// set the length of the array to zero (but keep the allocated memory)
#define array_clear(a)	                _arr_clear(a)

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

// search array for key with bsearch using compare function (see bsearch documentation)
// (the type of key should be pointer to array element)
// (the array needs to be sorted in ascending order according to the compare function)
#define array_bsearch(a, key, compare)  ((typeof(a))_arr_bsearch((a), sizeof((a)[0]), \
									 1 ? (key) : (a), compare))

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
// (Warning: do not modify the array inside these loops (unless you exit the loop right after),
//  use a custom fori loop that does the necessary index adjustments for this purpose instead)

// iterate over all array indices
// (a variable named 'itername' of type size_t will contain the current index)
#define array_fori(a, itername)                  _arr_fori(a, itername)

// iterate over all array indices in reverse
// (a variable named 'itername' of type size_t will contain the current index)
#define array_fori_reverse(a, itername)          _arr_fori_reverse(a, itername)

// iterate over all array elements,
// (a variable named 'itername' will contain a pointer to the current element)
#define array_foreach(a, itername)               _arr_foreach(a, itername)

// iterate over all array elements in reverse
// (a variable named 'itername' will contain a pointer to the current element)
#define array_foreach_reverse(a, itername)       _arr_foreach_reverse(a, itername)

// iterate over all array elements
// (a variable named 'itername' will contain the _value_ of the current element)
#define array_foreach_value(a, itername)         _arr_foreach_value(a, itername)

// iterate over all array elements in reverse
// (a variable named 'itername' will contain the _value_ of the current element)
#define array_foreach_value_reverse(a, itername) _arr_foreach_value_reverse(a, itername)

#ifndef ARRAY_MAGIC1
# define ARRAY_MAGIC1 ((size_t)0xcccccccccccccccc)
#endif
#ifndef ARRAY_MAGIC2
# define ARRAY_MAGIC2 ((size_t)0xdeadbabebeefcafe)
#endif

typedef struct {
	size_t len;
	size_t capacity;
#ifdef ARRAY_SAFETY_CHECKS
	size_t magic1;
	size_t magic2;
#endif
} _arr;

#ifdef ARRAY_SAFETY_CHECKS
# define _arrhead_unchecked(a) ((_arr *)(a) - 1)
# define _arrhead(a) (assert((a) &&					\
			     _arrhead_unchecked(a)->magic1 == ARRAY_MAGIC1 && \
			     _arrhead_unchecked(a)->magic2 == ARRAY_MAGIC2), \
		      _arrhead_unchecked(a))
#else
# define _arrhead(a) ((_arr *)(a) - 1)
#endif

#define _arrhead_const(a) ((const _arr *)_arrhead(a))

__AD_LINKAGE _attr_warn_unused_result _attr_unused void *_arr_resize_internal(void *arr, size_t elem_size, size_t capacity);
__AD_LINKAGE _attr_warn_unused_result _attr_unused void *_arr_copy(const void *arr, size_t elem_size);
__AD_LINKAGE _attr_unused void _arr_grow(void **arrp, size_t elem_size, size_t n);
__AD_LINKAGE _attr_unused void _arr_make_valid(void **arrp, size_t elem_size, size_t i);
__AD_LINKAGE _attr_unused void *_arr_addn(void **arrp, size_t elem_size, size_t n);
__AD_LINKAGE _attr_unused void *_arr_insertn(void **arrp, size_t elem_size, size_t i, size_t n);
__AD_LINKAGE _attr_unused void _arr_ordered_deleten(void *arr, size_t elem_size, size_t i, size_t n);
__AD_LINKAGE _attr_unused void _arr_fast_deleten(void *arr, size_t elem_size, size_t i, size_t n);
__AD_LINKAGE _attr_nonnull(3) _attr_unused void _arr_sort(void *arr, size_t elem_size, int (*compare)(const void *, const void *));
__AD_LINKAGE _attr_unused void *_arr_bsearch(void *arr, size_t elem_size, const void *key, int (*compare)(const void *, const void *));

static inline _attr_unused size_t _arr_len(const void *arr)
{
	return arr ? _arrhead_const(arr)->len : 0;
}

static inline _attr_nonnull(1) _attr_unused size_t _arr_lasti(const void *arr)
{
	size_t len = _arr_len(arr);
#ifdef ARRAY_SAFETY_CHECKS
	assert(len != 0);
#endif
	return len - 1;
}

static inline _attr_unused size_t _arr_capacity(const void *arr)
{
	return arr ? _arrhead_const(arr)->capacity : 0;
}

static inline _attr_unused void _arr_clear(void *arr)
{
	if (arr) {
		_arrhead(arr)->len = 0;
	}
}

static inline _attr_unused void _arr_truncate(void *arr, size_t newlen)
{
	if (newlen < _arr_len(arr)) {
		_arrhead(arr)->len = newlen;
	}
}

static inline _attr_unused void _arr_free(void **arrp)
{
	*arrp = _arr_resize_internal(*arrp, 0, 0);
}

static inline _attr_unused void _arr_resize(void **arrp, size_t elem_size, size_t capacity)
{
	*arrp = _arr_resize_internal(*arrp, elem_size, capacity);
}

static inline _attr_unused _attr_warn_unused_result void *_arr_move(void **arrp)
{
	void *arr = *arrp;
	*arrp = NULL;
	return arr;
}

static inline _attr_unused void _arr_reserve(void **arrp, size_t elem_size, size_t n)
{
	size_t rem = _arr_capacity(*arrp) - _arr_len(*arrp);
	if (n > rem) {
		_arr_grow(arrp, elem_size, n - rem);
	}
}

static inline _attr_unused void *_arr_addn_zero(void **arrp, size_t elem_size, size_t n)
{
	void *p = _arr_addn(arrp, elem_size, n);
	if (likely(p)) {
		memset(p, 0, elem_size * n);
	}
	return p;
}

static inline _attr_unused void *_arr_insertn_zero(void **arrp, size_t elem_size, size_t i, size_t n)
{
	void *p = _arr_insertn(arrp, elem_size, i, n);
	if (likely(p)) {
		memset(p, 0, elem_size * n);
	}
	return p;
}

static inline _attr_unused void _arr_popn(void *arr, size_t n)
{
#ifdef ARRAY_SAFETY_CHECKS
	assert(n <= _arr_len(arr));
#endif
	_arrhead(arr)->len -= n;
}

static inline _attr_unused void _arr_shrink_to_fit(void **arrp, size_t elem_size)
{
	*arrp = _arr_resize_internal(*arrp, elem_size, _arr_len(*arrp));
}

static inline _attr_nonnull(1, 3) _attr_unused
size_t _arr_index_of(const void *arr, size_t elem_size, const void *ptr)
{
	size_t diff = (char *)ptr - (char *)arr;
	size_t index = diff / elem_size;
#ifdef ARRAY_SAFETY_CHECKS
	assert(diff % elem_size == 0);
	assert(index < _arr_len(arr));
#endif
	return index;
}

static inline _attr_unused void _arr_add_arrayn(void **arrp, size_t elem_size, const void *arr2, size_t n)
{
	void *dst = _arr_addn(arrp, elem_size, n);
	memcpy(dst, arr2, n * elem_size);
}

static inline _attr_unused void _arr_add_array(void **arrp, size_t elem_size, const void *arr2)
{
	_arr_add_arrayn(arrp, elem_size, arr2, _arr_len(arr2));
}

static _attr_unused _attr_warn_unused_result
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

static inline _attr_nonnull(1, 3) _attr_unused
void _arr_swap_elements_unchecked(void *arr, size_t elem_size, unsigned char *buf, size_t i, size_t j)
{
	size_t n = elem_size;
	unsigned char *a = arr;
	memcpy(buf,       &a[n * i], n);
	memcpy(&a[n * i], &a[n * j], n);
	memcpy(&a[n * j], buf,       n);
}

static inline _attr_nonnull(1, 3) _attr_unused
void _arr_swap_elements(void *arr, size_t elem_size, unsigned char *buf, size_t i, size_t j)
{
#ifdef ARRAY_SAFETY_CHECKS
	assert(i < array_len(arr));
	assert(j < array_len(arr));
#endif
	if (likely(i != j)) {
		_arr_swap_elements_unchecked(arr, elem_size, buf, i, j);
	}
}

static inline _attr_nonnull(3) _attr_unused
void _arr_reverse(void *arr, size_t elem_size, unsigned char *buf)
{
	for (size_t i = 0; i < _arr_len(arr) / 2; i++) {
		_arr_swap_elements_unchecked(arr, elem_size, buf, i, _arr_lasti(arr) - i);
	}
}

static inline _attr_nonnull(3, 4) _attr_unused
void _arr_shuffle_elements(void *arr, size_t elem_size, unsigned char *buf, size_t (*random_index)(void))
{
	if (unlikely(array_empty(arr))) {
		return;
	}
	for (size_t i = _arr_lasti(arr); i > 0; i--) {
		_arr_swap_elements_unchecked(arr, elem_size, buf, i, random_index() % (i + 1));
	}
}

#define _arr_swap(a, idx1, idx2)					\
	do {								\
		unsigned char _Alignas(16) buf[sizeof((a)[0])];		\
		_arr_swap_elements((a), sizeof((a)[0]), buf, (idx1), (idx2)); \
	} while (0)

#define _arr_reverse(a)						\
	do {							\
		unsigned char _Alignas(16) buf[sizeof((a)[0])];	\
		_arr_reverse((a), sizeof((a)[0]), buf);		\
	} while (0)

#define _arr_shuffle(a, random)						\
	do {								\
		unsigned char _Alignas(16) buf[sizeof((a)[0])];		\
		_arr_shuffle_elements((a), sizeof((a)[0]), buf, (random)); \
	} while (0)

#define _arr_last(a) (*(typeof(a))_arr_last_pointer((a), sizeof((a)[0])))
static inline _attr_nonnull(1) _attr_unused void *_arr_last_pointer(void *arr, size_t elem_size)
{
	return (char *)arr + _arr_lasti(arr) * elem_size;
}

#define _arr_add(a, v) ((void)(*array_addn(a, 1) = (v)))

#define _arr_insert(a, i, v) ((void)(*array_insertn(a, i, 1) = (v)))

#define _arr_pop(a) (*(typeof(a))_arr_pop_and_return_pointer((a), sizeof((a)[0])))
static inline _attr_nonnull(1) _attr_unused void *_arr_pop_and_return_pointer(void *arr, size_t elem_size)
{
#ifdef ARRAY_SAFETY_CHECKS
	assert(!array_empty(arr));
#endif
	_arrhead(arr)->len--;
	return (char *)arr + _arr_len(arr) * elem_size;
}

#define _arr_fori(a, itername) for (size_t (itername) = 0, _arr_iter_end_##__LINE__ = array_len(a); \
				    (itername) < _arr_iter_end_##__LINE__; \
				    (itername)++)

#define _arr_fori_reverse(a, itername) for (size_t (itername) = array_len(a); (itername)-- > 0;)

#define _arr_foreach(a, itername)					\
	for (typeof((a)[0]) *(itername) = (a), *_arr_end_##__LINE__ = (itername) + array_len(itername); \
	     (itername) < (_arr_end_##__LINE__);			\
	     (itername)++)

#define _arr_foreach_reverse(a, itername)				\
	for (typeof((a)[0]) *_arr_base_##__LINE__ = (a),	\
		     *(itername) = (_arr_base_##__LINE__) + array_len(_arr_base_##__LINE__); \
	     (itername)-- > _arr_base_##__LINE__;)

#define _arr_foreach_value(a, itername)					\
	for (typeof((a)[0]) (itername), *_arr_iter_##__LINE__ = (a), \
		     *_arr_iter_end_##__LINE__ = (_arr_iter_##__LINE__) + array_len(_arr_iter_##__LINE__); \
	     (_arr_iter_##__LINE__) < (_arr_iter_end_##__LINE__) && ((itername) = *(_arr_iter_##__LINE__), 1); \
	     (_arr_iter_##__LINE__)++)

#define _arr_foreach_value_reverse(a, itername)				\
	for (typeof((a)[0]) (itername),	*_arr_base_##__LINE__ = (a), \
		     *_arr_iter_##__LINE__ = (_arr_base_##__LINE__) + array_len(_arr_base_##__LINE__); \
	     (_arr_iter_##__LINE__)-- > (_arr_base_##__LINE__) && ((itername) = *(_arr_iter_##__LINE__), 1);)

#endif
