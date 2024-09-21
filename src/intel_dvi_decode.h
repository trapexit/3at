#pragma once

#include "types_ints.h"

typedef struct IntelDVIDecodeState IntelDVIDecodeState;
struct IntelDVIDecodeState
{
  u32 index;
  u32 prev_val;
};

#if defined __cplusplus
extern "C" {
#endif

void intel_dvi_decode(IntelDVIDecodeState *state,
                      const u8            *input_data,
                      const u32            input_data_sample_count,
                      s16                 *output_data);

#if defined __cplusplus
}
#endif
