#include "types_ints.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX(a,b) ((((a)<(b))?(b):(a)))
#define MIN(a,b) ((((a)<(b))?(a):(b)))
#define ABS(a) ((((a)<0))?-(a):(a))
#define dc(v) ((((s16)v)*(s16)(ABS(v)))<<1)

static
s16
decode(s32 curr_sample_,
       s32 prev_sample_) 
{
  if(curr_sample_ & 1) 
    return (prev_sample_ + dc(curr_sample_));
  else 
    return (dc(curr_sample_));
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

static
s8
set_exact_mode(const s8 v_)
{
  return (v & ~1);
}

/*********************************************************************
 **		Encode a s16 as a compressed byte
 **
 **		On error: does nothing  
 *********************************************************************/
static
s8
encode(s32 curr_sample_,
       s32 prev_sample_) 
{
  s8 exact;
  s8 delta;
  s32 temp;
	
  exact = helpEncode(curr_sample);
  exact = exact&~1;
  temp =  ABS(curr_sample-decode(exact,prev_sample));
  if (ABS(curr_sample-decode(exact+2,prev_sample)) < temp) exact+=2;
  else if (ABS(curr_sample-decode(exact-2,prev_sample)) < temp) exact-=2;

  if (ABS(curr_sample - prev_sample) > 32767) return exact;
	 
  delta = helpEncode(curr_sample-prev_sample);
  delta = delta|1;

  temp =  ABS(curr_sample - decode(delta,prev_sample));
	 
  /* check for wraparound on the delta case */
  if (temp > 30000) {
    // we overflowed 16 bits on this delta.
    // Pull it closer to the center
    delta = ((delta<0)?(delta+2):(delta-2));
    temp =  ABS(curr_sample - decode(delta,prev_sample));
  }
	 	
  if (ABS(curr_sample - decode(delta+2,prev_sample)) < temp) delta+=2;
  else if (ABS(curr_sample - decode(delta - 2,prev_sample)) < temp) delta-=2;
  if (ABS(curr_sample - decode(exact,prev_sample)) < ABS(curr_sample - decode(delta,prev_sample))) return exact;
  else return delta;
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
  
  for (i = 0; i < ibuf_len_; ++i)
    {
      curr_sample = ibuf_[i];

      if(i)
        {
          comp_sample = encode((s32)curr_sample,(s32)prev_sample);
        }
      else 
        {
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
sdx2_encode_stereo(const s16 *ibuf_,
                   const u32  ibuf_len_,
                   s8        *obuf_)
{
  s32 i;
  s8  comp_sample       = 0;
  s16 curr_left_sample  = 0;
  s16 prev_left_sample  = 0;
  s16 curr_right_sample = 0;
  s16 prev_right_sample = 0;
	
  for (i = 0; i < ibuf_len_; i += 2) 
    {
      curr_left_sample = ibuf_[i+0];
		
      if(i)
        {
          comp_sample = encode((s32)curr_left_sample,(s32)prev_left_sample);
        }
      else 
        {
          comp_sample = helpEncode((s32)curr_left_sample);
          comp_sample &= ~1;	
        }

      obuf_[i+0] = comp_sample;

      prev_left_sample = decode((s32)comp_sample,(s32)prev_left_sample);
		
      curr_right_sample = ibuf_[i+1];

      if(i)
        {
          comp_sample = encode((s32)curr_right_sample,(s32)prev_right_sample);
        }
      else 
        {
          comp_sample = helpEncode((s32)curr_right_sample);
          comp_sample &= ~1;	
        }

      obuf_[i+1] = comp_sample;
		
      prev_right_sample = decode((s32)comp_sample,(s32)prev_right_sample);
    }
			
  return 0;
}

s32
sdx2_encode(const s16 *ibuf_,
            const u32  ibuf_len_,
            const u8   num_channels_,            
            s8        *obuf_)
{
  switch(num_channels_)
    {
    case 1:
      return sdx2_encode_mono(ibuf_,ibuf_len_,obuf_);
    case 2:
      return sdx2_encode_stereo(ibuf_,ibuf_len_,obuf_);
    default:
      break;
    }

  return -1;
}


