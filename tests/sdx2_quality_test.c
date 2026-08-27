// Codec regression coverage for SDX2 quality, DNS failures, and raw ADPCM streams.
// Only codec translation units redirect allocations; these wrappers use the real allocator.
#undef malloc
#undef free

#include "adpcm-lib.h"
#include "dpcm-xq.h"
#include "sdx2_decode.h"
#include "sdx2_encode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
  Test_DNS_BLOCK_FRAMES = 4410,
  Test_DNS_FILTER_FRAMES = 15,
  Test_MAX_CHANNELS = 2,
  Test_DNS_TAIL_FRAMES = Test_DNS_BLOCK_FRAMES + Test_DNS_FILTER_FRAMES,
  Test_MAX_FRAMES = Test_DNS_TAIL_FRAMES + 1,
  Test_MAX_SAMPLES = Test_MAX_FRAMES * Test_MAX_CHANNELS,
  Test_QUALITY_FRAMES = 288,
  Test_MAX_BEAM_WIDTH = 64,
  Test_PREDICTIVE_CODES = 8,
  Test_GUARD_BYTES = 2,
  Test_STREAM_FRAMES = 32,
  Test_STREAM_SAMPLES = Test_STREAM_FRAMES * Test_MAX_CHANNELS * 2,
  Test_STREAM_BYTES = 128,
  Test_SAMPLE_RATE = 22050,
  Test_OUTPUT_MARKER = 0x5a
};

static size_t g_ALLOCATIONS;
static size_t g_FAIL_ALLOCATION;
static size_t g_LIVE_ALLOCATIONS;

static
void
_require(int         condition_,
         const char *message_)
{
  if(condition_)
    return;

  fprintf(stderr,"FAIL: %s\n",message_);
  exit(EXIT_FAILURE);
}


void *
codec_test_malloc(size_t size_)
{
  void *allocation;

  g_ALLOCATIONS++;
  if((g_FAIL_ALLOCATION != 0) && (g_ALLOCATIONS == g_FAIL_ALLOCATION))
    return NULL;

  allocation = malloc(size_);
  if(allocation != NULL)
    g_LIVE_ALLOCATIONS++;
  return allocation;
}


void
codec_test_free(void *allocation_)
{
  if(allocation_ != NULL)
    {
      _require(g_LIVE_ALLOCATIONS != 0,"balanced codec allocation lifetime");
      g_LIVE_ALLOCATIONS--;
    }

  free(allocation_);
}


static
void
_require_bytes(const u8 *bytes_,
               size_t    count_,
               u8        expected_)
{
  size_t index;

  for(index = 0; index < count_; index++)
    _require(bytes_[index] == expected_,"output bytes match expected value");
}


static
void
_require_decoded(const s16 *input_,
                 const u8  *encoded_,
                 u32        samples_,
                 u8         channels_)
{
  s16 decoded[Test_MAX_SAMPLES];
  s32 result;

  result = sdx2_decode(encoded_,samples_,channels_,decoded,samples_);
  _require(result == SDX2_SUCCESS,"decode SDX2 regression stream");
  _require(memcmp(input_,decoded,samples_ * sizeof(*input_)) == 0,
           "exactly representable SDX2 signal has zero reconstruction error");
}


static
void
_fill_exact_signal(s16   *input_,
                   size_t samples_)
{
  size_t index;
  s32 code;

  for(index = 0; index < samples_; index++)
    {
      code = ((s32)(index % 7) * 40 - 120);
      input_[index] = (code * abs(code) * 2);
    }
}


static
void
_require_beam_sweep(const s16 *input_,
                    u32        samples_,
                    u8         channels_)
{
  s8 encoded[Test_MAX_SAMPLES];
  int beam;

  for(beam = 1; beam <= Test_MAX_BEAM_WIDTH; beam++)
    {
      memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
      _require(sdx2_encode_trellis(input_,samples_,channels_,encoded,samples_,beam) ==
                 SDX2_SUCCESS,
               "trellis encoder accepts every supported beam width");
      _require_decoded(input_,(const u8 *)encoded,samples_,channels_);
      memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
      _require(sdx2_encode_trellis_wide(input_,samples_,channels_,encoded,samples_,beam) ==
                 SDX2_SUCCESS,
               "wide trellis encoder accepts every supported beam width");
      _require_decoded(input_,(const u8 *)encoded,samples_,channels_);
    }
}


