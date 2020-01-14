// Copyright (c) 2018-2020, The Arqma Network
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
    ADD_CHECKPOINT(2500, "a44c8ba7f0f7d8b6c389a441854b35f0181b7a003ff863c7443a77def997e668");
    ADD_CHECKPOINT(5000, "485bc7372939f6edcac98eb0d175a446cb38d652614cb3528f2f3361e28455a2");
    ADD_CHECKPOINT(7500, "63a07de1d04052dde7c4beab312d71f66f883c23d49dcead15a5db4de618a53a");
    ADD_CHECKPOINT(10000, "1a35ebbe820d2cad63112750d602817c00ce1e11e48fce302a9edb697f635533");
    ADD_CHECKPOINT(12500, "e8055084f8f7e1da906d1c736512529a9b492a4c9ba939411afa7d856ca7b308");
    ADD_CHECKPOINT(15000, "395ab30253be389607079d9b7d7f4012b1747d7315ea7e6502cb5488e0b5c7ce");
    ADD_CHECKPOINT(17500, "a1a31475afa57532eb85f56c8e60bf4bbe32cbeb5e63327aa6eaafb4b2b7233e");
    ADD_CHECKPOINT(20000, "0feb87f0cae6bce22b652cfba2d9a462637c16da3437afc8c58bd24fcbe1854b");
    ADD_CHECKPOINT(22500, "e7f2e06be3a365191a470a0411f146f03cdcdad52b2cfeefaea28fafbcd4ec0e");
    ADD_CHECKPOINT(25000, "0c8486777e821731f323cc053bd61f3586e5003279d4f19d9d71760b64afc174");
    ADD_CHECKPOINT(27500, "75c2d5dbad7edc11ab8cb022b1a0b0cc49e512ad22dc5ea9e66d455a1180d836");
    ADD_CHECKPOINT(30000, "15e2edaa2162b3dd3e455a36ea500cdad6cbbd0ab3fcd895ad34d3914d5d986d");
    ADD_CHECKPOINT(32500, "df12693f88169751da5c8768d704bc4f8eb5599d7f50b841e5275dc7a7c13932");
    ADD_CHECKPOINT(35000, "4ee330698b72e89cf48698320b7de6be8a840a5a87a865d77f1d6ff2ec0cc484");
    ADD_CHECKPOINT(37500, "9a7202a270390679df5a89069c397d561e8716e8781835033a5fd59d41625994");
    ADD_CHECKPOINT(40000, "3b885d11726b935190b712f6f39ab28f57663555a74c1a6e91904f271b8e45cd");
    ADD_CHECKPOINT(42500, "9c65b007675902b1ca7985b829940b63bcf84dab3a8e5b25a43415aba7884640");
    ADD_CHECKPOINT(45000, "9fed3ec147d68e7e59465a37fb5dfd719a9e1ff1d77cdbc4b2bbe64a7f2d2203");
    ADD_CHECKPOINT(47500, "1756cb4ceedb1a8369ca6fd45219e81d2c74ba491c1f7ff7cf08f24e91b5c685");
    ADD_CHECKPOINT(50000, "c356f107ab8a85a9fb0a8b99f6ead7493d650acb1ec30a3c4b6d82171ce50cc5");
    ADD_CHECKPOINT(52500, "cbb6dc2ab0a9edfc43feb9563444d84b62203153b7d27110aec103600233536e");
    ADD_CHECKPOINT(55000, "b0cebe652532200f84c9ba472fa340b46572f87f1770d8399d8ddc06d14b768a");
    ADD_CHECKPOINT(57500, "140de065ddf799337595da9ce70d9df65c06e400cb84573448241c179fd3fe30");
    ADD_CHECKPOINT(60000, "2264cc2eb760b6fbd8f4afe3acbe9b5e0b73e946e468a5488ea09aae84c27b39");
    ADD_CHECKPOINT(62500, "73de31e6f047c7d64506d7c07f73a3ed2dc324225f98bf5ffa69f0c14c2954b6");
    ADD_CHECKPOINT(65000, "7bdf2245564aa3e1d5a1d61ecc14af8a153b69eccfeac4513da54e8f69841f45");
    ADD_CHECKPOINT(67500, "638c1cabb1909861cd8473f07c816966799957575a69f1d8bae51c6d2b89e8c8");
    ADD_CHECKPOINT(70000, "7929157aa7738dbcb623e49cefcd9b37e078d8026bfe29788a94cdf5b4de4b51");
    ADD_CHECKPOINT(72500, "f93510962f5e93bef1a89c196a6f8503d5ce231d066585370c2d6e71582209a8");
    ADD_CHECKPOINT(75000, "e3e68b83405dac5b3f563acb3ea541891d38b96f7d01255a87066077ec331475");
    ADD_CHECKPOINT(77500, "954ca1d7fa86dff6af7e2641c01b511f9f0882b9c7e8c5d88377ba221cdb2ce1");
    ADD_CHECKPOINT(80000, "0d45cca5d04410f7015bf36ca94d86e0eb29aa6371996704301c456061e13f75");
    ADD_CHECKPOINT(82500, "2c022ab39a0a811194d2d24b5fea4b999b064c58fb2ec3d8913bce2f2307366b");
    ADD_CHECKPOINT(85000, "232a54e262a3edd3f09f45ef285a093f6deddca5139ce4afa09aef04a4bfb7d7");
    ADD_CHECKPOINT(87500, "0af7832b7e1e3a0ce8b2335a7fd7e95a1c719bf3f7b7bbdabd914ecc7e7e68dd");
    ADD_CHECKPOINT(90000, "bbc0efdea50a48bca4cdadf23aac149fd69855028037777e9da040c283a57511");
    ADD_CHECKPOINT(92500, "dcbde95d84d6b1afa953e795dd2d76696d5b1ed8ae48148a983aa0b3461d31a3");
    ADD_CHECKPOINT(95000, "06b38872d46b0d95e746d63d0b69d0a0cf5f6b9b502c809fbf04c954c35074ae");
    ADD_CHECKPOINT(97500, "7216aa5248f925f0402924fd32e3b789135265ba5037fa1d503da4685248773e");
    ADD_CHECKPOINT(100000, "8d7251c892a048740b0dbb4da24f44a9e5433b04e61426eb1a9671ea7ad69639");
    ADD_CHECKPOINT(102500, "d449e9255254f8cb2e7b0b6c9b20289a947f6fa052785dd0b77463d40dd419a6");
    ADD_CHECKPOINT(105000, "e13384db4be6ea4bf475690acac412a47c0938d33d15f59e912eaf9f3e9c9871");
    ADD_CHECKPOINT(107500, "23c38ea617fe44be6ea2ef792072e7144593ffac96dbecc4b45b89ba32a2f285");
    ADD_CHECKPOINT(110000, "0c44032ccf3ab097cdad5f97be4ce7675273152386b1eef02fac6519c90621f6");
    ADD_CHECKPOINT(112500, "684011c8eb5d6a7db6f6d6278fb651cdd1806cac297e19851134ba362b54d28f");
    ADD_CHECKPOINT(115000, "58fc15353ebb0eed12e60d126ea8c0a31ea50511ff9b181de8c2136538041f17");
    ADD_CHECKPOINT(117500, "9b7f5dc00a65567a0ef6ad652647fe3ef37087b41b4bfdadae9491870cb301aa");
    ADD_CHECKPOINT(120000, "cf504edaea8db888c8a351bceea3690a2b24e37160e915c2c0c5faddc8df337d");
    ADD_CHECKPOINT(122500, "f0649162498b7a75dfed31dd2a282da039acc45ded97a1223916c1ae54b00644");
    ADD_CHECKPOINT(125000, "77e0f2c0d8e2033c77b1eca65027554f4849634756731d14d4b98f28de678ae6");
    ADD_CHECKPOINT(127500, "79d896b71df709d61ba2fb9f0411d4a5b478a49b568b39394dbb68df6666b1c9");
    ADD_CHECKPOINT(130000, "a3600a19054000559ba50ca2c8cf222a8408756a77394c2b8c8012a0950cf7e6");
    ADD_CHECKPOINT(132500, "0aa025192f0240799c7a8d339295efc476cf58b38b9a3afd90c39a131a679993");
    ADD_CHECKPOINT(135000, "dfba5847657e1c51bbdc099b058f590a10c7de329fe4bfcf70ce24258b11452b");
    ADD_CHECKPOINT(137500, "6a289f01bb74aeafeecf8c89923d7fec17911b7302e4f7d9e2f1bc679d9c9ed4");
    ADD_CHECKPOINT(140000, "5161d0ff36d90a706d884fa7d3570cdd9479be4e2a545059e92cafc7d592a652");
    ADD_CHECKPOINT(142500, "eae6a33489cf575986add4b55aeaebaba030f6a1d6953df3ed983094f7a2b3b3");
    ADD_CHECKPOINT(145000, "80acf103a5e39e1dc2c1f4287238b21e87097ca64a49ffac23818a8f871b8db4");
    ADD_CHECKPOINT(147500, "6ff0613230b63bee451c67b4a774bfaa1620a915770069da3445907f4358c60c");
    ADD_CHECKPOINT(150000, "fdb9d4132ccc0904dc9dc8f4746b6efe3e33d9c6051b5907d21b2005fc2dce29");
    ADD_CHECKPOINT(152500, "afc7118a216b7fe0e57a6fa157cfb99df819e58782b65f784dd12cb2f5e876dc");
    ADD_CHECKPOINT(155000, "b6af255a444e731a673846d5e261bb93c161ba7c0be7f9424dbf98532e44b647");
    ADD_CHECKPOINT(157500, "54986cf609ad5e1005fecd378cf1ef7eee24ecc7c015ec9254e96c2475573fe1");
    ADD_CHECKPOINT(160000, "9a119d7ce595f2231821f11148911d30134b8443ec9729ae74cb353fe0b16ebd");
    ADD_CHECKPOINT(162500, "f64d4c45a46fec281b7bf35afe950ffba97e981984e0f905bf06a94752c67a0d");
    ADD_CHECKPOINT(165000, "f1ab41526c958b3c344a1e9c04ece8c8c2f70b3eeb3feafdf8632868d7dffb2b");
    ADD_CHECKPOINT(167500, "e7bfc661eb290db4ba0c0253be57aff49f028b93e0eaafa6a1ce276b52169578");
    ADD_CHECKPOINT(170000, "47fb610c2e5747619ef071091665b914352b4a71be041891dba2528b046246b3");
    ADD_CHECKPOINT(172500, "ea1e3ed8af6ce148be8bbaaaee7ee0bdd3afca2cdb00063f9fabb39a9511265e");
    ADD_CHECKPOINT(175000, "d26a135e373447fd1603ec98022db674c1d4d320b4ff9cae9f9c322c81afd1a5");
    ADD_CHECKPOINT(177500, "815635e6651fa8fbbc659956a2bd6ea187d6bad875f949717ac7500270e3e020");
    ADD_CHECKPOINT(180000, "5dd2910559fda6bfd208e512e7be93a81588d3ecd2654fd044ff581325196408");
    ADD_CHECKPOINT(182500, "87206b920b72746c3af0bb56aedb7caa307c6d86fc745428b39f4cea6f356fa7");
    ADD_CHECKPOINT(185000, "11a1fcd2adb7a3271ab82bed2e46e330eb8f205b2fd09a5a7e724ad47f5a77b4");
    ADD_CHECKPOINT(187500, "bc9c1f0367bd296647af67354e0c5a0d06900cceeb50b36fb221a21b17081c9f");
    ADD_CHECKPOINT(190000, "8206b1f031b2c8e05bdb546f0c62663c0c0ea50169c1e8e20019b47c86849ffd");
    ADD_CHECKPOINT(192500, "a19ae444f5e218bcd7b323a5620863ba9c3a71e721fe4f9aa1ee80af4d911655");
    ADD_CHECKPOINT(195000, "ca51a6d9961c551bb91008c5202125254d167432ecdcd0e439c9cf35a50cbde8");
    ADD_CHECKPOINT(197500, "d2825c69c08c5499e94fd3d5e9ea0b6bdd4b0d9b56f1463cc7a40e9ad4a9b570");
    ADD_CHECKPOINT(200000, "c6ef0e39741b42c2cb4644988d31c2743a6ddefc5495a773cc8c807513d818c3");
    ADD_CHECKPOINT(202500, "3928b8135df0a416aea8ce192d72de5516a86178d5d10d8cf5e486bc9f770022");
    ADD_CHECKPOINT(205000, "b2b6cea6056305536b23042e32a7f39fa3643256945df8734a9a4f2bdc70e743");
    ADD_CHECKPOINT(207500, "d9e9a88ddfbec218df8850ec7c398830e27b241d0c4a797d832201d9a061c163");
    ADD_CHECKPOINT(210000, "2df3dbe2b5d88beab7102bb3139777332a6124b96634123a83acd90f8e2e580f");
    ADD_CHECKPOINT(212500, "31befd8f6dcb46b41227328e674857d417173d268b998900c075ba450fe7a5a2");
    ADD_CHECKPOINT(215000, "ebdc02e6997f85f799b60b8071886e1eec9f6ced5464de9d515ce24869675b52");
    ADD_CHECKPOINT(217500, "23bbf961e2a1b526cfd660540a2cbf5caceb64d092e4557b7d82649a8b65d698");
    ADD_CHECKPOINT(220000, "83d38d021554c9c43832596c48e4af30d80d1feae0a798cf783be8054313678a");
    ADD_CHECKPOINT(222500, "585e0f99f731cfa847eaf5ceef5bead470358db2516142429dec3ee5395f7dca");
    ADD_CHECKPOINT(225000, "13740a2cd1acf81b5fba31c772cb0174ac2f63ede287677090b1ad6762b295e9");
    ADD_CHECKPOINT(227500, "47433ec00f2ca93f3eb7b20191671e85d29cf69a5565cea779ce7ecd958de1cb");
    ADD_CHECKPOINT(230000, "296bc4ef307098881a0f8188533433c2e708f0aa07036968e2d05d2a1ed9c7ea");
    ADD_CHECKPOINT(232500, "940320d301a2a7ade923d92ee30c13a00ad5bf4c45adbbb2644419c1d36932c1");
    ADD_CHECKPOINT(235000, "45c7b139f83fb7bad130c49ecf3d11005726e7b381210b5aee54631cd32b23b6");
    ADD_CHECKPOINT(237500, "8d61a50652e10dbbfca6941d7e3c1d348676f75c586c640aaeefb0de1b824a37");
    ADD_CHECKPOINT(240000, "c37be4742abd62a0235decf238dc4b2e3db0b3734f20d5d533973388d47da3f8");
    ADD_CHECKPOINT(242500, "e91868e62c7302db85ebd06ad3c6658b3882f1de038ba463446f3c9415dc3968");
    ADD_CHECKPOINT(245000, "b5d6369450e0e65084bb302b72ca7311b27e34120d09662d6d7b8f19c169b338");
    ADD_CHECKPOINT(247500, "82a66b9c6552996c8df27a86752a03b57eda3f522a79627f7ed001201643c83d");
    ADD_CHECKPOINT(250000, "1b25cc5c39a3d7b4009df07c0f95901bde0785783eea0449d3aa6bbb3c74aff0");
    ADD_CHECKPOINT(252500, "917dc5259431873fe87c48758b065af8df8fcfccf11013fdff6bc4b7a729316b");
    ADD_CHECKPOINT(255000, "5feacd082efbcd5c22c1080f5b8a1796e7e529d328fa8f1d5af1276889532222");
    ADD_CHECKPOINT(257500, "2bbd89a2a10eec2337cdaec075e4673e1de82cd7b2381d26035ec479252758c6");
    ADD_CHECKPOINT(260000, "2ef27f9ef7ceb9bcbba184907ec5308432488740e4e99770c7cde92d292c0b0e");
    ADD_CHECKPOINT(262500, "321567c0a78922d0be5dc9d17de5ecb7ce5768aa31c2605a99fbbda32e699f74");
    ADD_CHECKPOINT(265000, "53c82b92657ab6d958a7d465e1a96a85c5664b3d415d79b53dc61058f741d5c0");
    ADD_CHECKPOINT(267500, "d0a3313845eac5aceeed77d66bfa66f1a51e239d0b2c83eed48775d6fb952187");
    ADD_CHECKPOINT(270000, "62c99aff5e63741f0f7eb812305285ede91e8126dbea5437dc0b4d13d8f34309");
    ADD_CHECKPOINT(272500, "837614d9927b035e8ec92f3bb46f54b09d274b399c65faab96f7c8ee6a9fa49e");
    ADD_CHECKPOINT(275000, "551df7a83ee6c417acc629fecaba5c0ba3e129385288b9e89cac475afb1b1f8a");
    ADD_CHECKPOINT(277500, "a07252b67c5f5132c5c387e811d677825b6f3870001ce5898a3d148a547a6171");
    ADD_CHECKPOINT(280000, "f124b3d14c7a8b656d3de6e9ca10daab4d481202d34f0861502a6f00bbf2c028");
    ADD_CHECKPOINT(282500, "a6263f7314a4ccd9c1af429853647a288ba02d4e68969a4ad6ec2dc8a7d94545");
    ADD_CHECKPOINT(285000, "f2437b557f45043724be73cb6166045cd755a6a0e125ed2be18e2a776226d311");
    ADD_CHECKPOINT(287500, "83d2de63c3fb475059feaed0247fb503beccbf679fb4cd369ae295c9b8adb6e5");
    ADD_CHECKPOINT(290000, "98359c237a1fe37f27f305cb0d1a0a00780ec4f8e3f98c84440826fee968912a");
    ADD_CHECKPOINT(292500, "293588a733fbdd30f80c74470166d04f23b5fd2295264e505a4354e8245b6a19");
    ADD_CHECKPOINT(295000, "b4077159872672a87003b8d8c26b32addfca0dd659c41d31c75d1f0f21ae1621");
    ADD_CHECKPOINT(297500, "d864d046df9500fd56168084868befe56c788d1c4ecc85ba5c46000f31a4b427");
    ADD_CHECKPOINT(300000, "1cd8edefb47332b6d5afc1b161a8f1845aff817988763b5dc2094762b5bc5551");
    ADD_CHECKPOINT(302500, "a3f17acd71cdcf240db8ddbf5a1e318ae93b6b39415214273cd895d5bf04dfa1");
    ADD_CHECKPOINT(305000, "908dfb0f814263346dbf6eba64b72aa3a3071e0a25dc31c209ce5c4e960a067f");
    ADD_CHECKPOINT(307500, "3d560457b7964eb40e7d4d5507e3a3ad8ffb258d24fbb8d898c29763f1840991");
    ADD_CHECKPOINT(310000, "d0d8b4d20e00a2c4eac87efc5f3e66e27c41764057133f6d8925883199dca5a9");
    ADD_CHECKPOINT(312500, "910334ed3a02e6367b283771f0d938834e0102a9ec9b15b0da01b39bec9cce1c");
    ADD_CHECKPOINT(315000, "cafe355b0628d855a1b883c3708c710dcf7e32b1d89974a4e721212c7077e657");
    ADD_CHECKPOINT(317500, "2b59a3eff8a0c80b050200327965b874d41d77dc374840dcfda83b66e5f185ce");
    ADD_CHECKPOINT(320000, "067fd73842ce4450e69fecc0360d5ffc019352de549dce296fbc9829b25726da");
    ADD_CHECKPOINT(322500, "743292c61151e575620a05a801a6a8ab3945a788ea49bb94cd7be8dd9969bc0b");
    ADD_CHECKPOINT(325000, "a9943db3be2a00a1c781eb2749794c29926cdab477da82efe50e05f4350ee1bd");
    ADD_CHECKPOINT(327500, "ec874e4ea0cd72624d53d856770605866e6540fe9ef6901565dafce23a46d859");
    ADD_CHECKPOINT(330000, "69960d3c8b8610cf7d828d56372dce220040a36a1681527bb75bb304cb122275");
    ADD_CHECKPOINT(332500, "2d432ee4e7c413453f76c02e6b5410a0c6bf861bf124b1b72dab4ed64b9047b7");
    ADD_CHECKPOINT(335000, "1090a788a5c49ddd1a531d6c0fad335f1c8b308efe40d539428b38d77cc0e9a8");
    ADD_CHECKPOINT(337500, "dcc926899ff13948043e979f32b8b5ec72c90ea6d625970f2981be0b9a4a58f5");
    ADD_CHECKPOINT(340000, "c34259499ee359f9a3c1675c6e32e0962c0133dac86dd960729e00dfbe537508");
    ADD_CHECKPOINT(342500, "0e3f9e96bd52bf6162761699c07e6e67d5e4a1e0a9ab79ff552b392ba3708c77");
    ADD_CHECKPOINT(345000, "53954b67841567245244c60cf47934d424f1753973cb4b0ad14d29ef5b42e2a0");
    ADD_CHECKPOINT(347500, "6a7a71a171ec79f810c724c898f1ddc4c082b3f6ac2270354e3fe201236f641d");
    ADD_CHECKPOINT(350000, "b0b6ddc595b4d72dea31aa004fc85db908057b8ea0cb9067d04a29f696ed7f6a");
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
