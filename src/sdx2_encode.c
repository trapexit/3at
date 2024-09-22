#include "types_ints.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct SDX2ContextBlk
{
  s16* inBufferPtr;
  char*	 outBufferPtr;
  s32	 bytesPerFrame;
  s32	 framesInBuffer;
  s32	 bytesInSample;
  s16	 prevLeftSamp;
  s16	 prevRightSamp;
  s16	 prevMonoSamp;
  s32 	 avgLeftErr;
  s32	 maxLeftErr;
  s32	 avgRightErr;
  s32	 maxRightErr;
  s32	 avgMonoErr;
  s32	 maxMonoErr;
} SDX2ContextBlk, *SDX2ContextBlkPtr;

/*********************************************************************
 ** Buffersize to use when encoding.
 ********************************************************************/
#define BUFFERSIZE	(256*1024)

/*********************************************************************
 ** 		Local ANSI function prototypes                                
 *********************************************************************/
static s8 helpEncode( s32 CurSamp);
static s8 encode( s32 CurSamp, s32 PrevSamp);
static s16 decode( s32 CurSamp, s32 PrevSamp);

/*********************************************************************
 ** 		Macros                                
 *********************************************************************/
#define MAX(a,b) ((((a)<(b))?(b):(a)))
#define MIN(a,b) ((((a)<(b))?(a):(b)))
#define ABS(a) ((((a)<0))?-(a):(a))
#define dc(v) ((((s16)v)*(s16)(ABS(v)))<<1)

/*********************************************************************
 **		Decode a sample
 **
 **		On error: does nothing  
 *********************************************************************/
static
s16
decode(s32 CurSamp,
       s32 PrevSamp) 
{
  if(CurSamp & 1) 
    return (PrevSamp + dc(CurSamp));
  else 
    return (dc(CurSamp));
}

/*********************************************************************
 **		Take Sqrt of a s16 and return a s8
 **
 **		On error: does nothing  
 *********************************************************************/
static
s8
helpEncode(s32 CurSamp)
{
  s32 neg;
  s8 EncSamp;

  neg = 0;
  if (CurSamp < 0)
    {
      neg = 1;
      CurSamp = -CurSamp;
    }			
		          
  EncSamp =  sqrt((float)(CurSamp>>1));
  return (neg?-EncSamp:EncSamp);
}

/*********************************************************************
 **		Encode a s16 as a compressed byte
 **
 **		On error: does nothing  
 *********************************************************************/
static s8 encode(s32 CurSamp, s32 PrevSamp) 
{
  s8 Exact,Delta;
  s32 temp;
	
  Exact = helpEncode(CurSamp);
  Exact = Exact&~1;
  temp =  ABS(CurSamp-decode(Exact,PrevSamp));
  if (ABS(CurSamp-decode(Exact+2,PrevSamp)) < temp) Exact+=2;
  else if (ABS(CurSamp-decode(Exact-2,PrevSamp)) < temp) Exact-=2;

  if (ABS(CurSamp - PrevSamp) > 32767) return Exact;
	 
  Delta = helpEncode(CurSamp-PrevSamp);
  Delta = Delta|1;

  temp =  ABS(CurSamp - decode(Delta,PrevSamp));
	 
  /* check for wraparound on the delta case */
  if (temp > 30000) {
    // we overflowed 16 bits on this delta.
    // Pull it closer to the center
    Delta = ((Delta<0)?(Delta+2):(Delta-2));
    temp =  ABS(CurSamp - decode(Delta,PrevSamp));
  }
	 	
  if (ABS(CurSamp - decode(Delta+2,PrevSamp)) < temp) Delta+=2;
  else if (ABS(CurSamp - decode(Delta - 2,PrevSamp)) < temp) Delta-=2;
  if (ABS(CurSamp - decode(Exact,PrevSamp)) < ABS(CurSamp - decode(Delta,PrevSamp))) return Exact;
  else return Delta;
}

/*********************************************************************
 **		Encodes a block of sample data.
 **		If verbose mode is on, prints stats on encoding
 **
 **		On error: returns failure code  
 *********************************************************************/
static
s32
sdx2_encode_mono(const s16 *ibuf_,
                 const u32  ibuf_len_,
                 s8        *obuf_)	
{
  s16*	inBufferPtr  = ibuf_;
  char*	outBufferPtr = obuf_;
  s32 	Result       = 0;
  s32	ix;
  s32	CurErr       = 0;
  s16 	curr_sample      = 0;
  s8    comp_sample     = 0;
  s16   prev_sample = 0;
	
  printf("\n  Encoding mono block...\n");
  fflush(stdout);
		
  for (ix = 0; ix < ibuf_len_; ++ix)   /* %Q (++ix) why did stever pre-increment counter */
    {
      curr_sample = *inBufferPtr++;

      if(ix)
        {
          comp_sample = encode((s32)curr_sample,(s32)prev_sample);
        }
      else 
        {	/* force literal first time */
          CompSamp = helpEncode((s32)CurSamp);
          CompSamp &= ~1;	
        }
		
      *outBufferPtr++ = CompSamp;
		
      CurErr = decode((s32)CompSamp,(s32)ctx->prevMonoSamp);
      CurErr = ABS((s32)CurSamp-CurErr);
      if (CurErr > ctx->maxMonoErr)
        {
          ctx->maxMonoErr = CurErr;
          printf("   (New Max Err = %ld...  Curr:%d, Prev:%d, Comp:%d, Deco:%d) \n", ctx->maxMonoErr, 
                 CurSamp, ctx->prevMonoSamp, CompSamp,decode((s32)CompSamp,(s32)ctx->prevMonoSamp));
        }
      /* debug printf for looking at each sample */
      if (0) 
        printf("   DEBUG-- Comp:%d = Curr:%d, Prev:%d .. Deco:%d) \n",CompSamp,
               CurSamp,ctx->prevMonoSamp,decode((s32)CompSamp,(s32)ctx->prevMonoSamp));
			
      ctx->avgMonoErr += CurErr;
      fflush(stdout);

      ctx->prevMonoSamp = (s32) decode((s32)CompSamp,(s32)ctx->prevMonoSamp);
    }

 error:
  return Result;
	
}
/*********************************************************************
 **		Loops through SSND chunk grabbing 16bits encoding them as 8 and
 **		writing them back out to new SSND chunk in outfile.
 **		Prints stats on encoding
 **
 **		On error: returns failure code  
 *********************************************************************/
