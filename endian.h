#ifndef __endian_include__
#define __endian_include__

#include <stdint.h>

#define is_little_endian() ((const union {uint16_t i; uint8_t c;}){1}.c == 1)
#define is_big_endian()    ((const union {uint16_t i; uint8_t c;}){1}.c == 0)

#define bswap64(x) ((((uint64_t)x & 0xff00000000000000) >> 56) |	\
                    (((uint64_t)x & 0x00ff000000000000) >> 40) |	\
                    (((uint64_t)x & 0x0000ff0000000000) >> 24) |	\
                    (((uint64_t)x & 0x000000ff00000000) >>  8) |	\
                    (((uint64_t)x & 0x00000000ff000000) <<  8) |	\
                    (((uint64_t)x & 0x0000000000ff0000) << 24) |	\
                    (((uint64_t)x & 0x000000000000ff00) << 40) |	\
                    (((uint64_t)x & 0x00000000000000ff) << 56))

#define bswap32(x) ((((uint32_t)x & 0xff000000) >> 24) |	\
                    (((uint32_t)x & 0x00ff0000) >>  8) |	\
                    (((uint32_t)x & 0x0000ff00) <<  8) |	\
                    (((uint32_t)x & 0x000000ff) << 24))

#define bswap16(x) ((((uint16_t)x & 0xff00) >> 8) |	\
                    (((uint16_t)x & 0x00ff) << 8))

typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

static inline __be16 cpu_to_be16(uint16_t u)
{
	return is_big_endian() ? u : bswap16(u);
}

static inline __be32 cpu_to_be32(uint32_t u)
{
	return is_big_endian() ? u : bswap32(u);
}

static inline __be64 cpu_to_be64(uint64_t u)
{
	return is_big_endian() ? u : bswap64(u);
}

static inline uint16_t be16_to_cpu(__be16 b)
{
	return is_big_endian() ? b : bswap16(b);
}

static inline uint32_t be32_to_cpu(__be32 b)
{
	return is_big_endian() ? b : bswap32(b);
}

static inline uint64_t be64_to_cpu(__be64 b)
{
	return is_big_endian() ? b : bswap64(b);
}

static inline __le16 cpu_to_le16(uint16_t u)
{
	return is_little_endian() ? u : bswap16(u);
}

static inline __le32 cpu_to_le32(uint32_t u)
{
	return is_little_endian() ? u : bswap32(u);
}

static inline __le64 cpu_to_le64(uint64_t u)
{
	return is_little_endian() ? u : bswap64(u);
}

static inline uint16_t le16_to_cpu(__le16 l)
{
	return is_little_endian() ? l : bswap16(l);
}

static inline uint32_t le32_to_cpu(__le32 l)
{
	return is_little_endian() ? l : bswap32(l);
}

static inline uint64_t le64_to_cpu(__le64 l)
{
	return is_little_endian() ? l : bswap64(l);
}

#endif
