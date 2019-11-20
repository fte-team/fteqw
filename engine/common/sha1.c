/*
SHA-1 in C
By Steve Reid <steve@edmweb.com>
100% Public Domain

Test Vectors (from FIPS PUB 180-1)
"abc"
A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F

This file came to FTE via EzQuake.
*/

/* #define SHA1HANDSOFF * Copies data before messing with it. */
#define SHA1HANDSOFF
#include "quakedef.h"
#include <string.h>

#define BigLong(l)  (((unsigned char*)&l)[0]<<24) | (((unsigned char*)&l)[1]<<16) | (((unsigned char*)&l)[2]<<8) | (((unsigned char*)&l)[3]<<0)


#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

#define blk0(i) (block->l[i] = BigLong(block->l[i]))

#define blk(i) (block->l[i&15] = rol(block->l[(i+13)&15]^block->l[(i+8)&15] \
^block->l[(i+2)&15]^block->l[i&15],1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

typedef struct
{
    unsigned int state[5];
    size_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define DIGEST_SIZE 20
void SHA1Transform(unsigned int state[5], const unsigned char buffer[64]);
void SHA1Init(SHA1_CTX* context);
void SHA1Update(SHA1_CTX* context, const unsigned char* data, size_t len);
void SHA1Final(unsigned char digest[DIGEST_SIZE], SHA1_CTX* context);


/* Hash a single 512-bit block. This is the core of the algorithm. */

void SHA1Transform(unsigned int state[5], const unsigned char buffer[64])
{
	unsigned int a, b, c, d, e;
	typedef union
	{
		unsigned char c[64];
		unsigned int l[16];
	} CHAR64LONG16;
	CHAR64LONG16* block;
	#ifdef SHA1HANDSOFF
	unsigned char workspace[64];
	block = (CHAR64LONG16*)workspace;
	memcpy(block, buffer, 64);
	#else
	block = (CHAR64LONG16*)buffer;
	#endif
	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	/* 4 rounds of 20 operations each. Loop unrolled. */
	R0(a,b,c,d,e, 0); R0(e,a,b,c,d, 1); R0(d,e,a,b,c, 2); R0(c,d,e,a,b, 3);
	R0(b,c,d,e,a, 4); R0(a,b,c,d,e, 5); R0(e,a,b,c,d, 6); R0(d,e,a,b,c, 7);
	R0(c,d,e,a,b, 8); R0(b,c,d,e,a, 9); R0(a,b,c,d,e,10); R0(e,a,b,c,d,11);
	R0(d,e,a,b,c,12); R0(c,d,e,a,b,13); R0(b,c,d,e,a,14); R0(a,b,c,d,e,15);
	R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
	R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
	R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
	R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
	R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
	R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
	R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
	R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
	R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
	R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
	R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
	R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
	R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
	R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
	R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
	R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);
	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
}


/* SHA1Init - Initialize new context */

void SHA1Init(SHA1_CTX* context)
{
    /* SHA1 initialization constants */
    context->state[0] = 0x67452301;
    context->state[1] = 0xEFCDAB89;
    context->state[2] = 0x98BADCFE;
    context->state[3] = 0x10325476;
    context->state[4] = 0xC3D2E1F0;
    context->count[0] = context->count[1] = 0;
}


/* Run your data through this. */

void SHA1Update(SHA1_CTX* context, const unsigned char* data, size_t len)
{
	size_t i, j;

	j = (context->count[0] >> 3) & 63;
	if ((context->count[0] += len << 3) < (len << 3)) context->count[1]++;
	context->count[1] += (len >> 29);
	if ((j + len) > 63)
	{
		memcpy(&context->buffer[j], data, (i = 64-j));
		SHA1Transform(context->state, context->buffer);
		for ( ; i + 63 < len; i += 64)
		{
			SHA1Transform(context->state, &data[i]);
		}
		j = 0;
	}
	else
		i = 0;
	memcpy(&context->buffer[j], &data[i], len - i);
}


/* Add padding and return the message digest. */

