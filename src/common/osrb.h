#pragma once
#include <sstream>
#include <string>

namespace tools
{

class one_shot_read_buffer : public std::stringbuf
{
public:
  one_shot_read_buffer(const char *s_in, size_t n) : std::stringbuf(std::ios::in)
  {
    auto *s = const_cast<char *>(s_in);
    setg(s, s, s+n);
  }

  explicit one_shot_read_buffer(const std::string &s_in) : one_shot_read_buffer{s_in.data(), s_in.size()} {}

  explicit one_shot_read_buffer(const std::string &&s) = delete;
};

}
