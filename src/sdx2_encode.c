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
static s8 helpEncode( s32 curr_sample);
static s8 encode( s32 curr_sample, s32 prev_sample);
static s16 decode( s32 curr_sample, s32 prev_sample);

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
decode(s32 curr_sample,
       s32 prev_sample) 
{
  if(curr_sample & 1) 
    return (prev_sample + dc(curr_sample));
  else 
    return (dc(curr_sample));
}

/*********************************************************************
 **		Take Sqrt of a s16 and return a s8
 **
 **		On error: does nothing  
 *********************************************************************/
static
s8
helpEncode(s32 curr_sample)
{
  s32 neg;
  s8 EncSamp;

  neg = 0;
  if (curr_sample < 0)
    {
      neg = 1;
      curr_sample = -curr_sample;
    }			
		          
  EncSamp =  sqrt((float)(curr_sample>>1));
  return (neg?-EncSamp:EncSamp);
}

/*********************************************************************
 **		Encode a s16 as a compressed byte
 **
 **		On error: does nothing  
 *********************************************************************/
static
s8
encode(s32 curr_sample,
       s32 prev_sample) 
{
  s8 Exact,Delta;
  s32 temp;
	
  Exact = helpEncode(curr_sample);
  Exact = Exact&~1;
  temp =  ABS(curr_sample-decode(Exact,prev_sample));
  if (ABS(curr_sample-decode(Exact+2,prev_sample)) < temp) Exact+=2;
  else if (ABS(curr_sample-decode(Exact-2,prev_sample)) < temp) Exact-=2;

  if (ABS(curr_sample - prev_sample) > 32767) return Exact;
	 
  Delta = helpEncode(curr_sample-prev_sample);
  Delta = Delta|1;

  temp =  ABS(curr_sample - decode(Delta,prev_sample));
	 
  /* check for wraparound on the delta case */
  if (temp > 30000) {
    // we overflowed 16 bits on this delta.
    // Pull it closer to the center
    Delta = ((Delta<0)?(Delta+2):(Delta-2));
    temp =  ABS(curr_sample - decode(Delta,prev_sample));
  }
	 	
  if (ABS(curr_sample - decode(Delta+2,prev_sample)) < temp) Delta+=2;
  else if (ABS(curr_sample - decode(Delta - 2,prev_sample)) < temp) Delta-=2;
  if (ABS(curr_sample - decode(Exact,prev_sample)) < ABS(curr_sample - decode(Delta,prev_sample))) return Exact;
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
  s32 i;
  s16 curr_sample = 0;
  s8  comp_sample = 0;
  s16 prev_sample = 0;
  
  
  for (i = 0; ix < ibuf_len_; ++i)
    {
      curr_sample = ibuf_[i];

      if(ix)
        {
          comp_sample = encode((s32)curr_sample,(s32)prev_sample);
        }
      else 
        {
          /* force literal first time */
          comp_sample = helpEncode((s32)curr_sample);
          comp_sample &= ~1;	
        }

      obuf_[i] = comp_sample;
		
      prev_sample = (s32)decode((s32)comp_sample,(s32)prev_sample);
    }

  return 0;
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
  s8    comp_sample     = 0;
  s16 	CurLeftSamp  = 0;
  s16 	CurRightSamp = 0;
  s32	err;
	
  printf("\n  Encoding stereo block...\n");
  fflush(stdout);

  for (ix = 0; ix < ctx->framesInBuffer; ++ix) 
    {
      /* Process Left Sample */
      CurLeftSamp = *inBufferPtr++;
		
      if(ix)
        {
          comp_sample = encode((s32)CurLeftSamp,(s32)ctx->prevLeftSamp);
        }
      else 
        {	/* force literal first time */
          comp_sample = helpEncode((s32)CurLeftSamp);
          comp_sample &= ~1;	
        }
			
      *outBufferPtr++ = comp_sample;

      err = decode((s32)comp_sample,(s32)ctx->prevLeftSamp);
      err = ABS((s32)CurLeftSamp-err);
      if (err > ctx->maxLeftErr) 
        {
          ctx->maxLeftErr = err;
          printf("   (New Max Err (Left) = %ld...  Curr:%d, Prev:%d, Comp:%d, Deco:%d) \n", ctx->maxLeftErr, 
                 CurLeftSamp, ctx->prevLeftSamp, comp_sample,decode((s32)comp_sample,(s32)ctx->prevLeftSamp));
        }
      /* debug printf for looking at each sample */
      if (0) 
        printf("   DEBUG-- Comp:%d = Curr:%d, Prev:%d .. Deco:%d) \n",comp_sample,
               CurLeftSamp,ctx->prevLeftSamp,decode((s32)comp_sample,(s32)ctx->prevLeftSamp));
		
      ctx->avgLeftErr += err;
      fflush(stdout);

      ctx->prevLeftSamp = decode((s32)comp_sample,(s32)ctx->prevLeftSamp);
		

      /* Process Right Sample */
      CurRightSamp = *inBufferPtr++;

      if (ix) 
        comp_sample = encode((s32)CurRightSamp,(s32)ctx->prevRightSamp);
      else 
        {	/* force literal first time */
          comp_sample = helpEncode((s32)CurRightSamp);
          comp_sample &= ~1;	
        }
		
      *outBufferPtr++ = comp_sample;
		
      if (1)
        {
          err = decode((s32)comp_sample,(s32)ctx->prevRightSamp);
          err = ABS((s32)CurRightSamp-err);
          if (err > ctx->maxRightErr) 
            {
              ctx->maxRightErr = err;
              printf("   (New Max Err (Right) = %ld...  Curr:%d, Prev:%d, Comp:%d, Deco:%d) \n", ctx->maxRightErr, 
                     CurRightSamp, ctx->prevRightSamp, comp_sample,decode((s32)comp_sample,(s32)ctx->prevRightSamp));
            }
          /* debug printf for looking at each sample */
          if (0) 
            printf("   DEBUG-- Comp:%d = Curr:%d, Prev:%d .. Deco:%d) \n",comp_sample,
                   CurRightSamp,ctx->prevRightSamp,decode((s32)comp_sample,(s32)ctx->prevRightSamp));
			
          ctx->avgRightErr += err;
          fflush(stdout);
        }

      ctx->prevRightSamp = decode((s32)comp_sample,(s32)ctx->prevRightSamp);
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


