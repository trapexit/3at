#include "CLI11.hpp"
#include "fmt.hpp"
#include "options.hpp"
#include "subcmd_to_adp4.hpp"

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

  subcmd = app_.add_subcommand("to-adp4","");
  subcmd->add_option("filepaths",opts.filepaths)
    ->description("Path to source file")
    ->type_name("PATH")
    ->check(CLI::ExistingFile)
    ->required();

  auto func = std::bind(SubCmd::to_adp4,
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
}


int
main(int    argc_,
     char **argv_)
{
  CLI::App app("APPNAME: APP DESCRIPTION");
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
