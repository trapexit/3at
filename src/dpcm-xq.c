////////////////////////////////////////////////////////////////////////////
//                           **** DPCM-XQ ****                            //
//                  Xtreme Quality DPCM Encoder/Decoder                   //
//                    Copyright (c) 2024 David Bryant                     //
//                          All Rights Reserved                           //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

#include "dpcm-xq.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t rms_error_t;     // best if "double" or "uint64_t", "float" okay in a pinch
#define MAX_RMS_ERROR UINT64_MAX
// typedef double rms_error_t;     // best if "double" or "uint64_t", "float" okay in a pinch
// #define MAX_RMS_ERROR DBL_MAX

#define CLIP(data, min, max) \
if ((data) > (max)) data = max; \
else if ((data) < (min)) data = min;

#define DPCM_SDX2   0   // Squareroot-Delta-Exact
#define DPCM_CBD2   1   // Cuberoot-Delta-Exact
#define DPCM_L2XP   2   // Linear-to-Exponential

#define DPCM_NOFUNC 0   // no function
#define DPCM_ENCODE 1   // encode function
#define DPCM_DECODE 2   // decode function

#define STATE_INIT  0   // initial state (no delta allowed)
#define STATE_SYNC  1   // last sample was a sync value (no delta)
#define STATE_DELTA 2   // last sample was a delta value
#define STATE_DELT2 3   // last sample was a double-delta value

static int16_t decode_table [256], *decode_index = decode_table + 128;
static int8_t nearest_0 [65536], *nearest_0_index = nearest_0 + 32768;
static int8_t nearest_1 [65536], *nearest_1_index = nearest_1 + 32768;
// Set once by dpcm_xq_init() and only read thereafter.
static int verbosity;

struct dpcm_state {
    int32_t pcmprev, pcmdata;   // previous & current PCM value
    int32_t weight, error;      // for noise shaping
    int nchans, state;          // num channels & optional state
};

static inline int16_t dpcm_decode_sample (struct dpcm_state *pchan, int8_t value)
{
    pchan->pcmprev = pchan->pcmdata;

    if (value & 1) {
        pchan->pcmdata += decode_index [value];
        CLIP (pchan->pcmdata, -32768, 32767);
        pchan->state = STATE_DELTA;
    }
    else {
        pchan->pcmdata = decode_index [value];
        pchan->state = STATE_SYNC;
    }

    return pchan->pcmdata;
}

// Diagnostics only; only touched when verbosity > 0, which dpcm_xq_init()
// leaves disabled (-1), so these are not written during encoding. They are
// nonetheless file-scope, so enabling verbosity > 0 is not thread-safe.
static int num_trials, num_improvements, sync_switch [9], sync_same [9], delta_switch [9], delta_same [9];

// Apply noise-shaping to the supplied sample value using the shaping weight
// and accumulated error term stored in the dpcm_state structure. Note that
// the error term in the structure is updated, but won't be "correct" until the
// final re-quantized sample value is added to it (and of course we don't know
// that value yet).

static inline int32_t noise_shape (struct dpcm_state *pchan, int32_t sample)
{
    int32_t temp = -((pchan->weight * pchan->error + 512) >> 10);

    if (pchan->weight < 0 && temp) {
        if (temp == pchan->error)
            temp = (temp < 0) ? temp + 1 : temp - 1;

        pchan->error = -sample;
        sample += temp;
    }
    else
        pchan->error = -(sample += temp);

    return sample;
}

// Deterministic cap on the number of branching search nodes (recursive
// dpcm_min_error() calls) explored for a single sample, per channel. The
// lookahead search is exponential in the depth on highly transient content,
// where one sample can otherwise occupy the CPU for minutes and an encode of a
// short block effectively never finishes; the cap makes the worst case finite
// and proportional to the budget. Only calls that find budget left may recurse,
// so the nodes entered as the last units are spent return immediately without
// branching and add only a constant fan-out of work. The counter is a node
// count, never a time limit, so the result is reproducible for a given input
// and settings, and samples that finish below the cap are encoded exactly as
// before.
#define DPCM_XQ_NODE_BUDGET 1000000

