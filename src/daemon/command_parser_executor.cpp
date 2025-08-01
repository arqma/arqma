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

#include <forward_list>

#include "common/dns_utils.h"
#include "common/command_line.h"
#include "version.h"
#include "daemon/command_parser_executor.h"
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "daemon"

namespace daemonize {

t_command_parser_executor::t_command_parser_executor(
    uint32_t ip
  , uint16_t port
  , const boost::optional<tools::login>& login
  , const epee::net_utils::ssl_options_t& ssl_options
  , bool is_rpc
  , cryptonote::core_rpc_server* rpc_server
  )
  : m_executor(ip, port, login, ssl_options, is_rpc, rpc_server)
{}

template <typename T>
static bool parse_if_present(std::forward_list<std::string> &list, T &var, const char *name)
{
  if (list.empty()) return true;
  if (epee::string_tools::get_xtype_from_string(var, list.front()))
  {
    list.pop_front();
    return true;
  }

  std::cout << "unexpected " << name << " argument: " << list.front() << std::endl;
  return false;
}

bool t_command_parser_executor::print_checkpoints(const std::vector<std::string> &args)
{
  uint64_t start_height = cryptonote::COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE;
  uint64_t end_height = cryptonote::COMMAND_RPC_GET_CHECKPOINTS::HEIGHT_SENTINEL_VALUE;

  std::forward_list<std::string> args_list(args.begin(), args.end());
  bool print_json = !args_list.empty() && args_list.front() == "+json";
  if (print_json)
    args_list.pop_front();

  if (!parse_if_present(args_list, start_height, "start height"))
    return false;

  if (!parse_if_present(args_list, end_height, "end height"))
    return false;

  if (!args_list.empty())
  {
    std::cout << "use: print_checkpoints [+json] [start_height] [end_height]\n"
              << "(omit arguments to print the last "
              << cryptonote::COMMAND_RPC_GET_CHECKPOINTS::NUM_CHECKPOINTS_TO_QUERY_BY_DEFAULT << " checkpoints) "
              << std::endl;
    return false;
  }

  return m_executor.print_checkpoints(start_height, end_height, print_json);
}

bool t_command_parser_executor::print_sn_state_changes(const std::vector<std::string>& args)
{
  uint64_t start_height;
  uint64_t end_height = cryptonote::COMMAND_RPC_GET_SN_STATE_CHANGES::HEIGHT_SENTINEL_VALUE;

  if (args.empty())
  {
    std::cout << "Missing first argument start_height" << std::endl;
    return false;
  }

  std::forward_list<std::string> args_list(args.begin(), args.end());
  if (!epee::string_tools::get_xtype_from_string(start_height, args_list.front()))
  {
    std::cout << "start_height should be a number" << std::endl;
    return false;
  }

  args_list.pop_front();

  if (!parse_if_present(args_list, end_height, "end_height"))
    return false;

  if (!args_list.empty())
  {
    std::cout << "use: print_sn_state_changes <start_height> [end_height]"
              << "(omit arguments to scan until the current block)"
              << std::endl;
    return false;
  }

  return m_executor.print_sn_state_changes(start_height, end_height);
}

bool t_command_parser_executor::print_peer_list(const std::vector<std::string>& args)
{
  if (args.size() > 3)
  {
    std::cout << "use: print_pl [white] [gray] [<limit>]" << std::endl;
    return true;
  }

  bool white = false;
  bool gray = false;
  size_t limit = 0;
  for (size_t i = 0; i < args.size(); ++i)
  {
    if (args[i] == "white")
    {
      white = true;
    }
    else if (args[i] == "gray")
    {
      gray = true;
    }
    else if (!epee::string_tools::get_xtype_from_string(limit, args[i]))
    {
      std::cout << "unexpected argument: " << args[i] << std::endl;
      return true;
    }
  }

  const bool print_both = !white && !gray;
  return m_executor.print_peer_list(white | print_both, gray | print_both, limit);
}

bool t_command_parser_executor::print_peer_list_stats(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_peer_list_stats();
}

bool t_command_parser_executor::save_blockchain(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.save_blockchain();
}

bool t_command_parser_executor::show_hash_rate(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.show_hash_rate();
}

bool t_command_parser_executor::hide_hash_rate(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.hide_hash_rate();
}

bool t_command_parser_executor::show_difficulty(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.show_difficulty();
}

bool t_command_parser_executor::show_status(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.show_status();
}

bool t_command_parser_executor::print_connections(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_connections();
}

bool t_command_parser_executor::print_net_stats(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_net_stats();
}

bool t_command_parser_executor::print_blockchain_info(const std::vector<std::string>& args)
{
  if(!args.size())
  {
    std::cout << "need block index parameter" << std::endl;
    return false;
  }
  uint64_t start_index = 0;
  uint64_t end_index = 0;
  if(!epee::string_tools::get_xtype_from_string(start_index, args[0]))
  {
    std::cout << "wrong starter block index parameter" << std::endl;
    return false;
  }
  if(args.size() >1 && !epee::string_tools::get_xtype_from_string(end_index, args[1]))
  {
    std::cout << "wrong end block index parameter" << std::endl;
    return false;
  }

  return m_executor.print_blockchain_info(start_index, end_index);
}

bool t_command_parser_executor::print_quorum_state(const std::vector<std::string>& args)
{
  uint64_t start_height = cryptonote::COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE;
  uint64_t end_height = cryptonote::COMMAND_RPC_GET_QUORUM_STATE::HEIGHT_SENTINEL_VALUE;

  std::forward_list<std::string> args_list(args.begin(), args.end());
  if (!parse_if_present(args_list, start_height, "start_height"))
    return false;

  if (!parse_if_present(args_list, end_height, "end_height"))
    return false;

  if (!args_list.empty())
  {
    std::cout << "use: print_quorum_state [start_height] [end_height]\n"
              << "(omit arguments to print the latest quorums" << std::endl;
    return false;
  }

  return m_executor.print_quorum_state(start_height, end_height);
}

bool t_command_parser_executor::print_sn_key(const std::vector<std::string>& args)
{
  if(!args.empty())
    return false;
  bool result = m_executor.print_sn_key();
  return result;
}

bool t_command_parser_executor::print_stake_requirement(const std::vector<std::string>& args)
{
  if(args.size() != 1)
  {
    std::cout << "Expected 1 argument, <height>" << std::endl;
    return false;
  }

  uint64_t height = 0;
  if(!epee::string_tools::get_xtype_from_string(height, args[0]))
  {
    std::cout << "Wrong height given" << std::endl;
    return false;
  }

  bool result = m_executor.print_stake_requirement(height);
  return result;
}

bool t_command_parser_executor::prepare_registration()
{
  bool result = m_executor.prepare_registration();
  return result;
}

bool t_command_parser_executor::print_sn(const std::vector<std::string>& args)
{
  bool result = m_executor.print_sn(args);
  return result;
}

bool t_command_parser_executor::print_sn_status(const std::vector<std::string>& args)
{
  bool result = m_executor.print_sn_status(args);
  return result;
}

bool t_command_parser_executor::set_log_level(const std::vector<std::string>& args)
{
  if(args.size() > 1)
  {
    std::cout << "use: set_log [<log_level_number_0-4> | <categories>]" << std::endl;
    return true;
  }

  if (args.empty())
  {
    return m_executor.set_log_categories("+");
  }

  uint16_t l = 0;
  if(epee::string_tools::get_xtype_from_string(l, args[0]))
  {
    if(4 < l)
    {
      std::cout << "wrong number range, use: set_log <log_level_number_0-4>" << std::endl;
      return true;
    }
    return m_executor.set_log_level(l);
  }
  else
  {
    return m_executor.set_log_categories(args.front());
  }
}

bool t_command_parser_executor::print_height(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_height();
}

bool t_command_parser_executor::print_block(const std::vector<std::string>& args)
{
  if (args.empty())
  {
    std::cout << "expected: print_block (<block_hash> | <block_height>)" << std::endl;
    return true;
  }

  const std::string& arg = args.front();
  try
  {
    uint64_t height = boost::lexical_cast<uint64_t>(arg);
    return m_executor.print_block_by_height(height);
  }
  catch (const boost::bad_lexical_cast&)
  {
    crypto::hash block_hash;
    if (parse_hash256(arg, block_hash))
    {
      return m_executor.print_block_by_hash(block_hash);
    }
  }

  return true;
}

bool t_command_parser_executor::print_transaction(const std::vector<std::string>& args)
{
  bool include_hex = false;
  bool include_json = false;

  // Assumes that optional flags come after mandatory argument <transaction_hash>
  for (unsigned int i = 1; i < args.size(); ++i) {
    if (args[i] == "+hex")
      include_hex = true;
    else if (args[i] == "+json")
      include_json = true;
    else
    {
      std::cout << "unexpected argument: " << args[i] << std::endl;
      return true;
    }
  }
  if (args.empty())
  {
    std::cout << "expected: print_tx <transaction_hash> [+hex] [+json]" << std::endl;
    return true;
  }

  const std::string& str_hash = args.front();
  crypto::hash tx_hash;
  if (parse_hash256(str_hash, tx_hash))
  {
    m_executor.print_transaction(tx_hash, include_hex, include_json);
  }

  return true;
}

bool t_command_parser_executor::is_key_image_spent(const std::vector<std::string>& args)
{
  if (args.empty())
  {
    std::cout << "expected: is_key_image_spent <key_image>" << std::endl;
    return true;
  }

  const std::string& str = args.front();
  crypto::key_image ki;
  crypto::hash hash;
  if (parse_hash256(str, hash))
  {
    memcpy(&ki, &hash, sizeof(ki));
    m_executor.is_key_image_spent(ki);
  }

  return true;
}

bool t_command_parser_executor::print_transaction_pool_long(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_transaction_pool_long();
}

bool t_command_parser_executor::print_transaction_pool_short(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_transaction_pool_short();
}

bool t_command_parser_executor::print_transaction_pool_stats(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_transaction_pool_stats();
}

bool t_command_parser_executor::start_mining(const std::vector<std::string>& args)
{
  if(!args.size())
  {
    std::cout << "Please specify a wallet address to mine for: start_mining <addr> [<threads>]" << std::endl;
    return true;
  }

  cryptonote::address_parse_info info;
  cryptonote::network_type nettype = cryptonote::MAINNET;
  if(!cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, args.front()))
  {
    if(!cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, args.front()))
    {
      if(!cryptonote::get_account_address_from_str(info, cryptonote::STAGENET, args.front()))
      {
        bool dnssec_valid;
        std::string address_str = tools::dns_utils::get_account_address_as_str_from_url(args.front(), dnssec_valid,
            [](const std::string &url, const std::vector<std::string> &addresses, bool dnssec_valid){return addresses[0];});
        if(!cryptonote::get_account_address_from_str(info, cryptonote::MAINNET, address_str))
        {
          if(!cryptonote::get_account_address_from_str(info, cryptonote::TESTNET, address_str))
          {
            if(!cryptonote::get_account_address_from_str(info, cryptonote::STAGENET, address_str))
            {
              std::cout << "target account address has wrong format" << std::endl;
              return true;
            }
            else
            {
              nettype = cryptonote::STAGENET;
            }
          }
          else
          {
            nettype = cryptonote::TESTNET;
          }
        }
      }
      else
      {
        nettype = cryptonote::STAGENET;
      }
    }
    else
    {
      nettype = cryptonote::TESTNET;
    }
  }
  if (info.is_subaddress)
  {
    tools::fail_msg_writer() << "subaddress for mining reward is not yet supported!" << std::endl;
    return true;
  }
  if(nettype != cryptonote::MAINNET)
    std::cout << "Mining to a " << (nettype == cryptonote::TESTNET ? "testnet" : "stagenet") << " address, make sure this is intentional!" << std::endl;
  uint64_t threads_count = 1;
  if(args.size() > 2)
  {
    return false;
  }

  if(args.size() >= 2)
  {
    bool ok = epee::string_tools::get_xtype_from_string(threads_count, args[1]);
    threads_count = (ok && 0 < threads_count) ? threads_count : 1;
  }

  m_executor.start_mining(info.address, threads_count, nettype);

  return true;
}

