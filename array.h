// based on https://github.com/nothings/stb/blob/master/stb.h

#ifndef __array_include__
#define __array_include__

#include <stdlib.h>
#include <string.h>

typedef struct {
	unsigned int len;
	unsigned int limit;
} __arr;
#define __arrhead(a) ((__arr *)a - 1)

#define array_len(a)                  (a ? __arrhead(a)->len : 0)
#define array_empty(a)                (array_len(a) == 0)
#define array_lasti(a)                (array_len(a) - 1)
#define array_last(a)                 (a[array_lasti(a)])
#define array_limit(a)                (a ? __arrhead(a)->limit : 0)
#define array_free(a)                  __array_free((void **)&(a))
#define array_copy(a)	               __array_copy((a), sizeof((a)[0]))
#define array_addn(a, n)               __array_addn((void **)&(a), sizeof((a)[0]), n)
#define array_add(a, v)                (array_addn(a, 1), (a[array_lasti(a)]  = v))
#define array_insertn(a, i, n)         __array_insertn((void **)&(a), sizeof((a)[0]), i, n)
#define array_insert(a, i, v)          (array_insertn(a, i, 1), ((a)[i] = v))
#define array_clear(a)	               (__arrhead(a)->len = 0)
#define array_resize(a, limit)         __array_resize((void **)&(a), sizeof((a)[0]), limit)
#define array_grow(a, n)               __array_grow((void **)&(a), sizeof((a)[0]), n)
#define array_shrink_to_fit(a)         array_resize(a, array_len(a));
#define array_make_valid(a, i)         __array_make_valid((void **)&(a), sizeof((a)[0]), i);
#define array_push(a, v)               array_add(a, v)
#define array_pop(a)                   ((a)[--__arrhead(a)->len])
#define array_popn(a, n)	       (__arrhead(a)->len -= n)
#define array_fast_deleten(a, i, n)    __array_fast_deleten((void **)&(a), sizeof((a)[0]), i, n)
#define array_fast_delete(a, i)        (((a)[i] = array_last(a)), __arrhead(a)->len--)
#define array_ordered_deleten(a, i, n) __array_ordered_deleten((void **)&(a), sizeof((a)[0]), i, n)
#define array_ordered_delete(a, i)     array_ordered_deleten(a, i, 1)
#define array_foreach(a, v)            for ((v) = (a); (v) < (a) + array_len(a); (v)++)
#define array_foreach_reverse(a, v)    for ((v) = &array_last(a); (v) >= (a); (v)--)

static void *__array_copy(void *arr, unsigned int elem_size)
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

static void __array_resize(void **arrp, unsigned int elem_size, unsigned int limit)
{
	if (limit == 0) {
		__array_free(arrp);
		return;
	}
	__arr *head;
	size_t new_size = sizeof(__arr) + (limit * elem_size);
	if (!(*arrp)) {
		head = malloc(new_size);
		head->len = 0;
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
static void __array_grow(void **arrp, unsigned int elem_size, unsigned int n)
{
	if (n == 0) {
		return;
	}
	unsigned int limit = array_limit(*arrp);
	unsigned int new_limit = n < limit ? 2 * limit : limit + n;
	__array_resize(arrp, elem_size, new_limit);
}

static void __array_make_valid(void **arrp, unsigned int elem_size, unsigned int i)
{
	unsigned int limit = array_limit(*arrp);
	if (i >= limit) {
		__array_grow(arrp, elem_size, i - limit + 1);
	}
	if (i >= array_len(*arrp)) {
		__arrhead(*arrp)->len = i + 1;
	}
}

static void __array_addn(void **arrp, unsigned int elem_size, unsigned int n)
{
	if (!(*arrp)) {
		__array_grow(arrp, elem_size, n);
		__arrhead(*arrp)->len = n;
		return;
	}
	__arr *head = __arrhead(*arrp);
	head->len += n;
	if (head->len > head->limit) {
		__array_grow(arrp, elem_size, head->len - head->limit);
	}
}

static void __array_insertn(void **arrp, unsigned int size, unsigned int i, unsigned int n)
{
	void *arr = *arrp;
	if (!arr) {
		__array_addn(arrp, size, n);
		return;
	}
	unsigned int len = array_len(arr);
	__array_addn(&arr, size, n);
	memmove((char *)arr + (i + n) * size, (char *)arr + i * size, (len - i) * size);
	*arrp = arr;
}

static void __array_ordered_deleten(void **arrp, unsigned int size, unsigned int i, unsigned int n)
{
	char *arr = *arrp;
	unsigned int len = array_len(arr);
	memmove(arr + i * size, arr + (i + n) * size, (len - (i + n)) * size);
	__arrhead(arr)->len -= n;
}

static void __array_fast_deleten(void **arrp, unsigned int size, unsigned int i, unsigned int n)
{
	char *arr = *arrp;
	unsigned int len = array_len(arr);
	unsigned int k = len - (i + n);
	if (k > n) {
		k = n;
	}
	memmove(arr + i * size, arr + (len - k) * size, k * size);
	__arrhead(arr)->len -= n;
}

#endif
