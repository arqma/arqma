// Copyright (c) 2019, The Arqma Network
// Copyright (c) 2018-2019, The Monero Project
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

#include <boost/archive/portable_binary_iarchive.hpp>
#include <boost/archive/portable_binary_oarchive.hpp>
#include "cryptonote_config.h"
#include "include_base_utils.h"
#include "string_tools.h"
#include "file_io_utils.h"
#include "int-util.h"
#include "common/util.h"
#include "serialization/crypto.h"
#include "common/unordered_containers_boost_serialization.h"
#include "cryptonote_basic/cryptonote_boost_serialization.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/difficulty.h"
#include "core_rpc_server_error_codes.h"
#include "rpc_payment.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "daemon.rpc.payment"

#define STALE_THRESHOLD 15 /* seconds */

#define PENALTY_FOR_STALE 0.02
#define PENALTY_FOR_BAD_HASH 0.2
#define PENALTY_FOR_DUPLICATE 0.2

#define DEFAULT_FLUSH_AGE (3600 * 24 * 180) // half a year

namespace
{
  void add64clamp(uint64_t &value, uint64_t add)
  {
    if (value > std::numeric_limits<uint64_t>::max() - add)
      value = std::numeric_limits<uint64_t>::max();
    else
      value += add;
  }
}

namespace cryptonote
{
  rpc_payment::client_info::client_info():
    top(crypto::null_hash),
    credits(0),
    update_time(time(NULL)),
    last_request_timestamp(0),
    credits_total(0),
    credits_used(0),
    nonces_good(0),
    nonces_stale(0),
    nonces_bad(0),
    nonces_dupe(0)
  {
  }

  rpc_payment::rpc_payment(const cryptonote::account_public_address &address, uint64_t diff, uint64_t credits_per_hash_found):
    m_address(address),
    m_diff(diff),
    m_credits_per_hash_found(credits_per_hash_found),
    m_credits_total(0),
    m_credits_used(0),
    m_nonces_good(0),
    m_nonces_stale(0),
    m_nonces_bad(0),
    m_nonces_dupe(0)
  {
  }

  bool rpc_payment::pay(const crypto::public_key &client, uint64_t ts, uint64_t payment, const std::string &rpc, bool same_ts, uint64_t &credits)
  {
    client_info &info = m_client_info[client]; // creates if not found
    if (ts < info.last_request_timestamp || (ts == info.last_request_timestamp && !same_ts))
    {
      MDEBUG("Invalid ts: " << ts << " <= " << info.last_request_timestamp);
      return false;
    }
    info.last_request_timestamp = ts;
    if (info.credits < payment)
    {
      MDEBUG("Not enough credits: " << info.credits << " < " << payment);
      return false;
    }
    info.credits -= payment;
    add64clamp(info.credits_used, payment);
    add64clamp(m_credits_used, payment);
    MDEBUG("client " << client << " paying " << payment << " for " << rpc << ", " << info.credits << " left");
    credits = info.credits;
    return true;
  }

  bool rpc_payment::get_info(const crypto::public_key &client, const std::function<bool(const cryptonote::blobdata&, cryptonote::block&)> &get_block_template, cryptonote::blobdata &hashing_blob, const crypto::hash &top, uint64_t &diff, uint64_t &credits_per_hash_found, uint64_t &credits)
  {
    client_info &info = m_client_info[client]; // creates if not found
    const uint64_t now = time(NULL);
    bool need_template = top != info.top || now >= info.block_template_update_time + STALE_THRESHOLD;
    if (need_template)
    {
      cryptonote::blobdata extra_nonce("\x42\x42\x42\x42", 4);
      if (!get_block_template(extra_nonce, info.block))
        return false;
      if(!remove_field_from_tx_extra(info.block.miner_tx.extra, typeid(cryptonote::tx_extra_nonce)))
        return false;
      extra_nonce = cryptonote::blobdata((const char*)&client, 4);
      if(!add_extra_nonce_to_tx_extra(info.block.miner_tx.extra, extra_nonce))
        return false;
      hashing_blob = get_block_hashing_blob(info.block);
      info.previous_top = info.top;
      info.previous_payments = info.payments;
      info.payments.clear();
      info.hashing_blob = hashing_blob;
      info.block_template_update_time = now;
    }
    info.top = top;
    info.update_time = now;
    hashing_blob = info.hashing_blob;
    diff = m_diff;
    credits_per_hash_found = m_credits_per_hash_found;
    credits = info.credits;
    return true;
  }