bool t_command_parser_executor::stop_mining(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.stop_mining();
}

bool t_command_parser_executor::stop_daemon(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.stop_daemon();
}

bool t_command_parser_executor::print_status(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;

  return m_executor.print_status();
}

bool t_command_parser_executor::set_limit(const std::vector<std::string>& args)
{
  if(args.size()>1) return false;
  if(args.size()==0) {
    return m_executor.get_limit();
  }
  int64_t limit;
  try {
      limit = std::stoll(args[0]);
  }
  catch(const std::exception& ex) {
      std::cout << "failed to parse argument" << std::endl;
      return false;
  }

  return m_executor.set_limit(limit, limit);
}

bool t_command_parser_executor::set_limit_up(const std::vector<std::string>& args)
{
  if(args.size()>1) return false;
  if(args.size()==0) {
    return m_executor.get_limit_up();
  }
  int64_t limit;
  try {
      limit = std::stoll(args[0]);
  }
  catch(const std::exception& ex) {
      std::cout << "failed to parse argument" << std::endl;
      return false;
  }

  return m_executor.set_limit(0, limit);
}

bool t_command_parser_executor::set_limit_down(const std::vector<std::string>& args)
{
  if(args.size()>1) return false;
  if(args.size()==0) {
    return m_executor.get_limit_down();
  }
  int64_t limit;
  try {
      limit = std::stoll(args[0]);
  }
  catch(const std::exception& ex) {
      std::cout << "failed to parse argument" << std::endl;
      return false;
  }

  return m_executor.set_limit(limit, 0);
}

