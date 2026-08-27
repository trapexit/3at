// Regression coverage for the cross-platform parallel helper and search bounds.

#include "parallel.hpp"
#include "search_limits.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>


static
void
require(const bool        condition_,
        const char       *message_)
{
  if(condition_)
    return;

  std::fprintf(stderr,"FAIL: %s\n",message_);
  std::exit(EXIT_FAILURE);
}

static
void
require_equal(const std::size_t left_,
              const std::size_t right_,
              const char       *message_)
{
  if(left_ == right_)
    return;

  std::fprintf(stderr,"FAIL: %s (%zu != %zu)\n",message_,left_,right_);
  std::exit(EXIT_FAILURE);
}


static
void
test_visits_every_index_once(void)
{
  static const std::size_t counts[] = {0,1,7,1000};
  static const unsigned thread_counts[] = {1,2,3,8};

  for(const std::size_t count : counts)
    for(const unsigned threads : thread_counts)
      {
        std::vector<int> visits(count,0);

        parallel::for_each(count,
                           threads,
                           [&](const std::size_t index_)
                           {
                             visits[index_]++;
                           });

        for(const int visit : visits)
          require_equal(static_cast<std::size_t>(visit),1,"each index runs exactly once");
      }
}


static
void
test_reduction_matches_serial(void)
{
  // Every candidate writes its own slot, so thread count must not change results.
  std::vector<std::size_t> expected(300,0);

  for(std::size_t index = 0; index < expected.size(); index++)
    expected[index] = (index * index);

  for(const unsigned threads : {1U,2U,3U,8U})
    {
      std::vector<std::size_t> squares(300,0);

      parallel::for_each(squares.size(),
                         threads,
                         [&](const std::size_t index_)
                         {
                           squares[index_] = (index_ * index_);
                         });

      require(squares == expected,"parallel results match a serial loop");
    }
}


static
void
test_worker_exception_propagates(void)
{
  bool caught = false;

  try
    {
      parallel::for_each(64,
                         4,
                         [](const std::size_t index_)
                         {
                           if(index_ == 5)
                             throw std::runtime_error("worker failure");
                         });
    }
  catch(const std::runtime_error &)
    {
      caught = true;
    }

  require(caught,"worker exceptions reach the caller");
}


static
void
test_thread_resolution(void)
{
  require(parallel::default_threads() >= 1,"default thread count is at least one");
  require_equal(parallel::resolve_threads(0),
                parallel::default_threads(),
                "zero threads resolve to the platform default");
  require_equal(parallel::resolve_threads(3),3,"explicit thread counts are preserved");
  require_equal(parallel::resolve_threads(-4),
                parallel::default_threads(),
                "negative requests fall back to the platform default");
}


static
void
test_search_limits(void)
{
  const search::Limits fast = search::limits("fast");
  const search::Limits balanced = search::limits("balanced");
  const search::Limits exhaustive = search::limits("exhaustive");
  const search::Limits unknown = search::limits("nonsense");

  require_equal(fast.max_beam,8,"fast effort caps the beam width");
  require_equal(fast.max_lookahead,8,"fast effort caps the lookahead");
  require_equal(balanced.max_beam,16,"balanced effort caps the beam width");
  require_equal(balanced.max_lookahead,12,"balanced effort caps the lookahead");
  require_equal(exhaustive.max_beam,64,"exhaustive effort searches every beam width");
  require_equal(exhaustive.max_lookahead,16,"exhaustive effort searches every lookahead");
  require_equal(unknown.max_beam,exhaustive.max_beam,"unknown effort keeps the full search");
  require_equal(unknown.max_lookahead,
                exhaustive.max_lookahead,
                "unknown effort keeps the full lookahead");
}


int
main(void)
{
  test_visits_every_index_once();
  test_reduction_matches_serial();
  test_worker_exception_propagates();
  test_thread_resolution();
  test_search_limits();

  std::puts("Parallel helper and search bound regressions passed.");
  return EXIT_SUCCESS;
}
