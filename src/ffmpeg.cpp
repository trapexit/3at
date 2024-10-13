#include "ffmpeg.hpp"
#include "subprocess.h"

#include "types_ints.h"

#include "fmt.hpp"

#include <cstdint>
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

      n = fread(tmpbuf.data(),2,tmpbuf.size(),outputf);
      buf.reserve(buf.size() + n);
      buf.insert(buf.end(),
                 tmpbuf.begin(),
                 tmpbuf.begin() + n);
    }

  subprocess_join(&subproc,&rv);
  subprocess_destroy(&subproc);

  return buf;
}

int
ffmpeg_write_aifc(const void                  *data_,
                  const int                    data_size_,
                  const std::filesystem::path &filepath_,
                  const std::string           &codec_,
                  const int                    channels_,
                  const int                    freq_)
{
  std::string filepath;
  std::string channels;
  std::string freq;
  std::vector<const char*> args;
  struct subprocess_s subproc;

  filepath = filepath_.string();
  channels = fmt::format("{}",channels_);
  freq     = fmt::format("{}",freq_);
  
  args =
    {
      "ffmpeg",
      "-hide_banner",
      "-acodec",codec_.c_str(),
      "-ac","",
      "-ar","",
      "-i","pipe:0",
      "-c:a","copy",
      filepath_str.c_str(),
      NULL
    };
  
  return 0;
}
