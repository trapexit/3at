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
    std::filesystem::path filepath;    
  };

  struct Options
  {
    Example example;
  };
}
