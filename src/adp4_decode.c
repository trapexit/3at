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

void
adp4_decode(const u8  *input_data_,
            const u32  input_data_sample_count_,
            s16*       output_data_)
{
  s32 difference;
  s32 original_sample_h;
  s32 original_sample_l;  
  s32 new_sample;
  adp4_state_t s;

  s.index = 0;
  s.stepsize = 7;
  s.predicted_sample = 0;

  new_sample = 0;
  for(u32 i = 0; i < input_data_sample_count_; i++)
    {
      original_sample_h = ((input_data_[i] & 0xF0) >> 4);
      original_sample_l = ((input_data_[i] & 0x0F) >> 0);      

      difference = 0;
      if(original_sample_h & 0x4)
        difference += s.stepsize;
      if(original_sample_h & 0x2)
        difference += s.stepsize >> 1;
      if(original_sample_h & 0x1)
        difference += s.stepsize >> 2;
      difference += s.stepsize >> 3;
      if(original_sample_h & 0x8)
        difference = -difference;
      new_sample += difference;
      new_sample = _clamp_s32(new_sample,-32768,32767);

      s.index += g_INDEX_TABLE[original_sample_h];
      s.index = _clamp_s32(s.index,0,STEPSIZE_TABLE_MAX);
      s.stepsize = g_STEPSIZE_TABLE[s.index];

      *output_data_++ = new_sample;

      difference = 0;
      if(original_sample_l & 0x4)
        difference += s.stepsize;
      if(original_sample_l & 0x2)
        difference += s.stepsize >> 1;
      if(original_sample_l & 0x1)
        difference += s.stepsize >> 2;
      difference += s.stepsize >> 3;
      if(original_sample_l & 0x8)
        difference = -difference;
      new_sample += difference;
      new_sample = _clamp_s32(new_sample,-32768,32767);

      s.index += g_INDEX_TABLE[original_sample_l];
      s.index = _clamp_s32(s.index,0,STEPSIZE_TABLE_MAX);
      s.stepsize = g_STEPSIZE_TABLE[s.index];

      *output_data_++ = new_sample;
    }  
}
