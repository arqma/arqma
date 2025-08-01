// Copyright (c) 2018-2022, The Arqma Network
// Copyright (c) 2014-2018, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <boost/optional.hpp>
#include <boost/endian/conversion.hpp>
#include <system_error>
#include <csignal>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>

#ifdef _WIN32
#include "windows.h"
#include "misc_log_ex.h"
#endif

#include "crypto/hash.h"
#include "cryptonote_config.h"

/*! \brief Various Tools
 *
 *
 *
 */
namespace tools
{
  //! Functional class for closing C file handles.
  struct close_file
  {
    void operator()(std::FILE* handle) const noexcept
    {
      if (handle)
      {
        std::fclose(handle);
      }
    }
  };

  void copy_file(const std::string& from, const std::string& to);

  //! A file restricted to process owner AND process. Deletes file on destruction.
  class private_file {
    std::unique_ptr<std::FILE, close_file> m_handle;
    std::string m_filename;

    private_file(std::FILE* handle, std::string&& filename) noexcept;
  public:

    //! `handle() == nullptr && filename.empty()`.
    private_file() noexcept;

    /*! \return File only readable by owner and only used by this process
      OR `private_file{}` on error. */
    static private_file create(std::string filename);

    private_file(private_file&&) = default;
    private_file& operator=(private_file&&) = default;

    //! Deletes `filename()` and closes `handle()`.
    ~private_file() noexcept;

    std::FILE* handle() const noexcept { return m_handle.get(); }
    const std::string& filename() const noexcept { return m_filename; }
  };

  class file_locker
  {
  public:
    file_locker(const std::string &filename);
    ~file_locker();
    bool locked() const;
  private:
 #ifdef WIN32
    HANDLE m_fd;
 #else
    int m_fd;
 #endif
  };

  /*! \brief Returns the default data directory.
   *
   * \details Windows < Vista: C:\\Documents and Settings\\Username\\Application Data\\CRYPTONOTE_NAME
   *
   * Windows >= Vista: C:\\Users\\Username\\AppData\\Roaming\\CRYPTONOTE_NAME
   *
   * Mac: ~/Library/Application Support/CRYPTONOTE_NAME
   *
   * Unix: ~/.CRYPTONOTE_NAME
   */
  std::string get_default_data_dir();

#ifdef WIN32
  /**
   * @brief
   *
   * @param nfolder
   * @param iscreate
   *
   * @return
   */
  std::string get_special_folder_path(int nfolder, bool iscreate);
#endif

  /*! \brief Returns the OS version string
   *
   * \details This is a wrapper around the primitives
   * get_windows_version_display_string() and
   * get_nix_version_display_string()
   */
  std::string get_os_version_string();

  /*! \brief creates directories for a path
   *
   *  wrapper around boost::filesyste::create_directories.
   *  (ensure-directory-exists): greenspun's tenth rule in action!
   */
  bool create_directories_if_necessary(const std::string& path);
  /*! \brief std::rename wrapper for nix and something strange for windows.
   */
  std::error_code replace_file(const std::string& replacement_name, const std::string& replaced_name);

  bool sanitize_locale();

  bool disable_core_dumps();

  bool on_startup();

  /*! \brief Defines a signal handler for win32 and *nix
   */
  class signal_handler
  {
  public:
    /*! \brief installs a signal handler  */
    template<typename T>
    static bool install(T t)
    {
#if defined(WIN32)
      bool r = TRUE == ::SetConsoleCtrlHandler(&win_handler, TRUE);
      if (r)
      {
        m_handler = t;
      }
      return r;
#else
      static struct sigaction sa;
      memset(&sa, 0, sizeof(struct sigaction));
      sa.sa_handler = posix_handler;
      sa.sa_flags = 0;
      /* Only blocks SIGINT, SIGTERM and SIGPIPE */
      sigaction(SIGINT, &sa, NULL);
      signal(SIGTERM, posix_handler);
      signal(SIGPIPE, SIG_IGN);
      m_handler = t;
      return true;
#endif
    }

