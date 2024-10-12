#pragma once

#include "types_ints.h"

#if defined __cplusplus
extern "C" {
#endif

void adp4_encode(const s16 *input_data,
                 const u32  input_data_sample_count,
                 u8        *output_data);

#if defined __cplusplus
}
#endif
