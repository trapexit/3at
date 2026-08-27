////////////////////////////////////////////////////////////////////////////
//                           **** DPCM-XQ ****                            //
//                  Xtreme Quality DPCM Encoder / Decoder                 //
//                    Copyright (c) 2024 David Bryant                     //
//                          All Rights Reserved                           //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "types_ints.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
  DPCM_XQ_SHAPING_OFF,
  DPCM_XQ_SHAPING_STATIC,
  DPCM_XQ_SHAPING_DYNAMIC
};

// One-time initialization of the encoder's shared lookup tables and fixed
// constants. Idempotent once complete, but the first call is not itself
// thread-safe: a caller that encodes from multiple threads must call this
// exactly once before starting those threads. Afterwards the shared state is
// read-only, so concurrent dpcm_xq_encode() calls are safe. Single-threaded
// callers may omit this call; dpcm_xq_encode() initializes lazily on first use.
void dpcm_xq_init(void);

// Encodes interleaved mono/stereo PCM into frame_count_ * channels_ output bytes.
// Input and output must be non-NULL. Returns 1 on success, or 0 for invalid
// arguments or dynamic shaping allocation failure. On allocation failure,
// previously completed blocks may have been written; discard the entire output.
// Thread safety: may be called concurrently after dpcm_xq_init() has completed;
// nothing outside its own stack is written during encoding.
//
// The per-sample lookahead search is bounded by a fixed node budget
// (DPCM_XQ_NODE_BUDGET in dpcm-xq.c) held in this call's own stack state and
// refilled before the search for each sample and channel; the recursion never
// refills it. The counter counts search nodes only, never time or thread
// scheduling, so a given input and settings always produce the same bytes, and
// concurrent encodes do not share it. Output is byte-identical to an unbounded
// search unless a single sample exhausts the budget; that happens only on
// highly transient content, where the unbounded search could spend minutes of
// CPU on one sample, while the bounded search always finishes in work
// proportional to the budget.
int dpcm_xq_encode(const s16 *input_,
                   u32        frame_count_,
                   u8         channels_,
                   u8        *output_,
                   u8         lookahead_,
                   u8         shaping_mode_);

#ifdef __cplusplus
}
#endif