static
s32
sdx2_encode_stereo(SDX2ContextBlkPtr ctx)
{
  s32 	Result       = 0;
  s32   ix;
  s16*	inBufferPtr  = ctx->inBufferPtr;
  char*	outBufferPtr = ctx->outBufferPtr;
  s8    CompSamp     = 0;
  s16 	CurLeftSamp  = 0;
  s16 	CurRightSamp = 0;
  s32	CurErr;
	
  printf("\n  Encoding stereo block...\n");
  fflush(stdout);

  for (ix = 0; ix < ctx->framesInBuffer; ++ix) 
    {
      /* Process Left Sample */
      CurLeftSamp = *inBufferPtr++;
		
      if(ix)
        {
          CompSamp = encode((s32)CurLeftSamp,(s32)ctx->prevLeftSamp);
        }
      else 
        {	/* force literal first time */
          CompSamp = helpEncode((s32)CurLeftSamp);
          CompSamp &= ~1;	
        }
			
      *outBufferPtr++ = CompSamp;

      CurErr = decode((s32)CompSamp,(s32)ctx->prevLeftSamp);
      CurErr = ABS((s32)CurLeftSamp-CurErr);
      if (CurErr > ctx->maxLeftErr) 
        {
          ctx->maxLeftErr = CurErr;
          printf("   (New Max Err (Left) = %ld...  Curr:%d, Prev:%d, Comp:%d, Deco:%d) \n", ctx->maxLeftErr, 
                 CurLeftSamp, ctx->prevLeftSamp, CompSamp,decode((s32)CompSamp,(s32)ctx->prevLeftSamp));
        }
      /* debug printf for looking at each sample */
      if (0) 
        printf("   DEBUG-- Comp:%d = Curr:%d, Prev:%d .. Deco:%d) \n",CompSamp,
               CurLeftSamp,ctx->prevLeftSamp,decode((s32)CompSamp,(s32)ctx->prevLeftSamp));
		
      ctx->avgLeftErr += CurErr;
      fflush(stdout);

      ctx->prevLeftSamp = decode((s32)CompSamp,(s32)ctx->prevLeftSamp);
		

      /* Process Right Sample */
      CurRightSamp = *inBufferPtr++;

      if (ix) 
        CompSamp = encode((s32)CurRightSamp,(s32)ctx->prevRightSamp);
      else 
        {	/* force literal first time */
          CompSamp = helpEncode((s32)CurRightSamp);
          CompSamp &= ~1;	
        }
		
      *outBufferPtr++ = CompSamp;
		
      if (1)
        {
          CurErr = decode((s32)CompSamp,(s32)ctx->prevRightSamp);
          CurErr = ABS((s32)CurRightSamp-CurErr);
          if (CurErr > ctx->maxRightErr) 
            {
              ctx->maxRightErr = CurErr;
              printf("   (New Max Err (Right) = %ld...  Curr:%d, Prev:%d, Comp:%d, Deco:%d) \n", ctx->maxRightErr, 
                     CurRightSamp, ctx->prevRightSamp, CompSamp,decode((s32)CompSamp,(s32)ctx->prevRightSamp));
            }
          /* debug printf for looking at each sample */
          if (0) 
            printf("   DEBUG-- Comp:%d = Curr:%d, Prev:%d .. Deco:%d) \n",CompSamp,
                   CurRightSamp,ctx->prevRightSamp,decode((s32)CompSamp,(s32)ctx->prevRightSamp));
			
          ctx->avgRightErr += CurErr;
          fflush(stdout);
        }

      ctx->prevRightSamp = decode((s32)CompSamp,(s32)ctx->prevRightSamp);
    }
			
 error:
  return Result;
	
}

s32
sdx2_encode(s16 *in_buf,
            u32  in_buf_len,
            s8  *out_buf,
            u8 num_channels)
{
  SDX2ContextBlk ctx = {0};

  ctx.inBufferPtr = in_buf;
  ctx.outBufferPtr = out_buf;
  ctx.bytesPerFrame = 2 * num_channels;
  ctx.bytesInSample = in_buf_len;
  
  switch(num_channels)
    {
    case 1:
      return sdx2_encode_mono(&ctx);
    case 2:
      return sdx2_encode_stereo(&ctx);
    default:
      break;
    }

  return -1;
}


