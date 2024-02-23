#pragma once

#include <filesystem>
#include <vector>

namespace Opts
{
  struct ToADP4
  {
    std::vector<std::filesystem::path> filepaths;
    std::string input_type;
    std::string output_type;
    std::string encoder;
    int output_freq;
    std::filesystem::path output_path;
  };

  struct ToSDX2
  {
    std::vector<std::filesystem::path> filepaths;
    std::string input_type;
    std::string output_type;    
    std::string encoder;
    int output_channels;
    int output_freq;
    std::filesystem::path output_path;    
  };

  struct FromADP4
  {
    std::vector<std::filesystem::path> filepaths;
    std::string output_type;
    int freq;
  };

  struct FromSDX2
  {
    std::vector<std::filesystem::path> filepaths;
    std::string output_type;
    int channels;
    int freq;
  };

  struct Options
  {
    ToADP4   to_adp4;
    ToSDX2   to_sdx2;
    FromADP4 from_adp4;
    FromSDX2 from_sdx2;    
  };
}
