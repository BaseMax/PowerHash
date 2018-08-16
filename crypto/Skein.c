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
#ifndef SKEIN_NEED_SWAP
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
#define IDENT32(x) ((uint32_t) (x))
#define IDENT64(x) ((uint64_t) (x))
#define SWAP32(x) ((((uint32_t) (x) & 0x000000ff) << 24) | \
  (((uint32_t) (x) & 0x0000ff00) <<  8) | \
  (((uint32_t) (x) & 0x00ff0000) >>  8) | \
  (((uint32_t) (x) & 0xff000000) >> 24))
#define SWAP64(x) ((((uint64_t) (x) & 0x00000000000000ff) << 56) | \
  (((uint64_t) (x) & 0x000000000000ff00) << 40) | \
  (((uint64_t) (x) & 0x0000000000ff0000) << 24) | \
  (((uint64_t) (x) & 0x00000000ff000000) <<  8) | \
  (((uint64_t) (x) & 0x000000ff00000000) >>  8) | \
  (((uint64_t) (x) & 0x0000ff0000000000) >> 24) | \
  (((uint64_t) (x) & 0x00ff000000000000) >> 40) | \
  (((uint64_t) (x) & 0xff00000000000000) >> 56))
static inline uint32_t ident32(uint32_t x) { return x; }
static inline uint64_t ident64(uint64_t x) { return x; }
#ifndef __OpenBSD__
#	if defined(__ANDROID__) && defined(__swap32) && !defined(swap32)
#			define swap32 __swap32
#	elif !defined(swap32)
static inline uint32_t swap32(uint32_t x)
{
	x = ((x & 0x00ff00ff) << 8) | ((x & 0xff00ff00) >> 8);
	return (x << 16) | (x >> 16);
}
#	endif
#	if defined(__ANDROID__) && defined(__swap64) && !defined(swap64)
#			define swap64 __swap64
#	elif !defined(swap64)
static inline uint64_t swap64(uint64_t x)
{
	x = ((x & 0x00ff00ff00ff00ff) <<  8) | ((x & 0xff00ff00ff00ff00) >>  8);
	x = ((x & 0x0000ffff0000ffff) << 16) | ((x & 0xffff0000ffff0000) >> 16);
	return (x << 32) | (x >> 32);
}
#	endif
#endif
#if defined(__GNUC__)
	#define UNUSED __attribute__((unused))
#else
	#define UNUSED
#endif
static inline void mem_inplace_ident(void *mem UNUSED, size_t n UNUSED) { }
	#undef UNUSED
static inline void mem_inplace_swap32(void *mem, size_t n)
{
	size_t i;
	for(i = 0; i < n; i++)
	{
		((uint32_t *) mem)[i] = swap32(((const uint32_t *) mem)[i]);
	}
}
static inline void mem_inplace_swap64(void *mem, size_t n)
{
	size_t i;
	for(i = 0; i < n; i++)
	{
		((uint64_t *) mem)[i] = swap64(((const uint64_t *) mem)[i]);
	}
}
static inline void memcpy_ident32(void *dst, const void *src, size_t n)
{
	memcpy(dst, src, 4 * n);
}
static inline void memcpy_ident64(void *dst, const void *src, size_t n)
{
	memcpy(dst, src, 8 * n);
}
static inline void memcpy_swap32(void *dst, const void *src, size_t n)
{
	size_t i;
	for(i = 0; i < n; i++)
	{
		((uint32_t *) dst)[i] = swap32(((const uint32_t *) src)[i]);
	}
}
static inline void memcpy_swap64(void *dst, const void *src, size_t n)
{
	size_t i;
	for(i = 0; i < n; i++)
	{
		((uint64_t *) dst)[i] = swap64(((const uint64_t *) src)[i]);
	}
}
#if BYTE_ORDER == LITTLE_ENDIAN
	#define SWAP32LE IDENT32
	#define SWAP32BE SWAP32
	#define swap32le ident32
	#define swap32be swap32
	#define mem_inplace_swap32le mem_inplace_ident
	#define mem_inplace_swap32be mem_inplace_swap32
	#define memcpy_swap32le memcpy_ident32
	#define memcpy_swap32be memcpy_swap32
	#define SWAP64LE IDENT64
	#define SWAP64BE SWAP64
	#define swap64le ident64
	#define swap64be swap64
	#define mem_inplace_swap64le mem_inplace_ident
	#define mem_inplace_swap64be mem_inplace_swap64
	#define memcpy_swap64le memcpy_ident64
	#define memcpy_swap64be memcpy_swap64
