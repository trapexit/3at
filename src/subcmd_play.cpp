#include "subcmd_play.hpp"

#include "fmt.hpp"


namespace l
{
  void
  play(Opts::Play const &opts_)
  {
    for(auto &filepath : opts_.filepaths)
      fmt::print("subcmd::play({});\n",
                 opts_.filepath);
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
