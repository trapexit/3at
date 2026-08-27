#include "sdx2_encode.h"

#include <stdlib.h>
#include <string.h>

/*
  David Bryant's dpcm-xq is prior art for lookahead and noise-shaped SDX2:
  https://github.com/dbry/dpcm-xq

  This encoder is an independently implemented block-beam alternative. It
  retains decoded accumulator states and searches past each output-block
  boundary. All modes emit standard SDX2 bytes.
*/

#define SDX2_TRELLIS_BLOCK 256
#define SDX2_TRELLIS_LOOKAHEAD 32
#define SDX2_TRELLIS_SEARCH_BLOCK (SDX2_TRELLIS_BLOCK + SDX2_TRELLIS_LOOKAHEAD)
#define SDX2_TRELLIS_PARITY_CODES 4
#define SDX2_TRELLIS_WIDE_PARITY_CODES 8
#define SDX2_TRELLIS_MAX_BRANCHES (SDX2_TRELLIS_WIDE_PARITY_CODES * 2)
#define SDX2_TRELLIS_MAX_WIDTH 64

// Beam-state dedupe table: one slot per signed 16-bit sample value, with the
// owning frame stamp packed above the slot index.
#define SDX2_TRELLIS_SAMPLE_TABLE 65536
#define SDX2_TRELLIS_SLOT_BITS 11
#define SDX2_TRELLIS_SLOT_MASK ((1U << SDX2_TRELLIS_SLOT_BITS) - 1U)
#define SDX2_TRELLIS_STAMP_LIMIT (1U << (32 - SDX2_TRELLIS_SLOT_BITS))

typedef struct sdx2_candidate_t
{
  s32 delta;
  s8  code;
} sdx2_candidate_t;

typedef struct sdx2_beam_state_t
{
  u64 error;
  s16 sample;
  u8  parent;
  u8  code;
} sdx2_beam_state_t;

static
s32
_trellis_delta(const s8 code_)
{
  s32 code = code_;
  s32 square = (code * code * 2);

  return (code < 0) ? -square : square;
}

static
s16
_trellis_decode(const s8  code_,
                const s16 previous_)
{
  s32 sample = _trellis_delta(code_);

  if(((u8)code_) & 1)
    sample += previous_;
  if(sample > S16_MAX)
    sample = S16_MAX;
  if(sample < S16_MIN)
    sample = S16_MIN;

  return (s16)sample;
}

static
int
_compare_candidate_delta(const void *left_,
                         const void *right_)
{
  const sdx2_candidate_t *left = left_;
  const sdx2_candidate_t *right = right_;

  if(left->delta < right->delta)
    return -1;
  if(left->delta > right->delta)
    return 1;
  return 0;
}

// Selects the width_ lowest-error states, storing them in ascending error order
// at the front of states_ and preserving insertion order for equal errors. This
// defines the tie order explicitly instead of inheriting qsort's unspecified
// ordering, and avoids the per-frame comparator call overhead. Returns the
// number of states kept.
static
int
_select_beam_states(sdx2_beam_state_t *states_,
                    const int          count_,
                    const int          width_)
{
  sdx2_beam_state_t kept[SDX2_TRELLIS_MAX_WIDTH];
  int kept_count = 0;

  for(int index = 0; index < count_; index++)
    {
      const sdx2_beam_state_t state = states_[index];
      int position;

      if((kept_count == width_) && (state.error >= kept[width_ - 1].error))
        continue;

      position = ((kept_count < width_) ? kept_count : (width_ - 1));

      while((position > 0) && (kept[position - 1].error > state.error))
        {
          kept[position] = kept[position - 1];
          position--;
        }

      kept[position] = state;

      if(kept_count < width_)
        kept_count++;
    }

  memcpy(states_,kept,(size_t)kept_count * sizeof(*states_));
  return kept_count;
}

static
void
_build_candidate_tables(sdx2_candidate_t even_[128],
                        sdx2_candidate_t odd_[128])
{
  int even_count = 0;
  int odd_count = 0;

  for(int code = -128; code < 128; code++)
    {
      sdx2_candidate_t candidate =
        {
          .delta = _trellis_delta((s8)code),
          .code = (s8)code
        };

      if(((u8)code) & 1)
        odd_[odd_count++] = candidate;
      else
        even_[even_count++] = candidate;
    }

  qsort(even_,128,sizeof(*even_),_compare_candidate_delta);
  qsort(odd_,128,sizeof(*odd_),_compare_candidate_delta);
}

