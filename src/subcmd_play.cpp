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
        std::vector<const char*> args;

        args.push_back("ffplay");
        args.push_back("-volume");
        args.push_back("25");
        args.push_back("-f");
        args.push_back("u8");
        args.push_back("-acodec");
        args.push_back("adpcm_ima_ws");
        args.push_back("-ac");
        args.push_back("1");
        args.push_back("-ar");
        args.push_back("22050");
        args.push_back(filepath.string().c_str());

        fmt::print("{}\n",args);
        fmt::print("subcmd::play({});\n",filepath);
        
      }
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
