#include "encoder_score.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <utility>

namespace
{
  constexpr std::size_t FFT_SIZE = 1024;
  constexpr std::size_t FFT_HOP = (FFT_SIZE / 2);
  constexpr double PI = 3.14159265358979323846;
  constexpr double A_WEIGHTING_GAIN_POWER = 1.5848931924611136;

  using Spectrum = std::array<std::complex<double>,FFT_SIZE>;

  static
  void
  fft(Spectrum &values_)
  {
    for(std::size_t index = 1, reversed = 0; index < FFT_SIZE; index++)
      {
        std::size_t bit = (FFT_SIZE >> 1);

        while(reversed & bit)
          {
            reversed ^= bit;
            bit >>= 1;
          }

        reversed ^= bit;
        if(index < reversed)
          std::swap(values_[index],values_[reversed]);
      }

    for(std::size_t length = 2; length <= FFT_SIZE; length <<= 1)
      {
        const double angle = (-2.0 * PI / static_cast<double>(length));
        const std::complex<double> step(std::cos(angle),std::sin(angle));
        const std::size_t half = (length >> 1);

        for(std::size_t base = 0; base < FFT_SIZE; base += length)
          {
            std::complex<double> rotation(1.0,0.0);

            for(std::size_t offset = 0; offset < half; offset++)
              {
                const std::complex<double> even = values_[base + offset];
                const std::complex<double> odd =
                  (values_[base + offset + half] * rotation);

                values_[base + offset] = (even + odd);
                values_[base + offset + half] = (even - odd);
                rotation *= step;
              }
          }
      }
  }

  static
  const std::array<double,FFT_SIZE> &
  analysis_window(void)
  {
    static const std::array<double,FFT_SIZE> result =
      []
      {
        std::array<double,FFT_SIZE> values = {};

        // Square root of a periodic Hann window. Squaring it during scoring
        // makes the overlap-add power complementary at the 50% hop, so every
        // sample carries the same weight regardless of its phase in the hop.
        for(std::size_t index = 0; index < FFT_SIZE; index++)
          values[index] =
            std::sqrt(0.5 -
                      0.5 * std::cos(2.0 * PI * static_cast<double>(index) /
                                     static_cast<double>(FFT_SIZE)));

        return values;
      }();

    return result;
  }


  static
  double
  a_weighting_power(const double frequency_)
  {
    constexpr double F1 = 20.6;
    constexpr double F2 = 107.7;
    constexpr double F3 = 737.9;
    constexpr double F4 = 12194.0;

    if(frequency_ <= 0.0)
      return 0.0;

    const double squared = (frequency_ * frequency_);
    const double numerator = (F4 * F4 * squared * squared);
    const double denominator =
      ((squared + F1 * F1) *
       std::sqrt((squared + F2 * F2) * (squared + F3 * F3)) *
       (squared + F4 * F4));
    const double amplitude = (numerator / denominator);

    return (amplitude * amplitude * A_WEIGHTING_GAIN_POWER);
  }


  using WeightTable = std::array<double,(FFT_SIZE / 2) + 1>;

  // The weighting depends only on the sample rate, so it is built once per
  // scoring call instead of once per bin of every window.
  static
  void
  fill_weight_table(WeightTable &table_,
                    const int    sample_rate_)
  {
    for(std::size_t bin = 1; bin <= (FFT_SIZE / 2); bin++)
      {
        const double frequency =
          (static_cast<double>(bin) * sample_rate_ / FFT_SIZE);
        const double mirror = ((bin == (FFT_SIZE / 2)) ? 1.0 : 2.0);

        table_[bin] = (mirror * a_weighting_power(frequency));
      }
  }

