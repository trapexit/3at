#include "ffmpeg.hpp"
#include "subprocess.h"

#include "types_ints.h"

#include "fmt.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace l
{
  static
  bool
  executable_exists(const std::string &executable_)
  {
    int rv;
    struct subprocess_s subproc;
    std::vector<const char*> args;

    args =
      {
        executable_.c_str(),
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

    return true;
  }
}

bool
ffmpeg::ffmpeg_available(void)
{
  return l::executable_exists("ffmpeg");
}

bool
ffmpeg::ffplay_available(void)
{
  return l::executable_exists("ffplay");
}

bool
ffmpeg::ffprobe_available(void)
{
  return l::executable_exists("ffprobe");
}

bool
ffmpeg::file_recognizable(const std::filesystem::path &path_)
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
      "-map","0:a:0",
      "-frames:a","0",
      "-f","null",
      "-"
    };
  args.push_back(NULL);

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    return false;

  const bool join_error = (subprocess_join(&subproc,&rv) != 0);

  subprocess_destroy(&subproc);

  return (!join_error && (rv == 0));
}

std::vector<s16>
ffmpeg::to_s16le(const std::filesystem::path &filepath_,
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

  filepath = "file:" + filepath_.string();
  channels = fmt::format("{}",channels_);
  freq     = fmt::format("{}",freq_);

  args =
    {
      "ffmpeg",
      "-hide_banner",
      "-i",filepath.c_str(),
      "-vn",
      "-dn",
      "-sn",
      "-ac",channels.c_str(),
      "-ar",freq.c_str(),
      "-f","s16le",
      "-acodec","pcm_s16le",
      "pipe:1",
      SUBPROCESS_NULL
    };

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    throw fmt::exception("failed to start ffmpeg for {}",filepath_);

  outputf = subprocess_stdout(&subproc);

  std::vector<s16> tmpbuf;

  tmpbuf.resize(1024 * 64);
  while(!feof(outputf) && !ferror(outputf))
    {
      const size_t n = fread(tmpbuf.data(),2,tmpbuf.size(),outputf);

      // Let insert() grow geometrically rather than reallocating per chunk.
      buf.insert(buf.end(),
                 tmpbuf.begin(),
                 tmpbuf.begin() + n);
    }

  // Join before deciding: a failed or partial decode must not pass as complete.
  const bool read_error = (ferror(outputf) != 0);
  const bool join_error = (subprocess_join(&subproc,&rv) != 0);
  const bool failed = (read_error || join_error || (rv != 0));

  subprocess_destroy(&subproc);

  if(failed)
    throw fmt::exception("ffmpeg failed to decode {}",filepath_);

  return buf;
}

int
ffmpeg::freq(const std::filesystem::path &filepath_)
{
  int rv;
  int value;
  int proc_rv;
  FILE *outputf;
  std::string filepath;
  std::vector<const char*> args;
  struct subprocess_s subproc;

  filepath = filepath_.string();
  args =
    {
      "ffprobe",
      "-hide_banner",
      "-select_streams","a:0",
      "-show_entries","stream=sample_rate",
      "-of","default=noprint_wrappers=1:nokey=1",
      filepath.c_str(),
      NULL
    };

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    return -1;

  outputf = subprocess_stdout(&subproc);

  const bool parsed = (fscanf(outputf,"%i",&value) == 1);
  const bool join_error = (subprocess_join(&subproc,&proc_rv) != 0);

  subprocess_destroy(&subproc);

  if(!parsed || join_error || (proc_rv != 0))
    return -1;

  return value;
}

int
ffmpeg::channels(const std::filesystem::path &filepath_)
{
  int rv;
  int value;
  int proc_rv;
  FILE *outputf;
  std::string filepath;
  struct subprocess_s subproc;
  std::vector<const char*> args;

  filepath = filepath_.string();
  args = 
    {
      "ffprobe",
      "-hide_banner",
      "-select_streams","a:0",
      "-show_entries","stream=channels",
      "-of","default=noprint_wrappers=1:nokey=1",
      filepath.c_str(),
      NULL
    };

  rv = subprocess_create(args.data(),
                         subprocess_option_inherit_environment|
                         subprocess_option_search_user_path,
                         &subproc);
  if(rv != 0)
    return -1;

  outputf = subprocess_stdout(&subproc);

  const bool parsed = (fscanf(outputf,"%i",&value) == 1);
  const bool join_error = (subprocess_join(&subproc,&proc_rv) != 0);

  subprocess_destroy(&subproc);

  if(!parsed || join_error || (proc_rv != 0))
    return -1;

  return value;
}

u64
ffmpeg::write(const void                  *data_,
              const u64                    data_size_,
              const std::filesystem::path &filepath_,
              const std::string           &format_,
              const std::string           &codec_,
              const int                    channels_,
              const int                    freq_)
{
  u64 rv;
  int proc_rv;
  FILE *stdinf;
  std::string filepath;
  std::string channels;
  std::string freq;
  std::vector<const char*> args;
  struct subprocess_s subproc;

  filepath = "file:" + filepath_.string();
  channels = fmt::format("{}",channels_);
  freq     = fmt::format("{}",freq_);
  args =
    {
      "ffmpeg",
      "-y",
      "-hide_banner",
      "-f",format_.c_str(),
      "-acodec",codec_.c_str(),
      "-ac",channels.c_str(),
      "-ar",freq.c_str(),
      "-i","pipe:0",
      "-c:a","copy",
      filepath.c_str(),
      NULL
    };

  if(subprocess_create(args.data(),
                       subprocess_option_inherit_environment|
                       subprocess_option_search_user_path,
                       &subproc) != 0)
    return 0;

  stdinf = subprocess_stdin(&subproc);

  rv = fwrite(data_,1,data_size_,stdinf);
  fflush(stdinf);

  const bool join_error = (subprocess_join(&subproc,&proc_rv) != 0);

  subprocess_destroy(&subproc);

  // Zero signals failure so callers cannot mistake a rejected or truncated
  // conversion for a complete one.
  if(join_error || (proc_rv != 0) || (rv != data_size_))
    return 0;

  return rv;
}
