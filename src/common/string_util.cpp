#include "string_util.h"
#include <cassert>

namespace tools {

using namespace std::literals;

std::vector<std::string_view> split(std::string_view str, const std::string_view delim, bool trim)
{
  std::vector<std::string_view> results;
  if (delim.empty())
  {
    results.reserve(str.size());
    for (size_t i = 0; i < str.size(); i++)
      results.emplace_back(str.data() + i, 1);
    return results;
  }

  for (size_t pos = str.find(delim); pos != std::string_view::npos; pos = str.find(delim))
  {
    if (!trim || !results.empty() || pos > 0)
      results.push_back(str.substr(0, pos));
    str.remove_prefix(pos + delim.size());
  }
  if (!trim || str.size())
    results.push_back(str);
  else
    while (!results.empty() && results.back().empty())
      results.pop_back();
  return results;
}

std::vector<std::string_view> split_any(std::string_view str, const std::string_view delims, bool trim)
{
  if (delims.empty())
    return split(str, delims);
  std::vector<std::string_view> results;
  for (size_t pos = str.find_first_of(delims); pos != std::string_view::npos; pos = str.find_first_of(delims))
  {
    if (!trim || !results.empty() || pos > 0)
      results.push_back(str.substr(0, pos));
    size_t until = str.find_first_not_of(delims, pos+1);
    if (until == std::string_view::npos)
      str.remove_prefix(str.size());
    else
      str.remove_prefix(until);
  }
  if (!trim || str.size())
    results.push_back(str);
  else
    while (!results.empty() && results.back().empty())
      results.pop_back();
  return results;
}

void trim(std::string_view& s)
{
  constexpr auto simple_whitespace = " \t\r\n"sv;
  auto pos = s.find_first_not_of(simple_whitespace);
  if (pos == std::string_view::npos)
  {
    s.remove_prefix(s.size());
    return;
  }
  s.remove_prefix(pos);
  pos = s.find_last_not_of(simple_whitespace);
  assert(pos != std::string_view::npos);
  s.remove_suffix(s.size() - (pos + 1));
}

}