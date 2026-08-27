/*
  ISC License

  Copyright (c) 2024, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "options.hpp"
#include "subcmd.hpp"

#include "file.hpp"
#include "ffmpeg.hpp"
#include "adpcm-lib.h"
#include "adp4_stereo_layout.hpp"

#include "fmt.hpp"

#include "types_ints.h"

#include <iterator>
#include <limits>
#include <array>
#include <unistd.h>
#include <vector>
#include <cstdio>


namespace l
{
  static
  void
  from_adp4(const std::filesystem::path &filepath_,
            const std::string           &output_type_,
            const std::string           &stereo_layout_,
            const int                    channels_,
            const int                    freq_)
  {
    std::vector<u8> input_data;
    std::vector<s16> output_data;
    std::filesystem::path output_filepath;
    size_t max_input_size;
    size_t padding;
    size_t source_size;
    void *context;
    int expected_frame_count;
    int frames;

    input_data = file::load_u8(filepath_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    source_size = input_data.size();
    if(source_size % channels_)
      throw fmt::exception("input byte count is not channel-aligned");

    max_input_size =
      (static_cast<size_t>(std::numeric_limits<int>::max()) * channels_) / 2;
    if(source_size > max_input_size)
      throw fmt::exception("input contains too many ADP4 frames");

    if((channels_ == 2) && (stereo_layout_ == "portfolio"))
      {
        if(source_size & 3)
          throw fmt::exception("Portfolio stereo ADP4 is not word aligned");

        padding = ((8 - (source_size % 8)) % 8);
        if(padding > (max_input_size - source_size))
          throw fmt::exception("input contains too many ADP4 frames");

        expected_frame_count = static_cast<int>(source_size);
        adp4_stereo_layout::portfolio_to_xq(input_data);
      }
    else
      {
        if((channels_ == 2) && (source_size % 8))
          throw fmt::exception("XQ stereo ADP4 is not 8-frame aligned");

        expected_frame_count = static_cast<int>((source_size * 2) / channels_);
      }

    output_filepath = filepath_;
    output_filepath += fmt::format(".{}",output_type_);

    // ADP4 is 4 bits per sample, 2 samples per byte.
    output_data.resize(input_data.size() * 2);

    context = adpcm_create_context(channels_,
                                   freq_,
                                   0,
                                   NOISE_SHAPING_OFF,
                                   FORMAT_NO_HEADERS | FORMAT_INTEL_DVI4);
    if(context == NULL)
      throw fmt::exception("failed to create ADP4 decoder context");

    frames = adpcm_decode_block(context,
                                output_data.data(),
                                input_data.data(),
                                input_data.size(),
                                channels_);
    adpcm_free_context(context);
    if(frames < expected_frame_count)
      throw fmt::exception("failed to decode complete ADP4 stream");
    output_data.resize(static_cast<size_t>(expected_frame_count) * channels_);

    if(output_type_ == "raw")
      {
        u64 rv;
        FILE *out_file;

        out_file = fopen(output_filepath.string().c_str(),"wb");
        if(out_file == NULL)
          throw fmt::exception("failed to open output {}",output_filepath);

        rv = fwrite(output_data.data(),
                    sizeof(decltype(output_data)::value_type),
                    output_data.size(), 
                    out_file);
        
        fclose(out_file);
        if(rv != output_data.size())
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
      }
    else if((output_type_ == "aiff") ||
            (output_type_ == "wav"))
      {
        u64 rv;
        const int channels = channels_;

        rv = ffmpeg::write(output_data.data(),
                           output_data.size() * 2, // 2 bytes per sample
                           output_filepath,
                           "s16le",
                           "pcm_s16le",
                           channels,
                           freq_);
        if(rv != (output_data.size() * sizeof(decltype(output_data)::value_type)))
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
      }
    else
      {
        throw fmt::exception("unknown output type '{}'",output_type_);        
      }

    fmt::print(" - output file name: {}\n"
               " - sample count: {}\n"
               " - input data size: {}b\n"
               " - output data size: {}b\n"
               ,
               output_filepath,
               output_data.size(),
               source_size,
               output_data.size() * sizeof(s16));
  }
}

void
SubCmd::from_adp4(const Opts::FromADP4 &opts_)
{
  std::size_t failures = 0;

  if(opts_.output_type != "raw")
    {
      if(!ffmpeg::ffmpeg_available())
        throw std::runtime_error("ffmpeg executable not found");
    }
  
  for(auto &filepath : opts_.filepaths)
    {
      fmt::print("{}:\n",filepath);

      try
        {
          l::from_adp4(filepath,
                       opts_.output_type,
                       opts_.stereo_layout,
                       opts_.channels,
                       opts_.freq);
        }
      catch(const std::system_error &e_)
        {
          fmt::print(" - ERROR - {} - {} ({})\n",filepath,e_.what(),e_.code().message());
          failures++;
        }
      catch(const std::runtime_error &e_)
        {
          fmt::print(" - ERROR - {} - {}\n",filepath,e_.what());
          failures++;
        }
    }

  if(failures != 0)
    throw std::runtime_error(fmt::format("{} of {} file(s) failed",
                                         failures,
                                         opts_.filepaths.size()));
}
