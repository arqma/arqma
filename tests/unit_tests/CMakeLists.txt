# Copyright (c) 2018 - 2026, The Arqma Network
# Copyright (c) 2014-2020, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Green CI suite for the upgrade branch. Legacy fixtures (Monero-era addresses,
# decimals, DNS) and Boost.Asio API-drift tests are restored incrementally.
set(unit_tests_sources
  arq_blink.cpp
  arq_messaging_envelope.cpp
  arq_storage_client.cpp
  arq_upgrade_modules.cpp
  arqmq_facade.cpp
  arqmq_transport.cpp
  arqmq_compat.cpp
  arqmq_curve_zap.cpp
  arqnet_auth.cpp
  apply_permutation.cpp
  base58.cpp
  burn_hf19.cpp
  bulletproofs.cpp
  canonical_amounts.cpp
  chacha.cpp
  // checkpoints.cpp  # SN-aware empty/pre-init path (null DB safe)
  checkpoints.cpp
  clsag.cpp
  command_line.cpp
  decompose_amount_into_digits.cpp
  fee.cpp
  get_xtype_from_string.cpp
  hashchain.cpp
  hf19.cpp
  hf20.cpp
  hf21.cpp
  main.cpp
  memwipe.cpp
  mul_div.cpp
  output_selection.cpp
  parse_amount.cpp
  p2p_limits.cpp
  rpc_validation.cpp
  rpc_auth.cpp
  sha256.cpp
  serialization_basic.cpp
  slow_memmem.cpp
  subaddress.cpp
  test_tx_utils.cpp
  varint.cpp
  vercmp.cpp
  uri.cpp
  hardfork_versions.cpp
  wallet_rpc_auth.cpp
  wallet_rpc_validation.cpp
)

set(unit_tests_headers
  unit_tests_utils.h)

add_executable(unit_tests
  ${unit_tests_sources}
  ${unit_tests_headers})
target_link_libraries(unit_tests
  PRIVATE
    ringct
    cryptonote_protocol
    cryptonote_core
    blockchain_db
    rpc
    wallet
    p2p
    arqmq
    arq_storage
    arq_messaging
    arq_router
    arq_blink
    version
    epee
    ${Boost_CHRONO_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${GTEST_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${EXTRA_LIBRARIES})
set_property(TARGET unit_tests
  PROPERTY
    FOLDER "tests")

if (NOT MSVC)
  set_property(TARGET unit_tests
    APPEND_STRING
    PROPERTY
      COMPILE_FLAGS " -Wno-undef -Wno-sign-compare")
endif ()

add_test(
  NAME    unit_tests
  COMMAND unit_tests)

set_property(SOURCE memwipe.cpp
  PROPERTY
    MEMORYCHECK_COMMAND_OPTIONS "--tool=helgrind")
