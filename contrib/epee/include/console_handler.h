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

#include "misc_log_ex.h"
#include "string_tools.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <iostream>
#ifdef __OpenBSD__
#include <stdio.h>
#endif
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#ifdef HAVE_READLINE
  #include "readline_buffer.h"
#endif

namespace epee
{
  class async_stdin_reader
  {
  public:
    async_stdin_reader()
      : m_run(true)
      , m_has_read_request(false)
      , m_read_status(state_init)
    {
#ifdef HAVE_READLINE
      m_readline_buffer.start();
#endif
      m_reader_thread = std::thread([this]() { return reader_thread_func(); });
    }

    ~async_stdin_reader()
    {
      try { stop(); }
      catch (...) { /* ignore */ }
    }

#ifdef HAVE_READLINE
    rdln::readline_buffer& get_readline_buffer()
    {
      return m_readline_buffer;
    }
#endif

    // Not thread safe. Only one thread can call this method at once.
    bool get_line(std::string& line)
    {
      if (!start_read())
        return false;

      if (state_eos == m_read_status)
        return false;

      std::unique_lock<std::mutex> lock(m_response_mutex);
      m_response_cv.wait(lock, [this] { return m_read_status != state_init; });

      bool res = false;
      if (state_success == m_read_status)
      {
        line = m_line;
        res = true;
      }

      if (!eos())
        m_read_status = state_init;

      return res;
    }

    bool eos() const { return m_read_status == state_eos; }

    void stop()
    {
      if (m_run)
      {
        m_run.store(false, std::memory_order_relaxed);

#if defined(WIN32)
        ::CloseHandle(::GetStdHandle(STD_INPUT_HANDLE));
#endif

        m_request_cv.notify_one();
        m_reader_thread.join();
#ifdef HAVE_READLINE
        m_readline_buffer.stop();
#endif
      }
    }

  private:
    bool start_read()
    {
      std::unique_lock<std::mutex> lock(m_request_mutex);
      if (!m_run.load(std::memory_order_relaxed) || m_has_read_request)
        return false;

      m_has_read_request = true;
      m_request_cv.notify_one();
      return true;
    }

    bool wait_read()
    {
      std::unique_lock<std::mutex> lock(m_request_mutex);
      m_request_cv.wait(lock, [this] { return m_has_read_request || !m_run; });

      if (m_has_read_request)
      {
        m_has_read_request = false;
        return true;
      }

      return false;
    }

    bool wait_stdin_data()
    {
#if !defined(WIN32)
      #if defined(__OpenBSD__) || defined(__ANDROID__)
      int stdin_fileno = fileno(stdin);
      #else
      int stdin_fileno = ::fileno(stdin);
      #endif

      while (m_run.load(std::memory_order_relaxed))
      {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(stdin_fileno, &read_set);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;

        int retval = ::select(stdin_fileno + 1, &read_set, NULL, NULL, &tv);
        if (retval < 0)
          return false;
        else if (0 < retval)
          return true;
      }
#else
      while (m_run.load(std::memory_order_relaxed))
      {
        DWORD retval = ::WaitForSingleObject(::GetStdHandle(STD_INPUT_HANDLE), 100);
        switch (retval)
        {
          case WAIT_FAILED:
            return false;
          case WAIT_OBJECT_0:
            return true;
          default:
            break;
        }
      }
#endif

      return true;
    }

    void reader_thread_func()
    {
      while (true)
      {
        if (!wait_read())
          break;

        std::string line;
        bool read_ok = true;
#ifdef HAVE_READLINE
reread:
#endif
        if (wait_stdin_data())
        {
          if (m_run.load(std::memory_order_relaxed))
          {
#ifdef HAVE_READLINE
            switch (m_readline_buffer.get_line(line))
            {
            case rdln::empty:   goto eof;
            case rdln::partial: goto reread;
            case rdln::full:    break;
            }
#else
            std::getline(std::cin, line);
#endif
            read_ok = !std::cin.eof() && !std::cin.fail();
          }
        }
        else
        {
          read_ok = false;
        }
        if (std::cin.eof()) {
#ifdef HAVE_READLINE
eof:
#endif
          m_read_status = state_eos;
          m_response_cv.notify_one();
          break;
        }
        else
        {
          std::unique_lock<std::mutex> lock(m_request_mutex);
          if (m_run.load(std::memory_order_relaxed))
          {
            m_line = std::move(line);
            m_read_status = read_ok ? state_success : state_error;
          }
          else
          {
            m_read_status = state_cancelled;
          }
          m_response_cv.notify_one();
        }
      }
    }