static rms_error_t dpcm_min_error (const struct dpcm_state *pchan, int32_t csample, const int16_t *psample, int8_t *best, int depth, rms_error_t max_error, int *node_budget)
{
    int32_t sample_delta = csample - pchan->pcmdata, csample2;
    int16_t best_sync_value, best_delta_value = -128;
    int16_t trial_values [7], trial_count = 0;
    struct dpcm_state chan = *pchan;
    rms_error_t min_error;
    int8_t value;

    if (csample < -32768)
        value = best_sync_value = -128;
    else if (csample > 32767)
        value = best_sync_value = 126;
    else
        value = best_sync_value = nearest_0_index [csample];

    if (best_sync_value > -128)
        trial_values [trial_count++] = best_sync_value - 2;

    if (best_sync_value < 126)
        trial_values [trial_count++] = best_sync_value + 2;

    if (pchan->state != STATE_INIT) {
        if (sample_delta < -32768)
            best_delta_value = -127;
        else if (sample_delta > 32767)
            best_delta_value = 127;
        else
            best_delta_value = nearest_1_index [sample_delta];

        if (best_delta_value > -127) {
            trial_values [trial_count++] = best_delta_value - 2;

            if (best_delta_value > -125)
                trial_values [trial_count++] = best_delta_value - 4;
        }

        if (best_delta_value < 127) {
            trial_values [trial_count++] = best_delta_value + 2;

            if (best_delta_value < 125)
                trial_values [trial_count++] = best_delta_value + 4;
        }

        if (abs (decode_index [best_delta_value] - sample_delta) < abs (decode_index [value] - csample)) {
            trial_values [trial_count++] = best_sync_value;
            value = best_delta_value;
        }
        else
            trial_values [trial_count++] = best_delta_value;
    }

    if (best) *best = value;
    min_error = dpcm_decode_sample (&chan, value) - csample;
    min_error = min_error * min_error;

    // Charge this node to the per-sample budget that dpcm_xq_encode() refills
    // before each sample. Once it is exhausted we stop branching and return the
    // error already computed above for the naively closest code, so the caller
    // still emits a valid code and the search on a pathological sample stays
    // finite. Nothing here depends on time or on other threads, so a given
    // input and settings always produce the same output.
    if (*node_budget <= 0)
        return min_error;

    --*node_budget;

    // if we're at a leaf, or we're not at a leaf but have already exceeded the error limit, return
    if (!depth || min_error > max_error)
        return min_error;

    // otherwise we execute that naively closest value and search deeper for improvement

    if (chan.weight | chan.error) {
        chan.error += chan.pcmdata;
        csample2 = noise_shape (&chan, psample [chan.nchans]);
    }
    else
        csample2 = psample [chan.nchans];

    min_error += dpcm_min_error (&chan, csample2, psample + chan.nchans, NULL, depth - 1, max_error - min_error, node_budget);

    for (int tindex = 0; tindex < trial_count; ++tindex) {
        rms_error_t error, threshold;

        chan = *pchan;
        error = dpcm_decode_sample (&chan, trial_values [tindex]) - csample;
        error = error * error;
        threshold = max_error < min_error ? max_error : min_error;

        if (error < threshold) {
            if (chan.weight | chan.error) {
                chan.error += chan.pcmdata;
                csample2 = noise_shape (&chan, psample [chan.nchans]);
            }
            else
                csample2 = psample [chan.nchans];

            error += dpcm_min_error (&chan, csample2, psample + chan.nchans, NULL, depth - 1, threshold - error, node_budget);

            if (error < min_error) {
                if (best) *best = trial_values [tindex];
                min_error = error;
            }
        }
    }

    if (best && verbosity > 0) {
        ++num_trials;
        if (*best != value) {
            ++num_improvements;

            if (*best & 1) {        // used delta
                if (value & 1)      // original was delta
                    delta_same [abs (*best - best_delta_value)]++;
                else                // original was sync
                    delta_switch [abs (*best - best_delta_value)]++;
            }
            else {                  // used sync
                if (value & 1)      // original was delta
                    sync_switch [abs (*best - best_sync_value)]++;
                else                // original was sync
                    sync_same [abs (*best - best_sync_value)]++;
            }
        }
    }

    return min_error;
}

// Returns 0 if dynamic shaping scratch allocation fails, otherwise 1.
int dpcm_xq_generate_dns_values (const int16_t *samples, int sample_count, int num_chans,
    int16_t *values, int16_t min_value, int16_t last_value);

#define DPCM_XQ_BLOCK_SAMPLES 4410

