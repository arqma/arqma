// Copyright (c) 2018 - 2026, The Arqma Network
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

#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace arq_messaging {
struct X25519PublicKey
{
  static constexpr std::size_t bytes = 32;
  std::array<std::uint8_t, bytes> data{};

  bool is_null() const noexcept
  {
    for (auto b : data)
      if (b != 0)
        return false;
    return true;
  }
};

struct X25519PrivateKey
{
  static constexpr std::size_t bytes = 32;
  std::array<std::uint8_t, bytes> data{};
};

struct Identity
{
  X25519PublicKey public_key{};
  X25519PrivateKey private_key{};
};

/// Generates a Curve25519 identity via libsodium `crypto_box_keypair`.
std::error_code generate_identity(Identity& out) noexcept;

/// 64-byte file: public key || private key. Path overloads use Unicode on Windows.
std::error_code save_identity(const std::filesystem::path& path, const Identity& id);
std::error_code load_identity(const std::filesystem::path& path, Identity& out);
inline std::error_code save_identity(const std::string& path, const Identity& id)
{
  return save_identity(std::filesystem::path{path}, id);
}
inline std::error_code load_identity(const std::string& path, Identity& out)
{
  return load_identity(std::filesystem::path{path}, out);
}

inline std::error_code restrict_owner_file(const std::filesystem::path& path)
{
#if !defined(_WIN32)
  std::error_code perm_ec;
  std::filesystem::permissions(path.parent_path(), std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace, perm_ec);
  std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, perm_ec);
#endif
  return {};
}

/// `$HOME/.arqma/msg` (Windows: `%USERPROFILE%\.arqma\msg`). Override with `ARQMA_MSG_IDENTITY`.
inline std::filesystem::path default_msg_dir(const std::string_view home)
{
  return std::filesystem::path{std::string{home}} / ".arqma" / "msg";
}

inline std::filesystem::path default_identity_path()
{
  if (const char* path = std::getenv("ARQMA_MSG_IDENTITY"); path && *path)
    return path;
  const char* home = std::getenv("HOME");
#if defined(_WIN32)
  if (!home || !*home)
    home = std::getenv("USERPROFILE");
#endif
  if (!home || !*home)
    return std::filesystem::path{"arqma-msg"} / "identity";
  return default_msg_dir(home) / "identity";
}

inline std::filesystem::path default_contacts_path()
{
  return default_identity_path().parent_path() / "contacts";
}
} // namespace arq_messaging
