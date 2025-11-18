#pragma once

#include <mutex>
#include <tuple>

namespace tools {

/// Takes any number of lockable objects, locks them atomically, and returns a tuple of
/// std::unique_lock holding the individual locks.
template <typename... T>
[[nodiscard]]
std::tuple<std::unique_lock<T>...> unique_locks(T& ...lockables) {
    std::lock(lockables...);
    auto locks = std::make_tuple(std::unique_lock<T>(lockables, std::adopt_lock)...);
    return locks;
}

}