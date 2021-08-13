#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "array.h"
#include "config.h"
#include "macros.h"
#include "random.h"
#include "hashtable.h"

#define N 2

// TODO remove this eventually
#include <stdarg.h>
static char * _attr_format_printf(1, 2)
mprintf(const char *fmt, ...)
{
	char buf[256];

	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	char *str = malloc(n + 1);
	if (!str) {
		return NULL;
	}

	if (n < sizeof(buf)) {
		memcpy(str, buf, n + 1);
		return str;
	}

	va_start(args, fmt);
	vsnprintf(str, n + 1, fmt, args);
	va_end(args);

	return str;
}

static struct random_state g_random_state;

static size_t random_size_t(void)
{
	return (size_t)random_next_u64(&g_random_state);
}

static inline uint32_t integer_hash(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352d;
	x ^= x >> 15;
	x *= 0x846ca68b;
	x ^= x >> 16;
	return x;
}

static inline uint32_t bad_integer_hash(uint32_t x)
{
	return x;
}

// https://github.com/PeterScott/murmur3
static uint32_t string_hash(const void *string)
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

uint32_t bad_string_hash(const void *string)
{
	const unsigned char *s = string;
	uint32_t h = 0, high;
	while (*s) {
		h = (h << 4) + *s++;
		high = h & 0xF0000000;
		if (high) {
			h ^= high >> 24;
		}
		h &= ~high;
	}
	return h;
}

struct short_string {
	char s[128];
};

static unsigned long short_string_hash(const struct short_string s)
{
	return string_hash(s.s);
}

static unsigned long bad_short_string_hash(const struct short_string s)
{
	return bad_string_hash(s.s);
}

// static unsigned long long tp_to_ns(struct timespec *tp)
// {
// 	return tp->tv_nsec + 1000000000 * tp->tv_sec;
// }

static unsigned long long ns_elapsed(struct timespec *start, struct timespec *end)
{
	unsigned long long s = end->tv_sec - start->tv_sec;
	unsigned long long ns = end->tv_nsec - start->tv_nsec;
	return ns + 1000000000 * s;
}

static double get_avg_rate(unsigned long long nanoseconds[N], unsigned int num_entries)
{
	double avg = 0.0;
	for (unsigned int i = 0; i < N; i++) {
		avg += nanoseconds[i];
	}
	avg /= N;
	return num_entries / avg;
}

DEFINE_HASHTABLE(itable, int, int, 8, (*key == *entry));
DEFINE_HASHTABLE(stable, char *, char *, 8, (strcmp(*key, *entry) == 0));
DEFINE_HASHTABLE(sstable, char *, struct short_string, 8, (strcmp(*key, entry->s) == 0));
DEFINE_HASHTABLE(ssstable, struct short_string, struct short_string, 8, (strcmp(entry->s, key->s) == 0));

