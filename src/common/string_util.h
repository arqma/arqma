#pragma once
#include <string>
#include <string_view>
#include <chrono>
#include <iterator>
#include <charconv>
#include "span.h"

namespace tools {

using namespace std::literals;

std::string friendly_duration(std::chrono::nanoseconds dur);

inline bool string_iequal(std::string_view s1, std::string_view s2)
{
  return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

template <typename S1, typename... S>
bool string_iequal_any(const S1& s1, const S&... s) {
  return (string_iequal(s1, s) || ...);
}

inline bool starts_with(std::string_view str, std::string_view prefix) {
  return str.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

template <typename T>
std::string_view view_guts(const T& val) {
  static_assert((std::is_standard_layout_v<T> && std::has_unique_object_representations_v<T>)
    || epee::is_byte_spannable<T>, "cannot safely access non-trivial class as string_view");
  return {reinterpret_cast<const char *>(&val), sizeof(val)};
}

template <typename T>
std::string copy_guts(const T& val) {
  return std::string{view_guts(val)};
}

template <typename T>
bool parse_int(const std::string_view str, T& value, int base = 10) {
  T tmp;
  auto* strend = str.data() + str.size();
  auto [p, ec] = std::from_chars(str.data(), strend, tmp, base);
  if (ec != std::errc() || p != strend)
    return false;
  value = tmp;
  return true;
}

}
