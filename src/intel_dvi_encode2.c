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
#include <stdint.h>

#define INDEX_TABLE_SIZE 16
#define STEPSIZE_TABLE_SIZE 89
#define STEPSIZE_TABLE_MAX (STEPSIZE_TABLE_SIZE - 1)

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
int
clip_int(const s64 v_,
         const int l_,
         const int h_)
{
  if(v_ < l_)
    return l_;
  if(v_ > h_)
    return h_;
  return v_;
}

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
u8
_encode_delta(s32 step_,
              s32 delta_)
{
  u8 sample;

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
              u8  sample_)
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
u8
_encode_sample(struct state_t *s_,
               const s16       input_sample_)
{
  s32 delta;
  u8  encoded_sample;
  
  delta = (input_sample_ - s_->predictedSample);
  delta = clip_s16(delta,-32768,32767);

  encoded_sample = _encode_delta(s_->stepsize,delta);

  s_->predictedSample += _decode_delta(s_->stepsize,encoded_sample);
  s_->predictedSample = clip_s16(s_->predictedSample,-32768,32767);

  s_->index += indexTable[encoded_sample];
  s_->index = clip_int(s_->index,0,STEPSIZE_TABLE_MAX);
  s_->stepsize = stepsizeTable[s_->index]; 

  return encoded_sample;
}

long lastEstimateL, stepSizeL, stepIndexL;
long lastEstimateR, stepSizeR, stepIndexR;

u8
ADDVIEncode(short shortOne,
            short shortTwo,
            long channels)
{
  int delta;
  u8 encodedSample;
  u8 outputByte;

  outputByte = 0;
    
  /* First sample or left sample to be packed in first nibble */
  /* calculate delta */
  delta = shortOne - lastEstimateL;
  delta = clip_int(delta, -32768L, 32767L);

  /* encode delta relative to the current stepsize */
  encodedSample = _encode_delta(stepSizeL, delta);

  /* pack first nibble */
  outputByte = 0x00F0 & (encodedSample<<4);

  /* decode ADPCM code value to reproduce delta and generate an estimated InputSample */
  lastEstimateL += _decode_delta(stepSizeL, encodedSample);
  lastEstimateL = clip_int(lastEstimateL, -32768L, 32767L);

  /* adapt stepsize */
  stepIndexL += indexTable[encodedSample];
  stepIndexL = clip_int(stepIndexL, 0, 88);
  stepSizeL = stepsizeTable[stepIndexL];
    
  if(channels == 2L)
    {
      /* calculate delta for second sample */
      delta = shortTwo - lastEstimateR;
      delta = clip_s16(delta, -32768L, 32767L);

      /* encode delta relative to the current stepsize */
      encodedSample = _encode_delta(stepSizeR, delta);

      /* pack second nibble */
      outputByte |= 0x000F & encodedSample;

      /* decode ADPCM code value to reproduce delta and generate an estimated InputSample */
      lastEstimateR += _decode_delta(stepSizeR, encodedSample);
      lastEstimateR = clip_int(lastEstimateR, -32768L, 32767L);

      /* adapt stepsize */
      stepIndexR += indexTable[encodedSample];
      stepIndexR = clip_int(stepIndexR, 0, 88);      
      stepSizeR = stepsizeTable[stepIndexR];
    }
  else
    {
      /* calculate delta for second sample */
      delta = shortTwo - lastEstimateL;
      delta = clip_s16delta, -32768L, 32767L);

      /* encode delta relative to the current stepsize */
      encodedSample = EncodeDelta(stepSizeL, delta);

      /* pack second nibble */
      outputByte |= 0x000F & encodedSample;

      /* decode ADPCM code value to reproduce delta and generate an estimated InputSample */
      lastEstimateL += DecodeDelta(stepSizeL, encodedSample);
      CLIP(lastEstimateL, -32768L, 32767L);

      /* adapt stepsize */
      stepIndexL += gIndexDeltas[encodedSample];
      CLIP(stepIndexL, 0, 88);
      stepSizeL = gStepSizes[stepIndexL];
    }
  return(outputByte);
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
      u8 newSample;

      newSample = _encode_sample(&s,input_data_[i]);
      if(step)
        output = (output << 4);
      else
        *output_data_++ = (output | newSample);

      step = !step;
    }

  if(!step)
    *output_data_++ = output;
}
