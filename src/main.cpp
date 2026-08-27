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

#include "CLI11.hpp"
#include "fmt.hpp"
#include "version.hpp"
#include "options.hpp"

#include "subcmd.hpp"

#include <cstdlib>

static
void
generate_version_argparser(CLI::App &app_)
{
  CLI::App *subcmd;

  subcmd = app_.add_subcommand("version","print 3at version");

  subcmd->callback(std::bind(SubCmd::version));
}

static
void
generate_to_adp4_argparser(CLI::App      &app_,
                           Opts::Options &opts_)
{
  CLI::App *subcmd;
  Opts::ToADP4 &opts = opts_.to_adp4;

  subcmd = app_.add_subcommand("to-adp4","Convert input to Intel/DVI ADP4 codec");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();
  subcmd->add_option("--input-type",opts.input_type)
    ->description("raw: Load file as raw data. Channels and freq ignored.\n"
                  "auto: Try to use ffmpeg to load file and fall back to raw.")
    ->check(CLI::IsMember({"raw","auto"}))
    ->default_val("auto");
  subcmd->add_option("--output-type",opts.output_type)
    ->description("Output format")
    ->check(CLI::IsMember({"raw","aifc"}))
    ->default_val("raw");
  subcmd->add_option("--encoder",opts.encoder)
    ->description("Encoder to use\n"
                  "xq: adpcm-xq lookahead and noise-shaped encoder\n"
                  "best: bounded candidate search")
    ->check(CLI::IsMember({"xq","best"}))
    ->default_val("xq");
  subcmd->add_option("--lookahead",opts.lookahead)
    ->description("adpcm-xq lookahead depth")
    ->check(CLI::Range(0,16))
    ->default_val(3);
  subcmd->add_option("--noise-shaping",opts.noise_shaping)
    ->description("adpcm-xq noise shaping mode")
    ->check(CLI::IsMember({"off","static","dynamic"}))
    ->default_val("dynamic");
  subcmd->add_option("--search-effort",opts.search_effort)
    ->description("best-mode search bounds\n"
                  "fast: beam width and lookahead up to 8\n"
                  "balanced: beam width up to 16, lookahead up to 12\n"
                  "exhaustive: full search (beam width 64, lookahead 16)")
    ->check(CLI::IsMember({"fast","balanced","exhaustive"}))
    ->default_val("exhaustive");
  subcmd->add_option("--selection",opts.selection_metric)
    ->description("best-mode candidate selection metric\n"
                  "perceptual: minimize A-weighted reconstruction error\n"
                  "rms: minimize unweighted squared reconstruction error")
    ->check(CLI::IsMember({"perceptual","rms"}))
    ->default_val("perceptual");
  subcmd->add_option("--threads",opts.threads)
    ->description("Worker threads for best-mode search (0 = hardware concurrency)")
    ->check(CLI::Range(0,256))
    ->default_val(0);
  subcmd->add_option("--stereo-layout",opts.stereo_layout)
    ->description("Stereo byte layout; AIFC requires portfolio")
    ->check(CLI::IsMember({"portfolio","xq"}))
    ->default_val("portfolio");
  subcmd->add_option("--channels",opts.output_channels)
    ->description("Number of output audio channels")
    ->check(CLI::IsMember({1,2}))
    ->default_val(1);
  subcmd->add_option("--freq",opts.output_freq)
    ->description("Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);


  auto func = std::bind(SubCmd::to_adp4,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_to_sdx2_argparser(CLI::App      &app_,
                           Opts::Options &opts_)
{
  CLI::App *subcmd;
  auto &opts = opts_.to_sdx2;

  subcmd = app_.add_subcommand("to-sdx2","Convert input to SDX2 codec");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();
  subcmd->add_option("--input-type",opts.input_type)
    ->description("raw: Load file as raw data. Channels and freq ignored.\n"
                  "auto: Try to use ffmpeg to load file and fall back to raw.")
    ->check(CLI::IsMember({"raw","auto"}))
    ->default_val("auto");
  subcmd->add_option("--output-type",opts.output_type)
    ->description("Output format")
    ->check(CLI::IsMember({"raw","aifc"}))
    ->default_val("raw");
  subcmd->add_option("--encoder",opts.encoder)
    ->description("Encoder to use\n"
                  "default: one-state trellis encoder (beam width 1)\n"
                  "trellis: block-beam encoder with boundary lookahead\n"
                  "xq: dpcm-xq recursive lookahead encoder\n"
                  "best: bounded candidate search")
    ->check(CLI::IsMember({"default","trellis","xq","best"}))
    ->default_val("trellis");
  subcmd->add_option("--beam-width",opts.beam_width)
    ->description("Trellis encoder beam width")
    ->check(CLI::Range(1,64))
    ->default_val(16);
  subcmd->add_option("--lookahead",opts.lookahead)
    ->description("dpcm-xq lookahead depth")
    ->check(CLI::Range(0,16))
    ->default_val(6);
  subcmd->add_option("--noise-shaping",opts.noise_shaping)
    ->description("dpcm-xq noise shaping mode")
    ->check(CLI::IsMember({"off","static","dynamic"}))
    ->default_val("dynamic");
  subcmd->add_option("--search-effort",opts.search_effort)
    ->description("best-mode search bounds\n"
                  "fast: beam width and lookahead up to 8\n"
                  "balanced: beam width up to 16, lookahead up to 12\n"
                  "exhaustive: full search (beam width 64, lookahead 16)")
    ->check(CLI::IsMember({"fast","balanced","exhaustive"}))
    ->default_val("exhaustive");
  subcmd->add_option("--selection",opts.selection_metric)
    ->description("best-mode candidate selection metric\n"
                  "perceptual: minimize A-weighted reconstruction error\n"
                  "rms: minimize unweighted squared reconstruction error")
    ->check(CLI::IsMember({"perceptual","rms"}))
    ->default_val("perceptual");
  subcmd->add_option("--threads",opts.threads)
    ->description("Worker threads for best-mode search (0 = hardware concurrency)")
    ->check(CLI::Range(0,256))
    ->default_val(0);
  subcmd->add_option("--channels",opts.output_channels)
    ->description("Number of output audio channels")
    ->check(CLI::IsMember({1,2}))
    ->default_val(1);
  subcmd->add_option("--freq",opts.output_freq)
    ->description("Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);


  auto func = std::bind(SubCmd::to_sdx2,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_from_adp4_argparser(CLI::App      &app_,
                             Opts::Options &opts_)
{
  CLI::App *subcmd;
  Opts::FromADP4 &opts = opts_.from_adp4;

  subcmd = app_.add_subcommand("from-adp4",
                               "Convert from raw Intel/DVI ADP4");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();
  subcmd->add_option("--output-type",opts.output_type)
    ->description("")
    ->check(CLI::IsMember({"raw","aiff","wav"}))
    ->default_val("raw");
  subcmd->add_option("--stereo-layout",opts.stereo_layout)
    ->description("Stereo byte layout")
    ->check(CLI::IsMember({"portfolio","xq"}))
    ->default_val("portfolio");
  subcmd->add_option("--channels",opts.channels)
    ->description("Number of channels")
    ->check(CLI::IsMember({1,2}))
    ->default_val(1);
  subcmd->add_option("--freq",opts.freq)
    ->description("Input/Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);


  auto func = std::bind(SubCmd::from_adp4,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_from_sdx2_argparser(CLI::App      &app_,
                             Opts::Options &opts_)
{
  CLI::App *subcmd;
  Opts::FromSDX2 &opts = opts_.from_sdx2;

  subcmd = app_.add_subcommand("from-sdx2","Convert from raw SDX2");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();
  subcmd->add_option("--channels",opts.channels)
    ->description("Number of channels")
    ->check(CLI::IsMember({1,2}))
    ->default_val(1);
  subcmd->add_option("--output-type",opts.output_type)
    ->description("")
    ->check(CLI::IsMember({"raw","aiff","wav"}))
    ->default_val("raw");
  subcmd->add_option("--freq",opts.freq)
    ->description("Input/Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);  


  auto func = std::bind(SubCmd::from_sdx2,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_argparser(CLI::App      &app_,
                   Opts::Options &opts_)
{
  app_.set_help_all_flag("--help-all",
                         "List help for all subcommands");
  app_.require_subcommand();

  generate_to_adp4_argparser(app_,opts_);
  generate_to_sdx2_argparser(app_,opts_);
  generate_from_adp4_argparser(app_,opts_);
  generate_from_sdx2_argparser(app_,opts_);
  generate_version_argparser(app_);
}

int
main(int    argc_,
     char **argv_)
{
  CLI::App app;
  Opts::Options opts;
  std::string description;

  description = fmt::format("3at: 3DO Audio Tool v{}.{}.{}",
                            MAJOR,
                            MINOR,
                            PATCH);

  app.description(description);

  generate_argparser(app,opts);

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e_)
    {
      return app.exit(e_);
    }
  catch(const std::system_error &e_)
    {
      fmt::print("{} ({})\n",e_.what(),e_.code().message());
      return EXIT_FAILURE;
    }
  catch(const std::runtime_error &e_)
    {
      fmt::print("{}\n",e_.what());
      return EXIT_FAILURE;
    }

  return 0;
}
