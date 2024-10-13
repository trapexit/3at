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

#include "ffmpeg.hpp"
#include "adp4_encode.h"
#include "options.hpp"

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
  load_file_raw_s16(const std::filesystem::path &filepath_)
  {
    FILE *input;
    std::vector<s16> buf;
    std::array<s16,2048> tmpbuf;

    input = fopen(filepath_.string().c_str(),"rb");
    if(input == NULL)
      return {};

    while(!feof(input))
      {
        size_t n;
        
        n = fread(tmpbuf.data(),2,tmpbuf.size(),input);
        buf.reserve(buf.size() + n);
        buf.insert(buf.end(),
                   tmpbuf.begin(),
                   tmpbuf.begin() + n);
      }

    fclose(input);
    
    return buf;
  }

  std::vector<s16>
  load_file(const std::filesystem::path &filepath_)
  {
    std::vector<s16> buf;

    buf = ffmpeg_to_s16le(filepath_,1,22050);
    if(buf.empty())
      buf = l::load_file_raw_s16(filepath_);

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

    input_data = l::load_file_raw_s16(filepath_);
    if(input_data.empty())
      {
        return;
      }

    output_filepath = filepath_;
    output_filepath += ".adp4.1ch.raw";
    
    out_file = fopen(output_filepath.string().c_str(),"wb");
    if(out_file == NULL)
      {
        fmt::print(stderr,
                   "error:{}\n",
                   fmt::format("unable to open {}",output_filepath));
        return;
      }
    
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
  }
}

void
SubCmd::to_adp4(const Opts::ToADP4 &opts_)
{
  for(auto &filepath : opts_.filepaths)
    {
      try
        {
          l::to_adp4(filepath,opts_.encoder);        
        }
      catch(const std::system_error &e_)
        {
          fmt::print(" - ERROR - {} - {} ({})\n",filepath_,e_.what(),e_.code().message());
        }
      catch(const std::runtime_error &e_)
        {
          fmt::print(" - ERROR - {} - {}\n",filepath_,e_.what());
        }
    }
}
