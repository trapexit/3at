#include "subcmd_play.hpp"

#include "subprocess.h"

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

        args.push_back("/usr/bin/ffplay");
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
        args.push_back(NULL);

        fmt::print("subcmd::play({});\n",filepath);
        struct subprocess_s subproc;
        int rv = subprocess_create(args.data(),
                                   0,
                                   &subproc);
        fmt::print("rv = {}\n",rv);
        subprocess_join(&subproc,&rv);
        fmt::print("rv = {}\n",rv);
        subprocess_destroy(&subproc);
      }
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
