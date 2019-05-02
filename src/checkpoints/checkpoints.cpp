// Copyright (c) 2018-2019, The Arqma Network
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
#include <vector>

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
  checkpoints::checkpoints()
  {
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::parse_tpod_from_hex_string(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    is_a_checkpoint = it != m_points.end();
    if(!is_a_checkpoint)
      return true;

    if(it->second == h)
    {
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
      return true;
    }else
    {
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH: " << it->second << ", FETCHED HASH: " << h);
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h) const
  {
    bool ignored;
    return check_block(height, h, ignored);
  }
  //---------------------------------------------------------------------------
  //FIXME: is this the desired behavior?
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if (0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    // Is blockchain_height before the first checkpoint?
    if (it == m_points.begin())
      return true;

    --it;
    uint64_t checkpoint_height = it->first;
    return checkpoint_height < block_height;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    std::map< uint64_t, crypto::hash >::const_iterator highest =
        std::max_element( m_points.begin(), m_points.end(),
                         ( boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _1) <
                           boost::bind(&std::map< uint64_t, crypto::hash >::value_type::first, _2 ) ) );
    return highest->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
  }

  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for (auto& pt : other.get_points())
    {
      if (m_points.count(pt.first))
      {
        CHECK_AND_ASSERT_MES(pt.second == m_points.at(pt.first), false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
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
    ADD_CHECKPOINT(2501, "47397774d6fb89746382023a3f5569b606206e04433a31f239c559228e95e4a7");
    ADD_CHECKPOINT(5001, "c7e3eca6433708bce1825b3cd15cb95b0264a601101c51daeeb0a5dc0ccd7e42");
    ADD_CHECKPOINT(7501, "63f9167e807a6f5e6fb62568823b44f1e4092aecf786ba8956a8e7d24c112238");
    ADD_CHECKPOINT(10001, "bda3c8e2fa091da7165ca150eaf1a43607d182ca10d82fa9b48eaf005283a144");
    ADD_CHECKPOINT(12501, "b98614dd1c96e423c9e606feb8dca879762d6e60bf3e9b44e82a124de55d2153");
    ADD_CHECKPOINT(15001, "30a0ebd382cbd491d907a8a6096ec1069f9fe1002eeba262eeb518a058020878");
    ADD_CHECKPOINT(17501, "f60bab232ffe2881b03d009cbb1cff1a938d624b14ccf7e617b2f69b2e7d9933");
    ADD_CHECKPOINT(20001, "21d89bd3f01829f76df2229d36d7c53dba2c6efd07f4290af7829e5d918ac0be");
    ADD_CHECKPOINT(22501, "1e6137446c0af9c4c31eac38daddf0096b57389abb565b22c4e137b12150fb0f");
    ADD_CHECKPOINT(25001, "4b2f7a95e303351a22c829295aad7f46b5b8c0d7723a9ae1b49d5140d1c9d4db");
    ADD_CHECKPOINT(27501, "2fe2bc5151c500202ee25fca03e890322d4b62faf6d321ff2025bf717aef7069");
    ADD_CHECKPOINT(30001, "cff346069cf2ba26eafb239ef4126f79ee5c3c0e208a810d8abfd9cbabbbba1a");
    ADD_CHECKPOINT(32501, "b479effb8dece922b61c32246326ef074f585b9e801f4aff6b372cf3182748a2");
    ADD_CHECKPOINT(35001, "3902f65dd85e10e19638438d7fccc8e13070489ded7481ef6e8619e9335e7d60");
    ADD_CHECKPOINT(37501, "5eaddc2d5dafcc960a9ed865095d428679dba178201a6d20041426afcbe536ff");
    ADD_CHECKPOINT(40001, "53dd7de3ba4947722a0bd8ca672e17d006356187369cbbcf37b6fce631e4d2e5");
    ADD_CHECKPOINT(42501, "c62e91208beebf05b5274657ee689c4f85e5b3986c20fca7683922a0103aaed8");
    ADD_CHECKPOINT(45001, "8078e01a4ce01228d23275e0a769f7f026968540061ba8315b30a78cfb370bc7");
    ADD_CHECKPOINT(47501, "0596a214be859f15c407fbf3eefadccc6b794adf9f92ae939f182f553b6bfb8a");
    ADD_CHECKPOINT(50001, "cc447c4d939ac7824b556ed259f2af3c8b701cefca5828b421d723361618314d");
    ADD_CHECKPOINT(52501, "7a72173e18622156e5a5607ba532cc9d7be942d340caff626472293237a62fba");
    ADD_CHECKPOINT(55001, "8cf12387a1e539c69a55a4773747112f7f608b0908cb990ad41244d15b8dbc59");
    ADD_CHECKPOINT(57501, "716d86d3f747c137519fb29d591a4e298e470352807e1379b5a1153e4619b067");
    ADD_CHECKPOINT(60001, "dbd1fc31101c3b535c97e3cd998336e0ba2a2678cdac1149a7aa1d2178e206df");
    ADD_CHECKPOINT(62501, "ac76a2729d131565c2289bffb5a986479d9a5b22689c11552e37882f304a7495");
    ADD_CHECKPOINT(65001, "cc85d6ce17e6712b116ea3e57f45ef89a9b2bc1b120edc1a9dd44a706b60e5ab");
    ADD_CHECKPOINT(67501, "c984245e202c0e105ea072d385dafd24bd5aee97d785926c724d4b8e5d03a8f6");
    ADD_CHECKPOINT(70001, "6604edd93c773fc1f9cb599d41d66b3127234e59787315b5fb2ae38fc7d03cf9");
    ADD_CHECKPOINT(72501, "40bc9ed5e23dc852775305f5fc08a1f08fe70e63db0d46429c2fb0567cfc6afb");
    ADD_CHECKPOINT(75001, "e5aa7d0d5c89eaed5975ca61be0875f8b7531f38c5e43108ead68d95580fea84");
    ADD_CHECKPOINT(77501, "16181f7ec323950d15edaa4f2bff18133d52f527b54d4200929f3ac7c5a42aba");
    ADD_CHECKPOINT(80001, "10fa2eccce7653b6bb1f02da6824371477e0702d3e54d3cb492a7cf313b40733");
    ADD_CHECKPOINT(82501, "c73ce0ee241c07cfb8a724d5f7c1261a4f9ab7e9159e45b9928ec631c38b049e");
    ADD_CHECKPOINT(85001, "92400458aaf2e0ccaf1ad07cd070fff72cc07685954e3bd4873de2e0bd3d2aa3");
    ADD_CHECKPOINT(87501, "4adbaeb024ca0c0ff27469b611a6fdc664be9e4b33485c24befdca1c33cd8c75");
    ADD_CHECKPOINT(90001, "670b37c8999e8864431c700b734e32a24207e4b582e6879862f246a6d537ac1d");
    ADD_CHECKPOINT(92501, "a991694163ca8b22f31763ba5cb6225af225f1b70cbc57f8d38a86e5d0a5bcc9");
    ADD_CHECKPOINT(95001, "e19ea87f47698c9d8b6873d4a4ae3db2ce8c5d9f30a57dd3c1bece63eb4d0bfd");
    ADD_CHECKPOINT(97501, "d4ab26a0fdfe971a80436d1bb4419c433c69aa7a629bf9cc30e19b780248c05b");
    ADD_CHECKPOINT(100001, "2281f5c49432b56194ed3d5a1610ddb05fe95376a2ddc9909c2a5128f94425e9");
    ADD_CHECKPOINT(102501, "02d0173946f71438602c43fa899bb1241dd447c3e2a660cea73e25c4f30fe3ae");
    ADD_CHECKPOINT(105001, "1b1feb0668b6dd62f73122668c4596342693a27635a82941609cca12be6654d5");
    ADD_CHECKPOINT(107501, "7343b43c9ace82d6b5a5dabc73fbefdf6086151ea8224579525a489c72f3e38a");
    ADD_CHECKPOINT(110001, "1e9049664a677c4120ec3eb04839424ea36f8df5bfb0e54558261b8b3bb19836");
    ADD_CHECKPOINT(112501, "e83051da44795891749a8b2613c141e910354548a6e47987bb45c9207658c1cd");
    ADD_CHECKPOINT(115001, "c2d51e0873805263e71c101d5ac484cb4c129e57cf8fd20884839ebfb5880845");
    ADD_CHECKPOINT(117501, "a173c7586cb5651daf8f575da0cf300b2a03688dd37297546c7d9bbaf9ffe583");
    ADD_CHECKPOINT(120000, "cf504edaea8db888c8a351bceea3690a2b24e37160e915c2c0c5faddc8df337d");
    ADD_CHECKPOINT(125000, "77e0f2c0d8e2033c77b1eca65027554f4849634756731d14d4b98f28de678ae6");
    ADD_CHECKPOINT(127900, "15f010ec092b1fe44accaf21a79bcbb13564daebc1e4708b03845352aba925e2");
    ADD_CHECKPOINT(127940, "25c30d4db768ebeaf7bf973a22e51183e134ee5d2ccd6b4d1942f68f2061076e");
    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for (std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
	LOG_PRINT_L1("ignoring checkpoint height " << height);
      } else {
	std::string blockhash = it->hash;
	LOG_PRINT_L1("Adding checkpoint height " << height << ", hash=" << blockhash);
	ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

  bool checkpoints::load_checkpoints_from_dns(network_type nettype)
  {
    std::vector<std::string> records;

    // All four ArQ-Net domains have DNSSEC on and valid
    static const std::vector<std::string> dns_urls = {
        "checkpoints.arqma.com",
        "checkpoints.myarqma.com",
        "checkpoints.supportarqma.com",
        "checkpoints.supportarqma.eu"
	};

    static const std::vector<std::string> testnet_dns_urls = {
    };

    static const std::vector<std::string> stagenet_dns_urls = {
    };

    if (!tools::dns_utils::load_txt_records_from_dns(records, nettype == TESTNET ? testnet_dns_urls : nettype == STAGENET ? stagenet_dns_urls : dns_urls))
      return true; // why true ?

    for (const auto& record : records)
    {
      auto pos = record.find(":");
      if (pos != std::string::npos)
      {
        uint64_t height;
        crypto::hash hash;

        // parse the first part as uint64_t,
        // if this fails move on to the next record
        std::stringstream ss(record.substr(0, pos));
        if (!(ss >> height))
        {
    continue;
        }

        // parse the second part as crypto::hash,
        // if this fails move on to the next record
        std::string hashStr = record.substr(pos + 1);
        if (!epee::string_tools::parse_tpod_from_hex_string(hashStr, hash))
        {
    continue;
        }

        ADD_CHECKPOINT(height, hashStr);
      }
    }
    return true;
  }

  bool checkpoints::load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype, bool dns)
  {
    bool result;

    result = load_checkpoints_from_json(json_hashfile_fullpath);
    if (dns)
    {
      result &= load_checkpoints_from_dns(nettype);
    }

    return result;
  }
}
