// Copyright (c) 2018 - 2026, The Arqma Network
//
// All rights reserved.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace arq_messaging {
constexpr char k_onion_forward_magic[4] = {'A', 'R', 'Q', 'H'};
constexpr std::uint16_t max_forward_url_bytes = 256;

/// Frame leftover ciphertext for the next router hop: magic + u16-le URL length + URL + onion.
inline bool encode_onion_forward(const std::string_view next_url, const std::vector<std::uint8_t>& inner,
                                 std::vector<std::uint8_t>& out)
{
  if (next_url.empty() || next_url.size() > max_forward_url_bytes || inner.empty())
    return false;
  out.clear();
  out.insert(out.end(), k_onion_forward_magic, k_onion_forward_magic + 4);
  const auto n = static_cast<std::uint16_t>(next_url.size());
  out.push_back(static_cast<std::uint8_t>(n & 0xff));
  out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xff));
  out.insert(out.end(), next_url.begin(), next_url.end());
  out.insert(out.end(), inner.begin(), inner.end());
  return true;
}

inline bool decode_onion_forward(const std::string_view raw, std::string& next_url, std::vector<std::uint8_t>& inner)
{
  if (raw.size() < 8 || std::memcmp(raw.data(), k_onion_forward_magic, 4) != 0)
    return false;
  const std::uint16_t n = static_cast<std::uint16_t>(static_cast<unsigned char>(raw[4])) |
                          static_cast<std::uint16_t>(static_cast<unsigned char>(raw[5]) << 8);
  if (n == 0 || n > max_forward_url_bytes || raw.size() < 6u + static_cast<std::size_t>(n) + 1u)
    return false;
  next_url.assign(raw.data() + 6, n);
  inner.assign(raw.begin() + 6 + n, raw.end());
  return !inner.empty() && !next_url.empty();
}
} // namespace arq_messaging
