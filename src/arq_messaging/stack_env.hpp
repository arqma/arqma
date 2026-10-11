// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace arq_messaging {
struct StackEnv
{
  std::string storage_url = "http://127.0.0.1:22021";
  std::string router_url;
  std::string token;
};

inline std::string_view trim_env_token(std::string_view raw)
{
  while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t' || raw.front() == '\r'))
    raw.remove_prefix(1);
  while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t' || raw.back() == '\r'))
    raw.remove_suffix(1);
  return raw;
}

/// Parse `KEY=value` lines (`ARQMA_STORAGE_URL`, `ARQMA_ROUTER_URL`).
inline StackEnv parse_stack_env(const std::string_view text)
{
  StackEnv out;
  std::string line;
  std::string buffer{text};
  for (std::size_t i = 0, start = 0; i <= buffer.size(); ++i) {
    if (i < buffer.size() && buffer[i] != '\n')
      continue;
    line.assign(buffer, start, i - start);
    start = i + 1;
    auto view = trim_env_token(line);
    if (view.empty() || view.front() == '#')
      continue;
    const auto eq = view.find('=');
    if (eq == std::string_view::npos)
      continue;
    const auto key = trim_env_token(view.substr(0, eq));
    const auto value = std::string{trim_env_token(view.substr(eq + 1))};
    if (value.empty())
      continue;
    if (key == "ARQMA_STORAGE_URL")
      out.storage_url = value;
    else if (key == "ARQMA_ROUTER_URL")
      out.router_url = value;
    else if (key == "ARQMA_STACK_TOKEN")
      out.token = value;
  }
  return out;
}

inline std::filesystem::path default_stack_dir()
{
  if (const char* dir = std::getenv("ARQMA_STACK_DIR"); dir && *dir)
    return dir;
#if defined(_WIN32)
  if (const char* tmp = std::getenv("TEMP"); tmp && *tmp)
    return std::filesystem::path{tmp} / "arqma-stack";
  if (const char* tmp = std::getenv("TMP"); tmp && *tmp)
    return std::filesystem::path{tmp} / "arqma-stack";
  return "arqma-stack";
#else
  if (const char* tmp = std::getenv("TMPDIR"); tmp && *tmp)
    return std::filesystem::path{tmp} / "arqma-stack";
  return "/tmp/arqma-stack";
#endif
}

inline StackEnv load_stack_env()
{
  StackEnv out = parse_stack_env({});
  std::ifstream in{default_stack_dir() / "env"};
  if (in) {
    const std::string text{std::istreambuf_iterator<char>{in}, {}};
    out = parse_stack_env(text);
  }
  if (const char* url = std::getenv("ARQMA_STORAGE_URL"); url && *url)
    out.storage_url = url;
  if (const char* url = std::getenv("ARQMA_ROUTER_URL"); url && *url)
    out.router_url = url;
  if (const char* token = std::getenv("ARQMA_STACK_TOKEN"); token && *token)
    out.token = token;
  return out;
}
} // namespace arq_messaging
