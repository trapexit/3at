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

#include "subcmd_to_adp4.hpp"

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
  std::vector<s16>
  load_file(const std::filesystem::path &filepath_)
  {
    std::vector<s16> buf;

    buf = ffmpeg::to_s16le(filepath_,1,22050);
    if(buf.empty())
      buf = file::load_s16(filepath_);

    return buf;
  }

  void
  to_adp4(const std::filesystem::path &filepath_,
          const std::string           &encoder_)
  {
    FILE *out_file;
    std::vector<s16> input_data;
    std::vector<u8> output_data;
    std::filesystem::path output_filepath;

    input_data = l::load_file(filepath_,channels_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    output_filepath = filepath_;
    output_filepath += ".adp4.1ch.raw";
    
    out_file = fopen(output_filepath.string().c_str(),"wb");
    if(out_file == NULL)
      throw fmt::exception("failed to open output {}",output_filepath);
    
    // 4bits per sample, 2 samples per byte
    output_data.resize(input_data.size() >> 1);

    // Pad to word / 4 byte alignment for use with 3DO
    // https://3dodev.com/documentation/development/opera/pf25/ppgfldr/mgsfldr/mprfldr/01mpr021
    output_data.resize(((output_data.size() + 3) / 4) * 4);

    if(encoder_ == "default")
      adp4_encode(input_data.data(),
                  input_data.size(),
                  output_data.data());

    fwrite(&output_data[0],
           sizeof(u8),
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
SubCmd::to_adp4(const Opts::ToADP4 &opts_)
{
  for(auto &filepath : opts_.filepaths)
    {
      fmt::print("{}:\n",filepath);      
      
      try
        {
          l::to_adp4(filepath,opts_.encoder);        
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
