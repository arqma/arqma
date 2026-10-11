// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include "identity.hpp"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

namespace arq_messaging {
inline std::filesystem::path default_seen_path()
{
  return default_identity_path().parent_path() / "seen";
}

inline std::set<std::string> load_seen(const std::filesystem::path& path)
{
  std::set<std::string> out;
  std::ifstream in{path};
  if (!in)
    return out;
  std::string line;
  while (std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
      line.pop_back();
    if (!line.empty() && line.front() != '#')
      out.insert(line);
  }
  return out;
}

inline std::error_code save_seen(const std::filesystem::path& path, const std::set<std::string>& keys)
{
  std::error_code fs_ec;
  std::filesystem::create_directories(path.parent_path(), fs_ec);
  std::ofstream out{path, std::ios::binary | std::ios::trunc};
  if (!out)
    return std::make_error_code(std::errc::io_error);
  for (const auto& key : keys)
    out << key << '\n';
  if (!out)
    return std::make_error_code(std::errc::io_error);
  restrict_owner_file(path);
  return {};
}

inline std::error_code mark_seen(const std::filesystem::path& path, const std::string_view key)
{
  if (key.empty())
    return std::make_error_code(std::errc::invalid_argument);
  auto keys = load_seen(path);
  keys.insert(std::string{key});
  return save_seen(path, keys);
}
} // namespace arq_messaging
