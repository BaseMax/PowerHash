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
#ifndef RotL_64
	#define RotL_64(x,N)    (((x) << (N)) | ((x) >> (64-(N))))
#endif
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
#ifndef Skein_Put64_LSB_First
void    Skein_Put64_LSB_First(uint8_t *dst,const u64b_t *src,size_t bCnt)
{
	size_t n;
	for(n=0;n<bCnt;n++)
		dst[n] = (uint8_t) (src[n>>3] >> (8*(n&7)));
}
#endif
#ifndef Skein_Get64_LSB_First
	void    Skein_Get64_LSB_First(u64b_t *dst,const uint8_t *src,size_t wCnt)
	{
		size_t n;
		for(n=0;n<8*wCnt;n+=8)
			dst[n/8] = (((u64b_t) src[n  ])      ) +
					   (((u64b_t) src[n+1]) <<  8) +
					   (((u64b_t) src[n+2]) << 16) +
					   (((u64b_t) src[n+3]) << 24) +
					   (((u64b_t) src[n+4]) << 32) +
					   (((u64b_t) src[n+5]) << 40) +
					   (((u64b_t) src[n+6]) << 48) +
					   (((u64b_t) src[n+7]) << 56) ;
	}
#endif
typedef uint8_t BitSequence;
#define SKEIN_512_NIST_MAX_HASHBITS 512
#define SKEIN_MODIFIER_WORDS  2
#define SKEIN_256_STATE_WORDS 4
#define SKEIN_512_STATE_WORDS 8
#define SKEIN1024_STATE_WORDS 16
#define SKEIN_MAX_STATE_WORDS 16
#define SKEIN_256_STATE_BYTES 32
#define SKEIN_512_STATE_BYTES 64
#define SKEIN1024_STATE_BYTES 128
#define SKEIN_256_STATE_BITS  256
#define SKEIN_512_STATE_BITS  512
#define SKEIN1024_STATE_BITS  1024
#define SKEIN_256_BLOCK_BYTES 32
#define SKEIN_512_BLOCK_BYTES 64
#define SKEIN1024_BLOCK_BYTES 128
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
#define SKEIN_TREE_HASH (1)
#define SKEIN_T1_BIT(BIT)       ((BIT) - 64)
#define SKEIN_T1_POS_TREE_LVL   48
#define SKEIN_T1_POS_BIT_PAD    55
#define SKEIN_T1_POS_BLK_TYPE   56
#define SKEIN_T1_POS_FIRST      62
#define SKEIN_T1_POS_FINAL      63
#define SKEIN_T1_FLAG_FIRST     (((u64b_t)  1 ) << 62)
#define SKEIN_T1_FLAG_FINAL     (((u64b_t)  1 ) << 63)
#define SKEIN_T1_FLAG_BIT_PAD   (((u64b_t)  1 ) << 55)
#define SKEIN_T1_TREE_LEVEL(n)  (((u64b_t) (n)) << 48)
#define SKEIN_BLK_TYPE_KEY      (0)
#define SKEIN_BLK_TYPE_CFG      (4)
#define SKEIN_BLK_TYPE_PERS     (8)
#define SKEIN_BLK_TYPE_PK       (12)
#define SKEIN_BLK_TYPE_KDF      (16)
#define SKEIN_BLK_TYPE_NONCE    (20)
#define SKEIN_BLK_TYPE_MSG      (48)
#define SKEIN_BLK_TYPE_OUT      (63)
#define SKEIN_BLK_TYPE_MASK     (63)
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
#ifndef SKEIN_ID_STRING_LE
	#define SKEIN_ID_STRING_LE      (0x33414853)
