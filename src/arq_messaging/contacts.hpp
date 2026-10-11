// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include "identity.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>

namespace arq_messaging {
constexpr std::size_t max_contact_name_bytes = 32;
constexpr std::size_t max_contacts = 256;

inline bool is_pubkey_hex(const std::string_view raw)
{
  if (raw.size() != 64)
    return false;
  for (unsigned char c : raw) {
    if (!std::isxdigit(c))
      return false;
  }
  return true;
}

inline bool is_contact_name(const std::string_view raw)
{
  if (raw.empty() || raw.size() > max_contact_name_bytes || is_pubkey_hex(raw))
    return false;
  for (unsigned char c : raw) {
    if (!(std::isalnum(c) || c == '_' || c == '.' || c == '-'))
      return false;
  }
  return true;
}

inline std::string_view trim_contact_token(std::string_view raw)
{
  while (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t' || raw.front() == '\r'))
    raw.remove_prefix(1);
  while (!raw.empty() && (raw.back() == ' ' || raw.back() == '\t' || raw.back() == '\r'))
    raw.remove_suffix(1);
  return raw;
}

/// `name=64hex` remembers a contact. Bare 64-hex is a pubkey. Anything else is a name lookup.
inline bool parse_recipient_token(const std::string_view raw, std::string& name, std::string& hex)
{
  name.clear();
  hex.clear();
  const auto token = trim_contact_token(raw);
  const auto eq = token.find('=');
  if (eq != std::string_view::npos) {
    name = std::string{trim_contact_token(token.substr(0, eq))};
    hex = std::string{trim_contact_token(token.substr(eq + 1))};
    if (!is_contact_name(name) || !is_pubkey_hex(hex)) {
      name.clear();
      hex.clear();
      return false;
    }
    return true;
  }
  if (is_pubkey_hex(token)) {
    hex = std::string{token};
    return true;
  }
  if (is_contact_name(token)) {
    name = std::string{token};
    return true;
  }
  return false;
}

inline std::map<std::string, std::string> load_contacts(const std::filesystem::path& path)
{
  std::map<std::string, std::string> out;
  std::ifstream in{path};
  if (!in)
    return out;
  std::string line;
  while (std::getline(in, line) && out.size() < max_contacts) {
    auto view = trim_contact_token(line);
    if (view.empty() || view.front() == '#')
      continue;
    const auto sp = view.find(' ');
    if (sp == std::string_view::npos)
      continue;
    const auto name = std::string{trim_contact_token(view.substr(0, sp))};
    const auto hex = std::string{trim_contact_token(view.substr(sp + 1))};
    if (!is_contact_name(name) || !is_pubkey_hex(hex))
      continue;
    out.emplace(name, hex);
  }
  return out;
}

inline std::error_code save_contacts(const std::filesystem::path& path,
                                     const std::map<std::string, std::string>& contacts)
{
  std::error_code fs_ec;
  std::filesystem::create_directories(path.parent_path(), fs_ec);
  std::ofstream out{path, std::ios::binary | std::ios::trunc};
  if (!out)
    return std::make_error_code(std::errc::io_error);
  for (const auto& kv : contacts) {
    if (!is_contact_name(kv.first) || !is_pubkey_hex(kv.second))
      continue;
    out << kv.first << ' ' << kv.second << '\n';
  }
  if (!out)
    return std::make_error_code(std::errc::io_error);
  restrict_owner_file(path);
  return {};
}

inline std::error_code upsert_contact(const std::filesystem::path& path, const std::string& name,
                                      const std::string& hex)
{
  if (!is_contact_name(name) || !is_pubkey_hex(hex))
    return std::make_error_code(std::errc::invalid_argument);
  auto contacts = load_contacts(path);
  if (contacts.find(name) == contacts.end() && contacts.size() >= max_contacts)
    return std::make_error_code(std::errc::no_space_on_device);
  contacts[name] = hex;
  return save_contacts(path, contacts);
}

/// Resolve `name`, `64hex`, or `name=64hex`. On `name=hex`, persist the contact.
inline bool resolve_recipient(const std::string_view raw, const std::filesystem::path& contacts_path, std::string& hex)
{
  std::string name;
  if (!parse_recipient_token(raw, name, hex))
    return false;
  if (!hex.empty()) {
    if (!name.empty() && upsert_contact(contacts_path, name, hex))
      return false;
    return true;
  }
  const auto contacts = load_contacts(contacts_path);
  const auto it = contacts.find(name);
  if (it == contacts.end())
    return false;
  hex = it->second;
  return true;
}
} // namespace arq_messaging
