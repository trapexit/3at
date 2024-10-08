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

#include "fmt.hpp"
#include "sdx2_encode.h"
#include "options.hpp"
#include "types_ints.h"

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
  
  void
  to_sdx2(const std::filesystem::path &filepath_)
  {
    FILE *in_file;
    FILE *out_file;
    std::vector<s16> input_data;
    std::vector<s8>  output_data;
    u32  sample_count;

    in_file  = fopen(filepath_.string().c_str(),"rb");
    out_file = fopen("sdx2.test.raw","wb");

    sample_count = l::file_size(in_file);
    sample_count /= 2;

    input_data.resize(sample_count);
    output_data.resize(sample_count);
    
    // Pad to word / 4 byte alignment for use with 3DO
    // https://3dodev.com/documentation/development/opera/pf25/ppgfldr/mgsfldr/mprfldr/01mpr021
    output_data.resize(((output_data.size() + 3) / 4) * 4);

    fread(&input_data[0],
          sizeof(s16),
          input_data.size(),
          in_file);

    sdx2_encode(input_data.data(),
                input_data.size(),
                SDX2_MONO,
                output_data.data(),
                output_data.size());
    
    fwrite(&output_data[0],
           sizeof(u8),
           output_data.size(),
           out_file);

    fclose(out_file);
    fclose(in_file);
  }
}

void
SubCmd::to_sdx2(const Opts::ToSDX2 &opts_)
{
  for(auto &filepath : opts_.filepaths)
    l::to_sdx2(filepath);
}
