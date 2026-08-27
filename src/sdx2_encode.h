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

#pragma once

#include "types_ints.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDX2_SUCCESS                    0
#define SDX2_ERR_INVALID_OBUF_LEN       1
#define SDX2_ERR_UNSUPPORTED_CHANNELS   2
#define SDX2_ERR_NOMEM                  3
#define SDX2_ERR_INVALID_BEAM_WIDTH     4

#define SDX2_MONO   1
#define SDX2_STEREO 2
  
s32 sdx2_encode(const s16 *ibuf,
                const u32  ibuf_len,
                const u8   num_channels,                  
                s8        *obuf,
                const u32  obuf_len);

s32 sdx2_encode_trellis(const s16 *input_,
                        const u32  input_length_,
                        const u8   channels_,
                        s8        *output_,
                        const u32  output_length_,
                        const u8   beam_width_);

// Uses eight candidates of each parity instead of four; not necessarily lower error.
// The caller owns both buffers and supplies input_length_ interleaved samples.
// Requires complete mono/stereo frames, output_length_ >= input_length_, and beam width 1..64.
// Returns SDX2_SUCCESS or the same validation/allocation errors as sdx2_encode_trellis.
s32
sdx2_encode_trellis_wide(const s16 *input_,
                         const u32  input_length_,
                         const u8   channels_,
                         s8        *output_,
                         const u32  output_length_,
                         const u8   beam_width_);

#ifdef __cplusplus
}
#endif    
