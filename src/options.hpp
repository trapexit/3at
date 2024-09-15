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
  };

  struct Options
  {
    Example example;
    ToADP4  to_adp4;
  };
}
