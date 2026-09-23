# 3at: 3DO Audio Tool

A 3DO audio codec encoder / decoder. Supports SDX2 (square/xact/delta)
and Intel DVI / ADP4 codecs. Optionally uses FFmpeg for reading and
writing non-raw formats.

The eventual goal is to upstream the SDX2 and ADP4 encoders to FFmpeg.


## Build

Native builds use the system C and C++ compilers:

```sh
make
make test
make NDEBUG=1
make SANITIZE=1 test
```

The default executable is `build/3at`; `make install PREFIX=/usr/local`
installs it under `bin`. To cross-compile, install Zig 0.16.0 or run
`make zig-venv` to obtain it in `.venv`, then run `make release`. Zig from
`PATH` takes precedence; `ZIG=/path/to/zig` selects another installation.
The release builds are placed in `build/` for x86-64 and AArch64 Linux
(musl), 32- and 64-bit Windows (GNU), and AArch64 macOS.


## Usage

```
$ ./build/3at --help
3at: 3DO Audio Tool v1.1.0
Usage: ./build/3at [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit
  --help-all                  List help for all subcommands

Subcommands:
  to-adp4                     Convert input to Intel/DVI ADP4 codec
  to-sdx2                     Convert input to SDX2 codec
  from-adp4                   Convert from raw Intel/DVI ADP4
  from-sdx2                   Convert from raw SDX2
  version                     print 3at version
```

All subcommands have their own help and arguments. Use `--help` or
`--help-all` to see all available options.

The examples use `3at` after installation. If it is not installed, substitute
`./build/3at` when running them from the repository root.


## FFmpeg