#endif
#if BYTE_ORDER == BIG_ENDIAN
	#define SWAP32BE IDENT32
	#define SWAP32LE SWAP32
	#define swap32be ident32
	#define swap32le swap32
	#define mem_inplace_swap32be mem_inplace_ident
	#define mem_inplace_swap32le mem_inplace_swap32
	#define memcpy_swap32be memcpy_ident32
	#define memcpy_swap32le memcpy_swap32
	#define SWAP64BE IDENT64
	#define SWAP64LE SWAP64
	#define swap64be ident64
	#define swap64le swap64
	#define mem_inplace_swap64be mem_inplace_ident
	#define mem_inplace_swap64le mem_inplace_swap64
	#define memcpy_swap64be memcpy_ident64
	#define memcpy_swap64le memcpy_swap64
#endif
#define IS_BIG_ENDIAN      4321
#define IS_LITTLE_ENDIAN   1234
#if BYTE_ORDER == LITTLE_ENDIAN
	#define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif
#if BYTE_ORDER == BIG_ENDIAN
	#define PLATFORM_BYTE_ORDER IS_BIG_ENDIAN
#endif
#if defined(__ia64) || defined(__ia64__) || defined(_M_IA64)
	#define PLATFORM_MUST_ALIGN (1)
#ifndef PLATFORM_BYTE_ORDER
	#define PLATFORM_BYTE_ORDER IS_LITTLE_ENDIAN
#endif
#endif
#ifndef PLATFORM_MUST_ALIGN
	#define PLATFORM_MUST_ALIGN (0)
#endif
#if PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN
	#define SKEIN_NEED_SWAP   (1)
#elif PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN
	#define SKEIN_NEED_SWAP   (0)
#if PLATFORM_MUST_ALIGN == 0
	#define Skein_Put64_LSB_First(dst08,src64,bCnt) memcpy(dst08,src64,bCnt)
	#define Skein_Get64_LSB_First(dst64,src08,wCnt) memcpy(dst64,src08,8*(wCnt))
#endif
#else
	#error "Skein needs endianness setting!"
#endif
#endif
#if SKEIN_NEED_SWAP
#define Skein_Swap64(w64)                       \
  (((((u64b_t)(w64))       & 0xFF) << 56) |   \
	(((((u64b_t)(w64)) >> 8) & 0xFF) << 48) |   \
	(((((u64b_t)(w64)) >>16) & 0xFF) << 40) |   \
	(((((u64b_t)(w64)) >>24) & 0xFF) << 32) |   \
	(((((u64b_t)(w64)) >>32) & 0xFF) << 24) |   \
	(((((u64b_t)(w64)) >>40) & 0xFF) << 16) |   \
	(((((u64b_t)(w64)) >>48) & 0xFF) <<  8) |   \
	(((((u64b_t)(w64)) >>56) & 0xFF)      ) )
