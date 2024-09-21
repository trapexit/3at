/*
  This code is based on the public domain examples found in
  "Recommended Practices for Enhancing Digital Audio Compatibility in
  Multimedia Systems" by the IMA Digital Audio Focus and Technical
  Working Groups Revision 3.00 from 1992-10-21. Specifically Appendix
  D: Reference Algorithms section 6 "ADPCM Reference Algorithms" on
  page 28.

  ========================================================================

  ISC License

  Copyright (c) 2024, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "intel_dvi_encode.h"
#include "types_ints.h"

static
const
s8
g_intel_dvi_index_table[] =
  {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
  };

static
const
u32
g_intel_dvi_stepsize_table[] =
  {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
  };

void
intel_dvi_encode(IntelDVIEncodeState *state_,
                 const s16           *input_data_,
                 const u32            sample_count_,
                 u8                  *output_data_)
{
  const s16 *inp;		/* Input buffer pointer */
  u8        *outp;              /* Output buffer pointer */
  int        val;		/* Current input sample value */
  int        sign;		/* Current adpcm sign bit */
  int        delta;		/* Current adpcm output value */
  int        diff;		/* Difference between val and valprev */
  int        step;		/* Stepsize */
  int        valpred;           /* Predicted output value */
  int        vpdiff;		/* Current change to valpred */
  int        index;		/* Current step change index */
  int        outputbuffer;	/* place to keep previous 4-bit value */
  int        bufferstep;	/* toggle between outputbuffer/output */

  outp = output_data_;
  inp  = input_data_;

  valpred = state_->prev_val;
  index   = state_->index;
  step    = g_intel_dvi_stepsize_table[index];
    
  bufferstep = 1;

  for(int numSamples = sample_count_; numSamples > 0; numSamples--)
    {
      val = *inp++;

      /* Step 1 - compute difference with previous value */
      diff = val - valpred;
      sign = (diff < 0) ? 8 : 0;
      if ( sign ) diff = (-diff);

      /* Step 2 - Divide and clamp */
      /* Note:
      ** This code *approximately* computes:
      **    delta = diff*4/step;
      **    vpdiff = (delta+0.5)*step/4;
      ** but in shift step bits are dropped. The net result of this is
      ** that even if you have fast mul/div hardware you cannot put it to
      ** good use since the fixup would be too expensive.
      */
      delta  = 0;
      vpdiff = (step >> 3);
	
      if(diff >= step)
        {
          delta = 4;
          diff -= step;
          vpdiff += step;
        }
    
      step >>= 1;
      if(diff >= step)
        {
          delta |= 2;
          diff -= step;
          vpdiff += step;
        }
    
      step >>= 1;
      if(diff >= step)
        {
          delta |= 1;
          vpdiff += step;
        }

      /* Step 3 - Update previous value */
      if(sign)
        valpred -= vpdiff;
      else
        valpred += vpdiff;

      /* Step 4 - Clamp previous value to 16 bits */
      if(valpred > 32767)
        valpred = 32767;
      else if (valpred < -32768)
        valpred = -32768;

      /* Step 5 - Assemble value, update index and step values */
      delta |= sign;
	
      index += g_intel_dvi_index_table[delta];
      if(index < 0)
        index = 0;
      if(index > 88)
        index = 88;
      step = g_intel_dvi_stepsize_table[index];

      /* Step 6 - Output value */
      if(bufferstep)
        {
          outputbuffer = (delta << 4) & 0xf0;
        }
      else
        {
          *outp++ = (delta & 0x0f) | outputbuffer;
        }
      bufferstep = !bufferstep;
    }

  /* Output last step, if needed */
  if(!bufferstep)
    *outp++ = outputbuffer;
    
  state_->prev_val = valpred;
  state_->index    = index;  
}
