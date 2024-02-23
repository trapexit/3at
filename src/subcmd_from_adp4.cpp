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
#include "adp4_decode.h"

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
  void
  from_adp4(const std::filesystem::path &filepath_,
            const std::string           &output_type_,
            const int                    freq_)
  {
    std::vector<u8> input_data;
    std::vector<s16> output_data;
    std::filesystem::path output_filepath;

    input_data = file::load_u8(filepath_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    output_filepath = filepath_;
    output_filepath += fmt::format(".{}",output_type_);

    // ADP4 is 4bits per sample, 2 samples per byte
    output_data.resize(input_data.size() * 2);

    adp4_decode(input_data.data(),
                input_data.size(),
                output_data.data());

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
          fmt::print(" - ERROR: short write {}/{}\n",rv,output_data.size());
      }
    else if((output_type_ == "aiff") ||
            (output_type_ == "wav"))
      {
        u64 rv;
        const int channels = 1;

        rv = ffmpeg::write(output_data.data(),
                           output_data.size() * 2, // 2 bytes per sample
                           output_filepath,
                           "s16le",
                           "pcm_s16le",
                           channels,
                           freq_);
        if(rv != (output_data.size() * sizeof(decltype(output_data)::value_type)))
          fmt::print(" - ERROR: short write {}/{}\n",rv,output_data.size());
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
               input_data.size() * 2,
               input_data.size(),
               output_data.size() * sizeof(s16));
  }
}

void
SubCmd::from_adp4(const Opts::FromADP4 &opts_)
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
          l::from_adp4(filepath,
                       opts_.output_type,
                       opts_.freq);
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