    enum t_state
    {
      state_init,
      state_success,
      state_error,
      state_cancelled,
      state_eos
    };

  private:
    std::thread m_reader_thread;
    std::atomic<bool> m_run;
#ifdef HAVE_READLINE
    rdln::readline_buffer m_readline_buffer;
#endif

    std::string m_line;
    bool m_has_read_request;
    t_state m_read_status;

    std::mutex m_request_mutex;
    std::mutex m_response_mutex;
    std::condition_variable m_request_cv;
    std::condition_variable m_response_cv;
  };


  template<class t_server>
  bool empty_commands_handler(t_server* psrv, const std::string& command)
  {
    return true;
  }


  class async_console_handler
  {
  public:
    async_console_handler()
    {
    }

    template<class t_server, class chain_handler>
    bool run(t_server* psrv, chain_handler ch_handler, std::function<std::string(void)> prompt, const std::string& usage = "")
    {
      return run(prompt, usage, [&](const std::string& cmd) { return ch_handler(psrv, cmd); }, [&] { psrv->send_stop_signal(); });
    }

    template<class chain_handler>
    bool run(chain_handler ch_handler, std::function<std::string(void)> prompt, const std::string& usage = "", std::function<void(void)> exit_handler = NULL)
    {
      return run(prompt, usage, [&](const std::string& cmd) { return ch_handler(cmd); }, exit_handler);
    }

    void stop()
    {
      m_running = false;
      m_stdin_reader.stop();
    }

    void print_prompt()
    {
      std::string prompt = m_prompt();
      if (!prompt.empty())
      {
#ifdef HAVE_READLINE
        std::string color_prompt = "\001\033[1;33m\002" + prompt;
        if (' ' != prompt.back())
          color_prompt += " ";
        color_prompt += "\001\033[0m\002";
        m_stdin_reader.get_readline_buffer().set_prompt(color_prompt);
#else
        epee::set_console_color(epee::console_color_yellow, true);
        std::cout << prompt;
        if (' ' != prompt.back())
          std::cout << ' ';
        epee::reset_console_color();
        std::cout.flush();
#endif
      }
    }

  private:
    template<typename t_cmd_handler>
    bool run(std::function<std::string(void)> prompt, const std::string& usage, const t_cmd_handler& cmd_handler, std::function<void(void)> exit_handler)
    {
      bool continue_handle = true;
      m_prompt = prompt;
      while(continue_handle)
      {
        try
        {
          if (!m_running)
          {
            break;
          }
          print_prompt();

          std::string command;
          bool get_line_ret = m_stdin_reader.get_line(command);
          if (!m_running)
            break;
          if (m_stdin_reader.eos())
          {
            MGINFO("EOF on stdin, exiting");
            std::cout << std::endl;
            break;
          }
          if (!get_line_ret)
          {
            MERROR("Failed to read line.");
          }
          string_tools::trim(command);

          LOG_PRINT_L2("Read command: " << command);
          if (command.empty())
          {
            continue;
          }
          else if(cmd_handler(command))
          {
            continue;
          }
          else if(0 == command.compare("exit") || 0 == command.compare("q"))
          {
            continue_handle = false;
          }
          else
          {
#ifdef HAVE_READLINE
            rdln::suspend_readline pause_readline;
#endif
            std::cout << "unknown command: " << command << std::endl;
            std::cout << usage;
          }
        }
        catch (const std::exception& ex)
        {
          LOG_ERROR("Exception at [console_handler], what=" << ex.what());
        }
      }
      if (exit_handler)
        exit_handler();
      return true;
    }

  private:
    async_stdin_reader m_stdin_reader;
    std::atomic<bool> m_running = {true};
    std::function<std::string(void)> m_prompt;
  };


  template<class t_server, class t_handler>
  bool start_default_console(t_server* ptsrv, t_handler handlr, std::function<std::string(void)> prompt, const std::string& usage = "")
  {
    std::shared_ptr<async_console_handler> console_handler = std::make_shared<async_console_handler>();
    std::thread([=](){console_handler->run<t_server, t_handler>(ptsrv, handlr, prompt, usage);}).detach();
    return true;
  }

  template<class t_server, class t_handler>
  bool start_default_console(t_server* ptsrv, t_handler handlr, const std::string& prompt, const std::string& usage = "")
  {
    return start_default_console(ptsrv, handlr, [prompt](){ return prompt; }, usage);
  }

  template<class t_server>
  bool start_default_console(t_server* ptsrv, const std::string& prompt, const std::string& usage = "")
  {
    return start_default_console(ptsrv, empty_commands_handler<t_server>, prompt, usage);
  }