static
void
_nearest_codes(const sdx2_candidate_t table_[128],
               const s32              target_,
               s8                    *codes_,
               const u8               count_)
{
  int first = 0;
  int last = 128;

  while(first < last)
    {
      int middle = ((first + last) >> 1);

      if(table_[middle].delta < target_)
        first = (middle + 1);
      else
        last = middle;
    }

  // Clamp the window start so a target outside the table's delta range still
  // yields count_ distinct nearest codes; clamping each index independently
  // would repeat the endpoint code instead.
  int start = (first - count_ / 2);

  if(start < 0)
    start = 0;
  if(start > (128 - count_))
    start = (128 - count_);

  for(int i = 0; i < count_; i++)
    codes_[i] = table_[start + i].code;
}

// Looks up an existing beam state for sample_ through seen_, a 65536-entry table
// indexed by the signed sample value. Each entry packs the frame stamp that owns
// it with the slot holding that sample, so no per-frame clearing is needed.
static
void
_add_beam_candidate(sdx2_beam_state_t next_[SDX2_TRELLIS_MAX_WIDTH *
                                             SDX2_TRELLIS_MAX_BRANCHES],
                    int                *next_count_,
                    const u64           error_,
                    const s16           sample_,
                    const u8            parent_,
                    const u8            code_,
                    u32                *seen_,
                    const u32           stamp_)
{
  u32 *entry = &seen_[(u16)(sample_ + 32768)];

  if((*entry >> SDX2_TRELLIS_SLOT_BITS) == stamp_)
    {
      sdx2_beam_state_t *existing = &next_[*entry & SDX2_TRELLIS_SLOT_MASK];

      if(error_ < existing->error)
        {
          existing->error = error_;
          existing->parent = parent_;
          existing->code = code_;
        }

      return;
    }

  *entry = ((stamp_ << SDX2_TRELLIS_SLOT_BITS) | (u32)(*next_count_));
  next_[*next_count_].error = error_;
  next_[*next_count_].sample = sample_;
  next_[*next_count_].parent = parent_;
  next_[*next_count_].code = code_;
  (*next_count_)++;
}

static
s32
_sdx2_encode_trellis_channel(const s16               *input_,
                             const u32                frame_count_,
                             const u8                 stride_,
                             s8                      *output_,
                             const u8                 beam_width_,
                             const u8                 parity_codes_,
                             const sdx2_candidate_t   even_[128],
                             const sdx2_candidate_t   odd_[128])
{
  sdx2_beam_state_t *history;
  u32 *seen;
  u32 stamp = 1;
  s16 previous = 0;

  history = malloc(SDX2_TRELLIS_SEARCH_BLOCK * beam_width_ * sizeof(*history));
  seen = malloc(SDX2_TRELLIS_SAMPLE_TABLE * sizeof(*seen));

  if((history == NULL) || (seen == NULL))
    {
      free(history);
      free(seen);
      return SDX2_ERR_NOMEM;
    }

  memset(seen,0,SDX2_TRELLIS_SAMPLE_TABLE * sizeof(*seen));

  for(u32 base = 0; base < frame_count_;)
    {
      sdx2_beam_state_t current[SDX2_TRELLIS_MAX_WIDTH];
      u8 selected[SDX2_TRELLIS_SEARCH_BLOCK];
      u32 output_length = (frame_count_ - base);
      u32 search_length = (frame_count_ - base);
      int current_count = 1;

      if(output_length > SDX2_TRELLIS_BLOCK)
        output_length = SDX2_TRELLIS_BLOCK;
      if(search_length > SDX2_TRELLIS_SEARCH_BLOCK)
        search_length = SDX2_TRELLIS_SEARCH_BLOCK;
      current[0].error = 0;
      current[0].sample = previous;

      for(u32 frame = 0; frame < search_length; frame++)
        {
          sdx2_beam_state_t next[SDX2_TRELLIS_MAX_WIDTH *
                                  SDX2_TRELLIS_MAX_BRANCHES];
          s8 codes[SDX2_TRELLIS_MAX_BRANCHES];
          s16 target = input_[(base + frame) * stride_];
          int next_count = 0;

          _nearest_codes(even_,target,codes,parity_codes_);
          for(int state = 0; state < current_count; state++)
            {
              _nearest_codes(odd_,
                             target - current[state].sample,
                             codes + parity_codes_,
                             parity_codes_);
              for(int branch = 0; branch < (parity_codes_ * 2); branch++)
                {
                  s16 sample = _trellis_decode(codes[branch],
                                               current[state].sample);
                  s32 difference = (target - sample);
                  u64 error = (current[state].error +
                               (u64)((s64)difference * difference));

                  _add_beam_candidate(next,
                                      &next_count,
                                      error,
                                      sample,
                                      (u8)state,
                                      (u8)codes[branch],
                                      seen,
                                      stamp);
                }
            }

          if(++stamp == SDX2_TRELLIS_STAMP_LIMIT)
            {
              // Stamps must stay unique within the table; recycle it rarely.
              memset(seen,0,SDX2_TRELLIS_SAMPLE_TABLE * sizeof(*seen));
              stamp = 1;
            }

          current_count = _select_beam_states(next,next_count,beam_width_);
          for(int state = 0; state < current_count; state++)
            {
              current[state] = next[state];
              history[frame * beam_width_ + state] = next[state];
            }
        }

      int state = 0;
      for(u32 frame = search_length; frame > 0; frame--)
        {
          sdx2_beam_state_t chosen =
            history[(frame - 1) * beam_width_ + state];

          selected[frame - 1] = chosen.code;
          state = chosen.parent;
        }

      for(u32 frame = 0; frame < output_length; frame++)
        {
          s8 code = (s8)selected[frame];

          output_[(base + frame) * stride_] = code;
          previous = _trellis_decode(code,previous);
        }

      // The final partial block advances exactly to frame_count_; adding the
      // fixed block size here would wrap at the top of the u32 range.
      base += output_length;
    }

  free(seen);
  free(history);
  return SDX2_SUCCESS;
}

