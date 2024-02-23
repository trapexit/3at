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
                  "default: Intel/DVI encoder by trapexit")
    ->check(CLI::IsMember({"default"}))
    ->default_val("default");
  subcmd->add_option("--freq",opts.output_freq)
    ->description("Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);

  subcmd->footer("NOTE: Currently only outputs raw files.");

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
                  "default: SDX2 3DO encoder ported by trapexit")
    ->check(CLI::IsMember({"default"}))
    ->default_val("default");
  subcmd->add_option("--channels",opts.output_channels)
    ->description("Number of output audio channels")
    ->check(CLI::IsMember({1,2}))
    ->default_val(1);
  subcmd->add_option("--freq",opts.output_freq)
    ->description("Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);

  subcmd->footer("NOTE: Currently only outputs raw files.");

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
  subcmd->add_option("--freq",opts.freq)
    ->description("Input/Output frequency")
    ->check(CLI::IsMember({22050,44100}))
    ->default_val(22050);

  subcmd->footer("NOTE: Currently only outputs raw files.");

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

  subcmd->footer("NOTE: Currently only outputs raw files.");

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

static
void
set_locale(void)
{
  try
    {
      std::locale::global(std::locale(""));
    }
  catch(const std::runtime_error &e)
    {
      std::locale::global(std::locale("C"));
    }
}

int
main(int    argc_,
     char **argv_)
{
  CLI::App app;
  Opts::Options opts;
  std::string description;

  //  set_locale();
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
    }
  catch(const std::runtime_error &e_)
    {
      fmt::print("{}\n",e_.what());
    }

  return 0;
}