  template<class t_server, class t_handler>
    bool no_srv_param_adapter(t_server* ptsrv, const std::string& cmd, t_handler handlr)
    {
      return handlr(cmd);
    }

  template<class t_server, class t_handler>
  bool run_default_console_handler_no_srv_param(t_server* ptsrv, t_handler handlr, std::function<std::string(void)> prompt, const std::string& usage = "")
  {
    async_console_handler console_handler;
    return console_handler.run(ptsrv, [=](auto& a, auto& b) { return no_srv_param_adapter<t_server, t_handler>(a, b, handlr); }, prompt, usage);
  }

  template<class t_server, class t_handler>
  bool run_default_console_handler_no_srv_param(t_server* ptsrv, t_handler handlr, const std::string& prompt, const std::string& usage = "")
  {
    return run_default_console_handler_no_srv_param(ptsrv, handlr, [prompt](){return prompt;},usage);
  }

  template<class t_server, class t_handler>
  bool start_default_console_handler_no_srv_param(t_server* ptsrv, t_handler handlr, std::function<std::string(void)> prompt, const std::string& usage = "")
  {
    std::thread(std::bind(run_default_console_handler_no_srv_param<t_server, t_handler>, ptsrv, handlr, prompt, usage) );
    return true;
  }

  template<class t_server, class t_handler>
  bool start_default_console_handler_no_srv_param(t_server* ptsrv, t_handler handlr, const std::string& prompt, const std::string& usage = "")
  {
    return start_default_console_handler_no_srv_param(ptsrv, handlr, [prompt](){return prompt;}, usage);
  }

  class command_handler {
  public:
    typedef std::function<bool (const std::vector<std::string> &)> callback;
    typedef std::map<std::string, std::pair<callback, std::pair<std::string, std::string> > > lookup;

    template <typename Function>
    void for_each(Function f)
    {
      for (const auto& x : m_command_handlers)
        f(x.first, x.second.second.first, x.second.second.second);
    }

    std::pair<std::string, std::string> get_documentation(const std::vector<std::string>& cmd)
    {
      if(cmd.empty())
        return {"", ""};
      auto it = m_command_handlers.find(cmd.front());
      if(it == m_command_handlers.end())
        return {"", ""};
      return it->second.second;
    }

    void set_handler(const std::string& cmd, callback hndlr, std::string usage = "", std::string description = "")
    {
      lookup::mapped_type & vt = m_command_handlers[cmd];
      vt.first = std::move(hndlr);
      if (description.empty())
        vt.second = {cmd, std::move(usage)};
      else
        vt.second = {std::move(usage), std::move(description)};
#ifdef HAVE_READLINE
      rdln::readline_buffer::add_completion(cmd);
#endif
    }

    bool process_command(const std::vector<std::string>& cmd)
    {
      if(!cmd.size())
        return false;
      auto it = m_command_handlers.find(cmd.front());
      if(it == m_command_handlers.end())
        return false;
      std::vector<std::string> cmd_local(cmd.begin()+1, cmd.end());
      return it->second.first(cmd_local);
    }

    bool process_command(const std::string& cmd)
    {
      std::vector<std::string> cmd_v;
      boost::split(cmd_v,cmd,boost::is_any_of(" "), boost::token_compress_on);
      return process_command(cmd_v);
    }
  private:
    lookup m_command_handlers;
  };

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class console_handlers_binder : public command_handler
  {
    typedef command_handler::callback console_command_handler;
    typedef command_handler::lookup command_handlers_map;
    std::thread m_console_thread;
    async_console_handler m_console_handler;
  public:
    bool start_handling(std::function<std::string(void)> prompt, const std::string& usage_string = "", std::function<void(void)> exit_handler = NULL)
    {
      m_console_thread = std::thread{std::bind(&console_handlers_binder::run_handling, this, prompt, usage_string, exit_handler)};
      m_console_thread.detach();
      return true;
    }
    bool start_handling(const std::string &prompt, const std::string& usage_string = "", std::function<void(void)> exit_handler = NULL)
    {
      return start_handling([prompt](){ return prompt; }, usage_string, exit_handler);
    }

    void stop_handling()
    {
      m_console_handler.stop();
    }

    bool run_handling(std::function<std::string(void)> prompt, const std::string& usage_string, std::function<void(void)> exit_handler = NULL)
    {
      return m_console_handler.run([this](const auto& arg) { return process_command(arg); }, prompt, usage_string, exit_handler);
    }

    void print_prompt()
    {
      m_console_handler.print_prompt();
    }
  };
}
