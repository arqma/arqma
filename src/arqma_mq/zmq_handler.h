// Copyright (c) 2018-2020, The Arqma Network
// Copyright (c)2020, Gary Rusher
// Copyright (c) 2017-2018, The Monero Project
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

#pragma once

#include "rpc/daemon_messages.h"
#include "rpc/daemon_rpc_version.h"
#include "rpc/rpc_handler.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_protocol/cryptonote_protocol_handler.h"
#include "p2p/net_node.h"

#include "string_tools.h"

using namespace epee;


namespace
{
  typedef nodetool::node_server<cryptonote::t_cryptonote_protocol_handler<cryptonote::core> > t_p2p;
}  // anonymous namespace


namespace arqmaMQ
{

class ZmqHandler : public cryptonote::rpc::RpcHandler
{
  public:

    ZmqHandler(cryptonote::core& c, t_p2p& p2p) : m_core(c), m_p2p(p2p) { }

    ~ZmqHandler() { }

    void handle(const cryptonote::rpc::GetHeight::Request& req, cryptonote::rpc::GetHeight::Response& res);

    void handle(const cryptonote::rpc::GetBlocksFast::Request& req, cryptonote::rpc::GetBlocksFast::Response& res);

    void handle(const cryptonote::rpc::GetHashesFast::Request& req, cryptonote::rpc::GetHashesFast::Response& res);

    void handle(const cryptonote::rpc::GetTransactions::Request& req, cryptonote::rpc::GetTransactions::Response& res);

    void handle(const cryptonote::rpc::KeyImagesSpent::Request& req, cryptonote::rpc::KeyImagesSpent::Response& res);

    void handle(const cryptonote::rpc::GetTxGlobalOutputIndices::Request& req, cryptonote::rpc::GetTxGlobalOutputIndices::Response& res);

    void handle(const cryptonote::rpc::SendRawTx::Request& req, cryptonote::rpc::SendRawTx::Response& res);

    void handle(const cryptonote::rpc::SendRawTxHex::Request& req, cryptonote::rpc::SendRawTxHex::Response& res);

    void handle(const cryptonote::rpc::StartMining::Request& req, cryptonote::rpc::StartMining::Response& res);

    void handle(const cryptonote::rpc::GetInfo::Request& req, cryptonote::rpc::GetInfo::Response& res);

    void handle(const cryptonote::rpc::StopMining::Request& req, cryptonote::rpc::StopMining::Response& res);

    void handle(const cryptonote::rpc::MiningStatus::Request& req, cryptonote::rpc::MiningStatus::Response& res);

    void handle(const cryptonote::rpc::SaveBC::Request& req, cryptonote::rpc::SaveBC::Response& res);

    void handle(const cryptonote::rpc::GetBlockHash::Request& req, cryptonote::rpc::GetBlockHash::Response& res);

    void handle(const cryptonote::rpc::GetBlockTemplate::Request& req, cryptonote::rpc::GetBlockTemplate::Response& res);

    void handle(const cryptonote::rpc::SubmitBlock::Request& req, cryptonote::rpc::SubmitBlock::Response& res);

    void handle(const cryptonote::rpc::GetLastBlockHeader::Request& req, cryptonote::rpc::GetLastBlockHeader::Response& res);

    void handle(const cryptonote::rpc::GetBlockHeaderByHash::Request& req, cryptonote::rpc::GetBlockHeaderByHash::Response& res);

    void handle(const cryptonote::rpc::GetBlockHeaderByHeight::Request& req, cryptonote::rpc::GetBlockHeaderByHeight::Response& res);

    void handle(const cryptonote::rpc::GetBlockHeadersByHeight::Request& req, cryptonote::rpc::GetBlockHeadersByHeight::Response& res);

    void handle(const cryptonote::rpc::GetBlock::Request& req, cryptonote::rpc::GetBlock::Response& res);

    void handle(const cryptonote::rpc::GetPeerList::Request& req, cryptonote::rpc::GetPeerList::Response& res);

    void handle(const cryptonote::rpc::SetLogHashRate::Request& req, cryptonote::rpc::SetLogHashRate::Response& res);

    void handle(const cryptonote::rpc::SetLogLevel::Request& req, cryptonote::rpc::SetLogLevel::Response& res);

    void handle(const cryptonote::rpc::GetTransactionPool::Request& req, cryptonote::rpc::GetTransactionPool::Response& res);

    void handle(const cryptonote::rpc::GetConnections::Request& req, cryptonote::rpc::GetConnections::Response& res);

    void handle(const cryptonote::rpc::GetBlockHeadersRange::Request& req, cryptonote::rpc::GetBlockHeadersRange::Response& res);

    void handle(const cryptonote::rpc::StopDaemon::Request& req, cryptonote::rpc::StopDaemon::Response& res);

    void handle(const cryptonote::rpc::StartSaveGraph::Request& req, cryptonote::rpc::StartSaveGraph::Response& res);

    void handle(const cryptonote::rpc::StopSaveGraph::Request& req, cryptonote::rpc::StopSaveGraph::Response& res);

    void handle(const cryptonote::rpc::HardForkInfo::Request& req, cryptonote::rpc::HardForkInfo::Response& res);

    void handle(const cryptonote::rpc::GetBans::Request& req, cryptonote::rpc::GetBans::Response& res);

    void handle(const cryptonote::rpc::SetBans::Request& req, cryptonote::rpc::SetBans::Response& res);

    void handle(const cryptonote::rpc::FlushTransactionPool::Request& req, cryptonote::rpc::FlushTransactionPool::Response& res);

    void handle(const cryptonote::rpc::GetOutputHistogram::Request& req, cryptonote::rpc::GetOutputHistogram::Response& res);

    void handle(const cryptonote::rpc::GetOutputKeys::Request& req, cryptonote::rpc::GetOutputKeys::Response& res);

    void handle(const cryptonote::rpc::GetRPCVersion::Request& req, cryptonote::rpc::GetRPCVersion::Response& res);

    void handle(const cryptonote::rpc::GetFeeEstimate::Request& req, cryptonote::rpc::GetFeeEstimate::Response& res);

    void handle(const cryptonote::rpc::GetOutputDistribution::Request& req, cryptonote::rpc::GetOutputDistribution::Response& res);

    std::string handle(const std::string& request);

  private:

    bool getBlockHeaderByHash(const crypto::hash& hash_in, cryptonote::rpc::BlockHeaderResponse& response);

    void handleTxBlob(const std::string& tx_blob, bool relay, cryptonote::rpc::SendRawTx::Response& res);

    cryptonote::network_type nettype() const { return m_core.get_nettype(); }

    bool get_block_template(const cryptonote::account_public_address &address, const crypto::hash *prev_block, const cryptonote::blobdata &extra_nonce, size_t &reserved_offset, cryptonote::difficulty_type &difficulty, uint64_t &height, uint64_t &expected_reward, cryptonote::block &b, crypto::hash &seed_hash, crypto::hash &next_seed_hash, cryptonote::rpc::GetBlockTemplate::Response& res);

	bool check_core_ready();

    cryptonote::core& m_core;
    t_p2p& m_p2p;

};

}  // namespace arqmaMQ
