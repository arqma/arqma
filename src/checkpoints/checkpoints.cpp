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
    {1731481, "94b47df7c0895399f85dff9f7a1d7c2e67f8809e2a2b5fb14bc9714a2bb1490d"},
    {1731482, "942d66af0c7fd6f366e35f18dbb1ca668693a4ad37a6058d61bc920550fe10b7"},
    {1773359, "9c3aedb9eeace5a282d70507ac2ea1d26d61c907b80abd135e462371e3f99d82"},
    {1773360, "c6e832846ec8cbdf3f8f34695a21b995c74a25e279289746c9c4e3a06b659bbc"},
    {1773364, "839c5bb521bd9b28f313f8fb4cc84f6064c60db318468c234357a91af04c8774"},
    {1773368, "51691e50f463b42740a683a84b6c795b548b2aee7d61211577db04d3f3f1998a"},
    {1773372, "3e8f3cb068db474c2390e6650dfa6d7c60eaf69da9e79080a60cda46ebfb8a3e"},
    {1773376, "c7fb0080b04ce76097c9b65f73580b60622818aa5ff23115402f500ef933ab3d"},
    {1773380, "aded871574abcb5aec263582d733db61b5f6ac8fe2219d5238e2f77ee6b732b5"},
    {1773384, "e590456ea57b1a880365c5a107cbdb2ff70fe740fde00638c920b7eb8fa75927"},
    {1773388, "b53a8aa326bef07b7afccd31deb534e82e51fba08b0f2e7bd241e23bff644e32"},
    {1773392, "c859e9132646f5f41f0bf8569c8e42701b93446a06d85c672807735adc684a2c"},
    {1773396, "1a01e3ad21b6c5c2d20b367f055d73bf39743116241008601549a0bbd0b00d14"},
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
    try
    {
      auto guard = db_wtxn_guard(m_db);
      m_db->update_block_checkpoint(checkpoint);
    }
    catch (const std::exception& e)
    {
      MERROR("Failed to add checkpoint with hash: " << checkpoint.block_hash << " at height: " << checkpoint.height << ", what = " << e.what());
      result = false;
    }

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::block_added(const cryptonote::block& block, const std::vector<cryptonote::transaction>& txs, checkpoint_t const *checkpoint)
  {
    uint64_t const height = get_block_height(block);
    if (height < service_nodes::CHECKPOINT_STORE_PERSISTENTLY_INTERVAL || block.major_version < network_version_16)
      return true;

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

    if (checkpoint)
      update_checkpoint(*checkpoint);

    return true;
  }
  //---------------------------------------------------------------------------
  void checkpoints::blockchain_detached(uint64_t height, bool)
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
  bool checkpoints::init(network_type nettype, class BlockchainDB *db)
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