#else
#define Skein_Swap64(w64)  (w64)
#endif
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
//#define SKEIN_MODIFIER_WORDS  2
#define SKEIN_MODIFIER_WORDS  2
//#define SKEIN_256_STATE_WORDS 4
#define SKEIN_256_STATE_WORDS 4
//#define SKEIN_512_STATE_WORDS 8
#define SKEIN_512_STATE_WORDS 8
//#define SKEIN1024_STATE_WORDS 16
#define SKEIN1024_STATE_WORDS 16
//#define SKEIN_MAX_STATE_WORDS 16
#define SKEIN_MAX_STATE_WORDS 16
//#define SKEIN_256_STATE_BYTES 8*4
#define SKEIN_256_STATE_BYTES 32
//#define SKEIN_512_STATE_BYTES 8*8
#define SKEIN_512_STATE_BYTES 64
//#define SKEIN1024_STATE_BYTES 8*16
#define SKEIN1024_STATE_BYTES 128
//#define SKEIN_256_STATE_BITS  64*4
#define SKEIN_256_STATE_BITS  256
//#define SKEIN_512_STATE_BITS  64*8
#define SKEIN_512_STATE_BITS  512
//#define SKEIN1024_STATE_BITS  64*16
#define SKEIN1024_STATE_BITS  1024
//#define SKEIN_256_BLOCK_BYTES 8*4
#define SKEIN_256_BLOCK_BYTES 32
//#define SKEIN_512_BLOCK_BYTES 8*SKEIN_512_STATE_WORDS
//#define SKEIN_512_BLOCK_BYTES 8*8
#define SKEIN_512_BLOCK_BYTES 64
//#define SKEIN1024_BLOCK_BYTES (8*SKEIN1024_STATE_WORDS)
//#define SKEIN1024_BLOCK_BYTES (8*16)
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
//#define SKEIN_T1_POS_TREE_LVL   SKEIN_T1_BIT(112)
#define SKEIN_T1_POS_TREE_LVL   48
//#define SKEIN_T1_POS_BIT_PAD    SKEIN_T1_BIT(119)
#define SKEIN_T1_POS_BIT_PAD    55
//#define SKEIN_T1_POS_BLK_TYPE   SKEIN_T1_BIT(120)
#define SKEIN_T1_POS_BLK_TYPE   56
//#define SKEIN_T1_POS_FIRST      SKEIN_T1_BIT(126)
#define SKEIN_T1_POS_FIRST      62
//#define SKEIN_T1_POS_FINAL      SKEIN_T1_BIT(127)
#define SKEIN_T1_POS_FINAL      63
//#define SKEIN_T1_FLAG_FIRST     (((u64b_t)  1 ) << SKEIN_T1_POS_FIRST)
#define SKEIN_T1_FLAG_FIRST     (((u64b_t)  1 ) << 62)
//#define SKEIN_T1_FLAG_FINAL     (((u64b_t)  1 ) << SKEIN_T1_POS_FINAL)
#define SKEIN_T1_FLAG_FINAL     (((u64b_t)  1 ) << 63)
//#define SKEIN_T1_FLAG_BIT_PAD   (((u64b_t)  1 ) << SKEIN_T1_POS_BIT_PAD)
#define SKEIN_T1_FLAG_BIT_PAD   (((u64b_t)  1 ) << 55)
//#define SKEIN_T1_TREE_LEVEL(n)  (((u64b_t) (n)) << SKEIN_T1_POS_TREE_LVL)
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
#define SKEIN_CFG_TREE_LEAF_SIZE_POS  (0)
#define SKEIN_CFG_TREE_NODE_SIZE_POS  (8)
#define SKEIN_CFG_TREE_MAX_LEVEL_POS  (16)
#define SKEIN_CFG_TREE_INFO(leaf,node,maxLvl)                   \
	((((u64b_t)(leaf  )) << SKEIN_CFG_TREE_LEAF_SIZE_POS) |    \
	(((u64b_t)(node  )) << SKEIN_CFG_TREE_NODE_SIZE_POS) |    \
	(((u64b_t)(maxLvl)) << SKEIN_CFG_TREE_MAX_LEVEL_POS) )
