#pragma once

#include <mutex>
#include <tuple>

namespace tools
{
  template <typename... T>
  [[nodiscard]]
  std::tuple<std::unique_lock<T>...> unique_locks(T& ...lockables)
  {
    std::lock(lockables...);
    auto locks = std::make_tuple(std::unique_lock<T>(lockables, std::adopt_lock)...);
    return locks;
  }
}