static
void
_test_sdx2_quality(u8 channels_)
{
  s16 input[Test_QUALITY_FRAMES * Test_MAX_CHANNELS];
  s8 encoded[Test_QUALITY_FRAMES * Test_MAX_CHANNELS];
  u32 samples = (Test_QUALITY_FRAMES * channels_);
  s32 result;

  _fill_exact_signal(input,samples);
  result = sdx2_encode(input,samples,channels_,encoded,samples);
  _require(result == SDX2_SUCCESS,"encode exact signal with default SDX2");
  _require_decoded(input,(const u8 *)encoded,samples,channels_);
  result = sdx2_encode_trellis(input,samples,channels_,encoded,samples,16);
  _require(result == SDX2_SUCCESS,"encode exact signal across trellis boundary");
  _require_decoded(input,(const u8 *)encoded,samples,channels_);
  result = dpcm_xq_encode(input,
                          Test_QUALITY_FRAMES,
                          channels_,
                          (u8 *)encoded,
                          0,
                          DPCM_XQ_SHAPING_OFF);
  _require(result != 0,"encode exact signal with DPCM-XQ");
  _require_decoded(input,(const u8 *)encoded,samples,channels_);
  _require_beam_sweep(input,samples,channels_);
}


static
void
_require_wide_exact(const s16 *input_,
                    u32        samples_,
                    u8         channels_,
                    u8         beam_width_)
{
  u8 encoded[Test_QUALITY_FRAMES * Test_MAX_CHANNELS + Test_GUARD_BYTES];
  s32 result;

  memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
  result = sdx2_encode_trellis_wide(input_,
                                    samples_,
                                    channels_,
                                    (s8 *)encoded + 1,
                                    sizeof(encoded) - 1,
                                    beam_width_);
  _require(result == SDX2_SUCCESS,"wide trellis encodes exactly representable signal");
  _require_decoded(input_,encoded + 1,samples_,channels_);
  _require_bytes(encoded,1,Test_OUTPUT_MARKER);
  _require_bytes(encoded + samples_ + 1,
                 sizeof(encoded) - samples_ - 1,
                 Test_OUTPUT_MARKER);
}


static
void
_test_sdx2_wide_quality(u8 channels_)
{
  static const s8 codes[Test_PREDICTIVE_CODES] =
    {
      100,
      1,
      3,
      -5,
      -100,
      -1,
      -3,
      5
    };
  s16 input[Test_QUALITY_FRAMES * Test_MAX_CHANNELS];
  u8 encoded[Test_QUALITY_FRAMES * Test_MAX_CHANNELS];
  u32 samples = (Test_QUALITY_FRAMES * channels_);
  u32 index;
  s32 result;

  _fill_exact_signal(input,samples);
  _require_wide_exact(input,samples,channels_,1);
  _require_wide_exact(input,samples,channels_,Test_MAX_BEAM_WIDTH);
  for(index = 0; index < samples; index++)
    encoded[index] = (u8)codes[(index / channels_ + index % channels_) %
                               Test_PREDICTIVE_CODES];

  result = sdx2_decode(encoded,samples,channels_,input,samples);
  _require(result == SDX2_SUCCESS,"construct exact reset and predictive SDX2 signal");
  result = sdx2_encode_trellis(input,samples,channels_,(s8 *)encoded,samples,16);
  _require(result == SDX2_SUCCESS,"baseline trellis preserves predictive signal");
  _require_decoded(input,encoded,samples,channels_);
  _require_wide_exact(input,samples,channels_,1);
  _require_wide_exact(input,samples,channels_,Test_MAX_BEAM_WIDTH);
}


