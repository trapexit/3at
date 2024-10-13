#pragma once

#include <filesystem>
#include <vector>

namespace Opts
{
  struct Example
  {
    std::filesystem::path filepath;
  };

  struct ToADP4
  {
    std::vector<std::filesystem::path> filepaths;
    std::string input_type;    
    std::string encoder;
    int output_freq;
    std::filesystem::path output_path;
  };

  struct ToSDX2
  {
    std::vector<std::filesystem::path> filepaths;
    std::string input_type;
    std::string encoder;
    int output_channels;
    int output_freq;
    std::filesystem::path output_path;    
  };

  struct Play
  {
    std::vector<std::filesystem::path> filepaths;    
  };

  struct Options
  {
    Example example;
    ToADP4  to_adp4;
    ToSDX2  to_sdx2;
    Play    play;
  };
}
