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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#if defined(__ANDROID__)
	#include <byteswap.h>
#endif

#define RotL_64(x,N)    (((x) << (N)) | ((x) >> (64-(N))))

typedef struct
{
	uint64_t T[2];
} Skein_Ctxt_Hdr_t;
typedef struct
{
	Skein_Ctxt_Hdr_t h;
	uint64_t X[8];
	uint8_t  b[64];
} Skein_512_Ctxt_t;
#define SKEIN_MK_64(hi32,lo32)  ((lo32) + (((uint64_t) (hi32)) << 32))
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
const uint64_t SKEIN_512_IV_256[] =
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
#define ks              (kw+3)
#define ts              (kw)
static void Skein_512_Process_Block(Skein_512_Ctxt_t *ctx,const uint8_t *blkPtr,size_t blkCnt,size_t byteCntAdd)
{
	uint64_t kw[8+4];
	uint64_t X0,X1,X2,X3,X4,X5,X6,X7;
	uint64_t w [8];
	const uint64_t *Xptr[8];
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
		ks[8] = ks[0] ^ ks[1] ^ ks[2] ^ ks[3] ^ ks[4] ^ ks[5] ^ ks[6] ^ ks[7] ^ 2004413935125273122ull;
		ts[2] = ts[0] ^ ts[1];
		for(size_t n=0;n<64;n+=8)
			w[n/8] = (((uint64_t) blkPtr[n  ])      ) +
					 (((uint64_t) blkPtr[n+1]) <<  8) +
					 (((uint64_t) blkPtr[n+2]) << 16) +
					 (((uint64_t) blkPtr[n+3]) << 24) +
					 (((uint64_t) blkPtr[n+4]) << 32) +
					 (((uint64_t) blkPtr[n+5]) << 40) +
					 (((uint64_t) blkPtr[n+6]) << 48) +
					 (((uint64_t) blkPtr[n+7]) << 56) ;
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
		blkPtr += 64;
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
		ts[1] &= ~4611686018427387904ull;
	}
	while(--blkCnt);
	ctx->h.T[0] = ts[0];
	ctx->h.T[1] = ts[1];
}
int skein_hash(const uint8_t *data,uint8_t *hashval)
{
	Skein_512_Ctxt_t state;
	memcpy(state.X,SKEIN_512_IV_256,sizeof(state.X));
	state.h.T[0] = 0;
	state.h.T[1] = 8070450532247928832ull;
	Skein_512_Process_Block(&state,data,3,64);
	memcpy(&state.b[0],data+192,8);
	state.h.T[1] |= 9223372036854775808ull;
	memset(&state.b[8],0,56);
	Skein_512_Process_Block(&state,state.b,1,8);
	memset(state.b,0,sizeof(state.b));
	((uint64_t *)state.b)[0]= 0;
	state.h.T[0] = 0;
	state.h.T[1] = 18374686479671623680ull;
	Skein_512_Process_Block(&state,state.b,1,sizeof(uint64_t));
	hashval[0] = (uint8_t) (state.X[0] >> (0));
	hashval[1] = (uint8_t) (state.X[0] >> (8));
	hashval[2] = (uint8_t) (state.X[0] >> (16));
	hashval[3] = (uint8_t) (state.X[0] >> (24));
	hashval[4] = (uint8_t) (state.X[0] >> (32));
	hashval[5] = (uint8_t) (state.X[0] >> (40));
	hashval[6] = (uint8_t) (state.X[0] >> (48));
	hashval[7] = (uint8_t) (state.X[0] >> (56));
	hashval[8] = (uint8_t) (state.X[1] >> (0));
	hashval[9] = (uint8_t) (state.X[1] >> (8));
	hashval[10] = (uint8_t) (state.X[1] >> (16));
	hashval[11] = (uint8_t) (state.X[1] >> (24));
	hashval[12] = (uint8_t) (state.X[1] >> (32));
	hashval[13] = (uint8_t) (state.X[1] >> (40));
	hashval[14] = (uint8_t) (state.X[1] >> (48));
	hashval[15] = (uint8_t) (state.X[1] >> (56));
	hashval[16] = (uint8_t) (state.X[2] >> (0));
	hashval[17] = (uint8_t) (state.X[2] >> (8));
	hashval[18] = (uint8_t) (state.X[2] >> (16));
	hashval[19] = (uint8_t) (state.X[2] >> (24));
	hashval[20] = (uint8_t) (state.X[2] >> (32));
	hashval[21] = (uint8_t) (state.X[2] >> (40));
	hashval[22] = (uint8_t) (state.X[2] >> (48));
	hashval[23] = (uint8_t) (state.X[2] >> (56));
	hashval[24] = (uint8_t) (state.X[3] >> (0));
	hashval[25] = (uint8_t) (state.X[3] >> (8));
	hashval[26] = (uint8_t) (state.X[3] >> (16));
	hashval[27] = (uint8_t) (state.X[3] >> (24));
	hashval[28] = (uint8_t) (state.X[3] >> (32));
	hashval[29] = (uint8_t) (state.X[3] >> (40));
	hashval[30] = (uint8_t) (state.X[3] >> (48));
	hashval[31] = (uint8_t) (state.X[3] >> (56));
	return 0;
}