static
void
_test_sdx2_wide_boundaries(void)
{
  s16 input[Test_MAX_CHANNELS] =
    {
      0,
      0
    };
  u8 encoded[Test_MAX_CHANNELS + Test_GUARD_BYTES];
  u32 samples = Test_MAX_CHANNELS;
  s8 *output = (s8 *)encoded + 1;
  s32 result;

  memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
  result = sdx2_encode_trellis_wide(input,samples,SDX2_STEREO,output,samples - 1,1);
  _require(result == SDX2_ERR_INVALID_OBUF_LEN,"wide trellis rejects short output");
  result = sdx2_encode_trellis_wide(input,samples,0,output,samples,1);
  _require(result == SDX2_ERR_UNSUPPORTED_CHANNELS,"wide trellis rejects zero channels");
  result = sdx2_encode_trellis_wide(input,samples,Test_MAX_CHANNELS + 1,output,samples,1);
  _require(result == SDX2_ERR_UNSUPPORTED_CHANNELS,"wide trellis rejects excess channels");
  result = sdx2_encode_trellis_wide(input,samples,SDX2_STEREO,output,samples,0);
  _require(result == SDX2_ERR_INVALID_BEAM_WIDTH,"wide trellis rejects empty beam");
  result = sdx2_encode_trellis_wide(input,
                                    samples,
                                    SDX2_STEREO,
                                    output,
                                    samples,
                                    Test_MAX_BEAM_WIDTH + 1);
  _require(result == SDX2_ERR_INVALID_BEAM_WIDTH,"wide trellis rejects excess beam width");
  result = sdx2_encode_trellis_wide(input,1,SDX2_STEREO,output,samples,1);
  _require(result == SDX2_ERR_INVALID_OBUF_LEN,"wide trellis rejects partial stereo frame");
  result = sdx2_encode_trellis_wide(input,0,SDX2_STEREO,output,samples,1);
  _require(result == SDX2_SUCCESS,"wide trellis accepts empty stream");
  _require_bytes(encoded,sizeof(encoded),Test_OUTPUT_MARKER);
}


static
void
_test_sdx2_wide_allocation_failure(u8 channels_)
{
  s16 input[Test_MAX_CHANNELS] =
    {
      0,
      0
    };
  u8 encoded[Test_MAX_CHANNELS];
  s32 result;

  memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
  g_ALLOCATIONS = 0;
  g_FAIL_ALLOCATION = 1;
  result = sdx2_encode_trellis_wide(input,channels_,channels_,(s8 *)encoded,channels_,1);
  g_FAIL_ALLOCATION = 0;
  _require(result == SDX2_ERR_NOMEM,"wide trellis reports unavailable scratch storage");
  _require_bytes(encoded,sizeof(encoded),Test_OUTPUT_MARKER);
  _require(g_LIVE_ALLOCATIONS == 0,"wide trellis allocation failure leaves no scratch storage");
}


static
void
_test_dns_boundary(u32 frames_,
                   u8  channels_)
{
  s16 input[Test_MAX_SAMPLES] = {0};
  u8 encoded[Test_MAX_SAMPLES];
  u32 samples = (frames_ * channels_);
  int result;

  memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
  result = dpcm_xq_encode(input,
                          frames_,
                          channels_,
                          encoded,
                          0,
                          DPCM_XQ_SHAPING_DYNAMIC);
  _require(result != 0,"dynamic shaping handles one filtered sample");
  _require_decoded(input,encoded,samples,channels_);
  _require_bytes(encoded + samples,sizeof(encoded) - samples,Test_OUTPUT_MARKER);
  _require(g_LIVE_ALLOCATIONS == 0,"DNS boundary releases scratch allocations");
}


static
void
_test_dns_allocation_failure(size_t failure_,
                             u8     channels_)
{
  s16 input[Test_MAX_SAMPLES] = {0};
  u8 encoded[Test_MAX_SAMPLES];
  u32 frames = (Test_DNS_FILTER_FRAMES + 1);
  size_t committed_samples = 0;
  int result;

  if(failure_ > 4)
    {
      frames += Test_DNS_BLOCK_FRAMES;
      committed_samples = (Test_DNS_BLOCK_FRAMES * channels_);
    }

  memset(encoded,Test_OUTPUT_MARKER,sizeof(encoded));
  g_ALLOCATIONS = 0;
  g_FAIL_ALLOCATION = failure_;
  result = dpcm_xq_encode(input,frames,channels_,encoded,0,DPCM_XQ_SHAPING_DYNAMIC);
  g_FAIL_ALLOCATION = 0;
  _require(result == 0,"DNS allocation failure propagates to encoder caller");
  _require(g_LIVE_ALLOCATIONS == 0,"DNS allocation failure releases scratch buffers");
  _require_bytes(encoded,committed_samples,0);
  _require_bytes(encoded + committed_samples,
                 sizeof(encoded) - committed_samples,
                 Test_OUTPUT_MARKER);
}


