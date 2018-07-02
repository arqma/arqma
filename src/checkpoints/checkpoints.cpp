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

#include "include_base_utils.h"

using namespace epee;

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "include_base_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "checkpoints"

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
      //ADD_CHECKPOINT(1, "");
      return true;
    }
    if (nettype == STAGENET)
    {
      //ADD_CHECKPOINT(1, "");
      return true;
    }
    //ADD_CHECKPOINT(1, "");
						ADD_CHECKPOINT(1, "6115a8e9902af15d31d14c698621d54e9bb594b0da053591ec5d1ceb537960ea");
						ADD_CHECKPOINT(101, "a71e3a9586848a2eb39e86a191e0b87e9dcac4a4b31541a06fa969004866d1fd");
						ADD_CHECKPOINT(201, "65e45b4a23ae426026c29f2eba45ffef0c42c2562b296e178ccf41b2fbedb606");
						ADD_CHECKPOINT(301, "02e7cac2339950a8629ceff7dfb2f50c357d52f855287fbe2f57df12d0e0eb71");
						ADD_CHECKPOINT(401, "c78bf50266f6d5c286ba2cad0f2011605fb40bad3e365087d190e7277b8ea5ad");
						ADD_CHECKPOINT(501, "f78ca5d4b2ed7193f0706936b67f04da7ac3f95114cbfac755379e1a98f3e436");
						ADD_CHECKPOINT(601, "f57980b6c6159fadd0efca3e168d57ab8b4b6a2a3b7bedfcd65d3bd1fdea424f");
						ADD_CHECKPOINT(701, "37efbd6814f81312514069d787e85daf8f1da0a8706d93d3cc33e8bead35b6ce");
						ADD_CHECKPOINT(801, "09e22852bc5b9dead2a5195aa5f0f04ace2de752159f53a486378e4304e8d20b");
						ADD_CHECKPOINT(901, "c45f51e55516c1c8e0c97fe3cdcb3dd862f078dbf9dbba2eeeb0d06fe1c4aee0");
						ADD_CHECKPOINT(1001, "69ab0d9e89e85bb85a62a0f4211637046ab34dc83cad77af91e3949a67a4d92e");
						ADD_CHECKPOINT(1101, "5b70c4093a3880691d8876cb592105d6f97b1a704d4e9d7d9f8fb500f6f26454");
						ADD_CHECKPOINT(1201, "732581126462b35dd9d5dbe5f0d9a7414651a98ae68cca1bdd3bba0968ba0167");
						ADD_CHECKPOINT(1301, "6d447366a94d6e2f2e1748af40d2657e62b5392b3669fd7c1535e2ca4342b9fb");
						ADD_CHECKPOINT(1401, "38379f124ecfa1e961b862ceafe4a140ac69522d6b108b30ffd37f18cb955f6f");
						ADD_CHECKPOINT(1501, "69b642af3d5e4663b9f4da67ad98caed18410423cb1c1f5509043ef3efeba12c");
						ADD_CHECKPOINT(1601, "e43748e044b66991a9d47d1f5db01b24ac10f198a36b0b602a5beb67371463b3");
						ADD_CHECKPOINT(1701, "69d76f1e1a30cce5da2d5a1fecaeb99d40b78ba25a59cd200d21e4208141ffb1");
						ADD_CHECKPOINT(1801, "96f29d75eb5ee8e17f58d7251af443db6a2b66fb9ba16e2c6518e2b4b4d4cd1d");
						ADD_CHECKPOINT(1901, "7f2ff88c74142df6e9c88bf168cdbbc06f47983a99cb7f246820892185da000a");
						ADD_CHECKPOINT(2001, "2a6fafbf65409a1a2c02c18621b61f542ba008cbafe1cafdb3db659412649c31");
						ADD_CHECKPOINT(2101, "44005a071058feb78cec71925656c02a71945991145da3fabad776fe1890594b");
						ADD_CHECKPOINT(2201, "e77aee582c8a3df4ccdd8177a4f502d412995c16d39821bdd9fc7e058fb0c57c");
						ADD_CHECKPOINT(2301, "2242516ef98ba213c76113c13b7bc8a6b7f14f087e398e34efee5f5759f5d914");
						ADD_CHECKPOINT(2401, "3ac8fef2896ecd3400a38fb6c8940036c9c8ad6458cc0676fb2ed2995604b3e7");
						ADD_CHECKPOINT(2501, "47397774d6fb89746382023a3f5569b606206e04433a31f239c559228e95e4a7");
						ADD_CHECKPOINT(2601, "e5fb909af6d7a47a562326e8772808eeb999cb128b79ea673110664420c2060f");
						ADD_CHECKPOINT(2701, "cc672d4f2080a2d097f0d7b35b4e26680e07f059549439101c5407d3946ee149");
						ADD_CHECKPOINT(2801, "e9192cad9b52cc7f585d8154d54f03f1f1549107c421aad376680d57194efda4");
						ADD_CHECKPOINT(2901, "140a3642e12611c5f13eb9bf4a0cf82b298351cb7ef065252925a296846ab055");
						ADD_CHECKPOINT(3001, "dde848feb6a637451320a784eb8a273aee6738a9164232cb2b2515d11e3d3b10");
						ADD_CHECKPOINT(3101, "e6cea92a51c023ba430e948af6da9fafbc8e6f89d14c4c469de1a80574973a58");
						ADD_CHECKPOINT(3201, "2eba3766d1254317c30c9f1d73fbca548df45abb5b4ddac8a181efcc894b2582");
						ADD_CHECKPOINT(3301, "f82a41591a281bb728b5d670dc8b53a49bd3ef00cdcb4b58eb56345c67ba196d");
						ADD_CHECKPOINT(3401, "b17e2c6d503d96313b91bb3387ed2421b5658996bdde1e4cef6221147e93f38e");
						ADD_CHECKPOINT(3501, "8523e653b3584e6ca6531bc37bb4d1b09afbc274fd9a793e0fe354ba97166f2e");
						ADD_CHECKPOINT(3601, "b885e16f15e297b2d4ce233dd1d0515f199ff02bcf7323e257bbb6c93d139b50");
						ADD_CHECKPOINT(3701, "aba3fa3722e868b83fa6ec29d3a744ec43e64945fc0ff72487c712bd41486aac");
						ADD_CHECKPOINT(3801, "cb5c338f7c517ecdfc2d417aa30073e5324e846bb24728748e99d2c4efa1bea0");
						ADD_CHECKPOINT(3901, "c844dc9e48fd0e5d1a5b123e16a02f7db9de73fb901de7ee18c79ea2875d7aa4");
						ADD_CHECKPOINT(4001, "c8a6df41ed07aa8be7593a680972221adead47ee4537926ae8690660a416d3c2");
						ADD_CHECKPOINT(4101, "3590b618c42163125bd4d7a198d06dcdd12dadf4159011f474045ee1736cd164");
						ADD_CHECKPOINT(4201, "e21015caa1eed03fc7bb53bff7b35bc5c99e7b5cd4f39abf4a59c1f24834a956");
						ADD_CHECKPOINT(4301, "6272c3f9ec8731baac2f4a74967e51091d3f441fcf32e90d0119fa5e83f54021");
						ADD_CHECKPOINT(4401, "262c2243163a49abbca1ec3c386def1e7c30e0701b86e60634db74a9501cc9d3");
						ADD_CHECKPOINT(4501, "d1456dd45f41599f69b1d5bac869a882a7eb99b47e6713bff9328dc543963e27");
						ADD_CHECKPOINT(4601, "14e60e8106eb57adbc828ee3c94f4b7c5578af745845bc524e34789c8116da56");
						ADD_CHECKPOINT(4701, "f06918c1e6938283ee13df6cf01fedb5bb7f9526590b69e8bec0a8b02f538778");
						ADD_CHECKPOINT(4801, "42f68559ebdb413f89a6baf4af75a306416ebc9cdd7caf0a8ba027ba3ff37571");
						ADD_CHECKPOINT(4901, "ec4b7404430cfb02b789849a61c0f26d3887f1af127acf5b1855e8951a14f4fc");
						ADD_CHECKPOINT(5001, "c7e3eca6433708bce1825b3cd15cb95b0264a601101c51daeeb0a5dc0ccd7e42");
						ADD_CHECKPOINT(5101, "5426503e615e6c592bba5c8798c6dc23ee9cd399c6bd6224d03fc4c3c707a6f0");
						ADD_CHECKPOINT(5201, "c44e01cef86866bcfe6dd13e562cb49f588e78d6bc43fdf4c0e6e3400c13f89f");
						ADD_CHECKPOINT(5301, "cb5abc765b137cc7ca938e17a3bfdab67677f37c72521d7adada98a54bf56307");
						ADD_CHECKPOINT(5401, "373b942eb3058768feedd6e606365c9aaab7a49e642ccf15d2e385b11ad18ff0");
						ADD_CHECKPOINT(5501, "3df674086d717f5811d96d9f584eac421cda8ce28e4738a6214c88b98af0fe8f");
						ADD_CHECKPOINT(6000, "7a484c09c956105890f147001d329b40af70756558a425dbd837fecd3d3c70b4");
						ADD_CHECKPOINT(6500, "3592d98ddf9ecc50eeec339074984ac2c3526c5e2d35bf76018277f81550a1ba");

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