  private:
#if defined(WIN32)
    /*! \brief Handler for win */
    static BOOL WINAPI win_handler(DWORD type)
    {
      if (CTRL_C_EVENT == type || CTRL_BREAK_EVENT == type)
      {
        handle_signal(type);
      }
      else
      {
        MGINFO_RED("Got control signal " << type << ". Exiting without saving...");
        return FALSE;
      }
      return TRUE;
    }
#else
    /*! \brief handler for NIX */
    static void posix_handler(int type)
    {
      handle_signal(type);
    }
#endif

    /*! \brief calles m_handler */
    static void handle_signal(int type)
    {
      static std::mutex m_mutex;
      std::unique_lock lock{m_mutex};
      m_handler(type);
    }

    /*! \brief where the installed handler is stored */
    static std::function<void(int)> m_handler;
  };

  void set_strict_default_file_permissions(bool strict);

  ssize_t get_lockable_memory();

  void set_max_concurrency(unsigned n);
  unsigned get_max_concurrency();

  bool is_local_address(const std::string &address);
  int vercmp(const char *v0, const char *v1); // returns < 0, 0, > 0, similar to strcmp, but more human friendly than lexical - does not attempt to validate

  bool sha256sum(const uint8_t *data, size_t len, crypto::hash &hash);
  bool sha256sum(const std::string &filename, crypto::hash &hash);

  boost::optional<bool> is_hdd(const char *path);

  boost::optional<std::pair<uint32_t, uint32_t>> parse_subaddress_lookahead(const std::string& str);

  std::string glob_to_regex(const std::string &val);
#ifdef _WIN32
  std::string input_line_win();
#endif

  void closefrom(int fd);

  std::string get_human_readable_timestamp(uint64_t ts);

  std::string get_human_readable_timespan(std::chrono::seconds seconds);

  std::string get_human_readable_bytes(uint64_t bytes);

  uint64_t cumulative_block_sync_weight(cryptonote::network_type nettype, uint64_t start_block, uint64_t num_blocks);

  template <typename... T> constexpr size_t constexpr_sum(T... ns) { return (0 + ... + size_t{ns}); }

  namespace detail {
    // Copy an integer type, swapping to little-endian if needed
    template <typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    void memcpy_one(char *dest, T t) {
      boost::endian::native_to_little_inplace(t);
      std::memcpy(dest, &t, sizeof(T));
    }

    // Copy a class byte-for-byte (but only if it is standard layout and has byte alignment)
    template <typename T, std::enable_if_t<std::is_class<T>::value, int> = 0>
    void memcpy_one(char *dest, const T &t) {
      static_assert(std::is_trivially_copyable<T>::value && alignof(T) == 1, "memcpy_le() may only be used on simple (1-byte alignment) struct types");
      std::memcpy(dest, &t, sizeof(T));
    }

    // Copy a string literal
    template <typename T, size_t N>
    void memcpy_one(char *dest, const T (&arr)[N]) {
      for (const T &t : arr) {
        memcpy_one(dest, t);
        dest += sizeof(T);
      }
    }

    // Recursion terminator
    inline void memcpy_le_impl(char *) {}

    // Helper function for public memcpy_le
    template <typename T, typename... Tmore>
    void memcpy_le_impl(char *dest, const T &t, const Tmore &...more) {
      memcpy_one(dest, t);
      memcpy_le_impl(dest + sizeof(T), more...);
    }
  }

  // Does a memcpy of one or more values into a char array; for any given values that are basic
  // integer types the value is first converted from native to little-endian representation (if
  // necessary).  Non-integer types with alignment of 1 (typically meaning structs containing only
  // char, bools, and arrays of those) and fixed-size arrays of the above (including string
  // literals) are also permitted; more complex types are not.
  //
  // The 1-byte alignment is here to protect you: if you have a larger alignment that usually means
  // you have a contained type with a larger alignment, which is probably an integer.
  template <typename... T, size_t N = constexpr_sum(sizeof(T)...)>
  std::array<char, N> memcpy_le(const T &...t) {
    std::array<char, N> r;
    detail::memcpy_le_impl(r.data(), t...);
    return r;
  }

  template <typename Enum>
  constexpr auto enum_count = static_cast<std::underlying_type_t<Enum>>(Enum::_count);

  template <typename Enum>
  constexpr Enum enum_top = static_cast<Enum>(enum_count<Enum> - 1);

}