#endif
#define SKEIN_MK_64(hi32,lo32)  ((lo32) + (((u64b_t) (hi32)) << 32))
#define SKEIN_KS_PARITY         SKEIN_MK_64(0x1BD11BDA,0xA9FC1A22)
#define Skein_Set_Tweak(ctxPtr,TWK_NUM,tVal)    {(ctxPtr)->h.T[TWK_NUM] = (tVal);}
#define Skein_Set_T0(ctxPtr,T0) Skein_Set_Tweak(ctxPtr,0,T0)
#define Skein_Set_T1(ctxPtr,T1) Skein_Set_Tweak(ctxPtr,1,T1)
#define Skein_Set_T0_T1(ctxPtr,T0,T1)           \
{                                           \
	Skein_Set_T0(ctxPtr,(T0));                  \
	Skein_Set_T1(ctxPtr,(T1));                  \
}
#define Skein_Start_New_Type(ctxPtr,BLK_TYPE)   \
{ Skein_Set_T0_T1(ctxPtr,0,SKEIN_T1_FLAG_FIRST | SKEIN_T1_BLK_TYPE_##BLK_TYPE); (ctxPtr)->h.bCnt=0; }

enum    
{
	R_256_0_0=14,R_256_0_1=16,
	R_256_1_0=52,R_256_1_1=57,
	R_256_2_0=23,R_256_2_1=40,
	R_256_3_0= 5,R_256_3_1=37,
	R_256_4_0=25,R_256_4_1=33,
	R_256_5_0=46,R_256_5_1=12,
	R_256_6_0=58,R_256_6_1=22,
	R_256_7_0=32,R_256_7_1=32,
	R_512_0_0=46,R_512_0_1=36,R_512_0_2=19,R_512_0_3=37,
	R_512_1_0=33,R_512_1_1=27,R_512_1_2=14,R_512_1_3=42,
	R_512_2_0=17,R_512_2_1=49,R_512_2_2=36,R_512_2_3=39,
	R_512_3_0=44,R_512_3_1= 9,R_512_3_2=54,R_512_3_3=56,
	R_512_4_0=39,R_512_4_1=30,R_512_4_2=34,R_512_4_3=24,
	R_512_5_0=13,R_512_5_1=50,R_512_5_2=10,R_512_5_3=17,
	R_512_6_0=25,R_512_6_1=29,R_512_6_2=39,R_512_6_3=43,
	R_512_7_0= 8,R_512_7_1=35,R_512_7_2=56,R_512_7_3=22,
	R1024_0_0=24,R1024_0_1=13,R1024_0_2= 8,R1024_0_3=47,R1024_0_4= 8,R1024_0_5=17,R1024_0_6=22,R1024_0_7=37,
	R1024_1_0=38,R1024_1_1=19,R1024_1_2=10,R1024_1_3=55,R1024_1_4=49,R1024_1_5=18,R1024_1_6=23,R1024_1_7=52,
	R1024_2_0=33,R1024_2_1= 4,R1024_2_2=51,R1024_2_3=13,R1024_2_4=34,R1024_2_5=41,R1024_2_6=59,R1024_2_7=17,
	R1024_3_0= 5,R1024_3_1=20,R1024_3_2=48,R1024_3_3=41,R1024_3_4=47,R1024_3_5=28,R1024_3_6=16,R1024_3_7=25,
	R1024_4_0=41,R1024_4_1= 9,R1024_4_2=37,R1024_4_3=31,R1024_4_4=12,R1024_4_5=47,R1024_4_6=44,R1024_4_7=30,
	R1024_5_0=16,R1024_5_1=34,R1024_5_2=56,R1024_5_3=51,R1024_5_4= 4,R1024_5_5=53,R1024_5_6=42,R1024_5_7=41,
	R1024_6_0=31,R1024_6_1=44,R1024_6_2=47,R1024_6_3=46,R1024_6_4=19,R1024_6_5=42,R1024_6_6=44,R1024_6_7=25,
	R1024_7_0= 9,R1024_7_1=48,R1024_7_2=35,R1024_7_3=52,R1024_7_4=23,R1024_7_5=31,R1024_7_6=37,R1024_7_7=20
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
#define BLK_BITS        (WCNT*64)
#define KW_TWK_BASE     (0)
#define KW_KEY_BASE     (3)
#define ks              (kw + KW_KEY_BASE)
#define ts              (kw + KW_TWK_BASE)
#define DebugSaveTweak(ctx) { ctx->h.T[0] = ts[0]; ctx->h.T[1] = ts[1]; }
#if !(SKEIN_USE_ASM & 512)
	static void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const uint8_t *blkPtr,size_t blkCnt,size_t byteCntAdd)
	{
		enum
		{
			WCNT = SKEIN_512_STATE_WORDS
		};
		#undef RCNT
		#define RCNT  (SKEIN_512_ROUNDS_TOTAL/8)
		#define SKEIN_UNROLL_512 (((SKEIN_LOOP)/10)%10)
		#if SKEIN_UNROLL_512
		#if(RCNT % SKEIN_UNROLL_512)
			#error "Invalid SKEIN_UNROLL_512"
		#endif
			size_t r;
			u64b_t kw[WCNT+4+RCNT*2];
		#else
				u64b_t kw[WCNT+4];
		#endif
				u64b_t X0,X1,X2,X3,X4,X5,X6,X7;
				u64b_t w [WCNT];
		#ifdef SKEIN_DEBUG
				const u64b_t *Xptr[8];
				Xptr[0] = &X0;  Xptr[1] = &X1;  Xptr[2] = &X2;  Xptr[3] = &X3;
				Xptr[4] = &X4;  Xptr[5] = &X5;  Xptr[6] = &X6;  Xptr[7] = &X7;
		#endif
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
				ks[8] = ks[0] ^ ks[1] ^ ks[2] ^ ks[3] ^ 
								ks[4] ^ ks[5] ^ ks[6] ^ ks[7] ^ SKEIN_KS_PARITY;
				ts[2] = ts[0] ^ ts[1];
				Skein_Get64_LSB_First(w,blkPtr,WCNT);
				DebugSaveTweak(ctx);
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
					X##p6 += X##p7; X##p7 = RotL_64(X##p7,ROT##_3); X##p7 ^= X##p6; \

				#if SKEIN_UNROLL_512 == 0                       
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
				#else
				#define R512(p0,p1,p2,p3,p4,p5,p6,p7,ROT,rNum)                      \
					Round512(p0,p1,p2,p3,p4,p5,p6,p7,ROT,rNum)

				#define I512(R)                                                     \
					X0   += ks[r+(R)+0]; \
					X1   += ks[r+(R)+1];                                            \
					X2   += ks[r+(R)+2];                                            \
					X3   += ks[r+(R)+3];                                            \
					X4   += ks[r+(R)+4];                                            \
					X5   += ks[r+(R)+5] + ts[r+(R)+0];                              \
					X6   += ks[r+(R)+6] + ts[r+(R)+1];                              \
					X7   += ks[r+(R)+7] +    r+(R)   ;                              \
					ks[r +       (R)+8] = ks[r+(R)-1];   \
					ts[r +       (R)+2] = ts[r+(R)-1];

				for(r=1;r < 2*RCNT;r+=2*SKEIN_UNROLL_512)
				#endif
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
					#define R512_Unroll_R(NN) ((SKEIN_UNROLL_512 == 0 && SKEIN_512_ROUNDS_TOTAL/8 > (NN)) || (SKEIN_UNROLL_512 > (NN)))
					#if R512_Unroll_R(1)
						R512_8_rounds(1);
					#endif
					#if R512_Unroll_R(2)
						R512_8_rounds(2);
					#endif
					#if R512_Unroll_R(3)
						R512_8_rounds(3);
					#endif
					#if R512_Unroll_R(4)
						R512_8_rounds(4);
					#endif
					#if R512_Unroll_R(5)
						R512_8_rounds(5);
					#endif
					#if R512_Unroll_R(6)
						R512_8_rounds(6);
					#endif
					#if R512_Unroll_R(7)
						R512_8_rounds(7);
					#endif
					#if R512_Unroll_R(8)
						R512_8_rounds(8);
					#endif
					#if R512_Unroll_R(9)
						R512_8_rounds(9);
					#endif
					#if R512_Unroll_R(10)
						R512_8_rounds(10);
					#endif
					#if R512_Unroll_R(11)
						R512_8_rounds(11);
					#endif
					#if R512_Unroll_R(12)
						R512_8_rounds(12);
					#endif
					#if R512_Unroll_R(13)
						R512_8_rounds(13);
					#endif
					#if R512_Unroll_R(14)
						R512_8_rounds(14);
					#endif
					#if(SKEIN_UNROLL_512 > 14)
						#error "need more unrolling in Skein_512_Process_Block"
					#endif
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
#endif
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
	Skein_Start_New_Type(&state.u.ctx_512,MSG);
	Skein_512_Ctxt_t *ctx=&state.u.ctx_512;
	Skein_512_Process_Block(ctx,data,3,SKEIN_512_BLOCK_BYTES);
	data        += 192;
	memcpy(&ctx->b[ctx->h.bCnt],data,8);
	ctx->h.bCnt += 8;
	ctx=&state.u.ctx_512;
	uint8_t *hashVal=hashval;
	size_t i,byteCnt;
	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;
	//if(ctx->h.bCnt < SKEIN_512_BLOCK_BYTES)
	memset(&ctx->b[ctx->h.bCnt],0,SKEIN_512_BLOCK_BYTES - ctx->h.bCnt);
	Skein_512_Process_Block(ctx,ctx->b,1,ctx->h.bCnt);
	memset(ctx->b,0,sizeof(ctx->b));
	((u64b_t *)ctx->b)[0]= (u64b_t) 0;
	Skein_Start_New_Type(ctx,OUT_FINAL);
	Skein_512_Process_Block(ctx,ctx->b,1,sizeof(u64b_t));
	size_t bCnt=32;
	//for(n=0;n<bCnt;n++)
	//for(n=0;n<32;n++)
	//	hashVal[n] = (uint8_t) (ctx->X[n>>3] >> (8*(n&7)));
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
