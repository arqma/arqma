#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>
#include "crypto/crypto.h"

namespace tools
{
  class periodic_task
  {
  public:
    explicit periodic_task(std::chrono::microseconds interval, bool start_immediate = true, std::pair<int, int> random_delay_interval = {})
      : m_interval{interval}
      , m_last_worked_time{std::chrono::steady_clock::now()}
      , m_trigger_now{start_immediate}
      , m_random_delay_interval{random_delay_interval}
      , m_next_delay{std::chrono::microseconds(crypto::rand_range(m_random_delay_interval.first, m_random_delay_interval.second))}
    {}

    template <class functor_t>
    void do_call(functor_t functr)
    {
      if (m_trigger_now || std::chrono::steady_clock::now() - m_last_worked_time > (m_interval + m_next_delay))
      {
        functr();
        m_last_worked_time = std::chrono::steady_clock::now();
        m_trigger_now = false;
        m_next_delay = std::chrono::microseconds(crypto::rand_range(m_random_delay_interval.first, m_random_delay_interval.second));
      }
    }

    void reset() { m_trigger_now = true; }
    std::chrono::microseconds interval() const { return m_interval; }
    void interval(std::chrono::microseconds us) { m_interval = us; }

  private:
    std::chrono::microseconds m_interval;
    std::chrono::steady_clock::time_point m_last_worked_time;
    std::atomic<bool> m_trigger_now;
    std::pair<int, int> m_random_delay_interval;
    std::chrono::microseconds m_next_delay;
  };
};
