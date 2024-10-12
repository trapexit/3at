#include "subcmd_play.hpp"

#include "fmt.hpp"

#include <vector>

namespace l
{
  void
  play(Opts::Play const &opts_)
  {
    for(auto &filepath : opts_.filepaths)
      {
        std::vector<char*> args;

        args.push_back("ffplay");
        args.push_back("-volume");
        args.push_back("-volume");
        args.push_back("-volume");
        args.push_back("-volume");
        args.push_back("-volume");
        
          
          fmt::print("subcmd::play({});\n",
                     filepath);
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
