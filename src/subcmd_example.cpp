#include "subcmd_example.hpp"

#include "fmt.hpp"


namespace l
{
  void
  example(Opts::Example const &opts_)
  {
    fmt::print("subcmd::example({});\n",
               opts_.filepath);
  }
}

void
SubCmd::example(Opts::Example const &opts_)
{
  l::example(opts_);
}
