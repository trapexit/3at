////////////////////////////////////////////////////////////////////////////
//                           **** ADPCM-XQ ****                           //
//                  Xtreme Quality ADPCM Encoder/Decoder                  //
//                    Copyright (c) 2024 David Bryant.                    //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#ifndef ADPCMLIB_H_
#define ADPCMLIB_H_

#define NOISE_SHAPING_OFF       0       // flat noise (no shaping)
#define NOISE_SHAPING_STATIC    0x100   // static 1st-order shaping (configurable, highpass default)
#define NOISE_SHAPING_DYNAMIC   0x200   // dynamically tilted noise based on signal

#define LOOKAHEAD_DEPTH         0x0ff   // depth of search
#define LOOKAHEAD_EXHAUSTIVE    0x800   // full breadth of search (all branches taken)
#define LOOKAHEAD_NO_BRANCHING  0x400   // no branches taken (internal use only!)

#define FORMAT_INTEL_DVI4       0x1000  // Intel DVI4/ADP4 variant of IMA ADPCM (swapped nibbles)
#define FORMAT_NO_HEADERS       0x2000  // raw samples; padded final encode cannot be continued

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8  int8_t;
#else
#include <stdint.h>
#endif
#include <stddef.h>

#ifdef __cplusplus 
extern "C" {
#endif

/* adpcm-lib.c */

int adpcm_sample_count_to_block_size (int sample_count, int num_chans, int bps);
int adpcm_block_size_to_sample_count (int block_size, int num_chans, int bps);
int adpcm_align_block_size (int block_size, int num_chans, int bps, int round_up);
int adpcm_sample_count_to_block_size_no_header (int sample_count, int num_chans, int bps);
int adpcm_block_size_to_sample_count_no_header (int block_size, int num_chans, int bps);
int adpcm_align_block_size_no_header (int block_size, int num_chans, int bps, int round_up);
void *adpcm_create_context (int num_channels, int sample_rate, int lookahead, int noise_shaping, int format);
void adpcm_set_shaping_weight (void *p, double shaping_weight);

// Both encoders borrow p from adpcm_create_context() and write the byte count to
// non-NULL outbufsize. inbufcount is the sample count per channel; inbuf/outbuf may
// be NULL only for an empty call. The caller supplies enough output space.
// Return 1 on success, 0 on invalid arguments, allocation failure, or nonempty
// encoding after raw finalization. Valid empty calls succeed with zero bytes.
// FORMAT_NO_HEADERS calls may continue only while sample counts are multiples of
// 16/32/8/32 for 2/3/4/5 bits respectively. A successful unaligned call retains
// zero-padded output and finalizes the context for encoding. Later nonempty calls
// fail with zero bytes and no output or context changes. Headered blocks do not
// finalize the context.
// The lookahead search is bounded by a fixed node budget per sample
// (ADPCM_NODE_BUDGET in adpcm-lib.c) so that highly transient content cannot
// recurse without bound. The budget only limits how far the search refines a
// sample; a valid code is always produced, and output is unaffected unless a
// single sample exhausts the budget.
int adpcm_encode_block_ex (void *p, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount, int bps);

// Four-bit wrapper with the same contract; raw continuation requires multiples of 8 samples.
int adpcm_encode_block (void *p, uint8_t *outbuf, size_t *outbufsize, const int16_t *inbuf, int inbufcount);

int adpcm_decode_block_ex (void *p, int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize, int channels, int bps);
int adpcm_decode_block (void *p, int16_t *outbuf, const uint8_t *inbuf, size_t inbufsize, int channels);
void adpcm_free_context (void *p);

/* adpcm-dns.c */

int generate_dns_values (const int16_t *samples, int sample_count, int num_chans, int sample_rate,
    int16_t *values, int16_t min_value, int16_t last_value);

#ifdef __cplusplus 
}
#endif


#endif /* ADPCMLIB_H_ */
