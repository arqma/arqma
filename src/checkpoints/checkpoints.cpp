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

using namespace epee;

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  /**
   * @brief struct for loading a checkpoint from json
   */
  struct t_hashline
  {
    uint64_t height; //!< the height of the checkpoint
    std::string hash; //!< the hash for the checkpoint
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(height)
          KV_SERIALIZE(hash)
        END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief struct for loading many checkpoints from json
   */
  struct t_hash_json {
    std::vector<t_hashline> hashlines; //!< the checkpoint lines from the file
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(hashlines)
        END_KV_SERIALIZE_MAP()
  };

  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    CRITICAL_REGION_LOCAL(m_lock);
    if (m_points.count(height))
    {
      checkpoint_t const &checkpoint = m_points[height];
      crypto::hash const &curr_hash = checkpoint.block_hash;
      CHECK_AND_ASSERT_MES(h == curr_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    else
    {
      checkpoint_t checkpoint = {};
      checkpoint.type = checkpoint_type::predefined_or_dns;
      checkpoint.block_hash = h;
      m_points[height] = checkpoint;
    }

    return true;
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

    CRITICAL_REGION_LOCAL(m_lock);
    std::array<int, service_nodes::QUORUM_SIZE> unique_vote_set = {};
    auto pre_existing_checkpoint_it = m_points.find(vote.block_height);
    if (pre_existing_checkpoint_it != m_points.end())
    {
      checkpoint_t &checkpoint = pre_existing_checkpoint_it->second;
      if(checkpoint.type == checkpoint_type::predefined_or_dns)
        return true;

      if(checkpoint.block_hash == vote.block_hash)
      {
        bool added = add_vote_if_unique(checkpoint, vote);
        return added;
      }

      for(service_nodes::voter_to_signature const &vote_to_sig : checkpoint.signatures)
      {
        if(vote_to_sig.quorum_index > unique_vote_set.size())
          return false;
        ++unique_vote_set[vote_to_sig.quorum_index];
      }

    }

    std::vector<checkpoint_t> &candidate_checkpoints    = m_staging_points[vote.block_height];
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
      new_checkpoint.type = checkpoint_type::service_node;
      new_checkpoint.block_hash = vote.block_hash;
      candidate_checkpoints.push_back(new_checkpoint);
      curr_checkpoint = (candidate_checkpoints.end() - 1);
    }

    if(add_vote_if_unique(*curr_checkpoint, vote))
    {
      if(curr_checkpoint->signatures.size() > service_nodes::MIN_VOTES_TO_CHECKPOINT)
      {
        uint64_t reorg_sentinel_height = 0;
        int num_checkpoints = 0;
        for(auto it = m_points.rbegin(); it != m_points.rend() && num_checkpoints < 2; it++, num_checkpoints++)
        {
          reorg_sentinel_height = it->first;
          checkpoint_t const &reorg_sentinel_checkpoint = it->second;
          if(reorg_sentinel_checkpoint.type == checkpoint_type::predefined_or_dns)
            break;
        }

        m_oldest_possible_reorg_limit = reorg_sentinel_height + 1;
        m_points[vote.block_height] = *curr_checkpoint;
        candidate_checkpoints.erase(curr_checkpoint);
      }
    }

    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool* is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    bool found  = (it != m_points.end());
    if(is_a_checkpoint) *is_a_checkpoint = found;

    if(!found)
      return true;

    checkpoint_t const &checkpoint = it->second;
    bool result = checkpoint.block_hash == h;
    if(result)
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
    else
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH " << checkpoint.block_hash << "FETCHED HASH: " << h);
    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if(0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    if(it == m_points.begin())
      return true;

    bool result = block_height >= m_oldest_possible_reorg_limit;
    return result;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    uint64_t result = 0;
    if(m_points.size() > 0)
    {
      auto last_it = m_points.rbegin();
      result = last_it->first;
    }

    return result;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for(auto& pt : other.get_points())
    {
      if(m_points.count(pt.first))
      {
        checkpoint_t const &our_checkpoint = m_points.at(pt.first);
        checkpoint_t const &their_checkpoint = pt.second;
        CHECK_AND_ASSERT_MES(our_checkpoint.block_hash == their_checkpoint.block_hash, false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
      }
    }
    return true;
  }

  bool checkpoints::init_default_checkpoints(network_type nettype)
  {
    if (nettype == TESTNET)
    {
      ADD_CHECKPOINT(0, "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328");
      return true;
    }
    if (nettype == STAGENET)
    {
      ADD_CHECKPOINT(0, "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328");
      return true;
    }
    ADD_CHECKPOINT(0, "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328");
    ADD_CHECKPOINT(1, "6115a8e9902af15d31d14c698621d54e9bb594b0da053591ec5d1ceb537960ea");
    ADD_CHECKPOINT(2500, "a44c8ba7f0f7d8b6c389a441854b35f0181b7a003ff863c7443a77def997e668");
    ADD_CHECKPOINT(10000, "1a35ebbe820d2cad63112750d602817c00ce1e11e48fce302a9edb697f635533");
    ADD_CHECKPOINT(50000, "c356f107ab8a85a9fb0a8b99f6ead7493d650acb1ec30a3c4b6d82171ce50cc5");
    ADD_CHECKPOINT(100000, "8d7251c892a048740b0dbb4da24f44a9e5433b04e61426eb1a9671ea7ad69639");
    ADD_CHECKPOINT(150000, "fdb9d4132ccc0904dc9dc8f4746b6efe3e33d9c6051b5907d21b2005fc2dce29");
    ADD_CHECKPOINT(200000, "c6ef0e39741b42c2cb4644988d31c2743a6ddefc5495a773cc8c807513d818c3");
    ADD_CHECKPOINT(300000, "1cd8edefb47332b6d5afc1b161a8f1845aff817988763b5dc2094762b5bc5551");
    ADD_CHECKPOINT(400000, "c43ff8acd01aef5f22a1a875e167d9b28b3c703110255bdd6faf010fad5b2efa");
    ADD_CHECKPOINT(480000, "616bdd82bbeb6ac46228c277d7fadb90ff2ba624ce088bb2c0970f7a1b2bdc69");
    ADD_CHECKPOINT(491520, "ac93c7e4759b54aac4055316e4eab841d82c47e478edfd6c8f6c363c36572e18");
    ADD_CHECKPOINT(525000, "8f674535f6915218d17cf496798047d5dedba8276b55f31ff89e3a73473bd1b1");
    ADD_CHECKPOINT(575000, "79d338bc881334518eb8fd6e8d867efbc102395c7f81a686df01c126cb619016");
    ADD_CHECKPOINT(600000, "597e137729e091884f03943a4af6e4026d6209ebfaa48e2554084111c8cc9960");
    ADD_CHECKPOINT(635000, "5c856125ed827250ab0f503daa2b467c8fb641f53399d263ba25fb95f477657c");
    ADD_CHECKPOINT(664600, "dc9828eb735c8038a125e927218e3ae78c543c56530db2e5855846e3de3ce280");
    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if(!(boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if(!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for(std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
        LOG_PRINT_L1("Ignoring checkpoint height: " << height);
      } else {
        std::string blockhash = it->hash;
        LOG_PRINT_L1("Adding checkpoint height: " << height << " , with hash = " << blockhash);
        ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

}
