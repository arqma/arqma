// Copyright (c) 2018 - 2026, The Arqma Network
// Copyright (c) 2014-2020, The Monero Project
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

#include "gtest/gtest.h"

#include <cstdint>
#include <string>

#include "serialization/binary_utils.h"

namespace
{
  struct TinyBlob
  {
    uint32_t a = 0;
    uint16_t b = 0;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(a)
      FIELD(b)
    END_SERIALIZE()
  };
}

TEST(serialization_basic, binary_roundtrip_tiny_object)
{
  TinyBlob in{};
  in.a = 0x11223344u;
  in.b = 0xabcd;

  std::string blob;
  ASSERT_TRUE(serialization::dump_binary(in, blob));
  ASSERT_FALSE(blob.empty());

  TinyBlob out{};
  ASSERT_TRUE(serialization::parse_binary(blob, out));
  EXPECT_EQ(in.a, out.a);
  EXPECT_EQ(in.b, out.b);
}

TEST(serialization_basic, rejects_truncated_blob)
{
  TinyBlob in{};
  in.a = 7;
  in.b = 9;
  std::string blob;
  ASSERT_TRUE(serialization::dump_binary(in, blob));
  ASSERT_GT(blob.size(), 1u);
  blob.resize(blob.size() - 1);

  TinyBlob out{};
  EXPECT_FALSE(serialization::parse_binary(blob, out));
}