static
void *
_create_adpcm(int channels_)
{
  void *context;

  context = adpcm_create_context(channels_,
                                  Test_SAMPLE_RATE,
                                  0,
                                  NOISE_SHAPING_OFF,
                                  FORMAT_NO_HEADERS | FORMAT_INTEL_DVI4);
  _require(context != NULL,"create raw ADPCM context");
  return context;
}


static
size_t
_encode_adpcm(void      *context_,
              u8        *output_,
              const s16 *input_,
              int        bits_)
{
  size_t bytes = 0;
  int result;

  result = adpcm_encode_block_ex(context_,
                                 output_,
                                 &bytes,
                                 input_,
                                 Test_STREAM_FRAMES,
                                 bits_);
  _require(result != 0,"encode aligned ADPCM chunk");
  return bytes;
}


static
void
_test_adpcm_aligned(int bits_,
                    int channels_)
{
  s16 input[Test_STREAM_SAMPLES];
  u8 split[Test_STREAM_BYTES];
  u8 whole[Test_STREAM_BYTES];
  void *context = _create_adpcm(channels_);
  size_t first;
  size_t second;
  size_t whole_bytes = 0;
  int result;

  _fill_exact_signal(input,Test_STREAM_SAMPLES);
  first = _encode_adpcm(context,split,input,bits_);
  second = _encode_adpcm(context,split + first,input + Test_STREAM_FRAMES * channels_,bits_);
  adpcm_free_context(context);
  context = _create_adpcm(channels_);
  result = adpcm_encode_block_ex(context,
                                 whole,
                                 &whole_bytes,
                                 input,
                                 Test_STREAM_FRAMES * 2,
                                 bits_);
  _require(result != 0,"encode equivalent uninterrupted ADPCM stream");
  _require((first + second) == whole_bytes,"aligned calls preserve encoded sample count");
  _require(memcmp(split,whole,whole_bytes) == 0,"aligned calls preserve predictor and index");
  adpcm_free_context(context);
}


static
void
_test_adpcm_final(int bits_,
                  int channels_)
{
  s16 input[Test_MAX_CHANNELS] = {1000, -1000};
  u8 output[Test_STREAM_BYTES];
  void *context = _create_adpcm(channels_);
  size_t bytes = 0;
  int result;

  result = adpcm_encode_block_ex(context,output,&bytes,input,1,bits_);
  _require(result != 0,"single final partial ADPCM block remains supported");
  _require(bytes == (size_t)adpcm_sample_count_to_block_size_no_header(1,channels_,bits_),
           "final partial block preserves padded output size");
  result = adpcm_encode_block_ex(context,NULL,&bytes,NULL,0,bits_);
  _require((result != 0) && (bytes == 0),"empty ADPCM call remains a no-op after finalization");
  memset(output,Test_OUTPUT_MARKER,sizeof(output));
  bytes = sizeof(output);
  result = adpcm_encode_block_ex(context,output,&bytes,input,1,bits_);
  _require((result == 0) && (bytes == 0),"reject continuation after final partial block");
  _require_bytes(output,sizeof(output),Test_OUTPUT_MARKER);
  adpcm_free_context(context);
}


