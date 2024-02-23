/*
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

#include "sdx2_decode.h"

#include "clamp.h"

#include "types_ints.h"

#define LOOKUP_TABLE_SIZE 256

static
void
_build_lookup_table(s16 lookup_table_[LOOKUP_TABLE_SIZE])
{
  for(int i = -128; i < 128; i++)
    {
      s16 square = (i * i * 2);

      lookup_table_[i+128] = ((i < 0) ? -square : square);
    }
}

static
inline
s32
_sdx2_decode_mono(const u8  *ibuf_,
                  const u32  ibuf_len_,
                  s16       *obuf_,
                  const u32  obuf_len_)
{
  s32 sample;
  s16 lookup_table[256];

  _build_lookup_table(lookup_table);

  for(u32 i = 0; i < ibuf_len_; i++)
    {
      s8 x;

      x = ibuf_[i];
      if(!(x & 1))
        sample = 0;
      sample += lookup_table[x + 128];
      sample = clamp_s32_to_s16(sample);
      *obuf_++ = sample;
    }
  
  return SDX2_SUCCESS;
}

static
inline
s32
_sdx2_decode_stereo(const u8  *ibuf_,
                    const u32  ibuf_len_,
                    s16       *obuf_,
                    const u32  obuf_len_)
{
  s32 l_sample;
  s32 r_sample;  
  s16 lookup_table[256];

  _build_lookup_table(lookup_table);

  for(u32 i = 0; i < ibuf_len_;)
    {
      s8 x;

      x = ibuf_[i++];
      if(!(x & 1))
        l_sample = 0;
      l_sample += lookup_table[x + 128];
      l_sample = clamp_s32_to_s16(l_sample);
      *obuf_++ = l_sample;

      x = ibuf_[i++];
      if(!(x & 1))
        r_sample = 0;
      r_sample += lookup_table[x + 128];
      r_sample = clamp_s32_to_s16(r_sample);
      *obuf_++ = r_sample;      
    }
  
  return SDX2_SUCCESS;
}

s32
sdx2_decode(const u8  *ibuf_,
            const u32  ibuf_len_,
            const u8   num_channels_,
            s16       *obuf_,
            const u32  obuf_len_)
{
  switch(num_channels_)
    {
    case SDX2_MONO:
      return _sdx2_decode_mono(ibuf_,ibuf_len_,obuf_,obuf_len_);      
    case SDX2_STEREO:
      return _sdx2_decode_stereo(ibuf_,ibuf_len_,obuf_,obuf_len_);
    }
  
  return SDX2_SUCCESS;
}
