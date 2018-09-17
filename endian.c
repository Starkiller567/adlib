#include <stdio.h>
#include "endian.h"

int main(void)
{
	volatile uint64_t x = 0xdeadbeefcafebabe;

	uint16_t x16 = (uint16_t)x;
	uint32_t x32 = (uint32_t)x;
	uint64_t x64 = (uint64_t)x;
	__be16 be16 = cpu_to_be16(x16);
	__be32 be32 = cpu_to_be32(x32);
	__be64 be64 = cpu_to_be64(x64);
	__le16 le16 = cpu_to_le16(x16);
	__le32 le32 = cpu_to_le32(x32);
	__le64 le64 = cpu_to_le64(x64);


	printf("le: %i, be: %i\n", is_little_endian(), is_big_endian());
	puts("");
	printf("%x\n", x16);
	printf("%x\n", be16);
	printf("%x\n", le16);
	printf("%x\n", be16_to_cpu(be16));
	printf("%x\n", le16_to_cpu(le16));
	puts("");
	printf("%x\n", x32);
	printf("%x\n", be32);
	printf("%x\n", le32);
	printf("%x\n", be32_to_cpu(be32));
	printf("%x\n", le32_to_cpu(le32));
	puts("");
	printf("%lx\n", x64);
	printf("%lx\n", be64);
	printf("%lx\n", le64);
	printf("%lx\n", be64_to_cpu(be64));
	printf("%lx\n", le64_to_cpu(le64));
}
