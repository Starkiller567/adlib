#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
// #include "hash_table.h"
#include "robin_hood.h"
// #include "flat_hash.h"

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

DEFINE_HASHTABLE(itable, unsigned int, unsigned int, 8, (*a == *b))
DEFINE_HASHTABLE(stable, const char *, char *, 8, (strcmp(*a, *b) == 0))
DEFINE_HASHTABLE(sstable, const char *, struct short_string, 8, (strcmp(*a, *b) == 0))
DEFINE_HASHTABLE(ssstable, struct short_string, struct short_string, 8, (strcmp(a->s, b->s) == 0))

int main(int argc, char **argv)
{
	struct itable itable;
	struct stable stable;
	struct sstable sstable;
	struct ssstable ssstable;
	unsigned int i, x = 0, seed = 12345, num_items = 1000000;
	struct timespec start_tp, end_tp;
	unsigned long long start_ns, ns, total_ns, sum_ns = 0;
	bool verbose = argc >= 2 && strcmp(argv[1], "verbose") == 0;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	start_ns = tp_to_ns(&start_tp);
#if 1
	itable_init(&itable, 128);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_insert(&itable, i, integer_hash(i));
		*item = i;
	}

	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_lookup(&itable, i, integer_hash(i));
		assert(*item == i);
		itable_remove(&itable, i, integer_hash(i), NULL, NULL);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	itable_destroy(&itable);
#endif

#if 1
	stable_init(&stable, 128);


	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		char *key = malloc(16);
		char *data = malloc(16);
		sprintf(data, "%u", i);
		sprintf(key, "%u", i);
		char **item = stable_insert(&stable, key, string_hash(key));
		*item = data;
	}

	for (i = 0; i < num_items; i++) {
		char **s;
		char key[16];
		char data[16];
		sprintf(key, "%u", i);
		sprintf(data, "%u", i);
		unsigned int hash = string_hash(key);
		s = stable_lookup(&stable, key, hash);
		assert(strcmp(*s, data) == 0);
		stable_remove(&stable, key, hash, NULL, NULL);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	stable_destroy(&stable);
#endif

#if 1
	sstable_init(&sstable, 128);


	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		char *key = malloc(16);
		sprintf(key, "%u", i);
		struct short_string *item = sstable_insert(&sstable, key, string_hash(key));
		sprintf(item->s, "%u", i);
	}

	for (i = 0; i < num_items; i++) {
		char key[16];
		struct short_string data;
		sprintf(key, "%u", i);
		sprintf(data.s, "%u", i);
		unsigned int hash = string_hash(key);
		struct short_string *item = sstable_lookup(&sstable, key, hash);
		assert(strcmp(item->s, data.s) == 0);
		sstable_remove(&sstable, key, hash, NULL, NULL);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	sstable_destroy(&sstable);
#endif

#if 1
	ssstable_init(&ssstable, 128);


	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		struct short_string key;
		sprintf(key.s, "%u", i);
		struct short_string *item = ssstable_insert(&ssstable, key, short_string_hash(key));
		sprintf(item->s, "%u", i);
	}

	for (i = 0; i < num_items; i++) {
		struct short_string key;
		struct short_string data;
		sprintf(key.s, "%u", i);
		sprintf(data.s, "%u", i);
		unsigned int hash = short_string_hash(key);
		struct short_string *item = ssstable_lookup(&ssstable, key, hash);
		assert(strcmp(item->s, data.s) == 0);
		ssstable_remove(&ssstable, key, hash, NULL, NULL);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	ssstable_destroy(&ssstable);
#endif

#if 1
	itable_init(&itable, 128);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		unsigned int *item = itable_insert(&itable, i, integer_hash(i));
		*item = i;
	}
	for (i = 0; i < num_items; i++) {
		i = *itable_lookup(&itable, i, integer_hash(i));
	}
	for (i = 0; i < num_items; i++) {
		unsigned int key;
		unsigned int item;
		itable_remove(&itable, i, integer_hash(i), &key, &item);
		assert(key == item);
		i = key;
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	itable_destroy(&itable);
#endif

#if 1
	itable_init(&itable, 128);

	srand(seed);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		unsigned int key = rand();
		unsigned int *item = itable_insert(&itable, key, integer_hash(i));
		*item = i;
		for (unsigned int j = 0; j < 100; j++) {
			item = itable_lookup(&itable, key, integer_hash(i));
			*item = j;
		}
		itable_remove(&itable, key, integer_hash(i), NULL, NULL);
		item = itable_insert(&itable, key, integer_hash(i));
		*item = key;
		item = itable_lookup(&itable, key, integer_hash(i));
		*item = i;
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	itable_destroy(&itable);
#endif

#if 1
	sstable_init(&sstable, 128);

	srand(seed);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		unsigned int r = rand();
		char key[16];
		sprintf(key, "%u", r);
		unsigned int hash = string_hash(key);
		struct short_string *item = sstable_insert(&sstable, key, hash);
		sprintf(item->s, "%u", i);
		for (unsigned int j = 0; j < 100; j++) {
			item = sstable_lookup(&sstable, key, hash);
			sprintf(item->s, "%u", j);
		}
		sstable_remove(&sstable, key, hash, NULL, NULL);
		item = sstable_insert(&sstable, key, hash);
		sprintf(item->s, "%u", r);
		item = sstable_lookup(&sstable, key, hash);
		sprintf(item->s, "%u", i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	sstable_destroy(&sstable);
#endif

#if 1
	sstable_init(&sstable, 128);

	srand(seed);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	for (i = 0; i < num_items; i++) {
		unsigned int op = rand() % 100;
		unsigned int r = rand() % num_items;
		char *key = malloc(16);
		sprintf(key, "%u", r);
		unsigned int hash = string_hash(key);
		if (op < 75) {
			struct short_string *item = sstable_lookup(&sstable, key, hash);
			if (!item) {
				item = sstable_insert(&sstable, key, hash);
			}
			sprintf(item->s, "%u", i);
		} else {
			const char *stored_key;
			if (sstable_remove(&sstable, key, hash, &stored_key, NULL)) {
				free((void *)stored_key);
			}
			free(key);
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	ns = ns_elapsed(&start_tp, &end_tp);
	if (verbose) printf("[%u]: %llu\n", x++, ns);
	sum_ns += ns;
	sstable_destroy(&sstable);
#endif

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	total_ns = tp_to_ns(&end_tp) - start_ns;
	printf("sum:   %llu\n", sum_ns);
	printf("total: %llu\n", total_ns);
}