  bool rpc_payment::submit_nonce(const crypto::public_key &client, uint32_t nonce, const crypto::hash &top, int64_t &error_code, std::string &error_message, uint64_t &credits, crypto::hash &hash, cryptonote::block &block)
  {
    client_info &info = m_client_info[client]; // creates if not found
    MINFO("client " << client << " sends nonce: " << nonce);
    const bool is_current = top == info.top;
    std::unordered_set<uint64_t> &payments = is_current ? info.payments : info.previous_payments;
    if (payments.find(nonce) != payments.end())
    {
      MWARNING("Duplicate nonce " << nonce << " from " << (is_current ? "current" : "previous"));
      ++m_nonces_dupe;
      ++info.nonces_dupe;
      info.credits = std::max(info.credits, PENALTY_FOR_DUPLICATE * m_credits_per_hash_found) - PENALTY_FOR_DUPLICATE * m_credits_per_hash_found;
      error_code = CORE_RPC_ERROR_CODE_DUPLICATE_PAYMENT;
      error_message = "Duplicate payment";
      return false;
    }
    payments.insert(nonce);

    if (info.hashing_blob.size() < 43)
    {
      // not initialized ?
      error_code = CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB;
      error_message = "not initialized";
      return false;
    }

    const uint64_t now = time(NULL);
    if (!is_current)
    {
      if (now > info.update_time + STALE_THRESHOLD)
      {
        MWARNING("Nonce is stale (top " << top << ", should be " << info.top << " or within " << STALE_THRESHOLD << " seconds");
        ++m_nonces_stale;
        ++info.nonces_stale;
        info.credits = std::max(info.credits, PENALTY_FOR_STALE * m_credits_per_hash_found) - PENALTY_FOR_STALE * m_credits_per_hash_found;
        error_code = CORE_RPC_ERROR_CODE_STALE_PAYMENT;
        error_message = "stale payment";
        return false;
      }
    }

    cryptonote::blobdata hashing_blob = info.hashing_blob;
    *(uint32_t*)(hashing_blob.data() + 39) = SWAP32LE(nonce);
    crypto::cn_turtle_hash(hashing_blob.data(), hashing_blob.size(), hash);
    if (!check_hash(hash, m_diff))
    {
      MWARNING("Payment too low");
      ++m_nonces_bad;
      ++info.nonces_bad;
      error_code = CORE_RPC_ERROR_CODE_PAYMENT_TOO_LOW;
      error_message = "Hash does not meet difficulty (could be wrong PoW hash, or mining at lower difficulty than required)";
      info.credits = std::max(info.credits, PENALTY_FOR_BAD_HASH * m_credits_per_hash_found) - PENALTY_FOR_BAD_HASH * m_credits_per_hash_found;
      return false;
    }

    add64clamp(info.credits, m_credits_per_hash_found);
    MINFO("client " << client << " credited for " << m_credits_per_hash_found << ", now " << info.credits << (is_current ? "" : " (close)"));

    m_hashrate[now] += m_diff;
    add64clamp(m_credits_total, m_credits_per_hash_found);
    add64clamp(info.credits_total, m_credits_per_hash_found);
    ++m_nonces_good;
    ++info.nonces_good;

    credits = info.credits;
    info.block.nonce = nonce;
    block = info.block;
    return true;
  }

  bool rpc_payment::foreach(const std::function<bool(const crypto::public_key &client, const client_info &info)> &f)
  {
    for (std::unordered_map<crypto::public_key, client_info>::const_iterator i = m_client_info.begin(); i != m_client_info.end(); ++i)
    {
      if (!f(i->first, i->second))
        return false;
    }
    return true;
  }

