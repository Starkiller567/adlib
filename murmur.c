#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <x86intrin.h>

static unsigned long murmur3(const void *data, const size_t nbytes)
{
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

static unsigned long fast_murmur3(const void *data, const size_t nbytes)
{
	if (data == NULL || nbytes == 0) {
		return 0;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const int nchunks = nblocks / 4;
	const uint32_t *blocks = (const uint32_t *)data;
	__m128i *chunks = (__m128i *)data;
	const uint8_t *tail = (const uint8_t *)data + (nblocks * 4);

	uint32_t h = 0;
	uint32_t k;
	int i;

	if (nchunks != 0) {
		__m128i c1_4x = _mm_set1_epi32(c1);
		__m128i c2_4x = _mm_set1_epi32(c2);

		for (i = 0; i < nchunks; i++) {
			__m128i k_4x = _mm_loadu_si128(&chunks[i]);

			k_4x = _mm_mullo_epi32(k_4x, c1_4x);

			k_4x = _mm_or_si128(_mm_slli_epi32(k_4x, 15), _mm_srli_epi32(k_4x, 32 - 15));
			k_4x = _mm_mullo_epi32(k_4x, c2_4x);

			k = _mm_cvtsi128_si32(k_4x);
			h ^= k;
			h = (h << 13) | (h >> (32 - 13));
			h = (h * 5) + 0xe6546b64;

			k = _mm_extract_epi32(k_4x, 1);
			h ^= k;
			h = (h << 13) | (h >> (32 - 13));
			h = (h * 5) + 0xe6546b64;

			k = _mm_extract_epi32(k_4x, 2);
			h ^= k;
			h = (h << 13) | (h >> (32 - 13));
			h = (h * 5) + 0xe6546b64;

			k = _mm_extract_epi32(k_4x, 3);
			h ^= k;
			h = (h << 13) | (h >> (32 - 13));
			h = (h * 5) + 0xe6546b64;
		}
	}

	for (i = nchunks * 4; i < nblocks; i++) {
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

int main(int argc, char **argv)
{
	srand(time(NULL));
	unsigned long long total1 = 0;
	unsigned long long total2 = 0;
	for (unsigned long n = 0; n < 100000; n++) {
		char buf[4096];
		size_t len = rand() % sizeof(buf);
		for (size_t i = 0; i < len; i++) {
			buf[i] = rand();
		}
		unsigned long long start = __rdtsc();
		unsigned long hash1 = murmur3(buf, len);
		unsigned long long time1 = __rdtsc() - start;

		start = __rdtsc();
		unsigned long hash2 = fast_murmur3(buf, len);
		unsigned long long time2 = __rdtsc() - start;

		assert(hash1 == hash2);

		total1 += time1;
		total2 += time2;
	}
	/* the "fast" murmur function is only fast when compiling with clang */
	printf("total1: %llu\n", total1);
	printf("total2: %llu\n", total2);
	return 0;
}
