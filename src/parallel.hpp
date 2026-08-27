#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>


// Cross-platform std::thread helpers for independent work items.
namespace parallel
{
  // Returns at least one worker even when the platform reports no concurrency.
  inline
  unsigned
  default_threads(void)
  {
    const unsigned hardware = std::thread::hardware_concurrency();

    return (hardware == 0) ? 1U : hardware;
  }

  // Resolves a user-supplied worker count, where zero requests the platform default.
  inline
  unsigned
  resolve_threads(const int requested_)
  {
    if(requested_ <= 0)
      return default_threads();

    return static_cast<unsigned>(requested_);
  }

  // Invokes body_ once for every index in [0,count_) using up to threads_ workers.
  // Work is claimed dynamically so unevenly sized items stay balanced. body_ must
  // not share mutable state with another invocation; identical inputs therefore
  // produce identical results regardless of the worker count. The lowest-index
  // worker exception is rethrown once every worker has stopped.
  inline
  void
  for_each(const std::size_t                       count_,
           const unsigned                          threads_,
           const std::function<void(std::size_t)> &body_)
  {
    if((count_ == 0) || (threads_ <= 1) || (count_ == 1))
      {
        for(std::size_t index = 0; index < count_; index++)
          body_(index);
        return;
      }

    const std::size_t workers = std::min<std::size_t>(threads_,count_);
    std::atomic<std::size_t> next(0);
    std::mutex failure_mutex;
    std::vector<std::exception_ptr> failures(workers);
    std::vector<std::thread> pool;

    pool.reserve(workers - 1);

    const auto work = [&](const std::size_t worker_)
      {
        try
          {
            for(;;)
              {
                const std::size_t index =
                  next.fetch_add(1,std::memory_order_relaxed);

                if(index >= count_)
                  return;
                body_(index);
              }
          }
        catch(...)
          {
            const std::lock_guard<std::mutex> guard(failure_mutex);

            if(failures[worker_] == nullptr)
              failures[worker_] = std::current_exception();
            next.store(count_,std::memory_order_relaxed);
          }
      };

    // A worker that cannot start must not leave joinable threads behind:
    // destroying them would call std::terminate instead of propagating.
    try
      {
        for(std::size_t worker = 1; worker < workers; worker++)
          pool.emplace_back(work,worker);
      }
    catch(...)
      {
        next.store(count_,std::memory_order_relaxed);

        for(std::thread &thread : pool)
          thread.join();

        throw;
      }

    work(0);

    for(std::thread &thread : pool)
      thread.join();

    for(const std::exception_ptr &failure : failures)
      if(failure != nullptr)
        std::rethrow_exception(failure);
  }
}