static
void
_test_adpcm_search_budget(void)
{
  s16 input[Test_STREAM_FRAMES];
  u8 output[Test_STREAM_BYTES];
  size_t bytes;
  void *context;
  int bits;
  int frame;
  int result;

  for(frame = 0; frame < Test_STREAM_FRAMES; frame++)
    input[frame] = (frame & 1) ? 32767 : -32768;

  for(bits = 2; bits <= 5; bits++)
    {
      if(bits == 4)
        continue;

      context = adpcm_create_context(1,
                                     Test_SAMPLE_RATE,
                                     16 | LOOKAHEAD_EXHAUSTIVE,
                                     NOISE_SHAPING_OFF,
                                     FORMAT_NO_HEADERS);
      _require(context != NULL,"create exhaustive ADPCM context");
      result = adpcm_encode_block_ex(context,
                                     output,
                                     &bytes,
                                     input,
                                     Test_STREAM_FRAMES,
                                     bits);
      _require(result != 0,"bounded exhaustive ADPCM search completes");
      _require(bytes != 0,"bounded exhaustive ADPCM search writes output");
      adpcm_free_context(context);
    }
}


static
size_t
_encode_adpcm_weight(double weight_,
                     u8    *output_)
{
  s16 input[Test_STREAM_FRAMES];
  size_t bytes = 0;
  void *context;
  int result;

  _fill_exact_signal(input,Test_STREAM_FRAMES);
  context = adpcm_create_context(1,
                                 Test_SAMPLE_RATE,
                                 0,
                                 NOISE_SHAPING_STATIC,
                                 FORMAT_NO_HEADERS | FORMAT_INTEL_DVI4);
  _require(context != NULL,"create static-shaping ADPCM context");
  adpcm_set_shaping_weight(context,weight_);
  result = adpcm_encode_block(context,
                              output_,
                              &bytes,
                              input,
                              Test_STREAM_FRAMES);
  _require(result != 0,"encode static-shaping ADPCM");
  adpcm_free_context(context);
  return bytes;
}


static
void
_test_adpcm_shaping_weight_bounds(void)
{
  u8 bounded[Test_STREAM_BYTES];
  u8 extreme[Test_STREAM_BYTES];
  size_t bounded_bytes;
  size_t extreme_bytes;

  bounded_bytes = _encode_adpcm_weight(1.0,bounded);
  extreme_bytes = _encode_adpcm_weight(HUGE_VAL,extreme);
  _require(bounded_bytes == extreme_bytes,"positive shaping saturation preserves size");
  _require(memcmp(bounded,extreme,bounded_bytes) == 0,
           "positive shaping saturation preserves output");

  bounded_bytes = _encode_adpcm_weight(-1.0,bounded);
  extreme_bytes = _encode_adpcm_weight(-HUGE_VAL,extreme);
  _require(bounded_bytes == extreme_bytes,"negative shaping saturation preserves size");
  _require(memcmp(bounded,extreme,bounded_bytes) == 0,
           "negative shaping saturation preserves output");

  bounded_bytes = _encode_adpcm_weight(0.0,bounded);
  extreme_bytes = _encode_adpcm_weight(NAN,extreme);
  _require(bounded_bytes == extreme_bytes,"NaN shaping fallback preserves size");
  _require(memcmp(bounded,extreme,bounded_bytes) == 0,
           "NaN shaping falls back to no shaping");
}


static
void
_test_channel(u8 channels_)
{
  size_t failure;
  int bits;

  _test_sdx2_quality(channels_);
  _test_sdx2_wide_quality(channels_);
  _test_sdx2_wide_allocation_failure(channels_);
  _test_dns_boundary(Test_DNS_FILTER_FRAMES,channels_);
  _test_dns_boundary(Test_DNS_TAIL_FRAMES,channels_);
  for(failure = 1; failure <= 8; failure++)
    _test_dns_allocation_failure(failure,channels_);
  for(bits = 2; bits <= 5; bits++)
    {
      _test_adpcm_aligned(bits,channels_);
      _test_adpcm_final(bits,channels_);
    }

  _require(g_LIVE_ALLOCATIONS == 0,"all codec regression allocations released");
}


int
main(void)
{
  _test_sdx2_wide_boundaries();
  _test_adpcm_search_budget();
  _test_adpcm_shaping_weight_bounds();
  _test_channel(SDX2_MONO);
  _test_channel(SDX2_STEREO);
  puts("Codec quality, DNS boundary/allocation, and ADPCM stream regressions passed.");
  return EXIT_SUCCESS;
}
