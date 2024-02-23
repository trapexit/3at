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

#pragma once

#include "types_ints.h"

#include <filesystem>
#include <vector>

namespace ffmpeg
{
  bool ffmpeg_available(void);
  bool ffplay_available(void);
  bool ffprobe_available(void);

  bool file_recognizable(const std::filesystem::path &path);

  int freq(const std::filesystem::path &path);
  int channels(const std::filesystem::path &path);

  std::vector<s16>
  to_s16le(const std::filesystem::path &path,
           const int                    channels,
           const int                    freq);

  u64
  write(const void                  *data,
        const u64                    data_size,
        const std::filesystem::path &filepath,
        const std::string           &format_,
        const std::string           &codec,
        const int                    channels,
        const int                    freq);
}