  static
  double
  score_a_weighted_channel(const std::vector<s16> &reference_,
                           const std::vector<s16> &decoded_,
                           const int               channels_,
                           const int               channel_,
                           const WeightTable      &weights_)
  {
    const std::size_t frames =
      (reference_.size() / static_cast<std::size_t>(channels_));
    const std::array<double,FFT_SIZE> &window = analysis_window();
    const std::ptrdiff_t signed_frames = static_cast<std::ptrdiff_t>(frames);
    double spectral_energy = 0.0;
    double window_energy = 0.0;

    for(std::ptrdiff_t start = -static_cast<std::ptrdiff_t>(FFT_HOP);
        start < signed_frames;
        start += FFT_HOP)
      {
        Spectrum spectrum = {};

        for(std::size_t offset = 0; offset < FFT_SIZE; offset++)
          {
            const std::ptrdiff_t frame =
              (start + static_cast<std::ptrdiff_t>(offset));

            if(frame < 0)
              continue;
            if(frame >= signed_frames)
              break;

            const std::size_t index =
              (static_cast<std::size_t>(frame) *
                 static_cast<std::size_t>(channels_) +
               static_cast<std::size_t>(channel_));
            const double difference =
              (static_cast<double>(reference_[index]) - decoded_[index]);

            spectrum[offset] = (difference * window[offset]);
            window_energy += (window[offset] * window[offset]);
          }

        fft(spectrum);

        for(std::size_t bin = 1; bin <= (FFT_SIZE / 2); bin++)
          spectral_energy += (weights_[bin] * std::norm(spectrum[bin]));
      }

    if(window_energy == 0.0)
      return 0.0;

    return (spectral_energy / FFT_SIZE *
            static_cast<double>(frames) / window_energy);
  }
}

encoder_score::Scores
encoder_score::score_by_channel(const std::vector<s16> &reference_,
                                 const std::vector<s16> &decoded_,
                                 const int               channels_,
                                 const int               sample_rate_,
                                 const SelectionMetric   metric_)
{
  if((channels_ != 1) && (channels_ != 2))
    throw std::invalid_argument("scoring requires one or two channels");
  if((sample_rate_ <= 0) ||
     (reference_.size() % channels_) ||
     (decoded_.size() % channels_) ||
     (decoded_.size() < reference_.size()))
    throw std::invalid_argument("invalid input for encoder scoring");

  Scores scores = {};
  WeightTable weights = {};

  if(metric_ == SelectionMetric::Perceptual)
    fill_weight_table(weights,sample_rate_);

  for(int channel = 0; channel < channels_; channel++)
    {
      for(std::size_t index = channel;
          index < reference_.size();
          index += channels_)
        {
          const s64 difference = (static_cast<s64>(reference_[index]) -
                                  decoded_[index]);

          scores.squared[channel] += static_cast<u64>(difference * difference);
        }

      if(metric_ == SelectionMetric::Perceptual)
        scores.a_weighted_squared[channel] =
          score_a_weighted_channel(reference_,
                                   decoded_,
                                   channels_,
                                   channel,
                                   weights);
    }

  return scores;
}


encoder_score::SelectionMetric
encoder_score::selection_metric(const std::string &name_)
{
  if(name_ == "perceptual")
    return SelectionMetric::Perceptual;
  if(name_ == "rms")
    return SelectionMetric::Rms;

  throw std::invalid_argument("unknown encoder selection metric");
}


bool
encoder_score::improves(const Scores                &candidate_,
                        const Scores                &best_,
                        const int                    channel_,
                        const SelectionMetric        metric_)
{
  if((channel_ < 0) || (channel_ >= 2))
    throw std::invalid_argument("invalid encoder score channel");

  if(metric_ == SelectionMetric::Perceptual)
    return (candidate_.a_weighted_squared[channel_] <
            best_.a_weighted_squared[channel_]);

  return (candidate_.squared[channel_] < best_.squared[channel_]);
}


double
encoder_score::rms_error(const u64         squared_error_,
                         const std::size_t sample_count_)
{
  if(!sample_count_)
    return 0.0;

  return std::sqrt(static_cast<double>(squared_error_) / sample_count_);
}


double
encoder_score::a_weighted_rms_error(const double      a_weighted_squared_error_,
                                     const std::size_t sample_count_)
{
  if(!sample_count_)
    return 0.0;

  return std::sqrt(a_weighted_squared_error_ / sample_count_);
}
