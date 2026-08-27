// Regression coverage for decoded candidate scoring and selection.

#include "encoder_score.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>


namespace
{
  constexpr double PI = 3.14159265358979323846;

  static
  void
  require(const bool  condition_,
          const char *message_)
  {
    if(condition_)
      return;

    std::fprintf(stderr,"FAIL: %s\n",message_);
    std::exit(EXIT_FAILURE);
  }


  static
  std::vector<s16>
  sine(const std::size_t samples_,
       const int         sample_rate_,
       const double      frequency_)
  {
    std::vector<s16> result(samples_);

    for(std::size_t index = 0; index < samples_; index++)
      result[index] = static_cast<s16>(
        std::lround(12000.0 * std::sin(2.0 * PI * frequency_ * index /
                                      sample_rate_)));

    return result;
  }


  static
  void
  test_a_weighting_tracks_frequency_sensitivity(void)
  {
    constexpr int SAMPLE_RATE = 22050;
    constexpr std::size_t SAMPLE_COUNT = 8192;
    const std::vector<s16> silence(SAMPLE_COUNT,0);
    const encoder_score::Scores low =
      encoder_score::score_by_channel(silence,
                                      sine(SAMPLE_COUNT,SAMPLE_RATE,100.0),
                                      1,
                                      SAMPLE_RATE,
                                      encoder_score::SelectionMetric::Perceptual);
    const encoder_score::Scores mid =
      encoder_score::score_by_channel(silence,
                                      sine(SAMPLE_COUNT,SAMPLE_RATE,1000.0),
                                      1,
                                      SAMPLE_RATE,
                                      encoder_score::SelectionMetric::Perceptual);
    const encoder_score::Scores high =
      encoder_score::score_by_channel(silence,
                                      sine(SAMPLE_COUNT,SAMPLE_RATE,9000.0),
                                      1,
                                      SAMPLE_RATE,
                                      encoder_score::SelectionMetric::Perceptual);

    require(mid.a_weighted_squared[0] > low.a_weighted_squared[0] * 20.0,
            "A-weighting strongly attenuates 100 Hz error");
    require(mid.a_weighted_squared[0] > high.a_weighted_squared[0] * 1.5,
            "A-weighting attenuates 9 kHz error relative to 1 kHz");
  }


  static
  void
  test_identical_audio_has_zero_error(void)
  {
    const std::vector<s16> input = {1,-2,300,-400,5000};
    const encoder_score::Scores scores =
      encoder_score::score_by_channel(
        input,input,1,22050,encoder_score::SelectionMetric::Perceptual);

    require(scores.squared[0] == 0,"identical audio has zero squared error");
    require(scores.a_weighted_squared[0] == 0.0,
            "identical audio has zero A-weighted error");

    const encoder_score::Scores boundary =
      encoder_score::score_by_channel(
        {1},{0},1,22050,encoder_score::SelectionMetric::Perceptual);

    require(boundary.a_weighted_squared[0] > 0.0,
            "the leading sample contributes to A-weighted error");
  }


  static
  void
  test_selection_metric_changes_ranking(void)
  {
    const encoder_score::Scores candidate = {{101,0},{1.0,0.0}};
    const encoder_score::Scores best = {{100,0},{2.0,0.0}};

    require(encoder_score::improves(candidate,
                                    best,
                                    0,
                                    encoder_score::SelectionMetric::Perceptual),
            "perceptual selection uses A-weighted error");
    require(!encoder_score::improves(candidate,
                                     best,
                                     0,
                                     encoder_score::SelectionMetric::Rms),
            "RMS selection uses exact squared error");
    require(encoder_score::selection_metric("perceptual") ==
              encoder_score::SelectionMetric::Perceptual,
            "perceptual metric name is accepted");
    require(encoder_score::selection_metric("rms") ==
              encoder_score::SelectionMetric::Rms,
            "RMS metric name is accepted");
  }


  static
  void
  test_error_weighting_is_phase_invariant(void)
  {
    // The squared analysis window must overlap-add to a constant, otherwise an
    // equal-magnitude error scores differently depending on where it falls in
    // the 512-sample hop and can outrank a smaller error.
    constexpr std::size_t FRAMES = 8192;
    constexpr std::size_t FIRST = 3000;
    constexpr std::size_t SECOND = (FIRST + 256);
    std::vector<s16> reference(FRAMES,0);
    std::vector<s16> first(FRAMES,0);
    std::vector<s16> second(FRAMES,0);

    first[FIRST] = 1000;
    second[SECOND] = 1000;

    const encoder_score::Scores a =
      encoder_score::score_by_channel(reference,
                                      first,
                                      1,
                                      22050,
                                      encoder_score::SelectionMetric::Perceptual);
    const encoder_score::Scores b =
      encoder_score::score_by_channel(reference,
                                      second,
                                      1,
                                      22050,
                                      encoder_score::SelectionMetric::Perceptual);

    require(a.a_weighted_squared[0] > 0.0,"an impulse produces A-weighted error");
    require(std::abs(a.a_weighted_squared[0] - b.a_weighted_squared[0]) <=
              a.a_weighted_squared[0] * 1e-12,
            "half-hop phase does not change the A-weighted error");
    require(a.squared[0] == b.squared[0],
            "half-hop phase does not change the squared error");
  }


  static
  void
  test_invalid_score_inputs_throw(void)
  {
    bool caught = false;

    try
      {
        encoder_score::score_by_channel(
          {0},{0},1,0,encoder_score::SelectionMetric::Perceptual);
      }
    catch(const std::invalid_argument &)
      {
        caught = true;
      }

    require(caught,"a non-positive sample rate is rejected");
  }
}


int
main(void)
{
  test_a_weighting_tracks_frequency_sensitivity();
  test_identical_audio_has_zero_error();
  test_selection_metric_changes_ranking();
  test_error_weighting_is_phase_invariant();
  test_invalid_score_inputs_throw();

  std::puts("Encoder score regressions passed.");
  return EXIT_SUCCESS;
}