static
s32
_sdx2_encode_trellis(const s16 *input_,
                     const u32  input_length_,
                     const u8   channels_,
                     s8        *output_,
                     const u32  output_length_,
                     const u8   beam_width_,
                     const u8   parity_codes_)
{
  sdx2_candidate_t even[128];
  sdx2_candidate_t odd[128];
  u32 frame_count;

  if(output_length_ < input_length_)
    return SDX2_ERR_INVALID_OBUF_LEN;
  if((channels_ < SDX2_MONO) || (channels_ > SDX2_STEREO))
    return SDX2_ERR_UNSUPPORTED_CHANNELS;
  if((beam_width_ < 1) || (beam_width_ > SDX2_TRELLIS_MAX_WIDTH))
    return SDX2_ERR_INVALID_BEAM_WIDTH;
  if(input_length_ % channels_)
    return SDX2_ERR_INVALID_OBUF_LEN;

  _build_candidate_tables(even,odd);
  frame_count = (input_length_ / channels_);
  for(u8 channel = 0; channel < channels_; channel++)
    {
      s32 result = _sdx2_encode_trellis_channel(input_ + channel,
                                                 frame_count,
                                                 channels_,
                                                 output_ + channel,
                                                 beam_width_,
                                                 parity_codes_,
                                                 even,
                                                 odd);

      if(result != SDX2_SUCCESS)
        return result;
    }

  return SDX2_SUCCESS;
}


s32
sdx2_encode_trellis(const s16 *input_,
                    const u32  input_length_,
                    const u8   channels_,
                    s8        *output_,
                    const u32  output_length_,
                    const u8   beam_width_)
{
  return _sdx2_encode_trellis(input_,
                              input_length_,
                              channels_,
                              output_,
                              output_length_,
                              beam_width_,
                              SDX2_TRELLIS_PARITY_CODES);
}


s32
sdx2_encode_trellis_wide(const s16 *input_,
                         const u32  input_length_,
                         const u8   channels_,
                         s8        *output_,
                         const u32  output_length_,
                         const u8   beam_width_)
{
  return _sdx2_encode_trellis(input_,
                              input_length_,
                              channels_,
                              output_,
                              output_length_,
                              beam_width_,
                              SDX2_TRELLIS_WIDE_PARITY_CODES);
}


s32
sdx2_encode(const s16 *ibuf_,
            const u32  ibuf_len_,
            const u8   num_channels_,
            s8        *obuf_,
            const u32  obuf_len_)
{
  return sdx2_encode_trellis(ibuf_,
                             ibuf_len_,
                             num_channels_,
                             obuf_,
                             obuf_len_,
                             1);
}
