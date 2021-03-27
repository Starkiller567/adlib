#ifndef __RANDOM_INCLUDE__
#define __RANDOM_INCLUDE__

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

struct random_state {
	uint64_t s[4];
};

// http://prng.di.unimi.it/splitmix64.c
/* Written in 2015 by Sebastiano Vigna (vigna@acm.org)

   This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state. */

static void random_state_init(struct random_state *state, uint64_t seed)
{
	uint64_t x = seed;
	uint64_t z;
#define next()							\
	do {							\
		z = (x += 0x9e3779b97f4a7c15);			\
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;	\
		z = (z ^ (z >> 27)) * 0x94d049bb133111eb;	\
		z =  z ^ (z >> 31);				\
	} while (0);
	next();
	state->s[0] = z;
	next();
	state->s[1] = z;
	next();
	state->s[2] = z;
	next();
	state->s[3] = z;
#undef next
}

// http://prng.di.unimi.it/xoshiro256starstar.c
/* Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

   This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static uint64_t random_next_u64(struct random_state *state)
{
#define rotl(x, k) ((uint64_t)(x) << (k)) | ((uint64_t)(x) >> (64 - (k)))
	const uint64_t result = rotl(state->s[1] * 5, 7) * 9;

	const uint64_t t = state->s[1] << 17;

	state->s[2] ^= state->s[0];
	state->s[3] ^= state->s[1];
	state->s[1] ^= state->s[2];
	state->s[0] ^= state->s[3];

	state->s[2] ^= t;

	state->s[3] = rotl(state->s[3], 45);

	return result;
#undef rotl
}

static uint32_t random_next_u32(struct random_state *state)
{
	return (uint32_t)random_next_u64(state);
}

static double random_next_uniform_double(struct random_state *state)
{
	return (random_next_u64(state) >> 11) * 0x1.0p-53;
}

static float random_next_uniform_float(struct random_state *state)
{
	// TODO should be able to do a similar thing as above for double
	return (float)random_next_uniform_double(state);
}

static bool random_next_bool(struct random_state *state)
{
	return random_next_u64(state) & 1;
}

static uint32_t random_next_u32_in_range(struct random_state *state, uint32_t min, uint32_t max)
{
	// TODO look into https://arxiv.org/pdf/1805.10941.pdf
	assert(min <= max);
	uint32_t n = max - min + 1;
	uint32_t remainder = n == 0 ? 0 : UINT32_MAX % n;
	uint32_t x;
	do {
		x = random_next_u32(state);
	} while (x >= UINT32_MAX - remainder);
	return min + x % n;
}

static uint64_t random_next_u64_in_range(struct random_state *state, uint64_t min, uint64_t max)
{
	// TODO look into https://arxiv.org/pdf/1805.10941.pdf
	assert(min <= max);
	uint64_t n = max - min + 1;
	uint64_t remainder = n == 0 ? 0 : UINT64_MAX % n;
	uint64_t x;
	do {
		x = random_next_u64(state);
	} while (x >= UINT64_MAX - remainder);
	return min + x % n;
}

static float random_next_float_in_range(struct random_state *state, float min, float max)
{
	assert(min <= max);
	return min + random_next_uniform_float(state) * (max - min);
}

static double random_next_double_in_range(struct random_state *state, double min, double max)
{
	assert(min <= max);
	return min + random_next_uniform_double(state) * (max - min);
}


/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations. */

static void random_jump(struct random_state *state)
{
	static const uint64_t JUMP[] = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for (int i = 0; i < sizeof(JUMP) / sizeof(*JUMP); i++)
		for (int b = 0; b < 64; b++) {
			if (JUMP[i] & UINT64_C(1) << b) {
				s0 ^= state->s[0];
				s1 ^= state->s[1];
				s2 ^= state->s[2];
				s3 ^= state->s[3];
			}
			random_next_u64(state);
		}

	state->s[0] = s0;
	state->s[1] = s1;
	state->s[2] = s2;
	state->s[3] = s3;
}



/* This is the long-jump function for the generator. It is equivalent to
   2^192 calls to next(); it can be used to generate 2^64 starting points,
   from each of which jump() will generate 2^64 non-overlapping
   subsequences for parallel distributed computations. */

static void random_long_jump(struct random_state *state)
{
	static const uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241, 0x39109bb02acbe635 };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for (int i = 0; i < sizeof(LONG_JUMP) / sizeof(*LONG_JUMP); i++)
		for (int b = 0; b < 64; b++) {
			if (LONG_JUMP[i] & UINT64_C(1) << b) {
				s0 ^= state->s[0];
				s1 ^= state->s[1];
				s2 ^= state->s[2];
				s3 ^= state->s[3];
			}
			random_next_u64(state);
		}

	state->s[0] = s0;
	state->s[1] = s1;
	state->s[2] = s2;
	state->s[3] = s3;
}

#endif
