/* 
 *
 * main.cpp
 * Purpose of review and rewrite is optimization. (https://github.com/BaseMax/)
 * Copyright (c) 2017, SUMOKOIN
 *
 */
#include <iostream>
#include <string.h>
#include "pow_hash/cn_slow_hash.hpp"

extern "C" void blake256_hash(uint8_t*, const uint8_t*);
extern "C" void groestl(const unsigned char*, unsigned char*);
extern "C" void jh_hash(const unsigned char*, unsigned char*);
extern "C" void skein_hash(const unsigned char*,unsigned char*);

int main(int argc, char **argv) 
{
	uint8_t hash[32];
	const uint8_t correct1[32] = { 0xeb, 0x14, 0xe8, 0xa8, 0x33, 0xfa, 0xc6, 0xfe, 0x9a, 0x43, 0xb5, 0x7b, 0x33, 0x67, 0x89, 0xc4, 
		0x6f, 0xfe, 0x93, 0xf2, 0x86, 0x84, 0x52, 0x24, 0x07, 0x20, 0x60, 0x7b, 0x14, 0x38, 0x7e, 0x11}; 
	const uint8_t correct2[32] = { 0x80, 0x47, 0x42, 0xcb, 0x8f, 0x59, 0xd8, 0x44, 0x6b, 0xc3, 0xc7, 0xc4, 0x39, 0x51, 0x4e, 0xc1,
		0xb8, 0xff, 0xce, 0x73, 0x4e, 0xc1, 0x43, 0xcc, 0x28, 0xa6, 0x83, 0x50, 0x75, 0xdc, 0x21, 0xcd };
	cn_pow_hash_v2 v2;
	v2.hash("", 0, hash);
	if(memcmp(hash, correct2, 32) == 0)
		printf("Hash B verified!\n");
	else
		printf("Hash B FAILED!\n");
	for(size_t i=0; i < 3; i++)
	{
		cn_pow_hash_v1 v1 = cn_pow_hash_v1::make_borrowed(v2);
		v1.hash("", 0, hash);
		if(memcmp(hash, correct1, 32) == 0)
			printf("Hash A verified!\n");
		else
			printf("Hash A FAILED!\n");
	}
	v2.hash("", 0, hash);
	if(memcmp(hash, correct2, 32) == 0)
		printf("Hash B verified!\n");
	else
		printf("Hash B FAILED!\n");
	uint8_t msg[200];
	for(uint8_t i=0; i < 200; i++)
		msg[i] = i;
	blake256_hash(hash, msg);
	if(memcmp(hash, "\xc4\xd9\x44\xc2\xb1\xc0\x0a\x8e\xe6\x27\x72\x6b\x35\xd4\xcd\x7f\xe0\x18\xde\x09\x0b\xc6\x37\x55\x3c\xc7\x82\xe2\x5f\x97\x4c\xba", 32) == 0)
		printf("blake256_hash verified!\n");
	else
		printf("blake256_hash FAILED!\n");
	groestl(msg,hash);
	if(memcmp(hash, "\x5e\x48\x74\x94\x12\x76\xba\xcd\x43\xcf\x9f\x50\x78\xa5\xd6\x20\x14\x3b\x0b\x10\x5f\x63\x3f\x44\xd6\x5e\xd1\x3d\x27\xf6\xa8\x49", 32) == 0)
		printf("groestl verified!\n");
	else
		printf("groestl FAILED!\n");

	jh_hash(msg,hash);
	if(memcmp(hash, "\x4a\xe8\xdb\xb5\xad\x87\x64\x0f\xf6\x6f\x12\x53\x80\xd2\x5d\x3c\x69\x14\x64\xd9\x69\x0e\xaa\x2d\xf5\x77\xe5\xfe\x11\xc7\xb7\x6b", 32) == 0)
		printf("jh_hash verified!\n");
	else
		printf("jh_hash FAILED!\n");
	skein_hash(msg,hash);
	if(memcmp(hash, "\x44\x69\x61\x76\x82\xc7\x66\x62\x7a\xa0\x83\x84\xcb\x41\x50\x2a\x02\x88\xc7\x11\xa6\xcc\x15\xc1\xa5\xf8\x01\x63\x10\xe5\xb5\x52", 32) == 0)
		printf("skein_hash verified!\n");
	else
		printf("skein_hash FAILED!\n");
	return 0;
}
