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

#include "options.hpp"

#include "ffmpeg.hpp"
#include "file.hpp"
#include "sdx2_encode.h"
#include "dpcm-xq.h"
#include "sdx2_decode.h"
#include "encoder_score.hpp"
#include "parallel.hpp"
#include "search_limits.hpp"

#include "fmt.hpp"

#include "types_ints.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

#include <cstdio>

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
  int
  xq_shaping_mode(const std::string &noise_shaping_)
  {
    if(noise_shaping_ == "off")
      return DPCM_XQ_SHAPING_OFF;
    if(noise_shaping_ == "static")
      return DPCM_XQ_SHAPING_STATIC;
    return DPCM_XQ_SHAPING_DYNAMIC;
  }

  static
  encoder_score::Scores
  score_sdx2(const std::vector<s16>          &input_,
             const std::vector<s8>           &encoded_,
             const int                        channels_,
             const int                        freq_,
             const encoder_score::SelectionMetric metric_,
             std::vector<s16>                &decoded_)
  {
    const s32 result = sdx2_decode(
      reinterpret_cast<const u8 *>(encoded_.data()),
      static_cast<u32>(encoded_.size()),
      channels_,
      decoded_.data(),
      static_cast<u32>(decoded_.size()));

    if(result != SDX2_SUCCESS)
      throw fmt::exception("failed to decode SDX2 candidate: {}",result);

    return encoder_score::score_by_channel(input_,
                                           decoded_,
                                           channels_,
                                           freq_,
                                           metric_);
  }

  struct Sdx2Candidate
  {
    enum Kind
    {
      Kind_Default,
      Kind_Trellis,
      Kind_TrellisWide,
      Kind_Xq
    };

    Kind kind;
    int  param;
    int  shaping;
  };

  static
  const char *
  shaping_name(const int shaping_)
  {
    static const char *names[] = { "off", "static", "dynamic" };

    return names[shaping_];
  }

  static
  std::string
  candidate_name(const Sdx2Candidate &candidate_)
  {
    switch(candidate_.kind)
      {
      case Sdx2Candidate::Kind_Trellis:
        return fmt::format("trellis/beam-width={}",candidate_.param);
      case Sdx2Candidate::Kind_TrellisWide:
        return fmt::format("trellis-wide/beam-width={}",candidate_.param);
      case Sdx2Candidate::Kind_Xq:
        return fmt::format("xq/lookahead={}/shaping={}",
                           candidate_.param,
                           shaping_name(candidate_.shaping));
      default:
        return "default";
      }
  }

  static
  std::vector<Sdx2Candidate>
  sdx2_candidates(const search::Limits &limits_)
  {
    std::vector<Sdx2Candidate> candidates;

    candidates.push_back({Sdx2Candidate::Kind_Default,0,0});

    // Beam width 1 is a duplicate of the default encoder above
    // (sdx2_encode calls sdx2_encode_trellis with width 1), so it is skipped.
    for(int beam = 2; beam <= limits_.max_beam; beam++)
      candidates.push_back({Sdx2Candidate::Kind_Trellis,beam,0});

    for(int lookahead = 0; lookahead <= limits_.max_lookahead; lookahead++)
      for(int shaping = 0; shaping < 3; shaping++)
        candidates.push_back({Sdx2Candidate::Kind_Xq,lookahead,shaping});

    for(int beam = 1; beam <= limits_.max_beam; beam++)
      candidates.push_back({Sdx2Candidate::Kind_TrellisWide,beam,0});

    return candidates;
  }

  static
  void
  encode_candidate(const Sdx2Candidate  &candidate_,
                   const std::vector<s16> &input_,
                   const int               channels_,
                   std::vector<s8>        &encoded_)
  {
    s32 result = SDX2_SUCCESS;

    std::fill(encoded_.begin(),encoded_.end(),0);

    switch(candidate_.kind)
      {
      case Sdx2Candidate::Kind_Trellis:
        result = sdx2_encode_trellis(input_.data(),
                                     static_cast<u32>(input_.size()),
                                     channels_,
                                     encoded_.data(),
                                     static_cast<u32>(encoded_.size()),
                                     candidate_.param);
        break;
      case Sdx2Candidate::Kind_TrellisWide:
        result = sdx2_encode_trellis_wide(input_.data(),
                                          static_cast<u32>(input_.size()),
                                          channels_,
                                          encoded_.data(),
                                          static_cast<u32>(encoded_.size()),
                                          candidate_.param);
        break;
      case Sdx2Candidate::Kind_Xq:
        if(!dpcm_xq_encode(input_.data(),
                           static_cast<u32>(input_.size() / channels_),
                           channels_,
                           reinterpret_cast<u8 *>(encoded_.data()),
                           candidate_.param,
                           candidate_.shaping))
          throw fmt::exception("dpcm-xq candidate failed");
        break;
      default:
        result = sdx2_encode(input_.data(),
                             static_cast<u32>(input_.size()),
                             channels_,
                             encoded_.data(),
                             static_cast<u32>(encoded_.size()));
        break;
      }

    if(result != SDX2_SUCCESS)
      throw fmt::exception("SDX2 candidate failed with error {}",result);
  }

  static
  void
  search_sdx2(const std::vector<s16>          &input_,
              const int                        channels_,
              const int                        freq_,
              const unsigned                   threads_,
              const search::Limits            &limits_,
              const encoder_score::SelectionMetric metric_,
              std::vector<s8>                 &output_,
              encoder_score::Scores           &best_scores_)
  {
    const std::vector<Sdx2Candidate> candidates = sdx2_candidates(limits_);
    std::vector<encoder_score::Scores> scores(candidates.size());
    const std::size_t output_size = output_.size();

    dpcm_xq_init();

    parallel::for_each(candidates.size(),
                       threads_,
                       [&](const std::size_t index_)
                       {
                         // Scratch is reused across candidates on this worker
                         // rather than allocated per candidate.
                         static thread_local std::vector<s8>  encoded;
                         static thread_local std::vector<s16> decoded;

                         encoded.resize(output_size);
                         decoded.resize(output_size);

                         encode_candidate(candidates[index_],input_,channels_,encoded);
                         scores[index_] =
                           score_sdx2(input_,
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
        std::vector<s8> encoded(output_size);

        encode_candidate(candidates[best_index[channel]],input_,channels_,encoded);

        const std::string name = candidate_name(candidates[best_index[channel]]);

        if(channels_ == 1)
          {
            output_ = encoded;
            fmt::print(" - selected encoder: {}\n",name);
          }
        else
          {
            // SDX2 channel streams occupy alternating bytes, including padding.
            for(std::size_t index = channel; index < output_size; index += channels_)
              output_[index] = encoded[index];

            fmt::print(" - selected encoder channel {}: {}\n",channel + 1,name);
          }
      }
  }

  static
  void
  encode_best_sdx2(const std::vector<s16> &input_,
                   const int               channels_,
                   const int               freq_,
                   const unsigned          threads_,
                   const search::Limits   &limits_,
                   const std::string      &selection_metric_,
                   std::vector<s8>        &output_)
  {
    const encoder_score::SelectionMetric metric =
      encoder_score::selection_metric(selection_metric_);
    encoder_score::Scores best_scores;
    double total_a_weighted = 0.0;
    u64 total_squared = 0;

    search_sdx2(input_,
                channels_,
                freq_,
                threads_,
                limits_,
                metric,
                output_,
                best_scores);

    if(metric == encoder_score::SelectionMetric::Rms)
      {
        std::vector<s16> decoded(output_.size());

        best_scores = score_sdx2(input_,
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
  to_sdx2(const std::filesystem::path &filepath_,
          const std::string           &input_type_,
          const std::string           &output_type_,
          const std::string           &encoder_,
          const int                    beam_width_,
          const int                    lookahead_,
          const std::string           &noise_shaping_,
          const std::string           &search_effort_,
          const std::string           &selection_metric_,
          const unsigned               threads_,
          const int                    channels_,
          const int                    freq_)
  {
    std::vector<s16> input_data;
    std::vector<s8>  output_data;
    std::filesystem::path output_filepath;
    std::size_t output_size;
    s32 result;

    input_data = l::load_file(input_type_,filepath_,channels_,freq_);
    if(input_data.empty())
      throw fmt::exception("failed to load {}",filepath_);

    if(input_data.size() % channels_)
      throw fmt::exception("input sample count is not channel-aligned");
    if(input_data.size() >
       (static_cast<std::size_t>(std::numeric_limits<u32>::max()) - 3))
      throw fmt::exception("input contains too many SDX2 samples");

    output_filepath = filepath_;
    output_filepath += fmt::format(".sdx2.{}ch.{}hz.{}",channels_,freq_,output_type_);

    // Pad the one-byte-per-sample output to a 3DO word boundary.
    output_size = (((input_data.size() + 3) / 4) * 4);
    output_data.resize(output_size);

    if(encoder_ == "default")
      {
        result = sdx2_encode(input_data.data(),
                             static_cast<u32>(input_data.size()),
                             channels_,
                             output_data.data(),
                             static_cast<u32>(output_data.size()));
      }
    else if(encoder_ == "trellis")
      {
        result = sdx2_encode_trellis(input_data.data(),
                                     static_cast<u32>(input_data.size()),
                                     channels_,
                                     output_data.data(),
                                     static_cast<u32>(output_data.size()),
                                     beam_width_);
      }
    else if(encoder_ == "xq")
      {
        if(!dpcm_xq_encode(input_data.data(),
                           static_cast<u32>(input_data.size() / channels_),
                           channels_,
                           reinterpret_cast<u8 *>(output_data.data()),
                           lookahead_,
                           xq_shaping_mode(noise_shaping_)))
          throw fmt::exception("dpcm-xq encoding failed");
        result = SDX2_SUCCESS;
      }
    else if(encoder_ == "best")
      {
        encode_best_sdx2(input_data,
                         channels_,
                         freq_,
                         threads_,
                         search::limits(search_effort_),
                         selection_metric_,
                         output_data);
        result = SDX2_SUCCESS;
      }
    else
      {
        throw fmt::exception("unknown encoder '{}'",encoder_);
      }
    if(result != SDX2_SUCCESS)
      throw fmt::exception("SDX2 encoder failed with error {}",result);

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
        const std::string format = "u8";
        const std::string codec = "sdx2_dpcm";

        rv = ffmpeg::write(output_data.data(),
                           output_data.size(),
                           output_filepath,
                           format,
                           codec,
                           channels_,
                           freq_);
        if(rv != output_data.size())
          throw fmt::exception("failed to write all data to file {} / {}",
                               rv,
                               output_data.size());
      }

    fmt::print(" - output file name: {}\n"
               " - sample count: {}\n"
               " - input data size: {}b\n"
               " - output data size: {}b\n"
               ,
               output_filepath,
               input_data.size(),
               input_data.size() * 2,
               output_data.size());
  }
}

void
SubCmd::to_sdx2(const Opts::ToSDX2 &opts_)
{
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
          l::to_sdx2(filepath,
                     opts_.input_type,
                     opts_.output_type,
                     opts_.encoder,
                     opts_.beam_width,
                     opts_.lookahead,
                     opts_.noise_shaping,
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