#define Skein_Get_Tweak2(ctxPtr,TWK_NUM)        ((ctxPtr).h.T[TWK_NUM])
#define Skein_Set_Tweak(ctxPtr,TWK_NUM,tVal)    {(ctxPtr)->h.T[TWK_NUM] = (tVal);}
#define Skein_Set_T0(ctxPtr,T0) Skein_Set_Tweak(ctxPtr,0,T0)
#define Skein_Set_T1(ctxPtr,T1) Skein_Set_Tweak(ctxPtr,1,T1)
#define Skein_Set_T0_T1(ctxPtr,T0,T1)           \
{                                           \
	Skein_Set_T0(ctxPtr,(T0));                  \
	Skein_Set_T1(ctxPtr,(T1));                  \
}
#define Skein_Set_Type(ctxPtr,BLK_TYPE)         \
	Skein_Set_T1(ctxPtr,SKEIN_T1_BLK_TYPE_##BLK_TYPE)
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
#define MK_64 SKEIN_MK_64
const u64b_t SKEIN_256_IV_128[] =
{
	MK_64(0xE1111906,0x964D7260),
	MK_64(0x883DAAA7,0x7C8D811C),
	MK_64(0x10080DF4,0x91960F7A),
	MK_64(0xCCF7DDE5,0xB45BC1C2)
};
const u64b_t SKEIN_256_IV_160[] =
{
	MK_64(0x14202314,0x72825E98),
	MK_64(0x2AC4E9A2,0x5A77E590),
	MK_64(0xD47A5856,0x8838D63E),
	MK_64(0x2DD2E496,0x8586AB7D)
};
const u64b_t SKEIN_256_IV_224[] =
{
	MK_64(0xC6098A8C,0x9AE5EA0B),
	MK_64(0x876D5686,0x08C5191C),
	MK_64(0x99CB88D7,0xD7F53884),
	MK_64(0x384BDDB1,0xAEDDB5DE)
};
const u64b_t SKEIN_256_IV_256[] =
{
	MK_64(0xFC9DA860,0xD048B449),
	MK_64(0x2FCA6647,0x9FA7D833),
	MK_64(0xB33BC389,0x6656840F),
	MK_64(0x6A54E920,0xFDE8DA69)
};
const u64b_t SKEIN_512_IV_128[] =
{
	MK_64(0xA8BC7BF3,0x6FBF9F52),
	MK_64(0x1E9872CE,0xBD1AF0AA),
	MK_64(0x309B1790,0xB32190D3),
	MK_64(0xBCFBB854,0x3F94805C),
	MK_64(0x0DA61BCD,0x6E31B11B),
	MK_64(0x1A18EBEA,0xD46A32E3),
	MK_64(0xA2CC5B18,0xCE84AA82),
	MK_64(0x6982AB28,0x9D46982D)
};
const u64b_t SKEIN_512_IV_160[] =
{
	MK_64(0x28B81A2A,0xE013BD91),
	MK_64(0xC2F11668,0xB5BDF78F),
	MK_64(0x1760D8F3,0xF6A56F12),
	MK_64(0x4FB74758,0x8239904F),
	MK_64(0x21EDE07F,0x7EAF5056),
	MK_64(0xD908922E,0x63ED70B8),
	MK_64(0xB8EC76FF,0xECCB52FA),
	MK_64(0x01A47BB8,0xA3F27A6E)
};
const u64b_t SKEIN_512_IV_224[] =
{
	MK_64(0xCCD06162,0x48677224),
	MK_64(0xCBA65CF3,0xA92339EF),
	MK_64(0x8CCD69D6,0x52FF4B64),
	MK_64(0x398AED7B,0x3AB890B4),
	MK_64(0x0F59D1B1,0x457D2BD0),
	MK_64(0x6776FE65,0x75D4EB3D),
	MK_64(0x99FBC70E,0x997413E9),
	MK_64(0x9E2CFCCF,0xE1C41EF7)
};
const u64b_t SKEIN_512_IV_256[] =
{
	MK_64(0xCCD044A1,0x2FDB3E13),
	MK_64(0xE8359030,0x1A79A9EB),
	MK_64(0x55AEA061,0x4F816E6F),
	MK_64(0x2A2767A4,0xAE9B94DB),
	MK_64(0xEC06025E,0x74DD7683),
	MK_64(0xE7A436CD,0xC4746251),
	MK_64(0xC36FBAF9,0x393AD185),
	MK_64(0x3EEDBA18,0x33EDFC13)
};
const u64b_t SKEIN_512_IV_384[] =
{
	MK_64(0xA3F6C6BF,0x3A75EF5F),
	MK_64(0xB0FEF9CC,0xFD84FAA4),
	MK_64(0x9D77DD66,0x3D770CFE),
	MK_64(0xD798CBF3,0xB468FDDA),
	MK_64(0x1BC4A666,0x8A0E4465),
	MK_64(0x7ED7D434,0xE5807407),
	MK_64(0x548FC1AC,0xD4EC44D6),
	MK_64(0x266E1754,0x6AA18FF8)
};
const u64b_t SKEIN_512_IV_512[] =
{
	MK_64(0x4903ADFF,0x749C51CE),
	MK_64(0x0D95DE39,0x9746DF03),
	MK_64(0x8FD19341,0x27C79BCE),
	MK_64(0x9A255629,0xFF352CB1),
	MK_64(0x5DB62599,0xDF6CA7B0),
	MK_64(0xEABE394C,0xA9D5C3F4),
	MK_64(0x991112C7,0x1A75B523),
	MK_64(0xAE18A40B,0x660FCC33)
};
const u64b_t SKEIN1024_IV_384[] =
{
	MK_64(0x5102B6B8,0xC1894A35),
	MK_64(0xFEEBC9E3,0xFE8AF11A),
	MK_64(0x0C807F06,0xE32BED71),
	MK_64(0x60C13A52,0xB41A91F6),
	MK_64(0x9716D35D,0xD4917C38),
	MK_64(0xE780DF12,0x6FD31D3A),
	MK_64(0x797846B6,0xC898303A),
	MK_64(0xB172C2A8,0xB3572A3B),
	MK_64(0xC9BC8203,0xA6104A6C),
	MK_64(0x65909338,0xD75624F4),
	MK_64(0x94BCC568,0x4B3F81A0),
	MK_64(0x3EBBF51E,0x10ECFD46),
	MK_64(0x2DF50F0B,0xEEB08542),
	MK_64(0x3B5A6530,0x0DBC6516),
	MK_64(0x484B9CD2,0x167BBCE1),
	MK_64(0x2D136947,0xD4CBAFEA)
};
const u64b_t SKEIN1024_IV_512[] =
{
	MK_64(0xCAEC0E5D,0x7C1B1B18),
	MK_64(0xA01B0E04,0x5F03E802),
	MK_64(0x33840451,0xED912885),
	MK_64(0x374AFB04,0xEAEC2E1C),
	MK_64(0xDF25A0E2,0x813581F7),
	MK_64(0xE4004093,0x8B12F9D2),
	MK_64(0xA662D539,0xC2ED39B6),
	MK_64(0xFA8B85CF,0x45D8C75A),
	MK_64(0x8316ED8E,0x29EDE796),
	MK_64(0x053289C0,0x2E9F91B8),
	MK_64(0xC3F8EF1D,0x6D518B73),
	MK_64(0xBDCEC3C4,0xD5EF332E),
	MK_64(0x549A7E52,0x22974487),
	MK_64(0x67070872,0x5B749816),
	MK_64(0xB9CD28FB,0xF0581BD1),
	MK_64(0x0E2940B8,0x15804974)
};
const u64b_t SKEIN1024_IV_1024[] =
{
	MK_64(0xD593DA07,0x41E72355),
	MK_64(0x15B5E511,0xAC73E00C),
	MK_64(0x5180E5AE,0xBAF2C4F0),
	MK_64(0x03BD41D3,0xFCBCAFAF),
	MK_64(0x1CAEC6FD,0x1983A898),
	MK_64(0x6E510B8B,0xCDD0589F),
	MK_64(0x77E2BDFD,0xC6394ADA),
	MK_64(0xC11E1DB5,0x24DCB0A3),
	MK_64(0xD6D14AF9,0xC6329AB5),
	MK_64(0x6A9B0BFC,0x6EB67E0D),
	MK_64(0x9243C60D,0xCCFF1332),
	MK_64(0x1A1F1DDE,0x743F02D4),
	MK_64(0x0996753C,0x10ED0BB8),
	MK_64(0x6572DD22,0xF2B4969A),
	MK_64(0x61FD3062,0xD00A579A),
	MK_64(0x1DE0536E,0x8682E539)
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
	uint_t statebits;
	union
	{
		Skein_Ctxt_Hdr_t h;
		Skein_512_Ctxt_t ctx_512;
	} u;
} hashState;
int skein_hash(const BitSequence *data,BitSequence *hashval)
{
	hashState state;
	state.statebits = 512;
	(&state.u.ctx_512)->h.hashBitLen = 256;
	memcpy((&state.u.ctx_512)->X,SKEIN_512_IV_256,sizeof((&state.u.ctx_512)->X));
	Skein_Start_New_Type(&state.u.ctx_512,MSG);
	Skein_512_Ctxt_t *ctx=&state.u.ctx_512;
	const uint8_t *msg=data;
	//size_t msgByteCnt=200;
	size_t msgByteCnt=8;
	size_t n;
	//if(msgByteCnt + ctx->h.bCnt > SKEIN_512_BLOCK_BYTES)
	//printf("yes\n");
	//if(ctx->h.bCnt)
	//if(msgByteCnt > SKEIN_512_BLOCK_BYTES)
	//n = (199) / SKEIN_512_BLOCK_BYTES;
	n = 3;
	Skein_512_Process_Block(ctx,msg,n,SKEIN_512_BLOCK_BYTES);
	//msgByteCnt -= n * SKEIN_512_BLOCK_BYTES;
	//msgByteCnt -= 192;
	//msg        += n * SKEIN_512_BLOCK_BYTES;
	msg        += 192;
	//if(msgByteCnt)
	memcpy(&ctx->b[ctx->h.bCnt],msg,msgByteCnt);
	ctx->h.bCnt += msgByteCnt;

	//return Skein_512_Final(&state.u.ctx_512,hashval);
	//static int Skein_512_Final(Skein_512_Ctxt_t *ctx,uint8_t *hashVal)
	ctx=&state.u.ctx_512;
	uint8_t *hashVal=hashval;

	size_t i,byteCnt;
	u64b_t X[SKEIN_512_STATE_WORDS];
	ctx->h.T[1] |= SKEIN_T1_FLAG_FINAL;
	if(ctx->h.bCnt < SKEIN_512_BLOCK_BYTES)
		memset(&ctx->b[ctx->h.bCnt],0,SKEIN_512_BLOCK_BYTES - ctx->h.bCnt);
	Skein_512_Process_Block(ctx,ctx->b,1,ctx->h.bCnt);
	//byteCnt = 263 >> 3;
	//byteCnt = 32;
	memset(ctx->b,0,sizeof(ctx->b));
	//memcpy(X,ctx->X,sizeof(X));
	//for(i=0;i*SKEIN_512_BLOCK_BYTES < byteCnt;i++)
	//for(i=0;i*64 < byteCnt;i++)
	/*
	0*64 < 32
	1*64 < 32
	2*64 < 32
	3*64 < 32
	*/
	//for(i=0;i*64 < 32;i++)
	((u64b_t *)ctx->b)[0]= Skein_Swap64((u64b_t) 0);
	Skein_Start_New_Type(ctx,OUT_FINAL);
	Skein_512_Process_Block(ctx,ctx->b,1,sizeof(u64b_t));
	//n = byteCnt - 0*SKEIN_512_BLOCK_BYTES;
	//n = byteCnt - 0*64;
	//n = byteCnt;
	//if(n >= SKEIN_512_BLOCK_BYTES)
	//if(n >= 64)
	//		n  = 64;
			//n  = SKEIN_512_BLOCK_BYTES;
	//Skein_Put64_LSB_First(hashVal+0*SKEIN_512_BLOCK_BYTES,ctx->X,n);
	//Skein_Put64_LSB_First(hashVal+0*64,ctx->X,n);
	Skein_Put64_LSB_First(hashVal,ctx->X,32);
	//memcpy(ctx->X,X,sizeof(X));
	return 0;
}
