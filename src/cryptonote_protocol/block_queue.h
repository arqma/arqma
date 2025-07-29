// Copyright (c) 2018-2022, The Arqma Network
// Copyright (c) 2017-2018, The Monero Project
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
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include "crypto/crypto.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "cn.block_queue"

namespace cryptonote
{
  struct block_complete_entry;

  class block_queue
  {
  public:
    struct span
    {
      uint64_t start_block_height;
      std::vector<crypto::hash> hashes;
      std::vector<cryptonote::block_complete_entry> blocks;
      boost::uuids::uuid connection_id;
      uint64_t nblocks;
      float rate;
      size_t size;
      std::chrono::steady_clock::time_point time;

      span(uint64_t start_block_height, std::vector<cryptonote::block_complete_entry> blocks, const boost::uuids::uuid &connection_id, float rate, size_t size):
        start_block_height(start_block_height), blocks(std::move(blocks)), connection_id(connection_id), nblocks(this->blocks.size()), rate(rate), size(size), time{std::chrono::steady_clock::now()} {}
      span(uint64_t start_block_height, uint64_t nblocks, const boost::uuids::uuid &connection_id, std::chrono::steady_clock::time_point time):
        start_block_height(start_block_height), connection_id(connection_id), nblocks(nblocks), rate(0.0f), size(0), time(time) {}

      bool operator<(const span &s) const { return start_block_height < s.start_block_height; }
    };
    typedef std::set<span> block_map;

  public:
    void add_blocks(uint64_t height, std::vector<cryptonote::block_complete_entry> bcel, const boost::uuids::uuid &connection_id, float rate, size_t size);
    void add_blocks(uint64_t height, uint64_t nblocks, const boost::uuids::uuid &connection_id, std::chrono::steady_clock::time_point time);
    void flush_spans(const boost::uuids::uuid &connection_id, bool all = false);
    void flush_stale_spans(const std::set<boost::uuids::uuid> &live_connections);
    bool remove_span(uint64_t start_block_height, std::vector<crypto::hash> *hashes = nullptr);
    void remove_spans(const boost::uuids::uuid &connection_id, uint64_t start_block_height);
    uint64_t get_max_block_height() const;
    void print() const;
    std::string get_overview(uint64_t blockchain_height) const;
    bool has_unpruned_height(uint64_t block_height, uint64_t blockchain_height, uint32_t pruning_seed) const;
    std::pair<uint64_t, uint64_t> reserve_span(uint64_t first_block_height, uint64_t last_block_height, uint64_t max_blocks, const boost::uuids::uuid &connection_id, uint32_t pruning_seed, uint64_t blockchain_height, const std::vector<crypto::hash> &block_hashes);
    uint64_t get_next_needed_height(uint64_t blockchain_height) const;
    std::pair<uint64_t, uint64_t> get_next_span_if_scheduled(std::vector<crypto::hash> &hashes, boost::uuids::uuid &connection_id) const;
    void reset_next_span_time();
    void set_span_hashes(uint64_t start_height, const boost::uuids::uuid &connection_id, std::vector<crypto::hash> hashes);
    bool get_next_span(uint64_t &height, std::vector<cryptonote::block_complete_entry> &bcel, boost::uuids::uuid &connection_id, bool filled = true) const;
    bool has_next_span(uint64_t height, bool &filled, std::chrono::steady_clock::time_point& time, boost::uuids::uuid &connection_id) const;
    size_t get_data_size() const;
    size_t get_num_filled_spans() const;
    crypto::hash get_last_known_hash(const boost::uuids::uuid &connection_id) const;
    bool has_spans(const boost::uuids::uuid &connection_id) const;
    float get_speed(const boost::uuids::uuid &connection_id) const;
    float get_download_rate(const boost::uuids::uuid &connection_id) const;
    bool foreach(std::function<bool(const span&)> f) const;
    bool requested(const crypto::hash &hash) const;
    bool have(const crypto::hash &hash) const;
    std::uint64_t have_height(const crypto::hash &hash) const;

  private:
    void erase_block(block_map::iterator j);
    inline bool requested_internal(const crypto::hash &hash) const;

  private:
    block_map blocks;
    mutable std::recursive_mutex mutex;
    std::unordered_set<crypto::hash> requested_hashes;
    std::unordered_map<crypto::hash, std::uint64_t> have_blocks;
  };
}
