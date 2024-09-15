#pragma once

#include <filesystem>
#include <variant>

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
  };
}
