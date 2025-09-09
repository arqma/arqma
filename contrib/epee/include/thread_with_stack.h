#pragma once

#include <pthread.h>
#include <functional>
#include <stdexcept>
#include <utility>

class ThreadWithStack {
public:
  ThreadWithStack(std::function<void()> func, size_t stack_size_bytes) {
    _func_ptr = new std::function<void()>(std::move(func));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stack_size_bytes);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&_thread, &attr, &ThreadWithStack::trampoline, _func_ptr) != 0) {
      pthread_attr_destroy(&attr);
      delete _func_ptr;
      throw std::runtime_error("Failed to launch thread with custom stack size");
    }

    pthread_attr_destroy(&attr);
  }

  void join() {
    pthread_join(_thread, nullptr);
  }

  void detach() {
    pthread_detach(_thread);
  }

  void get_id() {
    pthread_self();
  }

  ~ThreadWithStack() = default;

private:
  static void* trampoline(void* arg) {
    auto* fn = static_cast<std::function<void()>*>(arg);
    (*fn)();
    delete fn;
    return nullptr;
  }

  pthread_t _thread;
  std::function<void()>* _func_ptr;
};