bool t_command_parser_executor::out_peers(const std::vector<std::string>& args)
{
  bool set = false;
  uint32_t limit = 0;
  try
  {
    if(!args.empty())
    {
      limit = std::stoi(args[0]);
      set = true;
    }
  }

  catch(const std::exception& ex) {
    _erro("stoi exception");
    return false;
  }

  return m_executor.out_peers(set, limit);
}

bool t_command_parser_executor::in_peers(const std::vector<std::string>& args)
{
  bool set = false;
  uint32_t limit = 0;
  try
  {
    if(!args.empty())
    {
      limit = std::stoi(args[0]);
      set = true;
    }
  }

  catch(const std::exception& ex) {
    _erro("stoi exception");
    return false;
  }

  return m_executor.in_peers(set, limit);
}

bool t_command_parser_executor::hard_fork_info(const std::vector<std::string>& args)
{
  int version;
  if (args.size() == 0) {
    version = 0;
  }
  else if (args.size() == 1) {
    try {
      version = std::stoi(args[0]);
    }
    catch(const std::exception& ex) {
        return false;
    }
    if (version <= 0 || version > 255)
      return false;
  }
  else {
    return false;
  }
  return m_executor.hard_fork_info(version);
}

bool t_command_parser_executor::show_bans(const std::vector<std::string>& args)
{
  if (!args.empty()) return false;
  return m_executor.print_bans();
}

