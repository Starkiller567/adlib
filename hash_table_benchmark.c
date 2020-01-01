#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "array.h"
#include "mprintf.h"
// #include "hash_table.h"
// #include "block_table.h"
// #include "robin_hood.h"
#include "flat_hash.h"

#define N 50

static inline unsigned int integer_hash(unsigned int key)
{
	unsigned int h = key;
	/* h = ~h; */
	/* h ^= (key << 16) | (key >> 16); */
	return h;
}

static unsigned long string_hash(const void *string)
{
	const void *data = string;
	const size_t nbytes = strlen(string);
	if (data == NULL || nbytes == 0) {
		return 0;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *)data;
	const uint8_t *tail = (const uint8_t *)data + (nblocks * 4);

	uint32_t h = 0;

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
	case 3:
		k ^= tail[2] << 16;
	case 2:
		k ^= tail[1] << 8;
	case 1:
		k ^= tail[0];
		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;
		h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

struct short_string {
	char s[128];
};

static unsigned long short_string_hash(const struct short_string s)
{
	return string_hash(s.s);
}

static unsigned long long tp_to_ns(struct timespec *tp)
{
	return tp->tv_nsec + 1000000000 * tp->tv_sec;
}

static unsigned long long ns_elapsed(struct timespec *start, struct timespec *end)
{
	unsigned long long s = end->tv_sec - start->tv_sec;
	unsigned long long ns = end->tv_nsec - start->tv_nsec;
	return ns + 1000000000 * s;
}

static double get_avg_rate(unsigned long long nanoseconds[N], unsigned int num_items)
{
	double avg = 0.0;
	for (unsigned int i = 0; i < N; i++) {
		avg += nanoseconds[i];
	}
	avg /= N;
	return num_items / avg;
}

DEFINE_HASHTABLE(itable, int, int, 8, (*a == *b))
DEFINE_HASHTABLE(stable, char *, char *, 8, (strcmp(*a, *b) == 0))
DEFINE_HASHTABLE(sstable, char *, struct short_string, 8, (strcmp(*a, *b) == 0))
DEFINE_HASHTABLE(ssstable, struct short_string, struct short_string, 8, (strcmp(a->s, b->s) == 0))

#define BENCHMARK(name, hash, item_type, ...)				\
	for (unsigned int n = 0; n < N; n++) {				\
		fprintf(stderr, "%u/%u\r", n, N);			\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			item_type *item = name##_insert(&name, arr1[i], hash(arr1[i])); \
			*item = arr1[i];				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		insert[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			item_type *item = name##_lookup(&name, arr2[i], hash(arr2[i])); \
			item_type *a = item;				\
			item_type *b = &arr2[i];			\
			assert(__VA_ARGS__);				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup1[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			item_type *item = name##_lookup(&name, arr3[i], hash(arr3[i])); \
			assert(!item);					\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		for (i = 0; i < num_items; i++) {			\
			item_type *item = name##_insert(&name, arr3[i], hash(arr3[i])); \
			*item = arr3[i];				\
		}							\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			item_type key;					\
			item_type item;					\
			bool found = name##_remove(&name, arr4[i], hash(arr4[i]), &key, &item);	\
			item_type *a = &key;				\
			item_type *b = &item;				\
			assert(found && (__VA_ARGS__));			\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		delete[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			{						\
				item_type *item = name##_lookup(&name, arr2[i], hash(arr2[i])); \
				assert(!item);				\
			}						\
			{						\
				item_type *item = name##_insert(&name, arr2[i], hash(arr2[i])); \
				*item = arr2[i];			\
			}						\
			{						\
				item_type key;				\
				item_type item;				\
				bool found = name##_remove(&name, arr1[i], hash(arr1[i]), &key, &item);	\
				item_type *a = &key;			\
				item_type *b = &item;			\
				assert(!found || (__VA_ARGS__));	\
			}						\
			{						\
				item_type *item = name##_lookup(&name, arr4[i], hash(arr4[i])); \
				item_type *a = item;			\
				item_type *b = &arr4[i];		\
				assert(!item || (__VA_ARGS__));		\
			}						\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		mixed[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_items; i++) {			\
			{						\
				item_type *item = name##_insert(&name, arr1[i], hash(arr1[i])); \
				*item = arr1[i];			\
			}						\
			for (unsigned int j = i - 10; j < i; j++) {	\
				item_type *item = name##_lookup(&name, arr1[j], hash(arr1[j])); \
				item_type *a = item;			\
				item_type *b = &arr1[j];		\
				assert(__VA_ARGS__);			\
			}						\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		mixed2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
	}								\
									\
	printf("%10.2fM insertions per second\n", 1000.0 * get_avg_rate(insert, num_items)); \
	printf("%10.2fM lookups per second (found)\n", 1000.0 * get_avg_rate(lookup1, num_items)); \
	printf("%10.2fM lookups per second (not found)\n", 1000.0 * get_avg_rate(lookup2, num_items)); \
	printf("%10.2fM deletions per second\n", 1000.0 * get_avg_rate(delete, num_items)); \
	printf("%10.2fM mixed ops (with delete) per second\n", 1000.0 * get_avg_rate(mixed, num_items)); \
	printf("%10.2fM mixed ops (no delete) per second\n", 1000.0 * get_avg_rate(mixed2, num_items));	\


int main(int argc, char **argv)
{
	struct itable itable;
	struct stable stable;
	// struct sstable sstable;
	struct ssstable ssstable;
	unsigned int i, seed = 12345, num_items;
	struct timespec start_tp, end_tp;

	unsigned long long insert[N];
	unsigned long long lookup1[N];
	unsigned long long lookup2[N];
	unsigned long long delete[N];
	unsigned long long mixed[N];
	unsigned long long mixed2[N];

	srand(seed);

	if (1) {
		puts("\nitable");
		puts("--------------------------------------------------------");

		num_items = 1000000;

		int *arr1 = NULL;
		for (i = 0; i < num_items; i++) {
			array_add(arr1, i);
		}
		array_shuffle(arr1);

		int *arr2 = array_copy(arr1);
		array_shuffle(arr2);

		int *arr3 = NULL;
		for (i = 0; i < num_items; i++) {
			array_add(arr3, i + num_items);
		}
		array_shuffle(arr3);

		int *arr4 = array_copy(arr1);
		array_shuffle(arr4);

		BENCHMARK(itable, integer_hash, int, *a == *b);

		puts("--------------------------------------------------------");
		puts("\n");
	}

	srand(seed);

	if (1) {
		puts("\nstable");
		puts("--------------------------------------------------------");

		num_items = 100000;

		char **arr1 = NULL;
		for (i = 0; i < num_items; i++) {
			array_add(arr1, mprintf("%i", i));
		}
		array_shuffle(arr1);

		char **arr2 = array_copy(arr1);
		array_shuffle(arr2);

		char **arr3 = NULL;
		for (i = 0; i < num_items; i++) {
			array_add(arr3, mprintf("%i", i + num_items));
		}
		array_shuffle(arr3);

		char **arr4 = array_copy(arr1);
		array_shuffle(arr4);

		BENCHMARK(stable, string_hash, char *, strcmp(*a, *b) == 0);

		puts("--------------------------------------------------------");
		puts("\n");
	}

	srand(seed);

	if (1) {
		puts("\nssstable");
		puts("--------------------------------------------------------");

		num_items = 100000;

		struct short_string *arr1 = NULL;
		for (i = 0; i < num_items; i++) {
			struct short_string *item = array_addn(arr1, 1);
			sprintf(item->s, "%i", i);
		}
		array_shuffle(arr1);

		struct short_string *arr2 = array_copy(arr1);
		array_shuffle(arr2);

		struct short_string *arr3 = NULL;
		for (i = 0; i < num_items; i++) {
			struct short_string *item = array_addn(arr3, 1);
			sprintf(item->s, "%i", i + num_items);
		}
		array_shuffle(arr3);

		struct short_string *arr4 = array_copy(arr1);
		array_shuffle(arr4);

		BENCHMARK(ssstable, short_string_hash, struct short_string, strcmp(a->s, b->s) == 0);

		puts("--------------------------------------------------------");
		puts("\n");
	}
}