void SHA1Final(unsigned char digest[DIGEST_SIZE], SHA1_CTX* context)
{
	unsigned int i, j;
	unsigned char finalcount[8];

	for (i = 0; i < 8; i++)
	{
		finalcount[i] = (unsigned char)((context->count[(i >= 4 ? 0 : 1)] >> ((3-(i & 3)) * 8) ) & 255); /* Endian independent */
	}
	SHA1Update(context, (unsigned char *)"\200", 1);
	while ((context->count[0] & 504) != 448)
	{
		SHA1Update(context, (unsigned char *)"\0", 1);
	}
	SHA1Update(context, finalcount, 8); /* Should cause a SHA1Transform() */
	for (i = 0; i < DIGEST_SIZE; i++)
	{
		digest[i] = (unsigned char)
		((context->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
	}
	/* Wipe variables */
	i = j = 0;
	memset(context->buffer, 0, 64);
	memset(context->state, 0, 20);
	memset(context->count, 0, 8);
memset(&finalcount, 0, 8);
#ifdef SHA1HANDSOFF /* make SHA1Transform overwrite it's own static vars */
	SHA1Transform(context->state, context->buffer);
#endif
}


size_t SHA1(unsigned char *digest, size_t maxdigestsize, const unsigned char *string, size_t stringlen)
{
	SHA1_CTX context;
	if (maxdigestsize < DIGEST_SIZE)
		return 0;

	SHA1Init(&context);
	SHA1Update(&context, (unsigned char*) string, stringlen);
	SHA1Final(digest, &context);

	return DIGEST_SIZE;
}

size_t SHA1_m(unsigned char *digest, size_t maxdigestsize, size_t numstrings, const unsigned char **strings, size_t *stringlens)
{
	size_t i;
	SHA1_CTX context;
	if (maxdigestsize < DIGEST_SIZE)
		return 0;

	SHA1Init(&context);
	for (i = 0; i < numstrings; i++)
		SHA1Update(&context, (unsigned char*) strings[i], stringlens[i]);
	SHA1Final(digest, &context);

	return DIGEST_SIZE;
}

/* hmac-sha1.c -- hashed message authentication codes
Copyright (C) 2005, 2006 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. */

/* Written by Simon Josefsson.
hacked up a bit by someone else...
*/

#define IPAD 0x36
#define OPAD 0x5c

static void memxor(char *dest, const char *src, size_t length)
{
	size_t i;
	for (i = 0; i < length; i++)
	{
		dest[i] ^= src[i];
	}
}

//typedef size_t hashfunc_t(unsigned char *digest, size_t maxdigestsize, size_t numstrings, const unsigned char **strings, size_t *stringlens);
size_t HMAC(hashfunc_t *hashfunc, unsigned char *digest, size_t maxdigestsize,
				 const unsigned char *data, size_t datalen,
				 const unsigned char *key, size_t keylen)
{
#define HMAC_DIGEST_MAXSIZE 20
	char optkeybuf[HMAC_DIGEST_MAXSIZE];
	char innerhash[HMAC_DIGEST_MAXSIZE];

	char block[64];
	size_t innerhashsize;

	/* Reduce the key's size, so that it is never larger than a block. */

	if (keylen > sizeof(block))
	{
		keylen = hashfunc(optkeybuf, sizeof(optkeybuf), 1, &key, &keylen);
		key=optkeybuf;
	}

	/* Compute INNERHASH from KEY and IN. */

	memset (block, IPAD, sizeof (block));
	memxor (block, key, keylen);

	{
		const unsigned char *strings_i[2] = {block, data};
		size_t stringlens_i[2] = {sizeof(block), datalen};
		innerhashsize = hashfunc(innerhash, sizeof(innerhash), 2, strings_i, stringlens_i);
	}

	/* Compute result from KEY and INNERHASH. */

	memset (block, OPAD, sizeof (block));
	memxor (block, key, keylen);

	{
		const unsigned char *strings_o[2] = {block, innerhash};
		size_t stringlens_o[2] = {sizeof(block), innerhashsize};
		return hashfunc(digest, maxdigestsize, 2, strings_o, stringlens_o);
	}
}
