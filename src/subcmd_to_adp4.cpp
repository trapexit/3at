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

#include "subcmd.hpp"

#include "file.hpp"
#include "ffmpeg.hpp"
#include "adp4_encode.h"

#include "fmt.hpp"

#include "types_ints.h"

#include <iterator>
#include <array>
#include <unistd.h>
#include <vector>
#include <cstdio>


namespace l
{
  static
  std::vector<s16>
  load_file(const std::string           &input_type_,
            const std::filesystem::path &filepath_,
            const int                    channels_,
            const int                    freq_)
  {
    if(input_type_ == "raw")
      return file::load_s16(filepath_);

    if(input_type_ == "auto")
      {
        std::vector<s16> buf;

        buf = ffmpeg::to_s16le(filepath_,channels_,freq_);
        if(!buf.empty())
          return buf;

        return file::load_s16(filepath_);
      }

    return {};
  }

  static
  void
  to_adp4(const std::filesystem::path &filepath_,
          const std::string           &input_type_,
          const std::string           &output_type_,
          const std::string           &encoder_,
          const int                    freq_)
  {
    std::vector<s16> input_data;
    std::vector<u8> output_data;
    std::filesystem::path output_filepath;

    input_data = l::load_file(input_type_,filepath_,1,freq_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    output_filepath = filepath_;
    output_filepath += fmt::format(".adp4.1ch.{}hz.{}",freq_,output_type_);

    // 4bits per sample, 2 samples per byte
    output_data.resize(input_data.size() >> 1);

    // Pad to word / 4 byte alignment for use with 3DO
    output_data.resize(((output_data.size() + 3) / 4) * 4);

    if(encoder_ == "default")
      {
        adp4_encode(input_data.data(),
                    input_data.size(),
                    output_data.data());
      }
    else
      {
        throw fmt::exception("unknown encoder '{}'",encoder_);
      }

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
    else if(output_type_ == "aifc")
      {
        u64 rv;
        const int channels = 1;
        const std::string format = "u8";
        const std::string codec = "adpcm_ima_ws";

        rv = ffmpeg::write(output_data.data(),
                           output_data.size(),
                           output_filepath,
                           format,
                           codec,
                           channels,
                           freq_);
        if(rv != output_data.size())
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
      }

    fmt::print(" - output file name: {}\n"
               " - sample count: {}\n"
               " - input data size: {}b\n"
               " - output data size: {}b\n"
               ,
               output_filepath,
               input_data.size(),
               input_data.size() * 2,
               output_data.size());
  }
}

void
SubCmd::to_adp4(const Opts::ToADP4 &opts_)
{
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
          l::to_adp4(filepath,
                     opts_.input_type,
                     opts_.output_type,
                     opts_.encoder,
                     opts_.output_freq);
        }
      catch(const std::system_error &e_)
        {
          fmt::print(" - ERROR - {} - {} ({})\n",filepath,e_.what(),e_.code().message());
        }
      catch(const std::runtime_error &e_)
        {
          fmt::print(" - ERROR - {} - {}\n",filepath,e_.what());
        }
    }
}
