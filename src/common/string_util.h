#pragma once
#include <string_view>
#include <vector>
#include <iterator>
#include <charconv>
#include "span.h"

namespace tools
{
  inline bool starts_with(std::string_view str, std::string_view prefix)
  {
    return str.substr(0, prefix.size()) == prefix;
  }

  inline bool ends_with(std::string_view str, std::string_view suffix)
  {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
  }

  template <typename T>
  std::string_view view_guts(const T& val)
  {
    static_assert((std::is_standard_layout_v<T> && std::has_unique_object_representations_v<T>)
      || epee::is_byte_spannable<T>, "cannot safely access non-trivial class as string_view");
    return {reinterpret_cast<const char *>(&val), sizeof(val)};
  }

  template <typename T>
  std::string copy_guts(const T& val)
  {
    return std::string{view_guts(val)};
  }

  std::vector<std::string_view> split(std::string_view str, std::string_view delim, bool trim = false);
  std::vector<std::string_view> split_any(std::string_view str, std::string_view delims, bool trim = false);

  void trim(std::string_view& s);

  template <typename T>
  bool parse_int(const std::string_view str, T& value, int base = 10)
  {
    T tmp;
    auto* strend = str.data() + str.size();
    auto [p, ec] = std::from_chars(str.data(), strend, tmp, base);
    if (ec != std::errc() || p != strend)
      return false;
    value = tmp;
    return true;
  }

}