bool t_command_parser_executor::ban(const std::vector<std::string>& args)
{
  if (args.size() != 1 && args.size() != 2) return false;
  std::string ip = args[0];
  time_t seconds = P2P_IP_BLOCKTIME;
  if (args.size() > 1)
  {
    try
    {
      seconds = std::stoi(args[1]);
    }
    catch (const std::exception& e)
    {
      return false;
    }
    if (seconds == 0)
    {
      return false;
    }
  }
  return m_executor.ban(ip, seconds);
}

bool t_command_parser_executor::unban(const std::vector<std::string>& args)
{
  if (args.size() != 1) return false;
  std::string ip = args[0];
  return m_executor.unban(ip);
}

bool t_command_parser_executor::flush_txpool(const std::vector<std::string>& args)
{
  if (args.size() > 1) return false;

  std::string txid;
  if (args.size() == 1)
  {
    crypto::hash hash;
    if (!parse_hash256(args[0], hash))
    {
      std::cout << "failed to parse tx id" << std::endl;
      return true;
    }
    txid = args[0];
  }
  return m_executor.flush_txpool(txid);
}

bool t_command_parser_executor::output_histogram(const std::vector<std::string>& args)
{
  std::vector<uint64_t> amounts;
  uint64_t min_count = 3;
  uint64_t max_count = 0;
  size_t n_raw = 0;

  for (size_t n = 0; n < args.size(); ++n)
  {
    if (args[n][0] == '@')
    {
      amounts.push_back(boost::lexical_cast<uint64_t>(args[n].c_str() + 1));
    }
    else if (n_raw == 0)
    {
      min_count = boost::lexical_cast<uint64_t>(args[n]);
      n_raw++;
    }
    else if (n_raw == 1)
    {
      max_count = boost::lexical_cast<uint64_t>(args[n]);
      n_raw++;
    }
    else
    {
      std::cout << "Invalid syntax: more than two non-amount parameters" << std::endl;
      return true;
    }
  }
  return m_executor.output_histogram(amounts, min_count, max_count);
}

bool t_command_parser_executor::print_coinbase_tx_sum(const std::vector<std::string>& args)
{
  if(!args.size())
  {
    std::cout << "need block height parameter" << std::endl;
    return false;
  }
  uint64_t height = 0;
  uint64_t count = 0;
  if(!epee::string_tools::get_xtype_from_string(height, args[0]))
  {
    std::cout << "wrong starter block height parameter" << std::endl;
    return false;
  }
  if(args.size() >1 && !epee::string_tools::get_xtype_from_string(count, args[1]))
  {
    std::cout << "wrong count parameter" << std::endl;
    return false;
  }

  return m_executor.print_coinbase_tx_sum(height, count);
}

