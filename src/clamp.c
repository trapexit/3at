#include "types_ints.h"

s64
clamp_s64(const s64 v_,
          const s64 l_,
          const s64 h_)
{
  if(v_ < l_)
    return l_;
  if(v_ > h_)
    return h_;
  return v_;
}

s16
clamp_s32_to_s16(const s32 v_)
{
  return clamp_s64(v_,S16_MIN,S16_MAX);
}
