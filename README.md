# 3at: 3DO Audio Tool

A 3DO audio codec encoder / decoder. Supports SDX2 (square/xact/delta)
and Intel DVI / ADP4 codecs. Optionally uses FFmpeg for reading and
writing non-raw formats.

The eventual goal is to upstream the SDX2 and ADP4 encoders to FFmpeg.


## Usage

```
$ 3at --help
3at: 3DO Audio Tool v1.0.0
Usage: ./build/3at_linux_x86_64 [OPTIONS] SUBCOMMAND

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


## FFmpeg

Any input or output which is not `raw` requires FFmpeg to be
available. You can [download FFmpeg](https://ffmpeg.org) and place the
executable in your PATH or in the same directory as `3at`.


## Examples

```
$ 3at to-sdx2 --channels=1 --freq=22050 --output-type=raw input.wav
input.wav:
 - output file name: input.wav.sdx2.1ch.22050hz.raw
 - sample count: 661500
 - input file size: 1323000b
 - output file size: 661500b

$ 3at to-sdx2 --channels=1 --freq=22050 --output-type=aifc input.wav
input.wav:
 - output file name: input.wav.sdx2.1ch.22050hz.aifc
 - sample count: 661500
 - input file size: 1323000b
 - output file size: 661500b

$ 3at from-sdx2 --channels=1 --freq=22050 --output-type=wav input.wav.sdx2.1ch.22050hz.raw
orig.wav.sdx2.1ch.22050hz.raw:
 - output file name: orig.wav.sdx2.1ch.22050hz.raw.wav
 - sample count: 661500
 - input data size: 661500b
 - output data size: 1323000b
```


## FFmpeg / FFplay Examples

Rather than duplicate effort and place other format encoding/decoding
into `3at` here are some examples to encode audio using FFmpeg for
audio codecs and formats which can be used on the 3DO.


### Convert to uncompressed AIFF signed 16bit bigendian

```
ffmpeg -i input.file -ar 22050 -c:a pcm_s16be output.aiff"
```


### Convert to uncompressed AIFF signed 8bit

```
ffmpeg -i input.file -ar 22050 -c:a pcm_s8 output.aiff"
```


### Convert to uncompressed raw signed 16bit bigendian

Raw files can be useful if you want to create multiple samples at
runtime from the same file.

```
ffmpeg -i input.file -ar 22050 -f s16be -acodec pcm_s16be output.raw"
```

### Convert to uncompressed raw signed 8bit

```
ffmpeg -i input.file -ar 22050 -f s8 -acodec pcm_s8 output.raw"
```


### Play raw SDX2 file

```
$ ffplay -hide_banner -autoexit -f u8 -acodec sdx2_dpcm -ac 1 -ar 22050 input.wav.sdx2.1ch.22050hz.raw
[u8 @ 0x7f78a4000c40] Estimating duration from bitrate, this may be inaccurate
Input #0, u8, from 'orig.wav.sdx2.1ch.22050hz.raw':
  Duration: 00:00:30.00, bitrate: 176 kb/s
  Stream #0:0: Audio: pcm_u8, 22050 Hz, 1 channels, u8, 176 kb/s
```

### Play raw Intel DVI / ADP4 file

```
$ ffplay -hide_banner -autoexit -f u8 -acodec adpcm_ima_ws -ac 1 -ar 22050 input.wav.sdx2.1ch.22050hz.raw
[u8 @ 0x7f78a4000c40] Estimating duration from bitrate, this may be inaccurate
Input #0, u8, from 'orig.wav.sdx2.1ch.22050hz.raw':
  Duration: 00:00:30.00, bitrate: 176 kb/s
  Stream #0:0: Audio: pcm_u8, 22050 Hz, 1 channels, u8, 176 kb/s
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
