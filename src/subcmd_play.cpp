#include "subcmd_play.hpp"

#include "subprocess.h"

#include "fmt.hpp"

#include <vector>

namespace l
{
  void
  play(Opts::Play const &opts_)
  {
    for(auto const &filepath : opts_.filepaths)
      {
        std::vector<const char*> args;
        std::string filepath_str;

        filepath_str = filepath.string();
        args.push_back("ffplay");
        args.push_back("-hide_banner");
        args.push_back("-autoexit");
        args.push_back("-volume");
        args.push_back("5");
        args.push_back("-f");
        args.push_back("u8");
        args.push_back("-acodec");
        args.push_back("adpcm_ima_ws");
        args.push_back("-ac");
        args.push_back("1");
        args.push_back("-ar");
        args.push_back("22050");
        args.push_back(filepath_str.c_str());
        args.push_back(NULL);

        fmt::print("subcmd::play({});\n",filepath.string().c_str());
        struct subprocess_s subproc = {0};
        int rv = subprocess_create(args.data(),
                                   subprocess_option_combined_stdout_stderr|
                                   subprocess_option_enable_async|
                                   subprocess_option_inherit_environment|
                                   subprocess_option_search_user_path,
                                   &subproc);
        std::vector<char> buf;

        buf.resize(80 * 2 + 1);
        while(true)
          {
            int err;

            err = subprocess_read_stdout(&subproc,&buf[0],buf.size()-1);
            if(err == 0)
              break;
            buf[err] = 0;
            fmt::print("{}",buf.data());
          }

        subprocess_join(&subproc,&rv);
        subprocess_destroy(&subproc);
      }
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
