// Copyright (c) 2018-2022, The Arqma Network
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

#pragma once

#include <boost/program_options.hpp>
#include <boost/logic/tribool_fwd.hpp>
#include <boost/thread/thread.hpp>
#include <atomic>
#include "cryptonote_basic/blobdatatype.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/verification_context.h"
#include "cryptonote_basic/difficulty.h"
#include "common/periodic_task.h"
#include "syncobj.h"
#include "cryptonote_basic/blobdatatype.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace cryptonote
{
  using namespace std::literals;

  struct i_miner_handler
  {
    virtual bool handle_block_found(block& b, block_verification_context &bvc) = 0;
    virtual bool get_block_template(block &b, const account_public_address &adr, difficulty_type &diffic, uint64_t &height, uint64_t &expected_reward, const blobdata &ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash) = 0;
  protected:
    ~i_miner_handler(){};
  };

  typedef std::function<bool(const cryptonote::block&, uint64_t, const crypto::hash*, unsigned int, crypto::hash&)> get_block_hash_t;

  /************************************************************************/
  /*                                                                      */
  /************************************************************************/
  class miner
  {
  public:
    miner(i_miner_handler* phandler, const get_block_hash_t& gbh);
    ~miner();
    bool init(const boost::program_options::variables_map& vm, network_type nettype);
    static void init_options(boost::program_options::options_description& desc);
    bool set_block_template(const block &bl, const difficulty_type &diffic, uint64_t height);
    bool on_block_chain_update();
    bool start(const account_public_address& adr, size_t threads_count);
    uint64_t get_speed() const;
    uint32_t get_threads_count() const;
    bool stop();
    bool is_mining() const;
    const account_public_address& get_mining_address() const;
    bool on_idle();
    void on_synchronized();
    //synchronous analog (for fast calls)
    static bool find_nonce_for_given_block(const get_block_hash_t& gbh, block& bl, const difficulty_type& diffic, uint64_t height, const crypto::hash *seed_hash = NULL);
    void pause();
    void resume();
    void do_print_hashrate(bool do_hr);

  private:
    bool worker_thread();
    bool request_block_template();
    void merge_hr();

    struct miner_config
    {
      uint64_t current_extra_message_index;

      BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(current_extra_message_index)
      END_KV_SERIALIZE_MAP()
    };


    std::atomic<bool> m_stop;
    epee::critical_section m_template_lock;
    block m_template;
    std::atomic<uint32_t> m_template_no;
    std::atomic<uint32_t> m_starter_nonce;
    difficulty_type m_diffic;
    uint64_t m_height;
    std::atomic<uint32_t> m_thread_index;
    volatile uint32_t m_threads_total;
    std::atomic<int32_t> m_pausers_count;
    epee::critical_section m_miners_count_lock;

    std::list<boost::thread> m_threads;
    epee::critical_section m_threads_lock;
    i_miner_handler* m_phandler;
    get_block_hash_t m_gbh;
    account_public_address m_mine_address;
    tools::periodic_task m_update_block_template_interval{5s};
    tools::periodic_task m_update_merge_hr_interval{2s};
    std::vector<blobdata> m_extra_messages;
    miner_config m_config;
    std::string m_config_folder_path;
    std::atomic<uint64_t> m_last_hr_merge_time;
    std::atomic<uint64_t> m_hashes;
    std::atomic<uint64_t> m_current_hash_rate;
    epee::critical_section m_last_hash_rates_lock;
    std::list<uint64_t> m_last_hash_rates;
    bool m_do_print_hashrate;
    bool m_do_mining;
    boost::thread::attributes m_attrs;
  };
}
