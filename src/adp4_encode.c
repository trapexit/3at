/*
  This code is based on the public domain examples found in
  "Recommended Practices for Enhancing Digital Audio Compatibility in
  Multimedia Systems" by the IMA Digital Audio Focus and Technical
  Working Groups Revision 3.00 from 1992-10-21. Specifically Appendix
  D: Reference Algorithms section 6 "ADPCM Reference Algorithms" on
  page 28. See docs/IMA_ADPCM.pdf

  ========================================================================

  Other implementations:
    * https://github.com/XProger/OpenLara/blob/master/src/platform/gba/packer/IMA.h
    * https://github.com/Kalmalyzer/adpcm-68k/blob/master/Codec/codec.cpp

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

#include "types_ints.h"

#define INDEX_TABLE_SIZE 16
#define STEPSIZE_TABLE_SIZE 89
#define STEPSIZE_TABLE_MAX (STEPSIZE_TABLE_SIZE - 1)

static
const
s8
g_INDEX_TABLE[INDEX_TABLE_SIZE] =
  {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
  };

static
const
u16
g_STEPSIZE_TABLE[STEPSIZE_TABLE_SIZE] = 
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

typedef struct adp4_state_t adp4_state_t;
struct adp4_state_t
{
  s32 predicted_sample;
  s32 index;
  s32 stepsize;
};

static
s32
_clamp_s32(const s64 v_,
           const s32 l_,
           const s32 h_)
{
  if(v_ < l_)
    return l_;
  if(v_ > h_)
    return h_;
  return v_;
}

static
u8
_adp4_quantize_difference(const s32 stepsize_,
                          const s32 difference_,
                          const u8  sample_)
{
  int i;  
  u8 mask;
  u8 sample;
  s32 stepsize;
  s32 difference;
  
  sample     = sample_;
  stepsize   = stepsize_;
  difference = difference_;
  mask       = 0x04;
  for(i = 0; i < 3; i++)
    {
      if(difference >= stepsize)
        {
          sample |= mask;
          difference -= stepsize_;
        }

      stepsize >>= 1;
      mask     >>= 1;
    }

  return sample;
}

static
u8
_adp4_encode_difference(const s32 stepsize_,
                        const s32 difference_)
{
  u8 sample;
  s32 difference;

  sample = 0;
  difference = difference_;
  if(difference < 0)
    {
      sample = 0x08;
      difference = -difference;
    }

  return _adp4_quantize_difference(stepsize_,difference,sample);
}

static
s32
_adp4_decode_difference(const s32 stepsize_,
                        const u8  sample_)
{
  s32 difference;

  difference = 0;
    
  if(sample_ & 0x4)
    difference += stepsize_;
  if(sample_ & 0x2)
    difference += (stepsize_ >> 1);
  if(sample_ & 0x1)
    difference += (stepsize_ >> 2);
  difference += (stepsize_ >> 3);
  if(sample_ & 0x8)
    difference = -difference;
    
  return difference;
}

static
u8
_adp4_encode_sample(adp4_state_t *s_,
                    const s16     orig_sample_)
{
  s32 difference;
  u8  encoded_sample;
  
  difference = (orig_sample_ - s_->predicted_sample);
  difference = _clamp_s32(difference,-32768,32767);

  encoded_sample = _adp4_encode_difference(s_->stepsize,difference);

  s_->predicted_sample += _adp4_decode_difference(s_->stepsize,encoded_sample);
  s_->predicted_sample = _clamp_s32(s_->predicted_sample,-32768,32767);

  s_->index += g_INDEX_TABLE[encoded_sample];
  s_->index = _clamp_s32(s_->index,0,STEPSIZE_TABLE_MAX);
  s_->stepsize = g_STEPSIZE_TABLE[s_->index]; 

  return encoded_sample;
}

void
adp4_encode(const s16 *input_data_,
            const u32  sample_count_,
            u8        *output_data_)
{
  u32 i_idx;
  u32 o_idx;
  int shift;
  adp4_state_t s;
  u8 output_byte;

  s.index = 0;
  s.predicted_sample = 0;
  s.stepsize = 7;

  shift = 1;
  for(i_idx = 0, o_idx = 0; i_idx < sample_count_; i_idx++)
    {
      u8 adp4_sample;

      adp4_sample = _adp4_encode_sample(&s,input_data_[i_idx]);
      if(shift)
        output_byte = (adp4_sample << 4);
      else
        output_data_[o_idx++] = (output_byte | adp4_sample);

      shift = !shift;
    }

  if(!shift)
    output_data_[o_idx] = output_byte;
}
