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

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include "cryptonote_core/service_node_rules.h"
#include <functional>
#include <vector>
#include "syncobj.h"
#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

using namespace epee;

#include "common/arqma.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  bool checkpoint_t::check(crypto::hash const &hash) const
  {
    bool result = block_hash == hash;
    if (result)
      MINFO("Checkpoint passed for height: " << height << " " << block_hash);
    else
      MWARNING("Checkpoint failed for height: " << height << ". Expected hash: " << block_hash << ", Given hash: " << hash);

    return result;
  }

  height_to_hash const HARDCODED_MAINNET_CHECKPOINTS[] = {
    {0,       "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328"},
    {1,       "6115a8e9902af15d31d14c698621d54e9bb594b0da053591ec5d1ceb537960ea"},
    {1000,    "6b94e23fedee9ed59f0517805419062c0318e729ad19858f3f6fc51fd65e33d1"},
    {10000,   "1a35ebbe820d2cad63112750d602817c00ce1e11e48fce302a9edb697f635533"},
    {100000,  "8d7251c892a048740b0dbb4da24f44a9e5433b04e61426eb1a9671ea7ad69639"},
    {125000,  "77e0f2c0d8e2033c77b1eca65027554f4849634756731d14d4b98f28de678ae6"},
    {175000,  "d26a135e373447fd1603ec98022db674c1d4d320b4ff9cae9f9c322c81afd1a5"},
    {250000,  "1b25cc5c39a3d7b4009df07c0f95901bde0785783eea0449d3aa6bbb3c74aff0"},
    {300000,  "1cd8edefb47332b6d5afc1b161a8f1845aff817988763b5dc2094762b5bc5551"},
    {350000,  "b0b6ddc595b4d72dea31aa004fc85db908057b8ea0cb9067d04a29f696ed7f6a"},
    {400000,  "c43ff8acd01aef5f22a1a875e167d9b28b3c703110255bdd6faf010fad5b2efa"},
    {450000,  "28228697f98e0c3bbf15edec8ad6964ced21bdce850287a56d17830b4d0811f6"},
    {500000,  "0339514b6c97b1e2446038e996093ce0978b7e552f3365b476fe52b4d3ca9776"},
    {550000,  "73b7cd91e882854fabfd08fdd6ef9e6664496f054bf781b3d549c1245c9abdf0"},
    {600000,  "597e137729e091884f03943a4af6e4026d6209ebfaa48e2554084111c8cc9960"},
    {650000,  "6d67c572a34f84046ad4578eae049daec17bca618453ce038e6dd0235c2a762b"},
    {700000,  "b4aa021dd840af826e879751957076397fa23f38f47335a64b1226c2589e0b04"},
    {750000,  "c6f55549ceafa9cf7e63a0db377728bd6ec14739ef2ba0f531ee399d0722b512"},
    {800000,  "3e37bc9fdf22eecbf45c7172482e152166952389271fb84dd9fd9718e7c8171a"},
    {850000,  "4b19f6f638e13a30f06b8e04c6119441b4675c6a92a8799b0cc131b244fe6f94"},
    {900000,  "00a27db6706386c46c130b63e6b7f2293f8f2b8e217c0da9f806ecc1c1bae534"},
    {950000,  "f8568d8ebcd7fc6060af2b246e524fc98e51abc13481cab93319827be7fe0e00"},
    {1000000, "0db826448a9978bec4617f3f1a23c33c708f82949a2e504602c5d4faac878639"},
    {1050000, "931108145f3989036f3064b0ac74c77df28e39fd18f775f625cc5e03e4b5c034"},
    {1100000, "d50613bbbfec20514d5e9933fae7ca3f21bd4861a9757c9002133ae30998e199"},
    {1134090, "644d30e172bef0ae77e60383ef219ee7af722c703c80be5c8a432e7d0c379932"}
  };
  //---------------------------------------------------------------------------
  crypto::hash get_newest_hardcoded_checkpoint(cryptonote::network_type nettype, uint64_t *height)
  {
    crypto::hash result = crypto::null_hash;
    *height = 0;
    if(nettype != MAINNET)
      return result;

    uint64_t last_index = arqma::array_count(HARDCODED_MAINNET_CHECKPOINTS) - 1;
    height_to_hash const &entry = HARDCODED_MAINNET_CHECKPOINTS[last_index];

    if(epee::string_tools::hex_to_pod(entry.hash, result))
      *height = entry.height;

    return result;
  }
  //---------------------------------------------------------------------------
  bool load_checkpoints_from_json(const std::string &json_hashfile_fullpath, std::vector<height_to_hash> &checkpoint_hashes)
  {
    boost::system::error_code errcode;
    if(!(boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    height_to_hash_json hashes;
    if(!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }

    checkpoint_hashes = std::move(hashes.hashlines);
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::get_checkpoint(uint64_t height, checkpoint_t &checkpoint) const
  {
    try
    {
      auto guard = db_rtxn_guard(m_db);
      return m_db->get_block_checkpoint(height, checkpoint);
    }
    catch(const std::exception &e)
    {
      MERROR("Get block checkpoint from DB failed at height " << height << ". Reason: " << e.what());
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    checkpoint_t checkpoint = {};
    if(get_checkpoint(height, checkpoint))
    {
      crypto::hash const &curr_hash = checkpoint.block_hash;
      CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    else
    {
      checkpoint.type = checkpoint_type::hardcoded;
      checkpoint.height = height;
      checkpoint.block_hash = h;
      r = update_checkpoint(checkpoint);
    }

    return r;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::update_checkpoint(checkpoint_t const &checkpoint)
  {
    bool result = true;
    bool batch_started = false;
    try
    {
      batch_started = m_db->batch_start();
      m_db->update_block_checkpoint(checkpoint);
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to add checkpoint with hash: " << checkpoint.block_hash << " at height: " << checkpoint.height << ", what = " << e.what());
      result = false;
    }

    if (batch_started) m_db->batch_stop();
    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, checkpoint_t const *checkpoint)
  {
    uint64_t const height = get_block_height(block);
    if (height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL || block.major_version < network_version_16)
      return true;

    if (checkpoint)
      update_checkpoint(*checkpoint);

    uint64_t end_cull_height = 0;
    {
      checkpoint_t immutable_checkpoint;
      if (m_db->get_immutable_checkpoint(&immutable_checkpoint, height + 1))
        end_cull_height = immutable_checkpoint.height;
    }
    uint64_t start_cull_height = (end_cull_height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL) ? 0 : end_cull_height - service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL;

    if ((start_cull_height % service_nodes::CHECKPOINT_INTERVAL) > 0)
      start_cull_height += (service_nodes::CHECKPOINT_INTERVAL - (start_cull_height % service_nodes::CHECKPOINT_INTERVAL));

    m_last_cull_height = std::max(m_last_cull_height, start_cull_height);
    auto guard = db_wtxn_guard(m_db);
    for (; m_last_cull_height < end_cull_height; m_last_cull_height += service_nodes::CHECKPOINT_INTERVAL)
    {
      if (m_last_cull_height % service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL == 0) continue;

      try
      {
        m_db->remove_block_checkpoint(m_last_cull_height);
      }
      catch (const std::exception &e)
      {
        MERROR("Pruning block checkpoint on block added failed non-trivially at height: " << m_last_cull_height << ", what = " << e.what());
      }
    }

    return true;
  }
  //---------------------------------------------------------------------------
  void checkpoints::blockchain_detached(uint64_t height)
  {
    m_last_cull_height = std::min(m_last_cull_height, height);

    checkpoint_t top_checkpoint;
    auto guard = db_wtxn_guard(m_db);
    if (m_db->get_top_checkpoint(top_checkpoint))
    {
      uint64_t start_height = top_checkpoint.height;
      for (size_t delete_height = start_height; delete_height >= height && delete_height >= service_nodes::CHECKPOINT_INTERVAL; delete_height -= service_nodes::CHECKPOINT_INTERVAL)
      {
        try
        {
          m_db->remove_block_checkpoint(delete_height);
        }
        catch (const std::exception &e)
        {
          MERROR("Remove block checkpoint on detach failed non-trivially at height: " << delete_height << ", what = " << e.what());
        }
      }
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    uint64_t top_checkpoint_height = 0;
    checkpoint_t top_checkpoint;
    if(m_db->get_top_checkpoint(top_checkpoint))
      top_checkpoint_height = top_checkpoint.height;

    return height <= top_checkpoint_height;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool* is_a_checkpoint, bool *service_node_checkpoint) const
  {
    checkpoint_t checkpoint;
    bool found = get_checkpoint(height, checkpoint);
    if (is_a_checkpoint) *is_a_checkpoint = found;
    if (service_node_checkpoint) *service_node_checkpoint = false;

    if(!found)
      return true;

    bool result = checkpoint.check(h);
    if (service_node_checkpoint)
      *service_node_checkpoint = (checkpoint.type == checkpoint_type::service_node);

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height, bool *service_node_checkpoint)
  {
    if (service_node_checkpoint)
      *service_node_checkpoint = false;

    if(0 == block_height)
      return false;

    {
      std::vector<checkpoint_t> const first_checkpoint = m_db->get_checkpoints_range(0, blockchain_height, 1);
      if (first_checkpoint.empty() || blockchain_height < first_checkpoint[0].height)
        return true;
    }

    checkpoint_t immutable_checkpoint;
    uint64_t immutable_height = 0;
    if (m_db->get_immutable_checkpoint(&immutable_checkpoint, blockchain_height))
    {
      immutable_height = immutable_checkpoint.height;
      if (service_node_checkpoint)
        *service_node_checkpoint = (immutable_checkpoint.type == checkpoint_type::service_node);
    }

    m_immutable_height = std::max(immutable_height, m_immutable_height);
    bool result = block_height > m_immutable_height;
    return result;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    uint64_t result = 0;
    checkpoint_t top_checkpoint;
    if(m_db->get_top_checkpoint(top_checkpoint))
      result = top_checkpoint.height;

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::init(network_type nettype, BlockchainDB *db)
  {
    *this = {};
    m_db = db;
    m_nettype = nettype;

    if(nettype == MAINNET)
    {
      for(size_t i = 0; i < arqma::array_count(HARDCODED_MAINNET_CHECKPOINTS); ++i)
      {
        height_to_hash const &checkpoint = HARDCODED_MAINNET_CHECKPOINTS[i];
        ADD_CHECKPOINT(checkpoint.height, checkpoint.hash);
      }
    }

    return true;
  }

}
