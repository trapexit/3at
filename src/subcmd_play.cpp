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
    int rv;
    std::vector<const char*> args;
    std::string filepath_str;
    struct subprocess_s subproc;
    
    filepath_str = filepath_.string();
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

    rv = subprocess_create(args.data(),
                           subprocess_option_combined_stdout_stderr|
                           subprocess_option_enable_async|
                           subprocess_option_inherit_environment|
                           subprocess_option_search_user_path,
                           &subproc);
    if(rv != 0)
      return;

    std::array<char,1024> buf;
    while(true)
      {
        int err;

        err = subprocess_read_stdout(&subproc,&buf[0],buf.size()-1);
        if(err == 0)
          break;
        fwrite(buf.data(),1,err,stdout);
      }

    subprocess_join(&subproc,&rv);
    subprocess_destroy(&subproc);
  }
  
  void
  play(Opts::Play const &opts_)
  {
    for(auto const &filepath : opts_.filepaths)
      {
        l::play_adp4(filepath_);
      }
  }
}

void
SubCmd::play(Opts::Play const &opts_)
{
  l::play(opts_);
}
