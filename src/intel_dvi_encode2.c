/*
  This code is based on two sources:

  1) https://github.com/Kalmalyzer/adpcm-68k

  2) The public domain examples found in "Recommended Practices for
  Enhancing Digital Audio Compatibility in Multimedia Systems" by the
  IMA Digital Audio Focus and Technical Working Groups Revision 3.00
  from 1992-10-21. Specifically Appendix D: Reference Algorithms
  section 6 "ADPCM Reference Algorithms" on page 28.

  ========================================================================
  
  MIT License

  Copyright (c) 2023 Mikael Kalms

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation files
  (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software,
  and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  
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
#include <cstdint>
#include <stdint.h>

#define INDEX_TABLE_SIZE 16
#define STEPSIZE_TABLE_SIZE 89

static
const
s8
indexTable[INDEX_TABLE_SIZE] =
  {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
  };

static
const
u32
stepsizeTable[STEPSIZE_TABLE_SIZE] = 
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

typedef struct state_t state_t;
struct state_t
{
  int predictedSample;
  int index;
  int stepsize;
};


static
s16
clip_s16(const s64 v_,
         const s16 l_,
         const s16 h_)
{
  if(v_ < l_)
    return l_;
  if(v_ > h_)
    return h_;
  return v_;
}
         

static
s8
_encode_delta(int step_,
              int delta_)
{
  s8 sample;

  sample = 0;
  if(delta_ < 0)
    {
      sample = 8;
      delta_ = -delta_;
    }

  if(delta_ >= step_)
    {
      sample |= 4;
      delta_ += step_;
    }

  step_ >>= 1;
  if(delta_ >= step_)
    {
      sample |= 2;
      delta_ -= step_;
    }

  step_ >>= 1;
  if(delta_ >= step_)
    {
      sample |= 1;
    }

  return sample;
}

static
int
_decode_delta(int step_,
              s8  sample_)
{
  int delta;

  delta = 0;
    
  if(sample_ & 4)
    delta = step_;
  if(sample_ & 2)
    delta += (step_ >> 1);
  if(sample_ & 1)
    delta += (step_ >> 2);
  delta += (step_ >> 3);
  if(sample_ & 8)
    delta = -delta;
    
  return delta;
}

static
int
_encode_sample(struct state_t *s_,
               const s16       input_sample_)
{
  int difference;
  int originalSample;
  int newSample;
  int tempStepsize;
  int i;
  int mask;

  originalSample = input_sample_;

  /* find difference from predicted sample: */
  difference = originalSample - s_->predictedSample;
  if(difference >= 0) /* set sign bit and find absolute value of difference */
    {
      newSample = 0; /* set sign bit(newSample[3]) to 0 */
    }
  else
    {
      newSample = 8; /*set sign bit(newSample[3]) to one */
      difference = -difference; /* absolute value of negative difference */
    }

  mask = 4; /* used to set bits in newSample*/
  tempStepsize = s_->stepsize; /* store quantizer stepsize for later use */
  for(i = 0; i < 3; i++) /* quantize difference down to four bits */
    {
      if(difference >= tempStepsize)
        { /* newSample[2:0] = 4 * (difference/stepsize) */
          newSample |= mask; /* perform division ... */
          difference -= tempStepsize; /* ... through repeated subtraction */
        }
      tempStepsize >>=1; /* adjust comparator for next iteration */
      mask >>=1; /* adjust bit-set mask for next iteration */
    }

  /* compute new sample estimate s_->predictedSample */
  /* calculate difference = (newSample + ½) * stepsize/4 */
  /* perform multiplication through repetitive addition */
  difference = 0; 
  if(newSample & 4) 
    difference += s_->stepsize;
  if(newSample & 2)
    difference += s_->stepsize >> 1;
  if(newSample & 1)
    difference += s_->stepsize >> 2;
  difference += s_->stepsize >> 3;
  /* (newSample + ½) * stepsize/4 = newSample * stepsize/4 + stepsize/8 */
  if (newSample & 8) /* account for sign bit */
    difference = -difference;
  /* adjust predicted sample based on calculated difference: */
  s_->predictedSample += difference;
  if(s_->predictedSample > 32767) /* check for overflow */
    s_->predictedSample = 32767;
  else if(s_->predictedSample < -32768)
    s_->predictedSample = -32768;
  /* compute new stepsize */
  /* adjust s_->index into stepsize lookup table using newSample */
  s_->index += indexTable[newSample];
  if(s_->index < 0) /* check for index underflow */
    s_->index = 0;
  else if(s_->index >= STEPSIZE_TABLE_SIZE) /* check for index overflow */
    s_->index = (STEPSIZE_TABLE_SIZE - 1);
  /* find new quantizer stepsize */  
  s_->stepsize = stepsizeTable[s_->index]; 

  return newSample;
}

void
intel_dvi_encode2(IntelDVIEncodeState *state_,
                  const s16           *input_data_,
                  const u32            sample_count_,
                  u8                  *output_data_)
{
  int i;
  u8 output;
  state_t s;
  int step;

  s.predictedSample = 0;
  s.index = 0;
  s.stepsize = 7;

  step = 1;
  for(i = 0; i < sample_count_; i++)
    {
      int newSample;

      newSample = _encode_sample(&s,input_data_[i]);
      if(step)
        output = (output << 4);
      else
        *output_data_++ = (newSample | output);

      step = !step;
    }

  if(!step)
    *output_data_++ = output;
}
