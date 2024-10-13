#include "subcmd_play.hpp"

#include "CLI11.hpp"
#include "subprocess.h"

#include "fmt.hpp"

#include <cstddef>
#include <vector>

namespace l
{
  void
  play_adp4(const std::filesystem::path &filepath_)
  {
    std::vector<const char*> args;
    std::string filepath_str;

    filepath_str = filepath.string();
    args =
      {
        "ffplay",
        "-hide_banner",
        "-autoexit",
        "-volume","100",
        "-f","u8",
        "-acodec","adpcm_ima_ws",
        //"-ac","1",
        "-ar","22050",
        filepath_str.c_str(),
        NULL
      };

  }
  
  void
  play(Opts::Play const &opts_)
  {
    for(auto const &filepath : opts_.filepaths)
      {
        std::vector<const char*> args;
        std::string filepath_str;

        filepath_str = filepath.string();
        args =
          {
            "ffplay",
            "-hide_banner",
            "-autoexit",
            "-volume","100",
            "-f","u8",
            "-acodec","adpcm_ima_ws",
            //"-ac","1",
            "-ar","22050",
            filepath_str.c_str(),
            NULL
          };

        fmt::print("subcmd::play({});\n",filepath_str);
        struct subprocess_s subproc = {0};
        int rv = subprocess_create(args.data(),
                                   subprocess_option_combined_stdout_stderr|
                                   subprocess_option_enable_async|
                                   subprocess_option_inherit_environment|
                                   subprocess_option_search_user_path,
                                   &subproc);
        if(rv != 0)
          {
            args.pop_back();
            fmt::print("Error running {}\n",args);
            continue;
          }
        
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
