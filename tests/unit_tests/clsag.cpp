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

#include "string_tools.h"
#include "ringct/rctTypes.h"
#include "ringct/rctSigs.h"
#include "ringct/rctOps.h"
#include "device/device.hpp"

using namespace rct;

// CLSAG coverage for HF19 (adapted from Monero unit tests; Goodell et al.).
TEST(clsag, prove_and_verify)
{
  const size_t N = 11;
  const size_t idx = 5;
  ctkeyV pubs;
  key p, t, t2, u;
  const key message = rct::identity();
  ctkey backup;
  clsag sig;

  for (size_t i = 0; i < N; ++i)
  {
    key sk;
    ctkey tmp;
    skpkGen(sk, tmp.dest);
    skpkGen(sk, tmp.mask);
    pubs.push_back(tmp);
  }

  skpkGen(p, pubs[idx].dest);
  t = skGen();
  u = skGen();
  addKeys2(pubs[idx].mask, t, u, H);

  key Cout;
  t2 = skGen();
  addKeys2(Cout, t2, u, H);

  ctkey insk;
  insk.dest = p;
  insk.mask = t;

  sig = proveRctCLSAGSimple(zero(), pubs, insk, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));

  try
  {
    sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, NULL, NULL, NULL, (idx + 1) % N, hw::get_device("default"));
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  }
  catch (...) {}

  try
  {
    ctkey insk2;
    insk2.dest = insk.dest;
    insk2.mask = skGen();
    sig = proveRctCLSAGSimple(message, pubs, insk2, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  }
  catch (...) {}

  backup = pubs[idx];
  pubs[idx].mask = scalarmultBase(skGen());
  try
  {
    sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  }
  catch (...) {}
  pubs[idx] = backup;

  try
  {
    ctkey insk2;
    insk2.dest = skGen();
    insk2.mask = insk.mask;
    sig = proveRctCLSAGSimple(message, pubs, insk2, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  }
  catch (...) {}

  backup = pubs[idx];
  pubs[idx].dest = scalarmultBase(skGen());
  try
  {
    sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  }
  catch (...) {}
  pubs[idx] = backup;

  sig = proveRctCLSAGSimple(message, pubs, insk, t2, Cout, NULL, NULL, NULL, idx, hw::get_device("default"));
  ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));

  auto sbackup = sig.s;
  sig.s.clear();
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.s = sbackup;

  key backup_key = sig.s.back();
  sig.s.pop_back();
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.s.push_back(backup_key);

  sig.s.push_back(skGen());
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.s.pop_back();

  for (auto &s: sig.s)
  {
    backup_key = s;
    s = skGen();
    ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
    s = backup_key;
  }

  backup_key = sig.c1;
  sig.c1 = skGen();
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.c1 = backup_key;

  backup_key = sig.I;
  sig.I = scalarmultBase(skGen());
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.I = backup_key;

  backup_key = sig.D;
  sig.D = scalarmultBase(skGen());
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.D = backup_key;

  backup_key = sig.D;
  key x;
  ASSERT_TRUE(epee::string_tools::hex_to_pod("c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac03fa", x));
  sig.D = addKeys(sig.D, x);
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  sig.D = backup_key;

  std::swap(sig.I, sig.D);
  ASSERT_FALSE(verRctCLSAGSimple(message, sig, pubs, Cout));
  std::swap(sig.I, sig.D);

  ASSERT_TRUE(verRctCLSAGSimple(message, sig, pubs, Cout));
}