#define BENCHMARK(name, hash, key_type, entry_type, keys1, values1, keys2, values2, keys3, values3, keys4, values4, ...) \
	for (unsigned int n = 0; n < N; n++) {				\
		fprintf(stderr, "%u/%u\r", n, N);			\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_entries; i++) {			\
			entry_type *entry = name##_insert(&name, keys1[i], (hash)(keys1[i])); \
			*entry = values1[i];				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		insert[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_entries; i++) {			\
			entry_type *entry = name##_lookup(&name, keys2[i], (hash)(keys2[i])); \
			key_type *k = &keys2[i];			\
			entry_type *v = entry;				\
			assert(__VA_ARGS__);				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup1[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_entries; i++) {			\
			entry_type *entry = name##_lookup(&name, keys3[i], (hash)(keys3[i])); \
			assert(!entry);					\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (i = 0; i < num_entries; i++) {			\
			entry_type entry;				\
			bool found = name##_remove(&name, keys4[i], (hash)(keys4[i]), &entry); \
			key_type *k = &keys4[i];				\
			entry_type *v = &entry;				\
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
		for (i = 0; i < num_entries; i++) {			\
			{						\
				entry_type *entry = name##_lookup(&name, keys2[i], (hash)(keys2[i])); \
				assert(!entry);				\
			}						\
			{						\
				entry_type *entry = name##_insert(&name, keys2[i], (hash)(keys2[i])); \
				*entry = values2[i];			\
			}						\
			{						\
				entry_type entry;				\
				bool found = name##_remove(&name, keys1[i], (hash)(keys1[i]), &entry); \
				key_type *k = &keys1[i];		\
				entry_type *v = &entry;			\
				assert(!found || (__VA_ARGS__));	\
			}						\
			{						\
				entry_type *entry = name##_lookup(&name, keys4[i], (hash)(keys4[i])); \
				key_type *k = &keys4[i];		\
				entry_type *v = entry;			\
				assert(!entry || (__VA_ARGS__));		\
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
		for (i = 0; i < num_entries; i++) {			\
			{						\
				entry_type *entry = name##_insert(&name, keys1[i], (hash)(keys1[i])); \
				*entry = values1[i];			\
			}						\
			for (unsigned int j = i - 10; j < i; j++) {	\
				entry_type *entry = name##_lookup(&name, keys1[j], (hash)(keys1[j])); \
				key_type *k = &keys1[j];		\
				entry_type *v = entry;			\
				assert(__VA_ARGS__);			\
			}						\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		mixed2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
	}								\
									\
	printf("%10.2fM insertions per second\n", 1000.0 * get_avg_rate(insert, num_entries)); \
	printf("%10.2fM lookups per second (found)\n", 1000.0 * get_avg_rate(lookup1, num_entries)); \
	printf("%10.2fM lookups per second (not found)\n", 1000.0 * get_avg_rate(lookup2, num_entries)); \
	printf("%10.2fM deletions per second\n", 1000.0 * get_avg_rate(delete, num_entries)); \
	printf("%10.2fM mixed ops (with delete) per second\n", 1000.0 * get_avg_rate(mixed, num_entries)); \
	printf("%10.2fM mixed ops (no delete) per second\n", 1000.0 * get_avg_rate(mixed2, num_entries));	\


int main(int argc, char **argv)
{
	struct itable itable;
	struct stable stable;
	struct sstable sstable;
	struct ssstable ssstable;
	unsigned int i, seed = 12345, num_entries;
	struct timespec start_tp, end_tp;
	bool bad_hash = false;
	bool shuffle = true; // TODO do partial sorts/shuffles

	unsigned long long insert[N];
	unsigned long long lookup1[N];
	unsigned long long lookup2[N];
	unsigned long long delete[N];
	unsigned long long mixed[N];
	unsigned long long mixed2[N];

	random_state_init(&g_random_state, seed);

	if (1) {
		puts("itable");
		puts("--------------------------------------------------------");

		num_entries = 500000;

		int *arr1 = NULL;
		array_reserve(arr1, num_entries);
		for (i = 0; i < num_entries; i++) {
			array_add(arr1, i);
		}
		if (shuffle) {
			array_shuffle(arr1, random_size_t);
		}

		int *arr2 = array_copy(arr1);
		array_shuffle(arr2, random_size_t);

		int *arr3 = NULL;
		array_reserve(arr3, num_entries);
		for (i = 0; i < num_entries; i++) {
			array_add(arr3, i + num_entries);
		}
		array_shuffle(arr3, random_size_t);

		int *arr4 = array_copy(arr1);
		array_shuffle(arr4, random_size_t);

		BENCHMARK(itable, bad_hash ? bad_integer_hash : integer_hash, int, int, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, *k == *v);

		array_free(arr1);
		array_free(arr2);
		array_free(arr3);
		array_free(arr4);

		puts("--------------------------------------------------------");
	}

	random_state_init(&g_random_state, seed);

	if (1) {
		puts("stable");
		puts("--------------------------------------------------------");

		num_entries = 200000;

		char **arr1 = NULL;
		array_reserve(arr1, num_entries);
		for (i = 0; i < num_entries; i++) {
			array_add(arr1, mprintf("%i", i));
		}
		if (shuffle) {
			array_shuffle(arr1, random_size_t);
		}

		char **arr2 = array_copy(arr1);
		array_shuffle(arr2, random_size_t);

		char **arr3 = NULL;
		array_reserve(arr3, num_entries);
		for (i = 0; i < num_entries; i++) {
			array_add(arr3, mprintf("%i", i + num_entries));
		}
		array_shuffle(arr3, random_size_t);

		char **arr4 = array_copy(arr1);
		array_shuffle(arr4, random_size_t);

		BENCHMARK(stable, bad_hash ? bad_string_hash : string_hash, char *, char *, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, strcmp(*k, *v) == 0);

		array_foreach_value(arr1, iter) {
			free(iter);
		}
		array_foreach_value(arr3, iter) {
			free(iter);
		}
		array_free(arr1);
		array_free(arr2);
		array_free(arr3);
		array_free(arr4);

		puts("--------------------------------------------------------");
	}

	random_state_init(&g_random_state, seed);

	if (1) {
		puts("sstable");
		puts("--------------------------------------------------------");

		num_entries = 200000;

		struct short_string *values1 = NULL;
		array_reserve(values1, num_entries);
		for (i = 0; i < num_entries; i++) {
			struct short_string *entry = array_addn(values1, 1);
			sprintf(entry->s, "%i", i);
		}
		if (shuffle) {
			array_shuffle(values1, random_size_t);
		}

		struct short_string *values2 = array_copy(values1);
		array_shuffle(values2, random_size_t);

		struct short_string *values3 = NULL;
		array_reserve(values3, num_entries);
		for (i = 0; i < num_entries; i++) {
			struct short_string *entry = array_addn(values3, 1);
			sprintf(entry->s, "%i", i + num_entries);
		}
		array_shuffle(values3, random_size_t);

		struct short_string *values4 = array_copy(values1);
		array_shuffle(values4, random_size_t);

		char **keys1 = NULL;
		array_reserve(keys1, num_entries);
		array_foreach(values1, iter) {
			array_add(keys1, strdup(iter->s));
		}
		char **keys2 = NULL;
		array_reserve(keys2, num_entries);
		array_foreach(values2, iter) {
			array_add(keys2, strdup(iter->s));
		}
		char **keys3 = NULL;
		array_reserve(keys3, num_entries);
		array_foreach(values3, iter) {
			array_add(keys3, strdup(iter->s));
		}
		char **keys4 = NULL;
		array_reserve(keys4, num_entries);
		array_foreach(values4, iter) {
			array_add(keys4, strdup(iter->s));
		}

		BENCHMARK(sstable, bad_hash ? bad_string_hash : string_hash, char *, struct short_string, keys1, values1, keys2, values2, keys3, values3, keys4, values4, strcmp(*k, v->s) == 0);

		array_foreach_value(keys1, s) {
			free(s);
		}
		array_foreach_value(keys2, s) {
			free(s);
		}
		array_foreach_value(keys3, s) {
			free(s);
		}
		array_foreach_value(keys4, s) {
			free(s);
		}
		array_free(keys1);
		array_free(keys2);
		array_free(keys3);
		array_free(keys4);
		array_free(values1);
		array_free(values2);
		array_free(values3);
		array_free(values4);

		puts("--------------------------------------------------------");
	}

	random_state_init(&g_random_state, seed);

	if (1) {
		puts("ssstable");
		puts("--------------------------------------------------------");

		num_entries = 100000;

		struct short_string *arr1 = NULL;
		array_reserve(arr1, num_entries);
		for (i = 0; i < num_entries; i++) {
			struct short_string *entry = array_addn(arr1, 1);
			sprintf(entry->s, "%i", i);
		}
		if (shuffle) {
			array_shuffle(arr1, random_size_t);
		}

		struct short_string *arr2 = array_copy(arr1);
		array_shuffle(arr2, random_size_t);

		struct short_string *arr3 = NULL;
		array_reserve(arr3, num_entries);
		for (i = 0; i < num_entries; i++) {
			struct short_string *entry = array_addn(arr3, 1);
			sprintf(entry->s, "%i", i + num_entries);
		}
		array_shuffle(arr3, random_size_t);

		struct short_string *arr4 = array_copy(arr1);
		array_shuffle(arr4, random_size_t);

		BENCHMARK(ssstable, bad_hash ? bad_short_string_hash : short_string_hash, struct short_string, struct short_string, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, strcmp(k->s, v->s) == 0);

		array_free(arr1);
		array_free(arr2);
		array_free(arr3);
		array_free(arr4);

		puts("--------------------------------------------------------");
	}
}
