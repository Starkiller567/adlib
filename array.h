// inspired by https://github.com/nothings/stb/blob/master/stb.h

#ifndef __array_include__
#define __array_include__

#include <stdlib.h>
#include <string.h>

#ifndef ARRAY_SAFETY_CHECKS
# define ARRAY_SAFETY_CHECKS 1
#endif

#define ARRAY_MAGIC1 0xdeadbabe
#define ARRAY_MAGIC2 0xbeefcafe

typedef struct {
	size_t len;
	size_t limit;
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

#define array_len(a)                   ((a) ? __arrhead(a)->len : 0)
#define array_empty(a)                 (array_len(a) == 0)
#define array_lasti(a)                 (array_len(a) - 1)
#define array_last(a)                  ((a)[array_lasti(a)])
#define array_limit(a)                 ((a) ? __arrhead(a)->limit : 0)
#define array_free(a)                   __array_free((void **)&(a))
#define array_copy(a)	                __array_copy((a), sizeof((a)[0]))
#define array_addn(a, n)                __array_addn((void **)&(a), sizeof((a)[0]), n)
#define array_add(a, v)                 (array_addn(a, 1), ((a)[array_lasti(a)]  = v))
#define array_insertn(a, i, n)          __array_insertn((void **)&(a), sizeof((a)[0]), i, n)
#define array_insert(a, i, v)           (array_insertn(a, i, 1), ((a)[i] = v))
#define array_reset(a)	                do { if (a) __arrhead(a)->len = 0; } while(0);
#define array_resize(a, limit)          __array_resize((void **)&(a), sizeof((a)[0]), limit)
#define array_reserve(a, n)             __array_reserve((void **)&(a), sizeof((a)[0]), n)
#define array_grow(a, n)                __array_grow((void **)&(a), sizeof((a)[0]), n)
#define array_shrink_to_fit(a)          array_resize(a, array_len(a));
#define array_make_valid(a, i)          __array_make_valid((void **)&(a), sizeof((a)[0]), i);
#define array_push(a, v)                array_add(a, v)
#define array_pop(a)                    ((a)[--__arrhead(a)->len])
#define array_popn(a, n)	        (__arrhead(a)->len -= n)
#define array_fast_deleten(a, i, n)     __array_fast_deleten((a), sizeof((a)[0]), i, n)
#define array_fast_delete(a, i)         (((a)[i] = array_last(a)), __arrhead(a)->len--)
#define array_ordered_deleten(a, i, n)  __array_ordered_deleten((a), sizeof((a)[0]), i, n)
#define array_ordered_delete(a, i)      array_ordered_deleten(a, i, 1)
#define array_fori(a, itername)         for (size_t itername = 0; itername < array_len(a); itername++)
#define array_fori_reverse(a, itername) for (size_t itername = array_len(a); \
					     !array_empty(a) && --itername != 0;)
#define array_foreach(a, v)             for ((v) = (a); (v) < (a) + array_len(a); (v)++)
#define array_foreach_reverse(a, v)     for ((v) = (a) + array_len(a);	\
					     !array_empty(a) && --(v) >= (a);)

static void *__array_copy(void *arr, size_t elem_size)
{
	if (!arr) {
		return NULL;
	}
	__arr *new_arr = (__arr *)malloc(sizeof(*new_arr) + elem_size * array_limit(arr));
	memcpy(new_arr, __arrhead(arr), sizeof(*new_arr) + elem_size * array_len(arr));
	return new_arr + 1;
}

static void __array_free(void **arrp)
{
	void *arr = *arrp;
	if (arr) {
		free(__arrhead(arr));
		*arrp = NULL;
	}
}

static void __array_resize(void **arrp, size_t elem_size, size_t limit)
{
	if (limit == 0) {
		__array_free(arrp);
		return;
	}
	size_t new_size = sizeof(__arr) + (limit * elem_size);
#if ARRAY_SAFETY_CHECKS
	assert(((limit * elem_size) / elem_size == limit) && new_size > sizeof(__arr));
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
		if (head->len > limit) {
			head->len = limit;
		}
	}
	head->limit = limit;
	*arrp = head + 1;
}

// increase limit by atleast n elements
static void __array_grow(void **arrp, size_t elem_size, size_t n)
{
	if (n == 0) {
		return;
	}
	size_t limit = array_limit(*arrp);
	size_t new_limit = n < limit ? 2 * limit : limit + n;
	__array_resize(arrp, elem_size, new_limit);
}

static void __array_reserve(void **arrp, size_t elem_size, size_t n)
{
	if (n == 0) {
		return;
	}
	size_t rem = array_limit(*arrp) - array_len(*arrp);
	if (n > rem) {
		__array_grow(arrp, elem_size, n - rem);
	}
}

static void __array_make_valid(void **arrp, size_t elem_size, size_t i)
{
	size_t limit = array_limit(*arrp);
	if (i >= limit) {
		__array_grow(arrp, elem_size, i - limit + 1);
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
	if (head->len > head->limit) {
		__array_grow(arrp, elem_size, head->len - head->limit);
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
