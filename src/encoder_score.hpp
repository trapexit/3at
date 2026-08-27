#pragma once

#include "types_ints.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace encoder_score
{
  enum class SelectionMetric
  {
    Perceptual,
    Rms
  };

  struct Scores
  {
    std::array<u64,2>    squared;
    std::array<double,2> a_weighted_squared;
  };

  // Scores mono/stereo interleaved PCM, ignoring decoded tail padding.
  // Squared error is always populated; A-weighted error is populated for
  // perceptual scoring. Throws std::invalid_argument for invalid input.
  Scores score_by_channel(const std::vector<s16> &reference_,
                          const std::vector<s16> &decoded_,
                          int                     channels_,
                          int                     sample_rate_,
                          SelectionMetric         metric_);
  SelectionMetric selection_metric(const std::string &name_);

  bool improves(const Scores          &candidate_,
                const Scores          &best_,
                int                    channel_,
                SelectionMetric        metric_);


  double rms_error(u64         squared_error_,
                   std::size_t sample_count_);

  double a_weighted_rms_error(double      a_weighted_squared_error_,
                              std::size_t sample_count_);
}
