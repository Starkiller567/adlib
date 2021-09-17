#ifndef __HASH_INCLUDE__
#define __HASH_INCLUDE__

#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "macros.h"

typedef union {
	uint8_t bytes[4];
	uint32_t u32;
} hash32_t;

typedef union {
	uint8_t bytes[8];
	uint64_t u64;
} hash64_t;

typedef union {
	_Alignas(uint64_t) uint8_t bytes[16];
} hash128_t;

// SipHash-2-4
// (the key k must have 16 bytes)
// (see also https://github.com/veorq/SipHash)
__AD_LINKAGE hash64_t siphash24_64(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;
__AD_LINKAGE hash128_t siphash24_128(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;

// SipHash-1-3
// (the key k must have 16 bytes)
// (see also https://github.com/veorq/SipHash)
__AD_LINKAGE hash64_t siphash13_64(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;
__AD_LINKAGE hash128_t siphash13_128(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;

// HalfSipHash-2-4
// (the key must have 8 bytes)
// (see also https://github.com/veorq/SipHash)
__AD_LINKAGE hash32_t halfsiphash24_32(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;
__AD_LINKAGE hash64_t halfsiphash24_64(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;

// HalfSipHash-1-3
// (the key must have 8 bytes)
// (see also https://github.com/veorq/SipHash)
__AD_LINKAGE hash32_t halfsiphash13_32(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;
__AD_LINKAGE hash64_t halfsiphash13_64(const void *in, size_t inlen, const void *key) _attr_unused _attr_pure;


// MurmurHash3_x86_32
// (use this on 32-bit architectures for short inputs)
// (see also https://github.com/PeterScott/murmur3)
__AD_LINKAGE hash32_t murmurhash3_x86_32(const void *in, size_t inlen, uint32_t seed) _attr_unused _attr_pure;

// MurmurHash3_x86_64/MurmurHash3_x86_128
// (use this on 32-bit architectures for long inputs or when you need more than 32 bits of output)
// (see also https://github.com/PeterScott/murmur3)
__AD_LINKAGE hash64_t murmurhash3_x86_64(const void *in, size_t inlen, uint32_t seed) _attr_unused _attr_pure;
__AD_LINKAGE hash128_t murmurhash3_x86_128(const void *in, size_t inlen, uint32_t seed) _attr_unused _attr_pure;

// MurmurHash3_x64_64/MurmurHash3_x64_128
// (use this on 64-bit architectures)
// (see also https://github.com/PeterScott/murmur3)
__AD_LINKAGE hash64_t murmurhash3_x64_64(const void *in, size_t inlen, uint32_t seed) _attr_unused _attr_pure;
__AD_LINKAGE hash128_t murmurhash3_x64_128(const void *in, size_t inlen, uint32_t seed) _attr_unused _attr_pure;

#endif