// One-time initialization of the file-scope lookup tables and the fixed
// encoder constants. Idempotent; not safe to race on the first call, so
// multithreaded callers must invoke it once before spawning their workers.
void
dpcm_xq_init(void)
{
    static int initialized;

    if (initialized)
        return;

    for (int dvalue = -128; dvalue <= 127; ++dvalue)
        decode_index [dvalue] = dvalue * abs (dvalue) * 2;

    for (int pvalue = -32768; pvalue <= 32767; ++pvalue) {
        int min_error = INT_MAX;

        for (int dvalue = -127; dvalue <= 127; ++dvalue)
            if (dvalue & 1) {
                int error = abs (pvalue - decode_index [dvalue]);

                if (error < min_error) {
                    nearest_1_index [pvalue] = dvalue;
                    min_error = error;
                }
            }

        min_error = INT_MAX;

        for (int dvalue = -128; dvalue <= 127; ++dvalue)
            if (!(dvalue & 1)) {
                int error = abs (pvalue - decode_index [dvalue]);

                if (error < min_error) {
                    nearest_0_index [pvalue] = dvalue;
                    min_error = error;
                }
            }
    }

    verbosity = -1;
    initialized = 1;
}

int
dpcm_xq_encode(const s16 *input_,
               u32        frame_count_,
               u8         channels_,
               u8        *output_,
               u8         lookahead_,
               u8         shaping_mode_)
{
    struct dpcm_state chans [2];
    int16_t shaping_values [DPCM_XQ_BLOCK_SAMPLES];
    size_t frame_count = frame_count_;
    int16_t last = 0;
    int use_dns, shaping_weight, node_budget;

    if (!input_ || !output_)
        return 0;
    if (channels_ < 1 || channels_ > 2)
        return 0;
    if (lookahead_ > 16)
        return 0;
    if (shaping_mode_ > DPCM_XQ_SHAPING_DYNAMIC)
        return 0;
    if (frame_count > (SIZE_MAX / channels_))
        return 0;

    dpcm_xq_init();
    memset (chans, 0, sizeof (chans));
    use_dns = shaping_mode_ == DPCM_XQ_SHAPING_DYNAMIC;
    shaping_weight = shaping_mode_ == DPCM_XQ_SHAPING_STATIC ? 1024 : 0;

    for (int ch = 0; ch < channels_; ++ch) {
        chans [ch].nchans = channels_;

        // SDX2 delta codes are valid from the decoder's initial zero accumulator.
        chans [ch].state = STATE_SYNC;
    }

    for (size_t base = 0; base < frame_count;) {
        size_t samples = frame_count - base;

        if (samples > DPCM_XQ_BLOCK_SAMPLES)
            samples = DPCM_XQ_BLOCK_SAMPLES;

        if (use_dns) {
            const int16_t *block = input_ + base * channels_;

            if(dpcm_xq_generate_dns_values(block,
                                          (int)samples,
                                          channels_,
                                          shaping_values,
                                          -256,
                                          last) == 0)
              return 0;
            last = shaping_values [samples - 1];
        }

        for (int ch = 0; ch < channels_; ++ch)
            for (size_t samp = 0; samp < samples; ++samp) {
                const int16_t *pcm_index =
                    input_ + (base + samp) * channels_ + ch;
                int8_t *dpcm_index =
                    (int8_t *)output_ + (base + samp) * channels_ + ch;
                int32_t csample = *pcm_index;
                int depth = lookahead_;

                chans [ch].weight =
                    use_dns ? shaping_values [samp] : shaping_weight;

                if (chans [ch].weight | chans [ch].error)
                    csample = noise_shape (chans + ch, csample);

                if ((size_t)depth > samples - samp - 1)
                    depth = (int)(samples - samp - 1);

                // Refill the deterministic node budget for this sample (and
                // channel) only; the search never resets it, so one pathological
                // sample cannot borrow from its neighbours and cannot recurse
                // without bound.
                node_budget = DPCM_XQ_NODE_BUDGET;

                dpcm_min_error (chans + ch, csample, pcm_index,
                    dpcm_index, depth, MAX_RMS_ERROR, &node_budget);
                dpcm_decode_sample (chans + ch, *dpcm_index);

                if (chans [ch].weight | chans [ch].error)
                    chans [ch].error += chans [ch].pcmdata;
            }

        base += samples;
    }

    return 1;
}


