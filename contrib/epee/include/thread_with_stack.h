#pragma once

#include <pthread.h>
#include <functional>
#include <stdexcept>
#include <utility>
#include <thread>
#include <sstream>

#define THREAD_STACK_SIZE 8*1024*1024

class thread_with_stack {
public:
  using id = std::thread::id;
  using native_handle_type = pthread_t;

  thread_with_stack() noexcept : thread_(0), joinable_(false) {}

  template <typename Function, typename... Args>
  explicit thread_with_stack(size_t stack_size, Function(f), Args&&... args) {
    start(stack_size, std::forward<Function>(f), std::forward<Args>(args)...);
  }

  thread_with_stack(const thread_with_stack&) = delete;
  thread_with_stack& operator=(const thread_with_stack&) = delete;

  thread_with_stack(thread_with_stack&& other) noexcept
    : thread_(other.thread_)
    , joinable_(other.joinable_)
    , id_(other.id_)
  {
    other.thread_ = 0;
    other.joinable_ = false;
    other.id_ = id();
  }

  thread_with_stack& operator=(thread_with_stack&& other) noexcept {
    if (this != &other) {
      if (joinable_)
        pthread_detach(thread_);
      thread_ = other.thread_;
      joinable_ = other.joinable_;
      id_ = other.id_;
      other.thread_ = 0;
      other.joinable_ = false;
      other.id_ = id();
    }
    return *this;
  }

  template <typename Function, typename... Args>
  void start(size_t stack_size, Function&& f, Args&&... args) {
    std::function<void()> bound = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);
    auto task_ptr = new std::function<void()>(std::move(bound));
    auto pack = new std::pair<std::function<void()>*, id*>(task_ptr, &id_);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stack_size > 0)
      pthread_attr_setstacksize(&attr, stack_size);

    int rc = pthread_create(&thread_, &attr,
      [](void* p) -> void* {
        auto pack = static_cast<std::pair<std::function<void()>*, id*>*>(p);
        *(pack->second) = std::this_thread::get_id();
        (*(pack->first))();
        delete pack->first;
        delete pack;
        return nullptr;
      },
      pack);

    pthread_attr_destroy(&attr);

    if (rc != 0) {
      delete task_ptr;
      delete pack;
      throw std::runtime_error("pthread_create failed with code " + std::to_string(rc));
    }
    joinable_ = true;
  }

  void join() {
    if (joinable_) {
      pthread_join(thread_, nullptr);
      joinable_ = false;
    }
  }

  void detach() {
    if (joinable_) {
      pthread_detach(thread_);
      joinable_ = false;
    }
  }

  bool joinable() const noexcept {
    return joinable_;
  }

  id get_id() const noexcept {
    return id_;
  }

  native_handle_type native_handle() const noexcept {
    return thread_;
  }

  ~thread_with_stack() {
    if (joinable_) {
      pthread_detach(thread_);
    }
  }

private:
  pthread_t thread_;
  bool joinable_;
  id id_;
};

