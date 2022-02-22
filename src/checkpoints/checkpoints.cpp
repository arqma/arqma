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
#include "cryptonote_core/service_node_rules.h"
#include <functional>
#include <vector>
#include "syncobj.h"
#include "blockchain_db/blockchain_db.h"

using namespace epee;

#include "common/arqma.h"

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  height_to_hash const HARDCODED_MAINNET_CHECKPOINTS[] = {
    {0,      "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328"},
    {1,      "6115a8e9902af15d31d14c698621d54e9bb594b0da053591ec5d1ceb537960ea"},
    {1000,   "6b94e23fedee9ed59f0517805419062c0318e729ad19858f3f6fc51fd65e33d1"},
    {10000,  "1a35ebbe820d2cad63112750d602817c00ce1e11e48fce302a9edb697f635533"},
    {100000, "8d7251c892a048740b0dbb4da24f44a9e5433b04e61426eb1a9671ea7ad69639"},
    {125000, "77e0f2c0d8e2033c77b1eca65027554f4849634756731d14d4b98f28de678ae6"},
    {175000, "d26a135e373447fd1603ec98022db674c1d4d320b4ff9cae9f9c322c81afd1a5"},
    {250000, "1b25cc5c39a3d7b4009df07c0f95901bde0785783eea0449d3aa6bbb3c74aff0"},
    {300000, "1cd8edefb47332b6d5afc1b161a8f1845aff817988763b5dc2094762b5bc5551"},
    {400000, "c43ff8acd01aef5f22a1a875e167d9b28b3c703110255bdd6faf010fad5b2efa"},
    {500000, "0339514b6c97b1e2446038e996093ce0978b7e552f3365b476fe52b4d3ca9776"},
    {600000, "597e137729e091884f03943a4af6e4026d6209ebfaa48e2554084111c8cc9960"},
    {700000, "b4aa021dd840af826e879751957076397fa23f38f47335a64b1226c2589e0b04"},
    {800000, "3e37bc9fdf22eecbf45c7172482e152166952389271fb84dd9fd9718e7c8171a"},
    {881000, "a5e535189f58431bba4a67939ffb4d9317ddba2438012f39a126ae95ada1ff8a"},
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
  static bool get_checkpoint_from_db_safe(BlockchainDB const *db, uint64_t height, checkpoint_t &checkpoint)
  {
    try
    {
      return db->get_block_checkpoint(height, checkpoint);
    }
    catch(const std::exception &e)
    {
      MERROR("Get block checkpoint from DB failed at height " << height << ". Reason: " << e.what());
      return false;
    }
  }
  //---------------------------------------------------------------------------
  static bool update_checkpoint_in_db_safe(BlockchainDB *db, checkpoint_t &checkpoint)
  {
    try
    {
      db->update_block_checkpoint(checkpoint);
    }
    catch(const std::exception &e)
    {
      MERROR("Failed to add checkpoint with hash: " << checkpoint.block_hash << " at height: " << checkpoint.height << " to database. Reason: " << e.what());
      return false;
    }

    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    checkpoint_t checkpoint = {};
    if(get_checkpoint_from_db_safe(m_db, height, checkpoint))
    {
      crypto::hash const &curr_hash = checkpoint.block_hash;
      CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    else
    {
      checkpoint.height = height;
      checkpoint.type = checkpoint_type::hardcoded;
      checkpoint.block_hash = h;
      r = update_checkpoint_in_db_safe(m_db, checkpoint);
    }

    return r;
  }
  //---------------------------------------------------------------------------
  static bool add_vote_if_unique(checkpoint_t &checkpoint, service_nodes::checkpoint_vote const &vote)
  {
    CHECK_AND_ASSERT_MES(checkpoint.block_hash == vote.block_hash, false, "DEV Error");

    CHECK_AND_ASSERT_MES(vote.voters_quorum_index < service_nodes::QUORUM_SIZE, false, "Vote is indexing out of bounds");

    const auto signature_it = std::find_if(checkpoint.signatures.begin(), checkpoint.signatures.end(), [&vote](service_nodes::voter_to_signature const &check)
    {
      return vote.voters_quorum_index == check.quorum_index;
    });

    if(signature_it == checkpoint.signatures.end())
    {
      service_nodes::voter_to_signature new_voter_to_signature = {};
      new_voter_to_signature.quorum_index = vote.voters_quorum_index;
      new_voter_to_signature.signature = vote.signature;
      checkpoint.signatures.push_back(new_voter_to_signature);
      return true;
    }

    return false;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint_vote(service_nodes::checkpoint_vote const &vote)
  {
#if 0
    uint64_t newest_checkpoint_height = get_max_height();
    if(vote.block_height < newest_checkpoint_height)
      return true;
#endif

    std::array<int, service_nodes::QUORUM_SIZE> unique_vote_set = {};
    std::vector<checkpoint_t> &candidate_checkpoints = m_staging_points[vote.block_height];
    std::vector<checkpoint_t>::iterator curr_checkpoint = candidate_checkpoints.end();
    for(auto it = candidate_checkpoints.begin(); it != candidate_checkpoints.end(); it++)
    {
      checkpoint_t const &checkpoint = *it;
      if(checkpoint.block_hash == vote.block_hash)
        curr_checkpoint = it;

      for(service_nodes::voter_to_signature const &vote_to_sig : checkpoint.signatures)
      {
        if(vote_to_sig.quorum_index > unique_vote_set.size())
          return false;

        if(++unique_vote_set[vote_to_sig.quorum_index] > 1)
        {
          return false;
        }
      }
    }

    if(curr_checkpoint == candidate_checkpoints.end())
    {
      checkpoint_t new_checkpoint = {};
      new_checkpoint.height = vote.block_height;
      new_checkpoint.type = checkpoint_type::service_node;
      new_checkpoint.block_hash = vote.block_hash;
      candidate_checkpoints.push_back(new_checkpoint);
      curr_checkpoint = (candidate_checkpoints.end() - 1);
    }

    if(add_vote_if_unique(*curr_checkpoint, vote))
    {
      if(curr_checkpoint->signatures.size() > service_nodes::MIN_VOTES_TO_CHECKPOINT)
      {
        update_checkpoint_in_db_safe(m_db, *curr_checkpoint);
      }
    }

    return true;
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
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool* is_a_checkpoint) const
  {
    checkpoint_t checkpoint;
    bool found = get_checkpoint_from_db_safe(m_db, height, checkpoint);
    if(is_a_checkpoint) *is_a_checkpoint = found;

    if(!found)
      return true;

    bool result = checkpoint.block_hash == h;
    if(result)
      MINFO("Checkpoint verified and validated for height: " << height << " " << h);
    else
      MWARNING("Wrong Checkpoint for height: " << height << "\nExpected checkpoint_hash: " << checkpoint.block_hash << " while Received checkpoint_hash: " << h);
    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if(0 == block_height)
      return false;

    size_t num_desired_checkpoints = 2;
    std::vector<checkpoint_t> checkpoints = m_db->get_checkpoints_range(blockchain_height, 0, num_desired_checkpoints);

    if(checkpoints.size() == 0)
      return true;

    uint64_t sentinel_reorg_height = 0;
    if(checkpoints[0].type == checkpoint_type::service_node)
    {
      if(checkpoints.size() == 1)
      {
        return true;
      }
      else
      {
        sentinel_reorg_height = checkpoints[1].height;
      }
    }
    else
    {
      sentinel_reorg_height = checkpoints[0].height;
    }

    bool result = sentinel_reorg_height < block_height;
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
  bool checkpoints::init(network_type nettype, struct BlockchainDB *db)
  {
    *this = {};
    m_db = db;

    db_wtxn_guard txn_guard(m_db);
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
