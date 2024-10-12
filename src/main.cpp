#include "CLI11.hpp"
#include "fmt.hpp"
#include "options.hpp"
#include "subcmd_to_adp4.hpp"
#include "subcmd_to_sdx2.hpp"
#include "subcmd_play.hpp"

#include <unistd.h>

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;


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
  subcmd->add_option("--encoder",opts.encoder)
    ->description("")
    ->check(CLI::IsMember({"default"}))
    ->default_val("default");
  subcmd->add_option("--freq",opts.freq)
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
  subcmd->add_option("--channels",opts.channels)
    ->description("Number of audio channels")
    ->check(CLI::IsMember({1,2}))
    ->required();

  auto func = std::bind(SubCmd::to_sdx2,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_play_argparser(CLI::App      &app_,
                        Opts::Options &opts_)
{
  CLI::App *subcmd;
  auto &opts = opts_.play;

  subcmd = app_.add_subcommand("play","Play file using ffplay if available");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();

  auto func = std::bind(SubCmd::play,
                        std::cref(opts));

  subcmd->callback(func);
}

static
void
generate_argparser(CLI::App      &app_,
                   Opts::Options &opts_)
{
  app_.set_help_all_flag("--help-all","List help for all subcommands");
  app_.require_subcommand();

  generate_to_adp4_argparser(app_,opts_);
  generate_to_sdx2_argparser(app_,opts_);
  generate_play_argparser(app_,opts_);
}


int
main(int    argc_,
     char **argv_)
{
  CLI::App app("3at: 3DO Audio Tool");
  Opts::Options opts;

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
