// Copyright (c) 2018 - 2026, The Arqma Network
// Copyright (c) 2014-2018, The Monero Project
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

#include <chrono>
#include <sstream>
#include <numeric>
#include <boost/algorithm/string.hpp>
#include "misc_language.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "file_io_utils.h"
#include "common/command_line.h"
#include "common/util.h"
#include "string_coding.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h"
#include "time_helper.h"
#include <boost/filesystem.hpp>

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "miner"

using namespace epee;
using namespace std::literals;

#include "miner.h"
#include "crypto/hash.h"

extern "C" void rx_slow_hash_allocate_state();
extern "C" void rx_slow_hash_free_state();

namespace cryptonote
{

  namespace
  {
    const command_line::arg_descriptor<std::string> arg_start_mining = {"start-mining", "Specify wallet address to mining for", "", true};
    const command_line::arg_descriptor<uint32_t> arg_mining_threads = {"mining-threads", "Specify mining threads count", 0, true};
  }


  miner::miner(i_miner_handler* phandler, const get_block_hash_t &gbh):m_stop(1),
    m_template{},
    m_phandler(phandler),
    m_gbh(gbh)
  {
  }
  //-----------------------------------------------------------------------------------------------------
  miner::~miner()
  {
    try { stop(); }
    catch (...) { /* ignore */ }
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::set_block_template(const block& bl, const difficulty_type& di, uint64_t height)
  {
    std::unique_lock lock{m_template_lock};
    m_template = bl;
    m_diffic = di;
    m_height = height;
    ++m_template_no;
    m_starter_nonce = crypto::rand<uint32_t>();
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_block_chain_update()
  {
    if(!is_mining())
      return true;

    return request_block_template();
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::request_block_template()
  {
    block bl;
    difficulty_type di{};
    uint64_t height{};
    uint64_t expected_reward; //only used for RPC calls - could possibly be useful here too?

    uint64_t seed_height;
    crypto::hash seed_hash;
    if(!m_phandler->get_block_template(bl, m_mine_address, di, height, expected_reward, ""s, seed_height, seed_hash))
    {
      LOG_ERROR("Failed to get_block_template(), stopping mining");
      return false;
    }
    set_block_template(bl, di, height);
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::on_idle()
  {
    m_update_block_template_interval.do_call([&](){
      if(is_mining())request_block_template();
      return true;
    });

    m_update_hashrate_interval.do_call([&](){
      update_hashrate();
      return true;
    });

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::update_hashrate()
  {
    std::unique_lock lock{m_hashrate_mutex};
    auto hashes = m_hashes.exchange(0);
    using dseconds = std::chrono::duration<double>;
    if (m_last_hr_update && is_mining())
      m_current_hash_rate = hashes / dseconds{std::chrono::steady_clock::now() - *m_last_hr_update}.count();
    m_last_hr_update = std::chrono::steady_clock::now();
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::init_options(boost::program_options::options_description& desc)
  {
    command_line::add_arg(desc, arg_start_mining);
    command_line::add_arg(desc, arg_mining_threads);
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::init(const boost::program_options::variables_map& vm, network_type nettype)
  {
    if(command_line::has_arg(vm, arg_start_mining))
    {
      address_parse_info info;
      if(!cryptonote::get_account_address_from_str(info, nettype, command_line::get_arg(vm, arg_start_mining)) || info.is_subaddress)
      {
        LOG_ERROR("Target account address " << command_line::get_arg(vm, arg_start_mining) << " has wrong format, starting daemon canceled");
        return false;
      }
      m_mine_address = info.address;
      m_threads_total = 1;
      m_do_mining = true;
      if(command_line::has_arg(vm, arg_mining_threads))
      {
        m_threads_total = command_line::get_arg(vm, arg_mining_threads);
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::is_mining() const
  {
    return !m_stop;
  }
  //-----------------------------------------------------------------------------------------------------
  const account_public_address& miner::get_mining_address() const
  {
    return m_mine_address;
  }
  //-----------------------------------------------------------------------------------------------------
  uint32_t miner::get_threads_count() const {
    return m_threads_total;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::start(const account_public_address& adr, int threads_count)
  {
    m_mine_address = adr;
    m_threads_total = std::max(threads_count, 1);
    m_starter_nonce = crypto::rand<uint32_t>();
    std::unique_lock lock{m_threads_lock};
    if(is_mining())
    {
      LOG_ERROR("Starting miner but it's already started");
      return false;
    }

    if(!m_threads.empty())
    {
      LOG_ERROR("Unable to start miner because there are active mining threads");
      return false;
    }

    request_block_template();//lets update block template

    m_stop = false;

    for(int i = 0; i < m_threads_total; i++)
      m_threads.emplace_back([=] { return worker_thread(i); });

    MINFO("Mining has started with " << m_threads_total << " threads, good luck!" );

    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  double miner::get_speed() const
  {
    if (is_mining())
    {
      std::unique_lock lock{m_hashrate_mutex};
      return m_current_hash_rate;
    }
    return 0.0;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::stop()
  {
    MTRACE("Miner has received stop signal");

    std::unique_lock lock{m_threads_lock};
    if (!is_mining())
    {
      MDEBUG("Not mining - nothing to stop" );
      return true;
    }

    m_stop = true;
    for (auto& th : m_threads)
      if (th.joinable())
        th.join();

    MINFO("Mining has been stopped, " << m_threads.size() << " finished" );
    m_threads.clear();
    return true;
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::find_nonce_for_given_block(const get_block_hash_t &gbh, block& bl, const difficulty_type& diffic, uint64_t height, const crypto::hash *seed_hash)
  {
    for(; bl.nonce != std::numeric_limits<uint32_t>::max(); bl.nonce++)
    {
      crypto::hash h;
      gbh(bl, height, seed_hash, tools::get_max_concurrency(), h);

      if(check_hash(h, diffic))
      {
        bl.invalidate_hashes();
        return true;
      }
    }
    bl.invalidate_hashes();
    return false;
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::on_synchronized()
  {
    if(m_do_mining)
    {
      start(m_mine_address, m_threads_total);
    }
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::pause()
  {
    std::unique_lock lock{m_miners_count_mutex};
    MDEBUG("miner::pause: " << m_pausers_count << " -> " << (m_pausers_count + 1));
    ++m_pausers_count;
    if(m_pausers_count == 1 && is_mining())
      MDEBUG("MINING PAUSED");
  }
  //-----------------------------------------------------------------------------------------------------
  void miner::resume()
  {
    std::unique_lock lock{m_miners_count_mutex};
    MDEBUG("miner::resume: " << m_pausers_count << " -> " << (m_pausers_count - 1));
    --m_pausers_count;
    if(m_pausers_count < 0)
    {
      m_pausers_count = 0;
      MERROR("Unexpected miner::resume() called");
    }
    if(!m_pausers_count && is_mining())
      MDEBUG("MINING RESUMED");
  }
  //-----------------------------------------------------------------------------------------------------
  bool miner::worker_thread(uint32_t index)
  {
    bool rx_set = false;

    MLOG_SET_THREAD_NAME(std::string("[miner ") + std::to_string(index) + "]");
    MGINFO("Miner thread was started [" << index << "]");
    uint32_t nonce = m_starter_nonce + index;
    uint64_t height = 0;
    difficulty_type local_diff = 0;
    uint32_t local_template_ver = 0;
    block b;
    rx_slow_hash_allocate_state();
    while(!m_stop)
    {
      if(m_pausers_count)//anti split workaround
      {
        std::this_thread::sleep_for(100ms);
        continue;
      }

      if(local_template_ver != m_template_no)
      {
        {
          std::unique_lock lock{m_template_lock};
          b = m_template;
          local_diff = m_diffic;
          height = m_height;
        }
        local_template_ver = m_template_no;
        nonce = m_starter_nonce + index;
      }

      if(!local_template_ver)//no any set_block_template call
      {
        LOG_PRINT_L2("Block template not set yet");
        std::this_thread::sleep_for(1s);
        continue;
      }

      b.nonce = nonce;
      crypto::hash h;
      if ((b.major_version >= RX_BLOCK_VERSION) && !rx_set)
      {
        crypto::rx_set_miner_thread(index, tools::get_max_concurrency());
        rx_set = true;
      }
      m_gbh(b, height, NULL, tools::get_max_concurrency(), h);

      if(check_hash(h, local_diff))
      {
        //we lucky!
        MGINFO_GREEN("Found block " << get_block_hash(b) << " at height " << height << " for difficulty: " << local_diff);
        cryptonote::block_verification_context bvc;
        m_phandler->handle_block_found(b, bvc);
      }
      nonce += static_cast<uint32_t>(m_threads_total);
      ++m_hashes;
    }
    rx_slow_hash_free_state();
    MGINFO("Miner thread stopped ["<< index << "]");
    return true;
  }
}