  bool rpc_payment::load(const std::string &directory)
  {
    TRY_ENTRY();
    m_directory = directory;
    std::string state_file_path = directory + "/" + RPC_PAYMENTS_DATA_FILENAME;
    MINFO("loading rpc payments data from " << state_file_path);
    std::ifstream data;
    data.open(state_file_path, std::ios_base::binary | std::ios_base::in);
    if (!data.fail())
    {
      try
      {
        boost::archive::portable_binary_iarchive a(data);
        a >> *this;
      }
      catch (const std::exception &e)
      {
        MERROR("Failed to load RPC payments file: " << e.what());
        m_client_info.clear();
      }
    }
    else
    {
      m_client_info.clear();
    }

    CATCH_ENTRY_L0("rpc_payment::load", false);
    return true;
  }

  bool rpc_payment::store(const std::string &directory_)
  {
    TRY_ENTRY();
    const std::string &directory = directory_.empty() ? m_directory : directory_;
    MDEBUG("storing rpc payments data to " << directory);
    if (!tools::create_directories_if_necessary(directory))
    {
      MWARNING("Failed to create data directory: " << directory);
      return false;
    }
    std::string state_file_path = (boost::filesystem::path(directory) / RPC_PAYMENTS_DATA_FILENAME).string();
    if (epee::file_io_utils::is_file_exist(state_file_path))
    {
      std::string state_file_path_old = state_file_path + ".old";
      boost::system::error_code ec;
      boost::filesystem::remove(state_file_path_old, ec);
      std::error_code e = tools::replace_file(state_file_path, state_file_path_old);
      if (e)
        MWARNING("Failed to rename " << state_file_path << " to " << state_file_path_old << ": " << e);
    }
    std::ofstream data;
    data.open(state_file_path, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
    if (data.fail())
    {
      MWARNING("Failed to save RPC payments to file " << state_file_path);
      return false;
    };
    boost::archive::portable_binary_oarchive a(data);
    a << *this;
    return true;
    CATCH_ENTRY_L0("rpc_payment::store", false);
    return true;
  }

  unsigned int rpc_payment::flush_by_age(time_t seconds)
  {
    unsigned int count = 0;
    const time_t now = time(NULL);
    if (seconds == 0)
      seconds = DEFAULT_FLUSH_AGE;
    const time_t t0 = seconds > now ? 0 : now - seconds;
    for (std::unordered_map<crypto::public_key, client_info>::iterator i = m_client_info.begin(); i != m_client_info.end(); )
    {
      std::unordered_map<crypto::public_key, client_info>::iterator j = i++;
      const time_t t = std::max(j->second.last_request_timestamp, j->second.update_time);
      if (j->second.credits == 0)
      {
        MINFO("Erasing " << j->first << " with " << j->second.credits << " credits");
        m_client_info.erase(j);
        ++count;
      }
      else if (t < t0)
      {
        MINFO("Erasing " << j->first << " with " << j->second.credits << " credits, inactive for " << (now-t)/86400 << " days");
        m_client_info.erase(j);
        ++count;
      }
    }
    return count;
  }

  uint64_t rpc_payment::get_hashes(unsigned int seconds) const
  {
    const time_t now = time(NULL);
    uint64_t hashes = 0;
    for (std::map<uint32_t, uint64_t>::const_reverse_iterator i = m_hashrate.crbegin(); i != m_hashrate.crend(); ++i)
    {
      unsigned age = now - i->first;
      if (age > seconds)
        break;
      hashes += i->second;
    }
    return hashes;
  }

  void rpc_payment::prune_hashrate(unsigned int seconds)
  {
    const time_t now = time(NULL);
    std::map<uint32_t, uint64_t>::iterator i;
    for (i = m_hashrate.begin(); i != m_hashrate.end(); ++i)
    {
      unsigned age = now - i->first;
      if (age <= seconds)
        break;
    }
    m_hashrate.erase(m_hashrate.begin(), i);
  }

  bool rpc_payment::on_idle()
  {
    flush_by_age();
    prune_hashrate(3600);
    return true;
  }
}
