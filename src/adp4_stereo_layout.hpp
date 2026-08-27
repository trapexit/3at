#pragma once

#include "types_ints.h"

#include <vector>

namespace adp4_stereo_layout
{
  void xq_to_portfolio(std::vector<u8> &data_);
  void portfolio_to_xq(std::vector<u8> &data_);
}
