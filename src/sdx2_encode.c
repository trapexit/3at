#include "sdx2_encode.h"

#include <math.h>
#include <stdint.h>

static
s16
abs_s16(const s16 v_)
{
  if(v_ == INT16_MIN)
    return INT16_MAX;
  return ((v_ < 0) ? -v_ : v_);
}

static
s16
abs_s16_4x(const s16 v_)
{
  return ((v_ * abs_s16(v_)) * 2);
}

static
s16
square_root(s16 sample_)
{
  s32 neg;

  neg = (sample_ < 0);
  if(neg)
    sample_ = -sample_;

  sample_ = (s16)sqrt(((double)sample_) / 2);

  return (neg ? -sample_ : sample_);
}

static
s8
set_exact_mode(const s8 v_)
{
  return (v_ & ~1);
}

static
s8
set_delta_mode(const s8 v_)
{
  return (v_ | 1);
}

static
int
is_delta_mode(const s32 v_)
{
  return !!(v_ & 1);
}

static
int
is_diff_clipping_s16(const s16 sample0_,
                     const s16 sample1_)
{
  s32 s0;
  s32 s1;
  s32 diff;

  s0 = sample0_;
  s1 = sample1_;
  diff = (s0 - s1);
  if(diff > INT16_MAX)
    return 1;
  if(diff < INT16_MIN)
    return 1;
  return 0;
}

static
s16
decode_sample(const s16 curr_sample_,
              const s16 prev_sample_)
{
  if(is_delta_mode(curr_sample_))
    return (prev_sample_ + abs_s16_4x(curr_sample_));

  return abs_s16_4x(curr_sample_);
}

static
s16
delta_sample(const s16 curr_,
             const s8  curr_exact_,
             const s16 prev_)
{
  s16 dec_sample;

  dec_sample = decode_sample(curr_exact_,prev_);

  return abs_s16(curr_ - dec_sample);
}


/*
  See FIG 5 on page 5 of US Patent US005617506A

  This code doesn't exactly match the patent but is inspired by the
  original MacOS SquashSnd app. It had some changes that are assumed
  to be relevant though unexplained.

  TODO: Try an exact match to patent and compare.
*/
static
s8
encode_sample(const s16 curr_sample_,
              const s16 prev_sample_)
{
  s8 exact;
  s8 delta;
  s16 tmp;

  exact = square_root(curr_sample_);
  exact = set_exact_mode(exact);

  tmp = delta_sample(curr_sample_,exact,prev_sample_);
  if(delta_sample(curr_sample_,exact+2,prev_sample_) < tmp)
    exact += 2;
  else if(delta_sample(curr_sample_,exact-2,prev_sample_) < tmp)
    exact -= 2;

  if(is_diff_clipping_s16(curr_sample_,prev_sample_))
    return exact;

  delta = square_root(curr_sample_ - prev_sample_);
  delta = set_delta_mode(delta);

  tmp = delta_sample(curr_sample_,delta,prev_sample_);

  /* This is straight from SquashSnd. Unclear what 30000 is chosen. */
  /* check for wraparound on the delta case */
  if(tmp > 30000)
    {
      /*
        Overflowed 16bit on this delta. Pull it closer to the
        center.
      */
      delta = ((delta < 0)? (delta + 2) : (delta - 2));
      tmp = delta_sample(curr_sample_,delta,prev_sample_);
    }

  if(delta_sample(curr_sample_,delta+2,prev_sample_) < tmp)
    delta += 2;
  else if(delta_sample(curr_sample_,delta-2,prev_sample_) < tmp)
    delta -= 2;

  if(delta_sample(curr_sample_,exact,prev_sample_) <
     delta_sample(curr_sample_,delta,prev_sample_))
    return exact;

  return delta;
}

static
void
sdx2_encode_mono(const s16 *ibuf_,
                 const u32  ibuf_len_,
                 s8        *obuf_)
{
  s32 i;
  s16 curr_sample = 0;
  s16 prev_sample = 0;
  s8  comp_sample = 0;

  curr_sample = ibuf_[0];
  comp_sample = square_root(curr_sample);
  comp_sample = set_exact_mode(comp_sample);
  obuf_[0]    = comp_sample;

  for(i = 1; i < ibuf_len_; i++)
    {
      curr_sample = ibuf_[i];

      comp_sample = encode_sample(curr_sample,prev_sample);

      obuf_[i] = comp_sample;

      prev_sample = decode_sample(comp_sample,prev_sample);
    }
}

static
void
sdx2_encode_stereo(const s16 *ibuf_,
                   const u32  ibuf_len_,
                   s8        *obuf_)
{
  s32 i;
  s8  comp_sample       = 0;
  s16 curr_sample       = 0;
  s16 prev_left_sample  = 0;
  s16 prev_right_sample = 0;

  curr_sample = ibuf_[0];
  comp_sample = square_root(curr_sample);
  comp_sample = set_exact_mode(comp_sample);
  obuf_[0]    = comp_sample;

  curr_sample = ibuf_[1];
  comp_sample = square_root(curr_sample);
  comp_sample = set_exact_mode(comp_sample);
  obuf_[1]    = comp_sample;

  for (i = 2; i < ibuf_len_; i += 2)
    {
      curr_sample = ibuf_[i+0];
      comp_sample = encode_sample(curr_sample, prev_left_sample);
      obuf_[i+0] = comp_sample;
      prev_left_sample = decode_sample(comp_sample,prev_left_sample);

      curr_sample = ibuf_[i+1];
      comp_sample = encode_sample(curr_sample,prev_right_sample);
      obuf_[i+1] = comp_sample;
      prev_right_sample = decode_sample(comp_sample,prev_right_sample);
    }
}

s32
sdx2_encode(const s16 *ibuf_,
            const u32  ibuf_len_,
            const u8   num_channels_,
            s8        *obuf_,
            const u32  obuf_len_)
{
  if(obuf_len_ < ibuf_len_)
    return SDX2_ERR_INVALID_OBUF_LEN;
  
  switch(num_channels_)
    {
    case 1:
      sdx2_encode_mono(ibuf_,ibuf_len_,obuf_);
      return SDX2_SUCCESS;
    case 2:
      sdx2_encode_stereo(ibuf_,ibuf_len_,obuf_);
      return SDX2_SUCCESS;
    default:
      break;
    }

  return SDX2_ERR_UNSUPPORTED_CHANNELS;
}
