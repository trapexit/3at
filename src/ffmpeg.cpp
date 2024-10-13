#include "ffmpeg.hpp"
#include "subprocess.h"

#include "types_ints.h"

#include "fmt.hpp"

#include <filesystem>
#include <string>
#include <vector>

bool
ffmpeg_file_recognizable(const std::filesystem::path &path_)
{
  int rv;
  std::string path;
  struct subprocess_s subproc;
  std::vector<const char*> args;

  path = path_.string();
  args =
    {
      "ffmpeg",
      "-hide_banner",
      "-loglevel","fatal",
      "-i",path.c_str(),
      "-f","null",
      "-",
      NULL
    };

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    return false;

  subprocess_join(&subproc,&rv);
  subprocess_destroy(&subproc);

  if(rv == 0)
    return true;
  return false;
}

std::vector<s16>
ffmpeg_to_s16le(const std::filesystem::path &filepath_,
                const int                    channels_,
                const int                    freq_)
{
  int rv;
  FILE *outputf;
  std::string filepath;
  std::string channels;
  std::string freq;
  std::vector<const char*> args;
  std::vector<s16> buf;
  struct subprocess_s subproc;

  filepath = filepath_.string();
  channels = fmt::format("{}",channels_);
  freq     = fmt::format("{}",freq_);
  
  args = {"ffmpeg",
          "-hide_banner",
          "-i",filepath.c_str(),
          "-ac",channels.c_str(),
          "-ar",freq.c_str(),
          "-f","s16le",
          "-acodec","pcm_s16le",
          "pipe:1",
          NULL};

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    return {};

  outputf = subprocess_stdout(&subproc);

  std::array<s16,2048> tmpbuf;
  while(!feof(outputf))
    {
      size_t n;

      n = fread(&tmpbuf[0],2,tmpbuf.size(),outputf);
      
      for(size_t i = 0; i < n; i++)
        buf.push_back(tmpbuf[i]);
    }

  subprocess_join(&subproc,&rv);
  subprocess_destroy(&subproc);

  return buf;
}
