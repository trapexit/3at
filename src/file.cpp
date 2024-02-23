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

#include "file.hpp"

#include <array>
#include <vector>

#include <cstdio>

std::vector<u8>
file::load_u8(const std::filesystem::path &filepath_)
{
  FILE *input;
  std::vector<u8> buf;
  std::array<u8,4096> tmpbuf;

  input = fopen(filepath_.string().c_str(),"rb");
  if(input == NULL)
    return {};

  while(!feof(input) && !ferror(input))
    {
      size_t n;
        
      n = fread(tmpbuf.data(),sizeof(u8),tmpbuf.size(),input);
      buf.reserve(buf.size() + n);
      buf.insert(buf.end(),
                 tmpbuf.begin(),
                 tmpbuf.begin() + n);
    }

  fclose(input);
    
  return buf;
}

std::vector<s16>
file::load_s16(const std::filesystem::path &filepath_)
{
  FILE *input;
  std::vector<s16> buf;
  std::array<s16,2048> tmpbuf;

  input = fopen(filepath_.string().c_str(),"rb");
  if(input == NULL)
    return {};

  while(!feof(input) && !ferror(input))
    {
      size_t n;
        
      n = fread(tmpbuf.data(),sizeof(s16),tmpbuf.size(),input);
      buf.reserve(buf.size() + n);
      buf.insert(buf.end(),
                 tmpbuf.begin(),
                 tmpbuf.begin() + n);
    }

  fclose(input);
    
  return buf;
}
