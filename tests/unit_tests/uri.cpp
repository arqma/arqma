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

#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "serialization/binary_utils.h"
#include "string_tools.h"
#include "wallet/wallet2.h"

#include <stdexcept>

namespace
{
  // Same fixture keys as base58 address tests — do not send funds here.
  const std::string test_serialized_keys(
      "\xf7\x24\xbc\x5c\x6c\xfb\xb9\xd9\x76\x02\xc3\x00\x42\x3a\x2f\x28"
      "\x64\x18\x74\x51\x3a\x03\x57\x78\xa0\xc1\x77\x8d\x83\x32\x01\xe9"
      "\x22\x09\x39\x68\x9e\xdf\x1a\xbd\x5b\xc1\xd0\x31\xf7\x3e\xcd\x6c"
      "\x99\x3a\xdd\x66\xd6\x80\x88\x70\x45\x6a\xfe\xb8\xe7\xee\xb6\x8d",
      64);

  cryptonote::account_public_address fixture_address()
  {
    cryptonote::account_public_address addr{};
    if (!serialization::parse_binary(test_serialized_keys, addr))
      throw std::runtime_error("failed to parse fixture address keys");
    return addr;
  }

  std::string test_address()
  {
    return cryptonote::get_account_address_as_str(cryptonote::TESTNET, false, fixture_address());
  }

  std::string test_integrated_address()
  {
    crypto::hash8 payment_id{};
    // included payment id: <f612cac0b6cb1cda>
    if (!epee::string_tools::hex_to_pod("f612cac0b6cb1cda", payment_id))
      throw std::runtime_error("failed to parse fixture payment id");
    return cryptonote::get_account_integrated_address_as_str(cryptonote::TESTNET, fixture_address(), payment_id);
  }

  void parse_uri(const std::string &uri, bool expected,
                 std::string *address = nullptr, std::string *payment_id = nullptr,
                 std::string *recipient_name = nullptr, std::string *description = nullptr,
                 std::vector<std::string> *unknown_parameters = nullptr)
  {
    std::string address_local, payment_id_local, recipient_name_local, description_local, error;
    uint64_t amount = 0;
    std::vector<std::string> unknown_local;
    tools::wallet2 w(cryptonote::TESTNET);
    const bool ret = w.parse_uri(uri, address_local, payment_id_local, amount, description_local,
                                 recipient_name_local, unknown_local, error);
    ASSERT_EQ(expected, ret) << error;
    if (address)
      *address = address_local;
    if (payment_id)
      *payment_id = payment_id_local;
    if (recipient_name)
      *recipient_name = recipient_name_local;
    if (description)
      *description = description_local;
    if (unknown_parameters)
      *unknown_parameters = unknown_local;
  }
}

TEST(uri, empty_string)
{
  parse_uri("", false);
}

TEST(uri, no_scheme)
{
  parse_uri("arqma", false);
}

TEST(uri, bad_scheme)
{
  parse_uri("http://foo", false);
}

TEST(uri, scheme_not_first)
{
  parse_uri(" arqma:", false);
}

TEST(uri, no_body)
{
  parse_uri("arqma:", false);
}

TEST(uri, no_address)
{
  parse_uri("arqma:?", false);
}

TEST(uri, bad_address)
{
  parse_uri("arqma:44444", false);
}

TEST(uri, good_address)
{
  const auto addr = test_address();
  std::string parsed;
  parse_uri("arqma:" + addr, true, &parsed);
  ASSERT_EQ(addr, parsed);
}

TEST(uri, good_integrated_address)
{
  parse_uri("arqma:" + test_integrated_address(), true);
}

TEST(uri, parameter_without_inter)
{
  parse_uri("arqma:" + test_address() + "&amount=1", false);
}

TEST(uri, parameter_without_equals)
{
  parse_uri("arqma:" + test_address() + "?amount", false);
}

TEST(uri, parameter_without_value)
{
  parse_uri("arqma:" + test_address() + "?tx_amount=", false);
}

TEST(uri, negative_amount)
{
  parse_uri("arqma:" + test_address() + "?tx_amount=-1", false);
}

TEST(uri, bad_amount)
{
  parse_uri("arqma:" + test_address() + "?tx_amount=alphanumeric", false);
}

TEST(uri, duplicate_parameter)
{
  parse_uri("arqma:" + test_address() + "?tx_amount=1&tx_amount=1", false);
}

TEST(uri, unknown_parameter)
{
  std::vector<std::string> unknown;
  parse_uri("arqma:" + test_address() + "?unknown=1", true, nullptr, nullptr, nullptr, nullptr, &unknown);
  ASSERT_EQ(1u, unknown.size());
  ASSERT_EQ("unknown=1", unknown[0]);
}

TEST(uri, unknown_parameters)
{
  std::vector<std::string> unknown;
  parse_uri("arqma:" + test_address() + "?tx_amount=1&unknown=1&tx_description=desc&foo=bar", true,
            nullptr, nullptr, nullptr, nullptr, &unknown);
  ASSERT_EQ(2u, unknown.size());
  ASSERT_EQ("unknown=1", unknown[0]);
  ASSERT_EQ("foo=bar", unknown[1]);
}

TEST(uri, empty_payment_id)
{
  parse_uri("arqma:" + test_address() + "?tx_payment_id=", false);
}

TEST(uri, bad_payment_id)
{
  parse_uri("arqma:" + test_address() + "?tx_payment_id=1234567890", false);
}

TEST(uri, short_payment_id)
{
  const auto addr = test_address();
  std::string parsed_addr, payment_id;
  parse_uri("arqma:" + addr + "?tx_payment_id=1234567890123456", true, &parsed_addr, &payment_id);
  ASSERT_EQ(addr, parsed_addr);
  ASSERT_EQ("1234567890123456", payment_id);
}

TEST(uri, long_payment_id)
{
  const auto addr = test_address();
  std::string parsed_addr, payment_id;
  parse_uri("arqma:" + addr + "?tx_payment_id=1234567890123456789012345678901234567890123456789012345678901234", true,
            &parsed_addr, &payment_id);
  ASSERT_EQ(addr, parsed_addr);
  ASSERT_EQ("1234567890123456789012345678901234567890123456789012345678901234", payment_id);
}

TEST(uri, payment_id_with_integrated_address)
{
  parse_uri("arqma:" + test_integrated_address() + "?tx_payment_id=1234567890123456", false);
}

TEST(uri, empty_description)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("", description);
}

TEST(uri, empty_recipient_name)
{
  std::string recipient_name;
  parse_uri("arqma:" + test_address() + "?recipient_name=", true, nullptr, nullptr, &recipient_name);
  ASSERT_EQ("", recipient_name);
}

TEST(uri, non_empty_description)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo", description);
}

TEST(uri, non_empty_recipient_name)
{
  std::string recipient_name;
  parse_uri("arqma:" + test_address() + "?recipient_name=foo", true, nullptr, nullptr, &recipient_name);
  ASSERT_EQ("foo", recipient_name);
}

TEST(uri, url_encoding)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo%20bar", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo bar", description);
}

TEST(uri, non_alphanumeric_url_encoding)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo%2x", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo%2x", description);
}

TEST(uri, truncated_url_encoding)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo%2", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo%2", description);
}

TEST(uri, percent_without_url_encoding)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo%", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo%", description);
}

TEST(uri, url_encoded_once)
{
  std::string description;
  parse_uri("arqma:" + test_address() + "?tx_description=foo%2020", true, nullptr, nullptr, nullptr, &description);
  ASSERT_EQ("foo 20", description);
}
