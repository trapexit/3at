#pragma once

#include <string>


// Bounds for the best-mode candidate search.
namespace search
{
  struct Limits
  {
    int max_beam;       // highest trellis and wide-trellis beam width searched
    int max_lookahead;  // highest dpcm-xq lookahead depth searched
  };

  // Maps a search-effort name to its candidate bounds. Unknown names, including
  // "exhaustive", return the full search.
  inline
  Limits
  limits(const std::string &effort_)
  {
    if(effort_ == "fast")
      return {8,8};
    if(effort_ == "balanced")
      return {16,12};

    return {64,16};
  }
}
