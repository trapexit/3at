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

#include <array>
#include <vector>
#include <cstdio>


namespace l
{
  static
  u32
  file_size(FILE *f_)
  {
    long orig_off;
    long size;

    orig_off = ftell(f_);
    fseek(f_,0,SEEK_END);
    size = ftell(f_);
    fseek(f_,orig_off,SEEK_SET);

    return size;
  }

  std::vector<s16>
  load_file_raw_s16(const std::filesystem::path &filepath_)
  {
    FILE *input;
    std::vector<s16> buf;
    std::array<s16> tmpbuf;

    input = fopen(filepath_.string().c_str(),"rb");

    while(!feof(

    fclose(input);
    
    return {};
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
    u32 sample_count;
    std::filesystem::path output_filepath;

    input_data = l::load_file(filepath_);
    fmt::print("size input_data: {}\n",input_data.size());
    if(input_data.empty())
      return;

    output_filepath = filepath_;
    output_filepath += ".adp4.1ch.raw";
    
    sample_count = input_data.size();

    out_file = fopen(output_filepath.string().c_str(),"wb");
    
    //    input_data.resize(sample_count);
    // 4bits per sample, 2 samples per byte
    output_data.resize(sample_count >> 1); 

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
    l::to_adp4(filepath,opts_.encoder);
}
