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

#include "subcmd_to_sdx2.hpp"
#include "options.hpp"

#include "ffmpeg.hpp"
#include "file.hpp"
#include "sdx2_encode.h"

#include "fmt.hpp"

#include "types_ints.h"

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
  to_sdx2(const std::filesystem::path &filepath_,
          const std::string           &input_type_,
          const std::string           &encoder_,
          const int                    channels_,
          const int                    freq_)
  {
    FILE *out_file;
    std::vector<s16> input_data;
    std::vector<s8>  output_data;
    std::filesystem::path output_filepath;

    input_data = l::load_file(input_type_,filepath_,channels_,freq_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    output_filepath = filepath_;
    output_filepath += fmt::format(".sdx2.{}ch.{}hz.raw",channels_,freq_);

    out_file = fopen(output_filepath.string().c_str(),"wb");
    if(out_file == NULL)
      throw fmt::exception("failed to open output {}",output_filepath);

    // 8bits per sample
    output_data.resize(input_data.size());

    // Pad to word / 4 byte alignment for use with 3DO
    output_data.resize(((output_data.size() + 3) / 4) * 4);

    if(encoder_ == "default")
      sdx2_encode(input_data.data(),
                  input_data.size(),
                  channels_,
                  output_data.data(),
                  output_data.size());

    fwrite(output_data.data(),
           sizeof(decltype(output_data)::value_type),
           output_data.size(),
           out_file);

    fclose(out_file);

    fmt::print(" - output file name: {}\n"
               " - sample count: {}\n"               
               " - input file size: {}b\n"
               " - output file size: {}b\n"               
               ,
               output_filepath,
               input_data.size(),
               input_data.size() * 2,
               output_data.size());
  }
}

void
SubCmd::to_sdx2(const Opts::ToSDX2 &opts_)
{
  for(auto &filepath : opts_.filepaths)
    {
      fmt::print("{}:\n",filepath);

      try
        {
          l::to_sdx2(filepath,
                     opts_.input_type,
                     opts_.encoder,
                     opts_.output_channels,
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
