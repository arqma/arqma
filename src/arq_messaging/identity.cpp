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

#include "identity.hpp"

#include <sodium/crypto_box.h>

#include <filesystem>
#include <fstream>

namespace arq_messaging {
std::error_code generate_identity(Identity& out) noexcept
{
  static_assert(X25519PublicKey::bytes == crypto_box_PUBLICKEYBYTES, "pubkey size");
  static_assert(X25519PrivateKey::bytes == crypto_box_SECRETKEYBYTES, "privkey size");

  if (crypto_box_keypair(out.public_key.data.data(), out.private_key.data.data()) != 0)
    return std::make_error_code(std::errc::io_error);

  if (out.public_key.is_null())
    return std::make_error_code(std::errc::io_error);
  return {};
}

std::error_code save_identity(const std::filesystem::path& path, const Identity& id)
{
  std::ofstream out{path, std::ios::binary | std::ios::trunc};
  if (!out)
    return std::make_error_code(std::errc::io_error);
  out.write(reinterpret_cast<const char*>(id.public_key.data.data()),
            static_cast<std::streamsize>(X25519PublicKey::bytes));
  out.write(reinterpret_cast<const char*>(id.private_key.data.data()),
            static_cast<std::streamsize>(X25519PrivateKey::bytes));
  if (!out)
    return std::make_error_code(std::errc::io_error);
  restrict_owner_file(path);
  return {};
}

std::error_code load_identity(const std::filesystem::path& path, Identity& out)
{
  std::ifstream in{path, std::ios::binary};
  if (!in)
    return std::make_error_code(std::errc::no_such_file_or_directory);
  in.read(reinterpret_cast<char*>(out.public_key.data.data()), static_cast<std::streamsize>(X25519PublicKey::bytes));
  if (!in || in.gcount() != static_cast<std::streamsize>(X25519PublicKey::bytes))
    return std::make_error_code(std::errc::io_error);
  in.read(reinterpret_cast<char*>(out.private_key.data.data()), static_cast<std::streamsize>(X25519PrivateKey::bytes));
  if (!in || in.gcount() != static_cast<std::streamsize>(X25519PrivateKey::bytes) || out.public_key.is_null())
    return std::make_error_code(std::errc::io_error);
  return {};
}
} // namespace arq_messaging
