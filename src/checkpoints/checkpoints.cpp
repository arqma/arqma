// Copyright (c) 2021-2021, The GNTL Project
// Copyright (c) 2014-2020, The Monero Project
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

using namespace epee;

#undef GNTL_DEFAULT_LOG_CATEGORY
#define GNTL_DEFAULT_LOG_CATEGORY "checkpoints"

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
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
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
    }
    else
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
    std::map<uint64_t, crypto::hash>::const_iterator highest = std::max_element(m_points.begin(), m_points.end(),
             (boost::bind(&std::map<uint64_t, crypto::hash>::value_type::first, boost::placeholders::_1) <
              boost::bind(&std::map<uint64_t, crypto::hash>::value_type::first, boost::placeholders::_2)));

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
//      ADD_CHECKPOINT(0, "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328");
      return true;
    }
    if (nettype == STAGENET)
    {
//      ADD_CHECKPOINT(0, "60077b4d5cd49a1278d448c58b6854993d127fcaedbdeab82acff7f7fd86e328");
      return true;
    }
        ADD_CHECKPOINT(0, "4010c82bbd835d9580dd95fdcd0834d90102782f06016c9f069a2303ddc682e0");
        ADD_CHECKPOINT(500, "c21af0c2886614fef259940910e1b850dd4afb684ffc67e1ccf549b9ec508674");
        ADD_CHECKPOINT(1000, "3d9edb82624f358ec47ad09835380fccb4659ca21371c72b6bce79dfba901681");
        ADD_CHECKPOINT(1500, "657351cb49dee907b5bf46a69d3db05632b9ac95274ed01b1b2ca74a4f96de0c");
        ADD_CHECKPOINT(2000, "092c3ccda5d388fb7d6cde25f7973799b6789ccdd90fa7c9b4eebd40142a275e");
        ADD_CHECKPOINT(2500, "d362848551a2eaa0c0fc558264c09b9a69afe37539f46f57e975731c9325ab58");
        ADD_CHECKPOINT(3000, "5f4c527be76393e8d55c2c323349de636757e94cfa131685aefa987f39bfbed4");
        ADD_CHECKPOINT(3500, "17a4fa4cd16d40d5e651527d2e07e4f3d8710d3040ca67ea2f9db94e041ba938");
        ADD_CHECKPOINT(4000, "ef6ba9f388d789bf708359b15d994a1d973e88c70763f5fd412ff92557e69369");
        ADD_CHECKPOINT(4500, "c92e604f262b485351142acf49a2cdbf71aed73dba303175ca8a2990d48537f4");
        ADD_CHECKPOINT(5000, "ce450b174764f4cf1132d32f1df95613a2e483174c5d627e39bda844c5da0cbf");
        ADD_CHECKPOINT(5500, "26621ba4536f139184acf89c447b598a781217ea4e7b8aafdf5d368054195891");
        ADD_CHECKPOINT(6000, "f07eab3951498675ed2b3595ce30c164db342f27db30194d83f9c960b38509fa");
        ADD_CHECKPOINT(6500, "d02c1762bd425f54a925290b9f41cc6ebdcd7ff7324e39d22f6602206659c5d1");
        ADD_CHECKPOINT(7000, "c4cb35dcd453bddfd3500d86dcd219a97c32f0659b062c302d2207a51872ebb3");
        ADD_CHECKPOINT(7500, "64d87b76b5d9ef5a1907489c2ea76c72d296f9c861b46aadcbe96e6dea55eb26");
        ADD_CHECKPOINT(8000, "3f96a8c5f18e16d4b316a1342d1021c1c3f3cc5b9f2ff94a2cc57e5bcd18c0a8");
        ADD_CHECKPOINT(8500, "34df9e634bcf18074c57ba1f6ace069ce623a50c2f5db30bec399ac561362244");
        ADD_CHECKPOINT(9000, "c171654980943f8321d7a2cde9fab903520c1b607e028929bfce6acac1b69498");
        ADD_CHECKPOINT(9500, "81772e8f1eb5d1e2d784a8daf64d1944ec484a7e6540456bcf4a3acab367aefe");
        ADD_CHECKPOINT(10000, "6c25524cd1cb51bdd7ca5e284f084e7edcccd2a552cb103c1f3feae8428edeab");
        ADD_CHECKPOINT(10500, "012f83ae0e95b459c3ca2ad7e7caa00464a5d342f7dd5c7a98bcab2a0a759698");
        ADD_CHECKPOINT(11000, "a5ba2214eb96934d2ea36d0000be2c4ce4bd50d3ef7c36168d4ee11452ee3b7f");
        ADD_CHECKPOINT(11500, "5e55feaeb6f1c3a269d83e5ffcc0b6639247f23936b3335a0205b173227ced4e");
        ADD_CHECKPOINT(12000, "10ebb4db4e1115f9bba949292328074bab7222b418d36925281d86bfcc095a84");
        ADD_CHECKPOINT(12500, "7f424a3998ffc40c0522383424768ba695928bee124f2f55a2620111f36c05ac");
        ADD_CHECKPOINT(13000, "0260ef7b9fc39c2e925cd5672ce8790c3f25e0df2975936605aa0b136e504887");
        ADD_CHECKPOINT(13500, "6098f65e35bff62e59a6664f72ea32eab1ceb81721f1ab9b357664ddc6f19c0e");
        ADD_CHECKPOINT(14000, "7da93fd07d0e5f0ba532127432f351f405b091ed6bce6943bb6922f22ac7d0df");
        ADD_CHECKPOINT(14500, "9eadf4b2603b53774df8e566e8cb0debe456851d67695d4b236edd084826b441");
        ADD_CHECKPOINT(15000, "7966167d73e99a81dac338f26c6892f121762c7ee079400c9edd2b73e751bee6");
        ADD_CHECKPOINT(15500, "734bbd5abe99f05e563bb51f87248cff8dc6240b00005b236e9f595203bc4705");
        ADD_CHECKPOINT(16000, "13e81ff41a1b9d0fb04823a0bc3b2d0949e55b4be8694791f12a12d8b0c567fe");
        ADD_CHECKPOINT(16500, "a71052d63ff29509847c69f4d439d9b4b97565c3727d0223d7a01301159ad66c");
        ADD_CHECKPOINT(17000, "2fe25178330111aa12998bb6ded0a4472e125d9ab06ad8a754b7dd3434d2d558");
        ADD_CHECKPOINT(17500, "747708fbeffe91d1c6261dd1139a4dc775c1f0e27f31e06226a1a9dc0392b8c3");
        ADD_CHECKPOINT(18000, "f99a5cc275f9461e61ba647198989f8e2a370f55a4c6b477f92adbc4df0425c9");
        ADD_CHECKPOINT(18500, "66fb710fbc2dbdb60a850025f14683f0ee14c4a93f5a4b02628760f8562b7194");
        ADD_CHECKPOINT(19000, "2c1d24befcc6ab4501c3875c2e6f73964f754030b3aea0947624fb9a7a2f4504");
        ADD_CHECKPOINT(19500, "a9d94305ac3da7c3c96c9367e7657f5b84cae096b6d3f3ec8169872b7c04d55d");
        ADD_CHECKPOINT(20000, "9dab170d2bf1b8832af876b610fefe0aa4e8f9e5031746c2804ecae15967e560");
        ADD_CHECKPOINT(20500, "65aa68fb56ede48b5f266a5c7ce5c1b89d6354ad40f9bd63472ee0d4e09a3766");
        ADD_CHECKPOINT(21000, "91365c3ef22412d1c958db7fe97fd9ad89e513408da6c37d15a7f09a2b2edcd8");
        ADD_CHECKPOINT(21500, "4f44ddddf5fc2a37ba60df922b19b229b34aa151f8a83acd15e2386b3c966d09");
        ADD_CHECKPOINT(22000, "90f32c37166df2ce999c311e73a260ccd8ccf69906529c6a0ac1d13e51bca420");
        ADD_CHECKPOINT(22500, "3b7e70a0ef6d8937ab1a8a068804fb3b1f46c67f2e31e6ba18499fbd173c4e9f");
        ADD_CHECKPOINT(23000, "cd8626b33d44976cb64f13351b22074ce737f0311cd20fc5d44841f534ec74e7");
        ADD_CHECKPOINT(23500, "f3e30dbd37c41b547a9cc2875e7ba1fa12e647bf49b38dc5cae55671ffcec38c");
        ADD_CHECKPOINT(24000, "2a1c25f6cf9fa4f3eab429b94d7cc7ad774472bd9efff1661c57966ad8bf2357");
        ADD_CHECKPOINT(24500, "11e01233d9cffb35679b9e32f3fddcf33da769e2861ccb0c94be7937111d9b72");
        ADD_CHECKPOINT(25000, "b0fba447e624bc46ddb558e542993f4bd72187d702b366a77633e45c83f87d55");
        ADD_CHECKPOINT(25500, "e3eb88f312d23cba6203b6a4f3aa6d283eb561adaa2d3be531ab508079312641");
        ADD_CHECKPOINT(26000, "8a407530cde5feb2475d8dfdc82481d6121eb9699c23a210e482ee6c191b85d4");
        ADD_CHECKPOINT(26500, "6d3cb1b36396149b8656d25cc92d82052db08cc9c2175d5acb73994f4f6807dd");
        ADD_CHECKPOINT(27000, "999612022d2fd65cca0c02f4b0b61493f3a53299a3abf287feb7eb4941b29295");
        ADD_CHECKPOINT(27500, "5d3c1df497912a78a621242a6baad547556eb52df41a75869d8f7d3d4c776a43");
        ADD_CHECKPOINT(28000, "2a3801312ed79b45a2a0e88ddc89068be1d9eeffb43e05e5419e3fd499c43255");
        ADD_CHECKPOINT(28500, "257d9cfd3196e7210499c2cb513b25941bf0ca34dc18be7243b600f0b5fdcced");
        ADD_CHECKPOINT(29000, "d50eba1d4688349f9b2134b1dae99edfdcf5e57f10a2e5a2739b240ac59e9143");
        ADD_CHECKPOINT(29500, "91e8b599ca9e27dea70a5208c3577cf136af264c1367c739de18d98db8a9d8c0");
        ADD_CHECKPOINT(30000, "c3debcd0f98abb24dfc8d98bc624fa7de05093bd716b54ea45599b88987909c9");
        ADD_CHECKPOINT(30500, "f1e3250662f61702380ff1fff3e32e58c5d287a6d03d425023723fea57e15639");
        ADD_CHECKPOINT(31000, "57905584f7137bf7e2125b070fb84110e96aff226f1ace424e210d2524e50d95");
        ADD_CHECKPOINT(31500, "265bf422d5a844cdeace89043575ee95b14c2728bde76b2cce79ec5a03e00118");
        ADD_CHECKPOINT(32000, "ab3ac8227473077be883a6334aa988835346bb032836ea1e229985e97c5a5db5");
        ADD_CHECKPOINT(32500, "5bd3fb43ed4637f7213a4ab91196b20947b23f7c09c4b7b350f1786e62d78bd0");
        ADD_CHECKPOINT(33000, "1b78cc843ee8e514296a11ca8c3cd3dfecf83073f9579013adbd39f12537aea3");
        ADD_CHECKPOINT(33500, "7899174ed1853aedec1f8af9671ef6ec1cafed8f7e58066f448735578c4a0e1d");
        ADD_CHECKPOINT(34000, "7aa0bb3a85fdd94546cc6c22404f19530d02a0edd6b4499db12f49b51088001d");
        ADD_CHECKPOINT(34500, "2e047329c62803e2df00b30dd5976c562e4406e17c5668842a0c19d5a035c8af");
        ADD_CHECKPOINT(35000, "f886de5cf5c58d373f7757d19b2941bc52278cfde597585ae18f5afdd2e7d85b");
        ADD_CHECKPOINT(35500, "77bc9e02e11cada1f4c626029fdad2759c477d02d148e8f9798d575b462e5a8a");
        ADD_CHECKPOINT(36000, "01c3286a5316d651606d58010db5a1ccd4f7f2693bda4c1220eff9763ad2d37f");
        ADD_CHECKPOINT(36500, "5bf4b0ec3958b6246f58a08d8c16ec9e0e2ba4366d2fd9250349951a83c9747e");
        ADD_CHECKPOINT(37000, "7fccd0d7e14da7c7eca8e500c1b232fe4a78cef8ddd1f1cf26deb44323d634fc");
        ADD_CHECKPOINT(37500, "8612666aca39ad750b9fb8cd19b9a0cf07f530e47655926eee236c0381f989a8");
        ADD_CHECKPOINT(38000, "d91f03e1d08432b624eb2715bd970b625b66f459037b1dc0607ec60d837f6e5a");
        ADD_CHECKPOINT(38500, "9de00b470227c60743ac089c8656b73bd50dea1af2528d06fad6e7e247653fbf");
        ADD_CHECKPOINT(39000, "b2edbf8b6e4b3ecb1e4157b0805d3c5826aad9ff1d441a08d78e2c699cc8a319");
        ADD_CHECKPOINT(39500, "4814beb043fe295bd724d4ed32645346d499c94fbf35ec18b04e113c7a36531f");
        ADD_CHECKPOINT(40000, "455ebe7cb764770bf5e69bf4f681ef1cefdc48d98b879bde5e24276e2265b3b2");
        ADD_CHECKPOINT(40500, "8bab0045fb7cc5b59455f416e04676e10d14565208516546884bc3f04c839ea3");
        ADD_CHECKPOINT(41000, "c3a2baad91b28b94053955ce1ba5838a0d8abaaf6fe1168c87548a59640657f1");
        ADD_CHECKPOINT(41500, "3544decd9e0c2f638a94c55e722ab501ee6e1145edf826be0fe77b85b698c987");
        ADD_CHECKPOINT(42000, "ad5c8acff425179c91fe3fa059b9260842d9b1c26419d6f006c7fe772ed9400f");
        ADD_CHECKPOINT(42500, "4757629c1d8fce0324e4b72ec6637fdb8374fc55a30d3480c308ec66fd181875");
        ADD_CHECKPOINT(43000, "044d325744b24349ca129bfad19027039901597e87a86f9c2883754cc5e75ad5");
        ADD_CHECKPOINT(43500, "0f39c878f7727f6215cb3bbf08d413d5d339f604419ddd491991f21511787641");
        ADD_CHECKPOINT(44000, "bb43238871432dfbaf52bf39da03d14ba583ce7ec88b234917366874c3742deb");
        ADD_CHECKPOINT(44500, "5aca8ff16d45023a1d44d725f4236fbc94cafe8e271eff92b9f6ecafe0c43d26");
        ADD_CHECKPOINT(45000, "d260e92c5f0375698fd08cee47146773addabf05581a440be0716cea70c10987");
        ADD_CHECKPOINT(45500, "9124832f5d8a16333e8913191fc7f930a759319de016a9cd9b92d2a1c72bb957");
        ADD_CHECKPOINT(46000, "45bb865634b50a7caff109a45fd132822f78b8acdf40c8d1e4334a1f789d0319");
        ADD_CHECKPOINT(46500, "fed6c9ffc86ab141d3005bd615d28764d225935cdb9919cc098ff050a2587dc3");
        ADD_CHECKPOINT(47000, "6cf27e39ab5b28c790a79267ce3e422d32589f9b75c7a2328ff534e218c9bb6e");
        ADD_CHECKPOINT(47500, "a9b308f6f6c48199b41102b31e5d8ea41ac4a6f11c03c15d3edc83af39fd5033");
        ADD_CHECKPOINT(48000, "5f351d47eda7fa2a269187c077be9eb46ffe293fccd825b8cf2b728cf15b27eb");
        ADD_CHECKPOINT(48500, "c7ef17d7eb11f80cfc11ec92533cbae4a2c6cbb20933c1a89f580c02f8a8fe52");
        ADD_CHECKPOINT(49000, "0aa5d2e2ce88c649e5047a0505b401d4098f3975023b6ea7f5902366697fed37");
        ADD_CHECKPOINT(49500, "b6365dd4c5d94adfc7c2df6f58a694628750002daf5c74e53795bedba3695820");
        ADD_CHECKPOINT(50000, "645a3f1ab164395437d0a9c61feb2815623dac06f632871e716472efae886c4a");
        ADD_CHECKPOINT(50500, "ad54e31cc9f28a4e2bbbc9da286c259c0901f65e2ed76f1658d12c56a65c3407");
        ADD_CHECKPOINT(51000, "ae5d0d430e79ccdd81abd3f3ec30fbf9ebf095537f5071acb6781e5907c93cea");
        ADD_CHECKPOINT(51500, "7769065aa1c1dae1702ee58f757db18e53bfb7ab85382a47d33ee796ccc33761");
        ADD_CHECKPOINT(52000, "69a9366a4686736d4d370dfc01ff5c5e7a0f7645df2a6072d21b27d3e2600659");
        ADD_CHECKPOINT(52500, "bbf08012d5b492072206daafe52c05f981de25c6b61e09899b91d296ab3a2692");
        ADD_CHECKPOINT(53000, "ee7792679b86139b071376ea50e557bcf7290fbb2c11c2e99ba0b8e454098037");
        ADD_CHECKPOINT(53500, "f437aebc2b8e452d0b7a4f025db16e8d34027b3f9369805a2e4624db467577c0");
        ADD_CHECKPOINT(54000, "63ca5c2aeec850f8e695499a81e0d478a122300a5072280a46d4f70425a24285");
        ADD_CHECKPOINT(54500, "644a64579952bd6166c823878fab0cd52d24daf2e607f323c3b58e8f3641055d");
        ADD_CHECKPOINT(55000, "18ff5a02e2e2f2055c71fb96ae3e2e75b531559f2b923138147a9f29f5c866f9");
        ADD_CHECKPOINT(55500, "50f5e880e117fb00346beb92bdacbf5e8cd19a959cc59dc718346e70759e2064");
        ADD_CHECKPOINT(56000, "9e35854403cddf12f7eeebc786e1d8b9fb4c9ca2ea06767260ebbbc787bb7d9f");
        ADD_CHECKPOINT(56500, "ce65f5020a873ba1a41d5bcfb8850befd7e6e76901bdf44f4d798e6488490071");
        ADD_CHECKPOINT(57000, "d8c725303fb3845921dc7e9d5a96630dc6a249e8d1ad628d814ddb6df191791a");
        ADD_CHECKPOINT(57500, "74090f7cc94e40c75ac8fe1aa2fc9c2f19ca2884fe6f7c5cddc30c92fc62787a");
        ADD_CHECKPOINT(58000, "0b366c9e4ceb34391717ff14e31f9ba7d349c1b8177f8ff4e514d8f3fc61dcd6");
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

    // All four GNTL Network domains have DNSSEC on and valid
    static const std::vector<std::string> dns_urls = {

	};

    static const std::vector<std::string> testnet_dns_urls = {
    };

    static const std::vector<std::string> stagenet_dns_urls = {
    };

    if (!tools::dns_utils::load_txt_records_from_dns(records, nettype == TESTNET ? testnet_dns_urls : nettype == STAGENET ? stagenet_dns_urls : dns_urls))
      return false; // why true ?

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
        if (!epee::string_tools::hex_to_pod(hashStr, hash))
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
