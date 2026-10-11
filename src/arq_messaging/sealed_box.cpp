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

#include "sealed_sender.hpp"

#include <sodium/crypto_box.h>

namespace arq_messaging {
std::error_code seal_payload(const X25519PublicKey& recipient, const std::vector<std::uint8_t>& plaintext,
                             std::vector<std::uint8_t>& ciphertext) noexcept
{
  ciphertext.assign(plaintext.size() + crypto_box_SEALBYTES, 0);
  if (crypto_box_seal(ciphertext.data(), plaintext.data(), plaintext.size(), recipient.data.data()) != 0) {
    ciphertext.clear();
    return std::make_error_code(std::errc::io_error);
  }
  return {};
}

std::error_code open_payload(const Identity& recipient, const std::vector<std::uint8_t>& ciphertext,
                             std::vector<std::uint8_t>& plaintext) noexcept
{
  if (ciphertext.size() < crypto_box_SEALBYTES)
    return std::make_error_code(std::errc::invalid_argument);

  plaintext.assign(ciphertext.size() - crypto_box_SEALBYTES, 0);
  if (crypto_box_seal_open(plaintext.data(), ciphertext.data(), ciphertext.size(), recipient.public_key.data.data(),
                           recipient.private_key.data.data()) != 0) {
    plaintext.clear();
    return std::make_error_code(std::errc::permission_denied);
  }
  return {};
}
} // namespace arq_messaging
