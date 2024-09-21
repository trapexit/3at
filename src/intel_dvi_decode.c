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

#include "intel_dvi_decode.h"
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
intel_dvi_decode(IntelDVIDecodeState *state_,
                 const u8            *input_data_,
                 const u32            sample_count_,
                 s16                 *output_data_)
{
  const u8 *inp;		/* Input buffer pointer */
  s16      *outp;		/* Output buffer pointer */
  int       sign;		/* Current adpcm sign bit */
  int       delta;		/* Current adpcm output value */
  int       step;		/* Stepsize */
  int       valpred;		/* Predicted value */
  int       vpdiff;		/* Current change to valpred */
  int       index;		/* Current step change index */
  int       inputbuffer;	/* place to keep next 4-bit value */
  int       bufferstep;		/* toggle between inputbuffer/input */

  outp = output_data_;
  inp  = input_data_;

  valpred = state_->prev_val;
  index = state_->index;
  step = g_intel_dvi_stepsize_table[index];

  bufferstep = 0;
    
  for (u32 numSamples = sample_count_ ; numSamples > 0 ; numSamples-- )
    {
      /* Step 1 - get the delta value */
      if(bufferstep)
        {
          delta = inputbuffer & 0xf;
        }
      else
        {
          inputbuffer = *inp++;
          delta = (inputbuffer >> 4) & 0xf;
        }
      bufferstep = !bufferstep;

      /* Step 2 - Find new index value (for later) */
      index += g_intel_dvi_index_table[delta];
      if(index < 0)
        index = 0;
      if(index > 88)
        index = 88;

      /* Step 3 - Separate sign and magnitude */
      sign  = delta & 8;
      delta = delta & 7;

      /* Step 4 - Compute difference and new predicted value */
      /*
      ** Computes 'vpdiff = (delta+0.5)*step/4', but see comment
      ** in adpcm_coder.
      */
      vpdiff = step >> 3;
      if(delta & 4)
        vpdiff += step;
      if(delta & 2)
        vpdiff += step>>1;
      if(delta & 1)
        vpdiff += step>>2;

      if(sign)
        valpred -= vpdiff;
      else
        valpred += vpdiff;

      /* Step 5 - clamp output value */
      if(valpred > 32767)
        valpred = 32767;
      else if (valpred < -32768)
        valpred = -32768;

      /* Step 6 - Update step value */
      step = g_intel_dvi_stepsize_table[index];

      /* Step 7 - Output value */
      *outp++ = valpred;
    }

  state_->prev_val = valpred;
  state_->index = index;
}
