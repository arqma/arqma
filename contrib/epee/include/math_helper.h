// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>

namespace epee
{
namespace math_helper
{
  class periodic_task
  {
  public:
    explicit periodic_task(std::chrono::microseconds interval, bool start_immediate = true)
      : m_interval{interval}, m_last_worked_time{std::chrono::steady_clock::now()}, m_trigger_now{start_immediate}
    {}

    template <class functor_t>
    void do_call(functor_t functr)
    {
      if (m_trigger_now || std::chrono::steady_clock::now() - m_last_worked_time > m_interval)
      {
        functr();
        m_last_worked_time = std::chrono::steady_clock::now();
        m_trigger_now = false;
      }
    }

    void reset() { m_trigger_now = true; }
    std::chrono::microseconds interval() const { return m_interval; }
    void interval(std::chrono::microseconds us) { m_interval = us; }

  private:
    std::chrono::microseconds m_interval;
    std::chrono::steady_clock::time_point m_last_worked_time;
    std::atomic<bool> m_trigger_now;
  };
}
}