bool t_command_parser_executor::alt_chain_info(const std::vector<std::string>& args)
{
  if(args.size() > 1)
  {
    std::cout << "usage: alt_chain_info [block_hash|>N|-N]" << std::endl;
    return false;
  }

  std::string tip;
  size_t above = 0;
  uint64_t last_blocks = 0;
  if (args.size() == 1)
  {
    if (args[0].size() > 0 && args[0][0] == '>')
    {
      if (!epee::string_tools::get_xtype_from_string(above, args[0].c_str() + 1))
      {
        std::cout << "invalid above parameter" << std::endl;
        return false;
      }
    }
    else if (args[0].size() > 0 && args[0][0] == '-')
    {
      if (!epee::string_tools::get_xtype_from_string(last_blocks, args[0].c_str() + 1))
      {
        std::cout << "invalid last_blocks parameter" << std::endl;
        return false;
      }
    }
    else
    {
      tip = args[0];
    }
  }

  return m_executor.alt_chain_info(tip, above, last_blocks);
}

bool t_command_parser_executor::print_blockchain_dynamic_stats(const std::vector<std::string>& args)
{
  if(args.size() != 1)
  {
    std::cout << "Exactly one parameter is needed" << std::endl;
    return false;
  }

  uint64_t nblocks = 0;
  if(!epee::string_tools::get_xtype_from_string(nblocks, args[0]) || nblocks == 0)
  {
    std::cout << "wrong number of blocks" << std::endl;
    return false;
  }

  return m_executor.print_blockchain_dynamic_stats(nblocks);
}

bool t_command_parser_executor::update(const std::vector<std::string>& args)
{
  if(args.size() != 1)
  {
    std::cout << "Exactly one parameter is needed: check, download, or update" << std::endl;
    return false;
  }

  return m_executor.update(args.front());
}

bool t_command_parser_executor::relay_tx(const std::vector<std::string>& args)
{
  if (args.size() != 1) return false;

  std::string txid;
  crypto::hash hash;
  if (!parse_hash256(args[0], hash))
  {
    std::cout << "failed to parse tx id" << std::endl;
    return true;
  }
  txid = args[0];
  return m_executor.relay_tx(txid);
}

bool t_command_parser_executor::sync_info(const std::vector<std::string>& args)
{
  if (args.size() != 0) return false;

  return m_executor.sync_info();
}

bool t_command_parser_executor::pop_blocks(const std::vector<std::string>& args)
{
  if (args.size() != 1)
  {
    std::cout << "Exactly one parameter is needed" << std::endl;
    return false;
  }

  try
  {
    uint64_t nblocks = boost::lexical_cast<uint64_t>(args[0]);
    if (nblocks < 1)
    {
      std::cout << "number of blocks must be greater than 0" << std::endl;
      return false;
    }
    return m_executor.pop_blocks(nblocks);
  }
  catch (const boost::bad_lexical_cast&)
  {
    std::cout << "number of blocks must be a number greater than 0" << std::endl;
  }
  return false;
}

bool t_command_parser_executor::version(const std::vector<std::string>& args)
{
  std::cout << "ArQmA '" << ARQMA_RELEASE_NAME << "' (v" << ARQMA_VERSION_FULL << ")" << std::endl;
  return true;
}

bool t_command_parser_executor::prune_blockchain(const std::vector<std::string>& args)
{
  if (args.size() > 1) return false;

  if (args.empty() || args[0] != "confirm")
  {
    std::cout << "Warning: pruning from within Arqmad will not shrink the database file size." << std::endl;
    std::cout << "Instead, parts of the file will be marked as free, so the file will not grow" << std::endl;
    std::cout << "until that newly free space is used up. If you want a smaller file size now," << std::endl;
    std::cout << "exit Arqmad and run arqma-blockchain-prune (you will temporarily need more" << std::endl;
    std::cout << "disk space for the database conversion though). If you are OK with the database" << std::endl;
    std::cout << "file keeping the same size, re-run this command with the \"confirm\" parameter." << std::endl;
    return true;
  }

  return m_executor.prune_blockchain();
}

bool t_command_parser_executor::check_blockchain_pruning(const std::vector<std::string>& args)
{
  return m_executor.check_blockchain_pruning();
}

} // namespace daemonize
