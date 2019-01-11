#ifndef __bitmap_include__
#define __bitmap_include__

#define BITS_PER_LONG (sizeof(long) * 8)

#define DECLARE_BITMAP(name, nbits) \
	unsigned long name[((nbits) + BITS_PER_LONG - 1) / BITS_PER_LONG]
#define DECLARE_EMPTY_BITMAP(name, nbits) DECLARE_BITMAP(name, nbits) = {0}

#define bm_foreach_zero(bitmap, nbits) for (size_t cur = bm_find_first_zero(bitmap, nbits); cur < nbits; cur = bm_find_next_zero(bitmap, cur + 1, nbits))
#define bm_foreach_set(bitmap, nbits) for (size_t cur = bm_find_first_set(bitmap, nbits); cur < nbits; cur = bm_find_next_set(bitmap, cur + 1, nbits))

static inline size_t __ffs(unsigned long word)
{
	if (word == 0) {
		return BITS_PER_LONG;
	}
#if 1
	return __builtin_ctzl(word);
#else
	for (size_t i = 0; i < BITS_PER_LONG; i++) {
		if (word & (1UL << i)) {
			return i;
		}
	}
	return BITS_PER_LONG;
#endif
}

static size_t bm_find_next(const void *bitmap, size_t start, size_t nbits, unsigned long xor_mask)
{
	if (start >= nbits) {
		return nbits;
	}
	const unsigned long *bm = bitmap;
	unsigned long word = bm[start / BITS_PER_LONG];
	word ^= xor_mask;
	word &= ~0UL << (start & (BITS_PER_LONG - 1));
	start &= ~(BITS_PER_LONG - 1);
	while (word == 0) {
		start += BITS_PER_LONG;
		if (start >= nbits) {
			return nbits;
		}
		word = bm[start / BITS_PER_LONG];
		word ^= xor_mask;
	}
	start += __ffs(word);
	return start >= nbits ? nbits : start;
}

static inline size_t bm_find_next_zero(const void *bitmap, size_t start, size_t nbits)
{
	return bm_find_next(bitmap, start, nbits, ~0UL);
}

static inline size_t bm_find_first_zero(const void *bitmap, size_t nbits)
{
	return bm_find_next_zero(bitmap, 0, nbits);
}

static inline size_t bm_find_next_set(const void *bitmap, size_t start, size_t nbits)
{
	return bm_find_next(bitmap, start, nbits, 0);
}

static inline size_t bm_find_first_set(const void *bitmap, size_t nbits)
{
	return bm_find_next_set(bitmap, 0, nbits);
}

static inline void bm_set_bit(void *bitmap, size_t bit)
{
	unsigned long *bm = bitmap;
	bm[bit / BITS_PER_LONG] |= 1UL << (bit & (BITS_PER_LONG - 1));
}

static inline void bm_clear_bit(void *bitmap, size_t bit)
{
	unsigned long *bm = bitmap;
	bm[bit / BITS_PER_LONG] &= ~(1UL << (bit & (BITS_PER_LONG - 1)));
}

static inline int bm_test_bit(const void *bitmap, size_t bit)
{
	const unsigned long *bm = bitmap;
	return (bm[bit / BITS_PER_LONG] >> (bit & (BITS_PER_LONG - 1))) & 1;
}

#endif