Encoding from WAV or other container inputs, and writing AIFC, AIFF, or WAV,
requires FFmpeg. Raw-to-raw conversions do not. You can
[download FFmpeg](https://ffmpeg.org) and put its executable on your `PATH`.


## Examples

```
$ 3at to-sdx2 --channels=1 --freq=22050 --output-type=raw input.wav
input.wav:
 - output file name: input.wav.sdx2.1ch.22050hz.raw
 - sample count: 661500
 - input data size: 1323000b
 - output data size: 661500b

$ 3at to-sdx2 --channels=1 --freq=22050 --output-type=aifc input.wav
input.wav:
 - output file name: input.wav.sdx2.1ch.22050hz.aifc
 - sample count: 661500
 - input data size: 1323000b
 - output data size: 661500b

$ 3at from-sdx2 --channels=1 --freq=22050 --output-type=wav input.wav.sdx2.1ch.22050hz.raw
input.wav.sdx2.1ch.22050hz.raw:
 - output file name: input.wav.sdx2.1ch.22050hz.raw.wav
 - sample count: 661500
 - input data size: 661500b
 - output data size: 1323000b
```

### High-quality ADP4 encoding

`to-adp4` defaults to the vendored
[adpcm-xq](https://github.com/dbry/adpcm-xq) encoder in blockless
Intel/DVI4 mode. It uses lookahead and dynamic noise shaping while producing
ADP4 codes for Portfolio playback.

`--encoder=xq` (the default) supports mono and stereo.

```
# Mono ADP4 AIFC:
3at to-adp4 --freq=44100 --output-type=aifc music.wav

# Portfolio/M2 stereo: one left/right nibble pair per byte:
3at to-adp4 --channels=2 --stereo-layout=portfolio \
    --freq=44100 --output-type=aifc music.wav

# adpcm-xq's channel-chunk raw layout remains available:
3at to-adp4 --channels=2 --stereo-layout=xq \
    --freq=44100 --output-type=raw music.wav
```

Lookahead accepts depths 0–16. Dynamic noise shaping usually makes audible
noise less objectionable, but can increase unweighted error; use
`--noise-shaping=off` when minimizing numeric RMS error is the goal.

Stereo AIFC requires the `portfolio` layout, which is the default. Its bytes
match M2 `sampler_adp4_v2`: high nibble left, low nibble right. FFmpeg preserves
this AIFC payload, but its `adpcm_ima_ws` decoder does not reproduce `3at`'s
Intel/DVI4 PCM even in mono, and it does not interpret the Portfolio stereo
layout. The Portfolio sampler can decode the AIFC directly.
`from-adp4` accepts only raw ADP4. Extract the compressed payload from the
AIFC `SSND` chunk after its 8-byte offset/block-size header and any declared
offset. For Portfolio stereo, retain the `COMM` sample-frame count rounded up
to a multiple of four bytes; trimming to the exact frame count makes the raw
input invalid. Pass `--channels=2 --stereo-layout=portfolio`, then trim the
decoded PCM to the `COMM` frame count if the file contains padded frames.

Library callers using `FORMAT_NO_HEADERS` must use multiples of eight frames
per call for continued 4-bit ADPCM encoding. A final partial call retains the
existing zero padding; later nonempty calls on that context fail rather than
silently desynchronizing the decoder. Empty calls remain no-ops. The CLI encodes
each file in a single call, so its output format is unchanged.

### High-quality SDX2 encoding

`to-sdx2` defaults to a blockwise trellis encoder. It commits 256-sample
blocks with up to 32 samples of boundary lookahead while retaining a
configurable beam of the best decoded states. The output remains ordinary
SDX2 understood by the stock mono and stereo Portfolio instruments.

`--encoder=default` uses the same decoder-exact search with a single retained
state, equivalent to trellis beam width 1. The former SquashSnd-derived greedy
encoder is no longer included.

Recursive SDX2 lookahead and dynamic noise shaping were implemented by David
Bryant in [dpcm-xq](https://github.com/dbry/dpcm-xq), created from
[adpcm-xq issue #19](https://github.com/dbry/adpcm-xq/issues/19). Its SDX2
core is vendored as `--encoder=xq` under Bryant's BSD license in
`license.txt`. The block-beam `trellis` encoder remains an independent
alternative.

```
3at to-sdx2 --channels=2 --freq=44100 --output-type=aifc music.wav

# Explicit controls:
3at to-sdx2 --encoder=trellis --beam-width=16 \
    --channels=2 --freq=44100 music.wav

# Upstream dpcm-xq recursive lookahead with dynamic shaping:
3at to-sdx2 --encoder=xq --lookahead=6 --noise-shaping=dynamic \
    --channels=2 --freq=44100 music.wav

# Minimize unweighted RMS error with dpcm-xq:
3at to-sdx2 --encoder=xq --lookahead=6 --noise-shaping=off \
    --channels=2 --freq=44100 music.wav

# Fast one-state encoder:
3at to-sdx2 --encoder=default --channels=2 --freq=44100 music.wav
```

Beam width accepts 1–64 and defaults to 16. Larger beams can reduce squared
reconstruction error at additional encoding cost; results are signal-dependent.
The 32-sample boundary lookahead improves decisions around committed blocks.
Neither setting changes the 8-bit SDX2 stream format or decoder.

### Candidate search

Both codecs accept `--encoder=best`. This does not reduce file size: ADP4
always uses 4 bits/sample and SDX2 always uses 8 bits/sample. It encodes and
decodes the candidate settings below, then ranks the reconstructed audio.
Stereo selects the lowest-scoring complete stream independently for each
channel; it does not splice segments or reset predictor state. The resulting
streams retain the normal stereo byte layout.

```
3at to-adp4 --encoder=best --freq=44100 --output-type=aifc input.wav
3at to-sdx2 --encoder=best --channels=2 --freq=44100 \
    --output-type=aifc input.wav
```

ADP4 searches all 51 xq combinations: lookahead 0–16 times
off/static/dynamic shaping. SDX2 searches the one-state default, trellis beam
widths 2–64 (width 1 is the same encoder as the default), and all 51 dpcm-xq
combinations, then tries wide-trellis beam widths 1–64. Wide trellis considers
eight nearby absolute codes and eight delta codes per state, versus four of
each in the original search. The ordinary SDX2 `default` and `trellis` modes
are unchanged.

`--selection=perceptual` is the default for `best`. It minimizes decoded
A-weighted reconstruction error, measured with 1,024-sample square-root Hann
windows and 50% overlap at the selected sample rate. The squared windows
overlap-add to a constant, so an error of a given size scores the same wherever
it falls in the hop. `--selection=rms` selects by exact
unweighted squared error, preserving the previous behavior. With `rms`, the
expanded candidate set cannot increase squared reconstruction error relative
to the original candidate set, even though wide trellis alone is not guaranteed
to improve it.

Ties retain the earliest candidate. Stereo prints each channel's selected
configuration; both modes print the combined RMS and A-weighted reconstruction
errors. A-weighting models frequency-dependent hearing sensitivity, not
masking or subjective preference, so neither score guarantees better listening
quality.

This is a search over encoder settings, not every possible encoded bitstream.
It is intentionally much slower than a single setting, and the additional
wide searches increase SDX2 `best` runtime.

`--search-effort` bounds that search. `fast` caps beam width and lookahead at
8, `balanced` caps beam width at 16 and lookahead at 12, and `exhaustive`
(default) searches the full range. Raising effort buys little: in the recorded
corpus, `exhaustive` improved aggregate RMS by 0.19% over `fast` for SDX2 and
0.26% for ADP4 while taking 18.7 and 27.1 times as long.

### Candidate search performance

Wall times below encode 60 s of mono 22,050 Hz audio with the release build and
the default worker count on a two-core (four-thread) 2.6 GHz Skylake-U laptop.
The `default` column is each subcommand's default encoder, which ignores
`--selection` because it performs a single deterministic encode.

| Encoder | `default` | `fast` | `balanced` | `exhaustive` |
|---|---|---|---|---|
| SDX2, `--selection=rms` | 2.4 s | 24.0 s | 102.0 s | 813.7 s |
| SDX2, `--selection=perceptual` | — | 26.1 s | 104.6 s | 822.8 s |
| ADP4, `--selection=rms` | 0.5 s | 11.3 s | 37.3 s | 123.0 s |
| ADP4, `--selection=perceptual` | — | 12.2 s | 38.0 s | 123.4 s |

`best` costs roughly 10x the default SDX2 encoder at `fast`, 42x at `balanced`
and 338x at `exhaustive`. ADP4 searches fewer candidates, so its `exhaustive`
search stays seven times cheaper than SDX2's, and its ratios against the ADP4
default encoder are 25x, 83x and 273x. Stereo searches each channel
independently, roughly doubling the work. Repeated runs on that laptop drifted
by up to 30% as it throttled, so treat the times as order of magnitude rather
than as a fixed cost.

`--selection=perceptual` decodes and A-weight-scores every candidate, which
costs about 50 ms per candidate per minute of mono audio at four threads, or
about 130 ms at one thread. The candidate count grows with effort, so the
perceptual overhead is largest in relative terms where the search is smallest:
+2.1 s, +2.6 s and +9.2 s over `rms` at `fast`, `balanced` and `exhaustive` for
SDX2, or +8.7%, +2.6% and +1.1%, and +0.9 s, +0.7 s and +0.4 s for ADP4.
Scoring cost scales with audio length and channel count but not with signal
content; search cost is content dependent.

`--threads` sets the worker count for that search; 0 uses the hardware
concurrency. Candidates are independent and selection is by candidate order,
so any worker count produces byte-identical output. Measured on the same
laptop, `fast` over 60 s of mono SDX2 took 46.8 s with one thread, 27.1 s with
two and 24.0 s with the default, a 1.9x speedup from four logical cores.
Candidate costs are uneven, so returns flatten past two cores.

The lookahead search is bounded per sample, because its cost is exponential
rather than linear in the lookahead depth. Cheap content is unaffected: tone,
speech and low-lookahead encoding stay byte-identical to an unbounded search.
Impulse trains and full-scale square waves previously ran for minutes per
second of audio and now finish in bounded time. On such content the bound can
cost a fraction of a percent of RMS error, and `--search-effort=fast` remains
the practical choice.


## Tests

Run the codec regression suites with `make test`, or `make SANITIZE=1 test`
to enable AddressSanitizer and UndefinedBehaviorSanitizer. The C suite covers
exactly representable mono/stereo SDX2 signals in ordinary/wide searches,
output boundaries, invalid arguments, 15-frame dynamic-shaping boundaries,
allocation-failure cleanup, and aligned/final-partial raw ADPCM calls.
Allocation failures are injected only into the test build; production codecs
use the normal allocator.

The C++ suites cover the parallel helper used by `best`, search-effort bounds,
A-weighting frequency sensitivity, half-hop error-weighting invariance, exact
RMS selection, and perceptual selection precedence.


## FFmpeg / FFplay Examples

Rather than duplicate effort and place other format encoding/decoding
into `3at` here are some examples to encode audio using FFmpeg for
audio codecs and formats which can be used on the 3DO.


### Convert to uncompressed AIFF signed 16bit bigendian

```
ffmpeg -i input.file -ar 22050 -c:a pcm_s16be output.aiff
```


### Convert to uncompressed AIFF signed 8bit

```
ffmpeg -i input.file -ar 22050 -c:a pcm_s8 output.aiff
```


### Convert to uncompressed raw signed 16bit bigendian

Raw files can be useful if you want to create multiple samples at
runtime from the same file.

```
ffmpeg -i input.file -ar 22050 -f s16be -acodec pcm_s16be output.raw
```

### Convert to uncompressed raw signed 8bit

```
ffmpeg -i input.file -ar 22050 -f s8 -acodec pcm_s8 output.raw
```


### Play raw SDX2 file

```
$ ffplay -hide_banner -autoexit -f u8 -acodec sdx2_dpcm -ar 22050 input.wav.sdx2.1ch.22050hz.raw
```

### Play raw Intel DVI / ADP4 file

Decode with `3at` first; FFmpeg's `adpcm_ima_ws` decoder does not reproduce
the same PCM from this ADP4 stream.

```
$ 3at from-adp4 --channels=1 --freq=22050 --output-type=wav input.wav.adp4.1ch.22050hz.raw
$ ffplay -hide_banner -autoexit input.wav.adp4.1ch.22050hz.raw.wav
```


## Documentation

* https://3dodev.com
* https://3dodev.com/documentation/development/opera/pf25/ppgfldr/mgsfldr/mpgfldr/03mpg004
* [IMA_ADPCM.pdf (Intel DVI/ADP4)](docs/IMA_ADPCM.pdf)
* [Patent US005617506A - Method for Communicating a Value Over a
  Transmission Medium and for Decoding Same (SDX2)](docs/pat5617506_-_method_for_communicating_a_value_over_a_transmission_medium_and_for_decoding_same.pdf)
* [AIFF-1.3.pdf](docs/AIFF-1.3.pdf)
* [AIFF-C.9.26.91.pdf](docs/AIFF-C.9.26.91.pdf)


## Links

* 3DO Dev Repo: https://3dodev.com
* 3DO Disc Tool: https://github.com/trapexit/3dt
* 3DO Image Tool: https://github.com/trapexit/3it
* 3DO Compression Tool: https://github.com/trapexit/3ct
* 'Modern' 3DO DevKit: https://github.com/trapexit/3do-devkit


## Donations / Sponsorship

If you find 3at useful please consider supporting its ongoing
development.

https://github.com/trapexit/support
