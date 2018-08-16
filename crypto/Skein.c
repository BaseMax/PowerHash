/* 
 *
 * Skein.c
 * Author: Curie Kief , Base Max
 * Purpose of review and rewrite is optimization.
 *
 */
#include <stddef.h>
#include <string.h>
#include <stdint.h>

typedef unsigned int    uint_t;
typedef uint8_t         u08b_t;
typedef uint64_t        u64b_t;

#define RotL_64(x,N)    (((x) << (N)) | ((x) >> (64-(N))))
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#if defined(__ANDROID__)
	#include <byteswap.h>
#endif
#if defined(_MSC_VER)
	#include <stdlib.h>
	static inline uint32_t rol32(uint32_t x, int r)
	{
		return _rotl(x, r);
	}
	static inline uint64_t rol64(uint64_t x, int r)
	{
		return _rotl64(x, r);
	}
#else
	static inline uint32_t rol32(uint32_t x, int r)
	{
		return (x << (r & 31)) | (x >> (-r & 31));
	}
	static inline uint64_t rol64(uint64_t x, int r)
	{
		return (x << (r & 63)) | (x >> (-r & 63));
	}
#endif
static inline uint64_t hi_dword(uint64_t val)
{
	return val >> 32;
}
static inline uint64_t lo_dword(uint64_t val)
{
	return val & 0xFFFFFFFF;
}
static inline uint64_t mul128(uint64_t multiplier, uint64_t multiplicand, uint64_t* product_hi)
{
	uint64_t a = hi_dword(multiplier);
	uint64_t b = lo_dword(multiplier);
	uint64_t c = hi_dword(multiplicand);
	uint64_t d = lo_dword(multiplicand);
	uint64_t ac = a * c;
	uint64_t ad = a * d;
	uint64_t bc = b * c;
	uint64_t bd = b * d;
	uint64_t adbc = ad + bc;
	uint64_t adbc_carry = adbc < ad ? 1 : 0;
	uint64_t product_lo = bd + (adbc << 32);
	uint64_t product_lo_carry = product_lo < bd ? 1 : 0;
	*product_hi = ac + (adbc >> 32) + (adbc_carry << 32) + product_lo_carry;
	return product_lo;
}
static inline uint64_t div_with_reminder(uint64_t dividend, uint32_t divisor, uint32_t* remainder)
{
	dividend |= ((uint64_t)*remainder) << 32;
	*remainder = dividend % divisor;
	return dividend / divisor;
}
static inline uint32_t div128_32(uint64_t dividend_hi, uint64_t dividend_lo, uint32_t divisor, uint64_t* quotient_hi, uint64_t* quotient_lo)
{
	uint64_t dividend_dwords[4];
	uint32_t remainder = 0;
	dividend_dwords[3] = hi_dword(dividend_hi);
	dividend_dwords[2] = lo_dword(dividend_hi);
	dividend_dwords[1] = hi_dword(dividend_lo);
	dividend_dwords[0] = lo_dword(dividend_lo);
	*quotient_hi  = div_with_reminder(dividend_dwords[3], divisor, &remainder) << 32;
	*quotient_hi |= div_with_reminder(dividend_dwords[2], divisor, &remainder);
	*quotient_lo  = div_with_reminder(dividend_dwords[1], divisor, &remainder) << 32;
	*quotient_lo |= div_with_reminder(dividend_dwords[0], divisor, &remainder);
	return remainder;
}
typedef uint8_t BitSequence;
#define SKEIN_512_NIST_MAX_HASHBITS 512
#define SKEIN_MODIFIER_WORDS  2
#define SKEIN_512_STATE_WORDS 8
#define SKEIN_MAX_STATE_WORDS 16
#define SKEIN_512_STATE_BYTES 64
#define SKEIN_512_STATE_BITS  512
#define SKEIN_512_BLOCK_BYTES 64
#define SKEIN_RND_SPECIAL       (1000u)
#define SKEIN_RND_KEY_INITIAL   (SKEIN_RND_SPECIAL+0u)
#define SKEIN_RND_KEY_INJECT    (SKEIN_RND_SPECIAL+1u)
#define SKEIN_RND_FEED_FWD      (SKEIN_RND_SPECIAL+2u)
typedef struct
{
	size_t hashBitLen;
	size_t bCnt;
	u64b_t T[SKEIN_MODIFIER_WORDS];
} Skein_Ctxt_Hdr_t;
typedef struct
{
	Skein_Ctxt_Hdr_t h;
	u64b_t X[SKEIN_512_STATE_WORDS];
	uint8_t  b[SKEIN_512_BLOCK_BYTES];
} Skein_512_Ctxt_t;
#define SKEIN_T1_POS_BLK_TYPE   56
#define SKEIN_T1_FLAG_FIRST     (((u64b_t)  1 ) << 62)
#define SKEIN_T1_FLAG_FINAL     (((u64b_t)  1 ) << 63)
#define SKEIN_BLK_TYPE_MSG      (48)
#define SKEIN_BLK_TYPE_OUT      (63)
#define SKEIN_T1_BLK_TYPE(T)   (((u64b_t) (SKEIN_BLK_TYPE_##T)) << SKEIN_T1_POS_BLK_TYPE)
#define SKEIN_T1_BLK_TYPE_KEY   SKEIN_T1_BLK_TYPE(KEY)
#define SKEIN_T1_BLK_TYPE_CFG   SKEIN_T1_BLK_TYPE(CFG)
#define SKEIN_T1_BLK_TYPE_PERS  SKEIN_T1_BLK_TYPE(PERS)
#define SKEIN_T1_BLK_TYPE_PK    SKEIN_T1_BLK_TYPE(PK)
#define SKEIN_T1_BLK_TYPE_KDF   SKEIN_T1_BLK_TYPE(KDF)
#define SKEIN_T1_BLK_TYPE_NONCE SKEIN_T1_BLK_TYPE(NONCE)
#define SKEIN_T1_BLK_TYPE_MSG   SKEIN_T1_BLK_TYPE(MSG)
#define SKEIN_T1_BLK_TYPE_OUT   SKEIN_T1_BLK_TYPE(OUT)
#define SKEIN_T1_BLK_TYPE_MASK  SKEIN_T1_BLK_TYPE(MASK)
#define SKEIN_T1_BLK_TYPE_CFG_FINAL       (SKEIN_T1_BLK_TYPE_CFG | SKEIN_T1_FLAG_FINAL)
#define SKEIN_T1_BLK_TYPE_OUT_FINAL       (SKEIN_T1_BLK_TYPE_OUT | SKEIN_T1_FLAG_FINAL)
#define SKEIN_VERSION           (1)
#define SKEIN_ID_STRING_LE      (0x33414853)
#define SKEIN_MK_64(hi32,lo32)  ((lo32) + (((u64b_t) (hi32)) << 32))
enum
{
	R_512_0_0=46,R_512_0_1=36,R_512_0_2=19,R_512_0_3=37,
	R_512_1_0=33,R_512_1_1=27,R_512_1_2=14,R_512_1_3=42,
	R_512_2_0=17,R_512_2_1=49,R_512_2_2=36,R_512_2_3=39,
	R_512_3_0=44,R_512_3_1= 9,R_512_3_2=54,R_512_3_3=56,
	R_512_4_0=39,R_512_4_1=30,R_512_4_2=34,R_512_4_3=24,
	R_512_5_0=13,R_512_5_1=50,R_512_5_2=10,R_512_5_3=17,
	R_512_6_0=25,R_512_6_1=29,R_512_6_2=39,R_512_6_3=43,
	R_512_7_0= 8,R_512_7_1=35,R_512_7_2=56,R_512_7_3=22,
};
#define SKEIN_512_ROUNDS_TOTAL (72)
const u64b_t SKEIN_512_IV_256[] =
{
	SKEIN_MK_64(0xCCD044A1,0x2FDB3E13),
	SKEIN_MK_64(0xE8359030,0x1A79A9EB),
	SKEIN_MK_64(0x55AEA061,0x4F816E6F),
	SKEIN_MK_64(0x2A2767A4,0xAE9B94DB),
	SKEIN_MK_64(0xEC06025E,0x74DD7683),
	SKEIN_MK_64(0xE7A436CD,0xC4746251),
	SKEIN_MK_64(0xC36FBAF9,0x393AD185),
	SKEIN_MK_64(0x3EEDBA18,0x33EDFC13)
};
#define SKEIN_USE_ASM   (0)
#define SKEIN_LOOP 001
#define BLK_BITS        512
#define KW_TWK_BASE     (0)
#define KW_KEY_BASE     (3)
#define ks              (kw + KW_KEY_BASE)
#define ts              (kw + KW_TWK_BASE)
static void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const uint8_t *blkPtr,size_t blkCnt,size_t byteCntAdd)
{
	u64b_t kw[8+4];
	u64b_t X0,X1,X2,X3,X4,X5,X6,X7;
	u64b_t w [8];
	const u64b_t *Xptr[8];
	Xptr[0] = &X0;  Xptr[1] = &X1;  Xptr[2] = &X2;  Xptr[3] = &X3;
	Xptr[4] = &X4;  Xptr[5] = &X5;  Xptr[6] = &X6;  Xptr[7] = &X7;
	ts[0] = ctx->h.T[0];
	ts[1] = ctx->h.T[1];
	do
	{
		ts[0] += byteCntAdd;
		ks[0] = ctx->X[0];
		ks[1] = ctx->X[1];
		ks[2] = ctx->X[2];
		ks[3] = ctx->X[3];
		ks[4] = ctx->X[4];
		ks[5] = ctx->X[5];
		ks[6] = ctx->X[6];
		ks[7] = ctx->X[7];
		ks[8] = ks[0] ^ ks[1] ^ ks[2] ^ ks[3] ^ ks[4] ^ ks[5] ^ ks[6] ^ ks[7] ^ ((0xA9FC1A22) + (((u64b_t) (0x1BD11BDA)) << 32));
		ts[2] = ts[0] ^ ts[1];
		for(size_t n=0;n<64;n+=8)
			w[n/8] = (((u64b_t) blkPtr[n  ])      ) +
					   (((u64b_t) blkPtr[n+1]) <<  8) +
					   (((u64b_t) blkPtr[n+2]) << 16) +
					   (((u64b_t) blkPtr[n+3]) << 24) +
					   (((u64b_t) blkPtr[n+4]) << 32) +
					   (((u64b_t) blkPtr[n+5]) << 40) +
					   (((u64b_t) blkPtr[n+6]) << 48) +
					   (((u64b_t) blkPtr[n+7]) << 56) ;
		ctx->h.T[0] = ts[0];
		ctx->h.T[1] = ts[1];
		X0   = w[0] + ks[0];
		X1   = w[1] + ks[1];
		X2   = w[2] + ks[2];
		X3   = w[3] + ks[3];
		X4   = w[4] + ks[4];
		X5   = w[5] + ks[5] + ts[0];
		X6   = w[6] + ks[6] + ts[1];
		X7   = w[7] + ks[7];
		blkPtr += SKEIN_512_BLOCK_BYTES;
		#define Round512(p0,p1,p2,p3,p4,p5,p6,p7,ROT,rNum)                  \
			X##p0 += X##p1; X##p1 = RotL_64(X##p1,ROT##_0); X##p1 ^= X##p0; \
			X##p2 += X##p3; X##p3 = RotL_64(X##p3,ROT##_1); X##p3 ^= X##p2; \
			X##p4 += X##p5; X##p5 = RotL_64(X##p5,ROT##_2); X##p5 ^= X##p4; \
			X##p6 += X##p7; X##p7 = RotL_64(X##p7,ROT##_3); X##p7 ^= X##p6; 
			#define R512(p0,p1,p2,p3,p4,p5,p6,p7,ROT,rNum)  \
				Round512(p0,p1,p2,p3,p4,p5,p6,p7,ROT,rNum)
			#define I512(R)                                                     \
				X0   += ks[((R)+1) % 9];  \
				X1   += ks[((R)+2) % 9];                                        \
				X2   += ks[((R)+3) % 9];                                        \
				X3   += ks[((R)+4) % 9];                                        \
				X4   += ks[((R)+5) % 9];                                        \
				X5   += ks[((R)+6) % 9] + ts[((R)+1) % 3];                      \
				X6   += ks[((R)+7) % 9] + ts[((R)+2) % 3];                      \
				X7   += ks[((R)+8) % 9] +     (R)+1;
		{
			#define R512_8_rounds(R)  \
				R512(0,1,2,3,4,5,6,7,R_512_0,8*(R)+ 1);   \
				R512(2,1,4,7,6,5,0,3,R_512_1,8*(R)+ 2);   \
				R512(4,1,6,3,0,5,2,7,R_512_2,8*(R)+ 3);   \
				R512(6,1,0,7,2,5,4,3,R_512_3,8*(R)+ 4);   \
				I512(2*(R));                              \
				R512(0,1,2,3,4,5,6,7,R_512_4,8*(R)+ 5);   \
				R512(2,1,4,7,6,5,0,3,R_512_5,8*(R)+ 6);   \
				R512(4,1,6,3,0,5,2,7,R_512_6,8*(R)+ 7);   \
				R512(6,1,0,7,2,5,4,3,R_512_7,8*(R)+ 8);   \
				I512(2*(R)+1);
				R512_8_rounds(0);
				R512_8_rounds(1);
				R512_8_rounds(2);
				R512_8_rounds(3);
				R512_8_rounds(4);
				R512_8_rounds(5);
				R512_8_rounds(6);
				R512_8_rounds(7);
				R512_8_rounds(8);
			}
		ctx->X[0] = X0 ^ w[0];
		ctx->X[1] = X1 ^ w[1];
		ctx->X[2] = X2 ^ w[2];
		ctx->X[3] = X3 ^ w[3];
		ctx->X[4] = X4 ^ w[4];
		ctx->X[5] = X5 ^ w[5];
		ctx->X[6] = X6 ^ w[6];
		ctx->X[7] = X7 ^ w[7];
		ts[1] &= ~SKEIN_T1_FLAG_FIRST;
	}
	while(--blkCnt);
	ctx->h.T[0] = ts[0];
	ctx->h.T[1] = ts[1];
}
typedef struct
{
	union
	{
		Skein_Ctxt_Hdr_t h;
		Skein_512_Ctxt_t ctx_512;
	} u;
} hashState;
int skein_hash(const BitSequence *data,BitSequence *hashval)
{
	hashState state;
	(&state.u.ctx_512)->h.hashBitLen = 256;
	memcpy((&state.u.ctx_512)->X,SKEIN_512_IV_256,sizeof((&state.u.ctx_512)->X));
	(&state.u.ctx_512)->h.T[0] = 0;
	(&state.u.ctx_512)->h.T[0] = 0;
	(&state.u.ctx_512)->h.T[1] = SKEIN_T1_FLAG_FIRST | SKEIN_T1_BLK_TYPE_MSG;
	(&state.u.ctx_512)->h.bCnt=0;
	Skein_512_Ctxt_t *ctx=&state.u.ctx_512;
	Skein_512_Process_Block(ctx,data,3,SKEIN_512_BLOCK_BYTES);
	data        += 192;
	memcpy(&ctx->b[ctx->h.bCnt],data,8);
	ctx->h.bCnt += 8;
	ctx=&state.u.ctx_512;
	uint8_t *hashVal=hashval;
	size_t i,byteCnt;
	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;
	memset(&ctx->b[ctx->h.bCnt],0,SKEIN_512_BLOCK_BYTES - ctx->h.bCnt);
	Skein_512_Process_Block(ctx,ctx->b,1,ctx->h.bCnt);
	memset(ctx->b,0,sizeof(ctx->b));
	((u64b_t *)ctx->b)[0]= (u64b_t) 0;
	(ctx)->h.T[0] = (0);
	(ctx)->h.T[1] = SKEIN_T1_FLAG_FIRST | SKEIN_T1_BLK_TYPE_OUT_FINAL;
	(ctx)->h.bCnt=0;
	Skein_512_Process_Block(ctx,ctx->b,1,sizeof(u64b_t));
	size_t bCnt=32;
	hashVal[0] = (uint8_t) (ctx->X[0] >> (0));
	hashVal[1] = (uint8_t) (ctx->X[0] >> (8));
	hashVal[2] = (uint8_t) (ctx->X[0] >> (16));
	hashVal[3] = (uint8_t) (ctx->X[0] >> (24));
	hashVal[4] = (uint8_t) (ctx->X[0] >> (32));
	hashVal[5] = (uint8_t) (ctx->X[0] >> (40));
	hashVal[6] = (uint8_t) (ctx->X[0] >> (48));
	hashVal[7] = (uint8_t) (ctx->X[0] >> (56));
	hashVal[8] = (uint8_t) (ctx->X[1] >> (0));
	hashVal[9] = (uint8_t) (ctx->X[1] >> (8));
	hashVal[10] = (uint8_t) (ctx->X[1] >> (16));
	hashVal[11] = (uint8_t) (ctx->X[1] >> (24));
	hashVal[12] = (uint8_t) (ctx->X[1] >> (32));
	hashVal[13] = (uint8_t) (ctx->X[1] >> (40));
	hashVal[14] = (uint8_t) (ctx->X[1] >> (48));
	hashVal[15] = (uint8_t) (ctx->X[1] >> (56));
	hashVal[16] = (uint8_t) (ctx->X[2] >> (0));
	hashVal[17] = (uint8_t) (ctx->X[2] >> (8));
	hashVal[18] = (uint8_t) (ctx->X[2] >> (16));
	hashVal[19] = (uint8_t) (ctx->X[2] >> (24));
	hashVal[20] = (uint8_t) (ctx->X[2] >> (32));
	hashVal[21] = (uint8_t) (ctx->X[2] >> (40));
	hashVal[22] = (uint8_t) (ctx->X[2] >> (48));
	hashVal[23] = (uint8_t) (ctx->X[2] >> (56));
	hashVal[24] = (uint8_t) (ctx->X[3] >> (0));
	hashVal[25] = (uint8_t) (ctx->X[3] >> (8));
	hashVal[26] = (uint8_t) (ctx->X[3] >> (16));
	hashVal[27] = (uint8_t) (ctx->X[3] >> (24));
	hashVal[28] = (uint8_t) (ctx->X[3] >> (32));
	hashVal[29] = (uint8_t) (ctx->X[3] >> (40));
	hashVal[30] = (uint8_t) (ctx->X[3] >> (48));
	hashVal[31] = (uint8_t) (ctx->X[3] >> (56));
	return 0;
}
