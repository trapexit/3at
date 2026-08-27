#include "adp4_stereo_layout.hpp"

#include <cstddef>
#include <stdexcept>

namespace
{
  constexpr std::size_t FRAMES_PER_CHUNK = 8;
  constexpr std::size_t BYTES_PER_CHANNEL_CHUNK = 4;

  void
  validate_xq(const std::vector<u8> &data_)
  {
    if(data_.size() % FRAMES_PER_CHUNK)
      throw std::runtime_error("XQ stereo ADP4 is not 8-frame aligned");
  }
}


void
adp4_stereo_layout::xq_to_portfolio(std::vector<u8> &data_)
{
  validate_xq(data_);
  std::vector<u8> converted(data_.size());

  for(std::size_t base = 0; base < data_.size(); base += FRAMES_PER_CHUNK)
    for(std::size_t frame = 0; frame < FRAMES_PER_CHUNK; frame++)
      {
        const int shift = (frame & 1) ? 0 : 4;
        const std::size_t byte = (frame >> 1);
        const u8 left = ((data_[base + byte] >> shift) & 0x0f);
        const u8 right = ((data_[base + BYTES_PER_CHANNEL_CHUNK + byte] >> shift) & 0x0f);

        converted[base + frame] = ((left << 4) | right);
      }

  data_.swap(converted);
}


void
adp4_stereo_layout::portfolio_to_xq(std::vector<u8> &data_)
{
  if(data_.size() & 3)
    throw std::runtime_error("Portfolio stereo ADP4 is not word aligned");
  data_.resize((data_.size() + 7) & ~static_cast<std::size_t>(7),0);
  std::vector<u8> converted(data_.size());

  for(std::size_t base = 0; base < data_.size(); base += FRAMES_PER_CHUNK)
    for(std::size_t frame = 0; frame < FRAMES_PER_CHUNK; frame++)
      {
        const int shift = (frame & 1) ? 0 : 4;
        const std::size_t byte = (frame >> 1);
        const u8 packed = data_[base + frame];

        converted[base + byte] |= (((packed >> 4) & 0x0f) << shift);
        converted[base + BYTES_PER_CHANNEL_CHUNK + byte] |= ((packed & 0x0f) << shift);
      }

  data_.swap(converted);
}
