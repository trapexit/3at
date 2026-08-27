/*
  ISC License

  Copyright (c) 2024, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "subcmd.hpp"

#include "file.hpp"
#include "ffmpeg.hpp"
#include "adpcm-lib.h"
#include "encoder_score.hpp"
#include "adp4_stereo_layout.hpp"
#include "parallel.hpp"
#include "search_limits.hpp"

#include "fmt.hpp"

#include "types_ints.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <array>
#include <unistd.h>
#include <vector>
#include <cstdio>
#include <string>


namespace l
{
  static
  std::vector<s16>
  load_file(const std::string           &input_type_,
            const std::filesystem::path &filepath_,
            const int                    channels_,
            const int                    freq_)
  {
    if(input_type_ == "raw")
      return file::load_s16(filepath_);

    if(input_type_ == "auto")
      {
        if(!ffmpeg::file_recognizable(filepath_))
          return file::load_s16(filepath_);

        return ffmpeg::to_s16le(filepath_,channels_,freq_);
      }

    return {};
  }

  static
  void
  patch_aifc_frame_count(const std::filesystem::path &path_,
                         const u32                    frame_count_)
  {
    std::fstream file(path_,std::ios::binary | std::ios::in | std::ios::out);

    if(!file)
      throw fmt::exception("failed to reopen AIFC {}",path_);
    file.seekg(12);
    while(file)
      {
        char id[4];
        u8 size_bytes[4];

        file.read(id,4);
        file.read(reinterpret_cast<char *>(size_bytes),4);
        if(!file)
          break;
        const u32 size = ((static_cast<u32>(size_bytes[0]) << 24) |
                          (static_cast<u32>(size_bytes[1]) << 16) |
                          (static_cast<u32>(size_bytes[2]) << 8) |
                           static_cast<u32>(size_bytes[3]));
        const std::streamoff data_offset = file.tellg();

        if(!memcmp(id,"COMM",4))
          {
            if(size < 6)
              throw fmt::exception("invalid AIFC COMM chunk in {}",path_);
            const u8 frames[4] =
              {
                static_cast<u8>(frame_count_ >> 24),
                static_cast<u8>(frame_count_ >> 16),
                static_cast<u8>(frame_count_ >> 8),
                static_cast<u8>(frame_count_)
              };
            file.seekp(data_offset + static_cast<std::streamoff>(2));
            file.write(reinterpret_cast<const char *>(frames),4);
            if(!file)
              throw fmt::exception("failed to patch AIFC {}",path_);
            return;
          }

        file.seekg(data_offset + static_cast<std::streamoff>(size + (size & 1)));
      }

    throw fmt::exception("AIFC COMM chunk not found in {}",path_);
  }

  static
  int
  xq_noise_shaping(const std::string &noise_shaping_)
  {
    if(noise_shaping_ == "off")
      return NOISE_SHAPING_OFF;
    if(noise_shaping_ == "static")
      return NOISE_SHAPING_STATIC;
    return NOISE_SHAPING_DYNAMIC;
  }

  static
  void
  encode_xq(const std::vector<s16> &input_,
            const int               frame_count_,
            const int               channels_,
            const int               freq_,
            const int               lookahead_,
            const int               noise_shaping_,
            std::vector<u8>        &output_)
  {
    size_t encoded_size = 0;
    void *context = adpcm_create_context(channels_,
                                         freq_,
                                         lookahead_,
                                         noise_shaping_,
                                         FORMAT_NO_HEADERS |
                                           FORMAT_INTEL_DVI4);

    if(context == NULL)
      throw fmt::exception("failed to create adpcm-xq encoder context");

    if(!adpcm_encode_block(context,
                           output_.data(),
                           &encoded_size,
                           input_.data(),
                           frame_count_))
      {
        adpcm_free_context(context);
        throw fmt::exception("adpcm-xq encoding failed");
      }

    adpcm_free_context(context);
    if(encoded_size != output_.size())
      throw fmt::exception("adpcm-xq size mismatch {} / {}",
                           encoded_size,
                           output_.size());
  }

  static
  encoder_score::Scores
  score_adp4(const std::vector<s16>          &input_,
             const std::vector<u8>           &encoded_,
             const int                        channels_,
             const int                        freq_,
             const encoder_score::SelectionMetric metric_,
             std::vector<s16>                &decoded_)
  {
    void *context = adpcm_create_context(channels_,
                                         freq_,
                                         0,
                                         NOISE_SHAPING_OFF,
                                         FORMAT_NO_HEADERS |
                                           FORMAT_INTEL_DVI4);

    if(context == NULL)
      throw fmt::exception("failed to create ADP4 decoder context");

    const int decoded_frames = adpcm_decode_block(context,
                                                   decoded_.data(),
                                                   encoded_.data(),
                                                   encoded_.size(),
                                                   channels_);

    adpcm_free_context(context);
    if((decoded_frames <= 0) ||
       (static_cast<size_t>(decoded_frames) * channels_ < input_.size()))
      throw fmt::exception("ADP4 candidate decoded too few samples");

    return encoder_score::score_by_channel(input_,
                                           decoded_,
                                           channels_,
                                           freq_,
                                           metric_);
  }

  struct Adp4Candidate
  {
    int lookahead;
    int shaping;
  };

  static
  const char *
  shaping_name(const int shaping_)
  {
    static const char *names[] = { "off", "static", "dynamic" };

    return names[shaping_];
  }

  static
  int
  shaping_flag(const int shaping_)
  {
    static const int flags[] =
      {
        NOISE_SHAPING_OFF,
        NOISE_SHAPING_STATIC,
        NOISE_SHAPING_DYNAMIC
      };

    return flags[shaping_];
  }

  static
  std::string
  candidate_name(const Adp4Candidate &candidate_)
  {
    return fmt::format("xq/lookahead={}/shaping={}",
                       candidate_.lookahead,
                       shaping_name(candidate_.shaping));
  }

  static
  std::vector<Adp4Candidate>
  adp4_candidates(const search::Limits &limits_)
  {
    std::vector<Adp4Candidate> candidates;

    for(int lookahead = 0; lookahead <= limits_.max_lookahead; lookahead++)
      for(int shaping = 0; shaping < 3; shaping++)
        candidates.push_back({lookahead,shaping});

    return candidates;
  }

  static
  void
  encode_candidate(const Adp4Candidate  &candidate_,
                   const std::vector<s16> &input_,
                   const int               frame_count_,
                   const int               channels_,
                   const int               freq_,
                   std::vector<u8>        &encoded_)
  {
    std::fill(encoded_.begin(),encoded_.end(),0);

    encode_xq(input_,
              frame_count_,
              channels_,
              freq_,
              candidate_.lookahead,
              shaping_flag(candidate_.shaping),
              encoded_);
  }

  static
  void
  search_adp4(const std::vector<s16>          &input_,
              const int                        frame_count_,
              const int                        channels_,
              const int                        freq_,
              const unsigned                   threads_,
              const search::Limits            &limits_,
              const encoder_score::SelectionMetric metric_,
              std::vector<u8>                 &output_,
              encoder_score::Scores           &best_scores_)
  {
    const std::vector<Adp4Candidate> candidates =
      adp4_candidates(limits_);
    std::vector<encoder_score::Scores> scores(candidates.size());
    const std::size_t output_size = output_.size();

    parallel::for_each(candidates.size(),
                       threads_,
                       [&](const std::size_t index_)
                       {
                         // Scratch is reused across candidates on this worker
                         // rather than allocated per candidate.
                         static thread_local std::vector<u8>  encoded;
                         static thread_local std::vector<s16> decoded;

                         encoded.resize(output_size);
                         decoded.resize(output_size * 2);

                         encode_candidate(candidates[index_],
                                          input_,
                                          frame_count_,
                                          channels_,
                                          freq_,
                                          encoded);
                         scores[index_] =
                           score_adp4(input_,
                                      encoded,
                                      channels_,
                                      freq_,
                                      metric_,
                                      decoded);
                       });

    // Strict improvement preserves the simplest-to-most-expensive tie order.
    std::array<std::size_t,2> best_index = {0,0};

    best_scores_.squared =
      {
        std::numeric_limits<u64>::max(),
        std::numeric_limits<u64>::max()
      };
    best_scores_.a_weighted_squared =
      {
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
      };

    for(std::size_t index = 0; index < candidates.size(); index++)
      for(int channel = 0; channel < channels_; channel++)
        if(encoder_score::improves(scores[index],
                                   best_scores_,
                                   channel,
                                   metric_))
          {
            best_scores_.squared[channel] = scores[index].squared[channel];
            best_scores_.a_weighted_squared[channel] =
              scores[index].a_weighted_squared[channel];
            best_index[channel] = index;
          }

    for(int channel = 0; channel < channels_; channel++)
      {
        std::vector<u8> encoded(output_size);

        encode_candidate(candidates[best_index[channel]],
                         input_,
                         frame_count_,
                         channels_,
                         freq_,
                         encoded);

        if(channels_ == 1)
          {
            output_ = encoded;
            fmt::print(" - selected encoder: {}\n",
                       candidate_name(candidates[best_index[channel]]));
          }
        else
          {
            // XQ stores four bytes per channel in each eight-byte group.
            // Copy the entire stream, including its final padded chunk.
            for(std::size_t offset = channel * 4; offset < output_size; offset += 8)
              std::copy_n(encoded.begin() + offset,4,output_.begin() + offset);

            fmt::print(" - selected encoder channel {}: {}\n",
                       channel + 1,
                       candidate_name(candidates[best_index[channel]]));
          }
      }
  }

  static
  void
  encode_best_adp4(const std::vector<s16> &input_,
                   const int               frame_count_,
                   const int               channels_,
                   const int               freq_,
                   const unsigned          threads_,
                   const search::Limits   &limits_,
                   const std::string      &selection_metric_,
                   std::vector<u8>        &output_)
  {
    const encoder_score::SelectionMetric metric =
      encoder_score::selection_metric(selection_metric_);
    encoder_score::Scores best_scores;
    double total_a_weighted = 0.0;
    u64 total_squared = 0;

    search_adp4(input_,
                frame_count_,
                channels_,
                freq_,
                threads_,
                limits_,
                metric,
                output_,
                best_scores);

    if(metric == encoder_score::SelectionMetric::Rms)
      {
        std::vector<s16> decoded(output_.size() * 2);

        best_scores = score_adp4(input_,
                                 output_,
                                 channels_,
                                 freq_,
                                 encoder_score::SelectionMetric::Perceptual,
                                 decoded);
      }

    for(int channel = 0; channel < channels_; channel++)
      {
        total_squared += best_scores.squared[channel];
        total_a_weighted += best_scores.a_weighted_squared[channel];
      }

    fmt::print(" - selection metric: {}\n"
               " - RMS reconstruction error: {:.6f}\n"
               " - A-weighted reconstruction error: {:.6f}\n",
               selection_metric_,
               encoder_score::rms_error(total_squared,input_.size()),
               encoder_score::a_weighted_rms_error(total_a_weighted,
                                                   input_.size()));
  }

  static
  void
  to_adp4(const std::filesystem::path &filepath_,
          const std::string           &input_type_,
          const std::string           &output_type_,
          const std::string           &encoder_,
          const int                    lookahead_,
          const std::string           &noise_shaping_,
          const std::string           &stereo_layout_,
          const std::string           &search_effort_,
          const std::string           &selection_metric_,
          const unsigned               threads_,
          const int                    channels_,
          const int                    freq_)
  {
    std::vector<s16> input_data;
    std::vector<u8> output_data;
    std::filesystem::path output_filepath;
    int encoded_frame_count;
    int output_size;
    size_t frame_count;

    input_data = l::load_file(input_type_,filepath_,channels_,freq_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);
    if(input_data.size() % channels_)
      throw fmt::exception("input sample count is not channel-aligned");

    frame_count = input_data.size() / channels_;
    if(frame_count > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw fmt::exception("input contains too many frames");

    encoded_frame_count = static_cast<int>(frame_count);
    output_filepath = filepath_;
    output_filepath += fmt::format(".adp4.{}ch.{}hz.{}",
                                   channels_,
                                   freq_,
                                   output_type_);

    output_size = adpcm_sample_count_to_block_size_no_header(encoded_frame_count,
                                                              channels_,
                                                              4);
    if(output_size <= 0)
      throw fmt::exception("failed to calculate ADP4 output size");

    output_data.resize(static_cast<size_t>(output_size));

    if(encoder_ == "xq")
      {
        encode_xq(input_data,
                  encoded_frame_count,
                  channels_,
                  freq_,
                  lookahead_,
                  xq_noise_shaping(noise_shaping_),
                  output_data);
      }
    else if(encoder_ == "best")
      {
        encode_best_adp4(input_data,
                         encoded_frame_count,
                         channels_,
                         freq_,
                         threads_,
                         search::limits(search_effort_),
                         selection_metric_,
                         output_data);
      }
    else
      {
        throw fmt::exception("unknown encoder '{}'",encoder_);
      }
    if((channels_ == 2) && (stereo_layout_ == "portfolio"))
      {
        adp4_stereo_layout::xq_to_portfolio(output_data);
        output_data.resize((frame_count + 3) & ~static_cast<size_t>(3));
      }

    if(output_type_ == "raw")
      {
        u64 rv;
        FILE *out_file;

        out_file = fopen(output_filepath.string().c_str(),"wb");
        if(out_file == NULL)
          throw fmt::exception("failed to open output {}",output_filepath);

        rv = fwrite(output_data.data(),
                    sizeof(decltype(output_data)::value_type),
                    output_data.size(),
                    out_file);

        fclose(out_file);

        if(rv != output_data.size())
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
      }
    else if(output_type_ == "aifc")
      {
        u64 rv;
        const int channels = channels_;
        const std::string format = "u8";
        const std::string codec = "adpcm_ima_ws";

        rv = ffmpeg::write(output_data.data(),
                           output_data.size(),
                           output_filepath,
                           format,
                           codec,
                           channels,
                           freq_);
        if(rv != output_data.size())
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
        if((channels_ == 2) && (stereo_layout_ == "portfolio"))
          patch_aifc_frame_count(output_filepath,
                                 static_cast<u32>(frame_count));
      }

    fmt::print(" - output file name: {}\n"
               " - sample count: {}\n"
               " - input data size: {}b\n"
               " - output data size: {}b\n"
               ,
               output_filepath,
               frame_count * channels_,
               input_data.size() * 2,
               output_data.size());
  }
}

void
SubCmd::to_adp4(const Opts::ToADP4 &opts_)
{
  if((opts_.output_type == "aifc") &&
     (opts_.output_channels == 2) &&
     (opts_.stereo_layout != "portfolio"))
    throw std::runtime_error("stereo ADP4 AIFC requires Portfolio layout");

  if(opts_.output_type != "raw")
    {
      if(!ffmpeg::ffmpeg_available())
        throw std::runtime_error("ffmpeg executable not found");
    }

  std::size_t failures = 0;

  for(auto &filepath : opts_.filepaths)
    {
      fmt::print("{}:\n",filepath);

      try
        {
        l::to_adp4(filepath,
                   opts_.input_type,
                   opts_.output_type,
                   opts_.encoder,
                   opts_.lookahead,
                   opts_.noise_shaping,
                   opts_.stereo_layout,
                   opts_.search_effort,
                   opts_.selection_metric,
                   parallel::resolve_threads(opts_.threads),
                   opts_.output_channels,
                   opts_.output_freq);
        }
      catch(const std::system_error &e_)
        {
          fmt::print(" - ERROR - {} - {} ({})\n",filepath,e_.what(),e_.code().message());
          failures++;
        }
      catch(const std::runtime_error &e_)
        {
          fmt::print(" - ERROR - {} - {}\n",filepath,e_.what());
          failures++;
        }
    }

  if(failures != 0)
    throw std::runtime_error(fmt::format("{} of {} file(s) failed",
                                         failures,
                                         opts_.filepaths.size()));
}
