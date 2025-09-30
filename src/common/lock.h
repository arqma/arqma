#pragma once

#include <mutex>
#include <shared_mutex>
#include <tuple>
#include <boost/thread/lock_algorithms.hpp>

namespace tools {

/// Takes any number of lockable objects, locks them atomically, and returns a tuple of
/// std::unique_lock holding the individual locks.
template <typename... T>
#ifdef __GNUG__
[[gnu::warn_unused_result]]
#endif
std::tuple<std::unique_lock<T>...> unique_locks(T& ...lockables) {
    boost::lock(lockables...);
    auto locks = std::make_tuple(std::unique_lock<T>(lockables, std::adopt_lock)...);
    return locks;
}

/// Shortcut around getting a std::unique_lock<T> without worrying about T.  The first argument is
/// the mutex (or other Lockable object); it and any remaining args (such as `std::defer_lock`) are
/// forwarded to the std::unique_lock<T> constructor.
template <typename T, typename... Args>
#ifdef __GNUG__
[[gnu::warn_unused_result]]
#endif
std::unique_lock<T> unique_lock(T& lockable, Args&&... args) {
    return std::unique_lock<T>(lockable, std::forward<Args>(args)...);
}

/// Shortcut for getting a std::shared_lock<T> without worrying about T.  First argument is the
/// shared lockable; it any any remaining args (such as `std::defer_lock`) are forwarded to the
/// std::shared_lock<T> constructor.
template <typename T, typename... Args>
#ifdef __GNUG__
[[gnu::warn_unused_result]]
#endif
std::shared_lock<T> shared_lock(T& lockable, Args&&... args) {
    return std::shared_lock<T>(lockable, std::forward<Args>(args)...);
}

}