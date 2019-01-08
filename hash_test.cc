#include <time.h>
#include <cassert>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>
#include "flat_hash_map.hpp"
#include "unordered_map.hpp"
#include "bytell_hash_map.hpp"

struct short_string {
	char s[128];
};

static unsigned long string_hash(const void *string)
{
	const void *data = string;
	const size_t nbytes = strlen((const char *)string);
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

struct MyHash1
{
    std::size_t operator()(char * const& s) const noexcept
    {
	    return string_hash(s);
    }
};

struct MyEqualTo1
{
	bool operator()(char * const &lhs, char * const &rhs) const
		{
			return strcmp(lhs, rhs) == 0;
		}
};

struct MyHash2
{
    std::size_t operator()(struct short_string const& s) const noexcept
    {
	    return string_hash(s.s);
    }
};

struct MyEqualTo2
{
	bool operator()(struct short_string const &lhs, struct short_string const &rhs) const
		{
			return strcmp(lhs.s, rhs.s) == 0;
		}
};

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

#define map_impl std::unordered_map

int main(int argc, char **argv)
{
	unsigned int i, num_items, x = 0;
	struct timespec start_tp, end_tp;
	unsigned long long start_ns, ns, total_ns, sum_ns = 0;
	bool verbose = argc >= 2 && strcmp(argv[1], "verbose") == 0;

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
	start_ns = tp_to_ns(&start_tp);
#if 1
	{
			map_impl<unsigned int, unsigned int> itable(128);
			num_items = 1000000;
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (i = 0; i < num_items; i++) {
				itable[i] = i;
			}

			for (i = 0; i < num_items; i++) {
				unsigned int &item = itable[i];
				assert(item == i);
				itable.erase(i);
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			ns = ns_elapsed(&start_tp, &end_tp);
			if (verbose) printf("[%u]: %llu\n", x++, ns);
			sum_ns += ns;
	}
#endif

#if 1
	{
			map_impl<char *, char *, MyHash1, MyEqualTo1> stable(128);
			num_items = 1000000;
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (i = 0; i < num_items; i++) {
				char *key = (char *)malloc(16);
				char *data = (char *)malloc(16);
				sprintf(data, "%u", i);
				sprintf(key, "%u", i);
				stable[key] = data;
			}

			for (i = 0; i < num_items; i++) {
				char **s;
				char key[16];
				char data[16];
				sprintf(key, "%u", i);
				sprintf(data, "%u", i);
				char *&item = stable[key];
				assert(strcmp(item, data) == 0);
				stable.erase(key);
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			ns = ns_elapsed(&start_tp, &end_tp);
			if (verbose) printf("[%u]: %llu\n", x++, ns);
			sum_ns += ns;
	}
#endif

#if 1
	{
		map_impl<char *, struct short_string, MyHash1, MyEqualTo1> sstable(128);
		num_items = 1000000;
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (i = 0; i < num_items; i++) {
			char *key = (char *)malloc(16);
			sprintf(key, "%u", i);
			struct short_string &item = sstable[key];
			sprintf(item.s, "%u", i);
		}

		for (i = 0; i < num_items; i++) {
			char key[16];
			struct short_string data;
			sprintf(key, "%u", i);
			sprintf(data.s, "%u", i);
			struct short_string &item = sstable[key];
			assert(strcmp(item.s, data.s) == 0);
			sstable.erase(key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		ns = ns_elapsed(&start_tp, &end_tp);
		if (verbose) printf("[%u]: %llu\n", x++, ns);
		sum_ns += ns;
	}
#endif

#if 1
	{
		map_impl<struct short_string, struct short_string, MyHash2, MyEqualTo2> ssstable(128);
		num_items = 1000000;
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (i = 0; i < num_items; i++) {
			struct short_string key;
			sprintf(key.s, "%u", i);
			struct short_string &item = ssstable[key];
			sprintf(item.s, "%u", i);
		}

		for (i = 0; i < num_items; i++) {
			struct short_string key;
			struct short_string data;
			sprintf(key.s, "%u", i);
			sprintf(data.s, "%u", i);
			struct short_string &item = ssstable[key];
			assert(strcmp(item.s, data.s) == 0);
			ssstable.erase(key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		ns = ns_elapsed(&start_tp, &end_tp);
		if (verbose) printf("[%u]: %llu\n", x++, ns);
		sum_ns += ns;
	}
#endif

#if 1
	{
		map_impl<unsigned int, unsigned int> itable(128);
		num_items = 1000000;
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (i = 0; i < num_items; i++) {
			itable[i] = i;
		}
		for (i = 0; i < num_items; i++) {
			i = itable[i];
		}
		for (i = 0; i < num_items; i++) {
			itable.erase(i);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		ns = ns_elapsed(&start_tp, &end_tp);
		if (verbose) printf("[%u]: %llu\n", x++, ns);
		sum_ns += ns;
	}
#endif

#if 1
	{
			map_impl<unsigned int, unsigned int> itable(128);
			num_items = 1000000;
			srand(1234);
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (i = 0; i < num_items; i++) {
				unsigned int key = rand();
				itable[key] = i;
				for (unsigned int j = 0; j < 100; j++) {
					itable[key] = j;
				}
				itable.erase(key);
				itable[key] = key;
				itable[key] = i;
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			ns = ns_elapsed(&start_tp, &end_tp);
			if (verbose) printf("[%u]: %llu\n", x++, ns);
			sum_ns += ns;
	}
#endif

#if 1
	{
		map_impl<char *, struct short_string, MyHash1, MyEqualTo1> sstable(128);
		num_items = 100000;
		srand(1234);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (i = 0; i < num_items; i++) {
			unsigned int r = rand();
			char key[16];
			sprintf(key, "%u", r);
			struct short_string &item = sstable[key];
			sprintf(item.s, "%u", i);
			for (unsigned int j = 0; j < 100; j++) {
				item = sstable[key];
				sprintf(item.s, "%u", j);
			}
			sstable.erase(key);
			item = sstable[key];
			sprintf(item.s, "%u", r);
			item = sstable[key];
			sprintf(item.s, "%u", i);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		ns = ns_elapsed(&start_tp, &end_tp);
		if (verbose) printf("[%u]: %llu\n", x++, ns);
		sum_ns += ns;
	}
#endif

#if 1
	{
		map_impl<char *, struct short_string, MyHash1, MyEqualTo1> sstable(128);
		num_items = 1000000;
		srand(1234);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (i = 0; i < num_items; i++) {
			unsigned int op = rand() % 100;
			unsigned int r = rand() % num_items;
			char key[16];
			sprintf(key, "%u", r);
			if (op < 75) {
				struct short_string &item = sstable[key];
				sprintf(item.s, "%u", i);
			} else {
				sstable.erase(key);
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		ns = ns_elapsed(&start_tp, &end_tp);
		if (verbose) printf("[%u]: %llu\n", x++, ns);
		sum_ns += ns;
	}
#endif

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	total_ns = tp_to_ns(&end_tp) - start_ns;
	printf("sum:   %llu\n", sum_ns);
	printf("total: %llu\n", total_ns);
}
