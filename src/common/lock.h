#pragma once

#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <boost/thread/lock_algorithms.hpp>

namespace tools
{
  template <typename... T>
  [[nodiscard]]
  std::tuple<std::unique_lock<T>...> unique_locks(T& ...lockables)
  {
    boost::lock(lockables...);
    auto locks = std::make_tuple(std::unique_lock<T>(lockables, std::adopt_lock)...);
    return locks;
  }

  template <typename T, typename... Args>
  [[nodiscard]]
  std::unique_lock<T> unique_lock(T& lockable, Args&&... args) {
    return std::unique_lock<T>(lockable, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  [[nodiscard]]
  std::shared_lock<T> shared_lock(T& lockable, Args&&... args) {
    return std::shared_lock<T>(lockable, std::forward<Args>(args)...);
  }
}