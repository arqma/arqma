#include "arqnet.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/service_node_voting.h"
#include "cryptonote_core/service_node_rules.h"
#include "cryptonote_core/tx_pool.h"
#include "arqnet/sn_network.h"
#include "arqnet/conn_matrix.h"
#include "cryptonote_config.h"

#include <shared_mutex>

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "arqnet"

namespace arqnet {

namespace {

using namespace service_nodes;
using namespace std::string_literals;
using namespace std::chrono_literals;

struct SNNWrapper {
  SNNetwork snn;
  cryptonote::core &core;

  boost::shared_mutex mutex;

  template <typename... Args>
  SNNWrapper(cryptonote::core &core, Args &&...args)
    : snn{std::forward<Args>(args)...}, core{core} {}

  static SNNWrapper &from(void* obj)
  {
    assert(obj);
    return *reinterpret_cast<SNNWrapper*>(obj);
  }
};

template <typename T>
std::string get_data_as_string(const T &key)
{
  static_assert(std::is_trivial<T>(), "cannot safely copy non-trivial class to string");
  return {reinterpret_cast<const char *>(&key), sizeof(key)};
}

crypto::x25519_public_key x25519_from_string(const std::string &pubkey)
{
  crypto::x25519_public_key x25519_pub = crypto::x25519_public_key::null();
  if (pubkey.size() == sizeof(crypto::x25519_public_key))
    std::memcpy(x25519_pub.data, pubkey.data(), pubkey.size());
  return x25519_pub;
}

std::string get_connect_string(const service_node_list &sn_list, const crypto::x25519_public_key &x25519_pub)
{
  if (!x25519_pub)
  {
    MDEBUG("no connection available: pubkey is epmty");
    return "";
  }
  auto pubkey = sn_list.get_pubkey_from_x25519(x25519_pub);
  if (!pubkey)
  {
    MDEBUG("no connection available: could not find primary pubkey from x25519 pubkey " << x25519_pub);
    return "";
  }
  bool found = false;
  uint32_t ip = 0;
  uint16_t port = 0;
  sn_list.for_each_service_node_info_and_proof(&pubkey, &pubkey + 1, [&](auto&, auto&, auto& proof)
  {
    found = true;
    ip = proof.public_ip;
    port = proof.arqnet_port;
  });
  if (!found)
  {
    MDEBUG("no connection available: primary pubkey " << pubkey << " is not registered");
    return "";
  }
  if (!(ip && port))
  {
    MDEBUG("no connection available: service node " << pubkey << " has no associated ip and/or port");
    return "";
  }
  return "tcp://" + epee::string_tools::get_ip_string_from_int32(ip) + ":" + std::to_string(port);
}

constexpr el::Level easylogging_level(LogLevel level)
{
  switch (level)
  {
    case LogLevel::fatal: return el::Level::Fatal;
    case LogLevel::error: return el::Level::Error;
    case LogLevel::warn: return el::Level::Warning;
    case LogLevel::info: return el::Level::Info;
    case LogLevel::debug: return el::Level::Debug;
    case LogLevel::trace: return el::Level::Trace;
  };
  return el::Level::Unknown;
};

bool snn_want_log(LogLevel level)
{
  return ELPP->vRegistry()->allowed(easylogging_level(level), ARQMA_DEFAULT_LOG_CATEGORY);
}

void snn_write_log(LogLevel level, const char *file, int line, std::string msg)
{
  el::base::Writer(easylogging_level(level), el::Color::Default, file, line, ELPP_FUNC, el::base::DispatchAction::NormalLog).construct(ARQMA_DEFAULT_LOG_CATEGORY) << msg;
}

void *new_snnwrapper(cryptonote::core &core, const std::string &bind)
{
  auto keys = core.get_service_node_keys();
  auto peer_lookup = [&sn_list = core.get_service_node_list()](const std::string &x25519_pub)
  {
    return get_connect_string(sn_list, x25519_from_string(x25519_pub));
  };
  auto allow = [&sn_list = core.get_service_node_list()](const std::string &ip, const std::string &x25519_pubkey_str)
  {
    auto x25519_pubkey = x25519_from_string(x25519_pubkey_str);
    auto pubkey = sn_list.get_pubkey_from_x25519(x25519_pubkey);
    if (pubkey)
    {
      MINFO("Accepting incoming SN connection authentication from ip/x25519/pubkey: " << ip << "/" << x25519_pubkey << "/" << pubkey);
      return SNNetwork::allow::service_node;
    }

    return SNNetwork::allow::client;
  };
  SNNWrapper *obj;
  if (!keys)
  {
    MINFO("Starting remote-only arqnet instance");
    obj = new SNNWrapper(core, peer_lookup, allow, snn_want_log, snn_write_log);
  }
  else
  {
    MINFO("Starting arqnet listener on " << bind << " with x25519 pubkey " << keys->pub_x25519);
    obj = new SNNWrapper(core, get_data_as_string(keys->pub_x25519), get_data_as_string(keys->key_x25519.data),
                         std::vector<std::string>{{bind}}, peer_lookup, allow, snn_want_log, snn_write_log);
  }

  obj->snn.data = obj;

  return obj;
}

void delete_snnwrapper(void *&obj)
{
  auto *snn = reinterpret_cast<SNNWrapper *>(obj);
  MINFO("Shutting down arqnet listener");
  delete snn;
  obj = nullptr;
}

template <typename E>
#ifdef __GNUG__
[[gnu::warn_unused_result]]
#endif
E get_enum(const bt_dict &d, const std::string &key)
{
  E result = static_cast<E>(get_int<std::underlying_type_t<E>>(d.at(key)));
  if (result < E::_count)
    return result;
  throw std::invalid_argument("invalid enum value for field " + key);
}

class peer_info {
public:
  using exclude_set = std::unordered_set<crypto::public_key>;

  std::unordered_map<crypto::public_key, std::pair<crypto::x25519_public_key, std::string>> remotes;
  std::unordered_map<std::string, std::string> peers;
  int strong_peers;
  std::vector<int> my_position;
  int my_position_count;

  peer_info(SNNWrapper &snw, quorum_type q_type, std::shared_ptr<const quorum> &quorum, bool opportunistic = true, exclude_set exclude = {})
    : peer_info(snw, q_type, &quorum, &quorum + 1, opportunistic, std::move(exclude))
  {}

  template <typename QuorumIt>
  peer_info(SNNWrapper &snw, quorum_type q_type, QuorumIt qbegin, QuorumIt qend, bool opportunistic = true, std::unordered_set<crypto::public_key> exclude = {})
    : snn{snw.snn}
  {
    auto keys = snw.core.get_service_node_keys();
    assert(keys);
    const auto &my_pubkey = keys->pub;
    exclude.insert(my_pubkey);

    my_position_count = 0;
    std::unordered_set<crypto::public_key> need_remotes;
    for (auto qit = qbegin; qit != qend; ++qit)
    {
      auto &v = (*qit)->validators;
      int my_pos = -1;
      for (size_t i = 0; i < v.size(); i++)
      {
        if (v[i] == my_pubkey)
          my_pos = 1;
        else if (!exclude.count(v[i]))
          need_remotes.insert(v[i]);
      }
      my_position.push_back(my_pos);
      if (my_pos >= 0)
        my_position_count++;
    }

    snw.core.get_service_node_list().for_each_service_node_info_and_proof(need_remotes.begin(), need_remotes.end(),
      [this](const auto &pubkey, const auto &info, const auto &proof)
      {
        if (info.is_active() && proof.pubkey_x25519 && proof.arqnet_port && proof.public_ip)
          remotes.emplace(pubkey, std::make_pair(proof.pubkey_x25519, "tcp://" + epee::string_tools::get_ip_string_from_int32(proof.public_ip) + ":" + std::to_string(proof.arqnet_port)));
      });
    compute_peers(qbegin, qend, opportunistic);
  }

  template <typename... T>
  void relay_to_peers(const std::string &cmd, const T &...data)
  {
    relay_to_peers_impl(cmd, std::array<send_option::serialized, sizeof...(T)>{send_option::serialized{data}...}, std::make_index_sequence<sizeof...(T)>{});
  }

private:
  SNNetwork &snn;

  bool add_peer(const crypto::public_key &pubkey, bool strong = true)
  {
    auto it = remotes.find(pubkey);
    if (it != remotes.end())
    {
      std::string remote_addr = strong ? it->second.second : ""s;
      auto ins = peers.emplace(get_data_as_string(it->second.first), std::move(remote_addr));
      if (strong && !ins.second && ins.first->second.empty())
      {
        ins.first->second = it->second.second;
        strong_peers++;
        return true;
      }
      if (strong && ins.second)
        strong_peers++;

      return ins.second;
    }
    return false;
  }

  template <typename QuorumIt>
  void compute_peers(QuorumIt qbegin, QuorumIt qend, bool opportunistic)
  {
    strong_peers = 0;

    size_t i = 0;
    for (QuorumIt qit = qbegin; qit != qend; ++i, ++qit)
    {
      if (my_position[i] < 0)
      {
        MTRACE("Not in quorum " << (i == 0 ? "Q" : "Q'"));
        continue;
      }

      auto &validators = (*qit)->validators;

      for (int j : quorum_outgoing_conns(my_position[i], validators.size()))
      {
        if (add_peer(validators[j]))
          MTRACE("Relaying within quorum " << (i == 0 ? "Q" : "Q'") << " to service node " << validators[j]);
      }

      for (int j : quorum_incoming_conns(my_position[i], validators.size()))
      {
        if (add_peer(validators[j], false))
          MTRACE("Optional opportunistic relay with quorum " << (i == 0 ? "Q" : "Q'") << " to service node " << validators[j]);
      }

      QuorumIt qnext = std::next(qit);
      if (qnext != qend && my_position[i + 1] < 0)
      {
        auto &next_validators = (*qnext)->validators;
        int half = std::min<int>(validators.size(), next_validators.size()) / 2;
        if (my_position[i] >= half && my_position[i] < half*2)
        {
          if (add_peer(validators[my_position[i] - half]))
            MTRACE("Inter-quorum relay from Q to Q' service node " << next_validators[my_position[i] - half]);
        }
        else
        {
          MTRACE("Not a Q -> Q' inter-quorum relay (Q position is " << my_position[i] << ")");
        }
      }

      if (qit != qbegin && my_position[i - 1] < 0)
      {
        auto &prev_validators = (*std::prev(qit))->validators;
        int half = std::min<int>(validators.size(), prev_validators.size()) / 2;
        if (my_position[i] < half)
        {
          if (add_peer(prev_validators[half + my_position[i]]))
            MTRACE("Inter-quorum relay from Q' to Q service node " << prev_validators[my_position[i] - half]);
        }
        else
        {
          MTRACE("Not a Q' -> Q inter-quorum relay (Q' position is " << my_position[i] << ")");
        }
      }
    }
  }

  template<size_t N, size_t... I>
  void relay_to_peers_impl(const std::string &cmd, std::array<send_option::serialized, N> relay_data, std::index_sequence<I...>)
  {
    for (auto &peer : peers)
    {
      MTRACE("Relaying " << cmd << " to peer " << as_hex(peer.first) << (peer.second.empty() ? " (if connected)"s : " @ " + peer.second));
      if (peer.second.empty())
        snn.send(peer.first, cmd, relay_data[I]..., send_option::optional{});
      else
        snn.send(peer.first, cmd, relay_data[I]..., send_option::hint{peer.second});
    }
  }

};

bt_dict serialize_vote(const quorum_vote_t &vote)
{
  bt_dict result{
    {"v", vote.version},
    {"t", static_cast<uint8_t>(vote.type)},
    {"h", vote.block_height},
    {"g", static_cast<uint8_t>(vote.group)},
    {"i", vote.index_in_group},
    {"s", get_data_as_string(vote.signature)},
  };
  if (vote.type == quorum_type::checkpointing)
    result["bh"] = std::string{vote.checkpoint.block_hash.data, sizeof(crypto::hash)};
  else
  {
    result["wi"] = vote.state_change.worker_index;
    result["sc"] = static_cast<std::underlying_type_t<new_state>>(vote.state_change.state);
  }
  return result;
}

quorum_vote_t deserialize_vote(const bt_value &v)
{
  const auto &d = boost::get<bt_dict>(v);
  quorum_vote_t vote;
  vote.version = get_int<uint8_t>(d.at("v"));
  vote.type = get_enum<quorum_type>(d, "t");
  vote.block_height = get_int<uint64_t>(d.at("h"));
  vote.group = get_enum<quorum_group>(d, "g");
  if (vote.group == quorum_group::invalid) throw std::invalid_argument("invalid vote group");
  vote.index_in_group = get_int<uint16_t>(d.at("i"));
  auto &sig = boost::get<std::string>(d.at("s"));
  if (sig.size() != sizeof(vote.signature)) throw std::invalid_argument("invalid vote signature size");
  std::memcpy(&vote.signature, sig.data(), sizeof(vote.signature));
  if (vote.type == quorum_type::checkpointing)
  {
    auto &bh = boost::get<std::string>(d.at("bh"));
    if (bh.size() != sizeof(vote.checkpoint.block_hash.data)) throw std::invalid_argument("invalid vote checkpoint block hash");
    std::memcpy(vote.checkpoint.block_hash.data, bh.data(), sizeof(vote.checkpoint.block_hash.data));
  }
  else
  {
    vote.state_change.worker_index = get_int<uint16_t>(d.at("wi"));
    vote.state_change.state = get_enum<new_state>(d, "sc");
  }

  return vote;
}

void relay_obligation_votes(void *obj, const std::vector<service_nodes::quorum_vote_t> &votes)
{
  auto &snw = SNNWrapper::from(obj);
  auto my_keys_ptr = snw.core.get_service_node_keys();
  assert(my_keys_ptr);
  const auto &my_keys = *my_keys_ptr;

  MDEBUG("Starting relay of " << votes.size() << " votes");
  std::vector<service_nodes::quorum_vote_t> relayed_votes;
  relayed_votes.reserve(votes.size());
  for (auto &vote : votes)
  {
    if (vote.type != quorum_type::obligations)
    {
      MERROR("Internal logic error: arqnet asked to relay a " << vote.type << " vote, but should only be called with obligations votes");
      continue;
    }

    auto quorum = snw.core.get_service_node_list().get_quorum(vote.type, vote.block_height);
    if (!quorum)
    {
      MWARNING("Unable to relay vote: no " << vote.type << " quorum available for height " << vote.block_height);
      continue;
    }

    auto &quorum_voters = quorum->validators;
    if (quorum_voters.size() < service_nodes::min_votes_for_quorum_type(vote.type))
    {
      MWARNING("Invalid vote relay: " << vote.type << " quorum @ height " << vote.block_height << " does not have enough validators ("
               << quorum_voters.size() << ") to reach the minimum required votes ("
               << service_nodes::min_votes_for_quorum_type(vote.type) << ")");
    }

    peer_info pinfo{snw, vote.type, quorum};
    if (!pinfo.my_position_count)
    {
      MWARNING("Invalid vote relay: vote to relay does not include this service node");
      continue;
    }

    pinfo.relay_to_peers("vote_ob", serialize_vote(vote));
    relayed_votes.push_back(vote);
  }
  MDEBUG("Relayed " << relayed_votes.size() << " votes");
  snw.core.set_service_node_votes_relayed(relayed_votes);
}

void handle_obligation_vote(SNNetwork::message &m, void *self)
{
  auto &snw = SNNWrapper::from(self);

  MDEBUG("Received a relayed obligation vote from " << as_hex(m.pubkey));

  if (m.data.size() != 1)
  {
    MINFO("Ignoring vote: expected 1 data part, not " << m.data.size());
    return;
  }

  try
  {
    std::vector<quorum_vote_t> vvote;
    vvote.push_back(deserialize_vote(m.data[0]));
    auto& vote = vvote.back();

    if (vote.type != quorum_type::obligations)
    {
      MWARNING("Received invalid vote via arqnet. Ignoring");
      return;
    }

    if (vote.block_height > snw.core.get_current_blockchain_height())
    {
      MDEBUG("Ignoring vote: block height " << vote.block_height << " is too high");
      return;
    }

    cryptonote::vote_verification_context vvc{};
    snw.core.add_service_node_vote(vote, vvc);
    if (vvc.m_verification_failed)
    {
      MWARNING("Vote verification failed: ignoring vote");
      return;
    }

    if (vvc.m_added_to_pool)
      relay_obligation_votes(self, std::move(vvote));
  }
  catch (const std::exception &e)
  {
    MWARNING("Deserialization of vote from " << as_hex(m.pubkey) << " failed: " << e.what());
  }
}

template <typename I>
std::enable_if_t<std::is_integral<I>::value, I> get_or(bt_dict &d, const std::string &key, I fallback)
{
  auto it = d.find(key);
  if (it != d.end())
  {
    try { return get_int<I>(it->second); }
    catch (...) { }
  }
  return fallback;
}

void handle_ping(SNNetwork::message &m, void *)
{
  uint64_t tag = 0;
  if (!m.data.empty())
  {
    auto &data = boost::get<bt_dict>(m.data[0]);
    tag = get_or<uint64_t>(data, "!", 0);
  }

  MINFO("Received ping request from " << (m.sn ? "SN" : "non-SN") << " " << as_hex(m.pubkey) << ", sending pong");
  m.reply("pong", bt_dict{{"!", tag}, {"sn", m.sn}});
}

void handle_pong(SNNetwork::message &m, void *)
{
  MINFO("Received pong from " << (m.sn ? "SN" : "non-SN") << " " << as_hex(m.pubkey));
}

} // end empty namespace

void init_core_callbacks()
{
  cryptonote::arqnet_new = new_snnwrapper;
  cryptonote::arqnet_delete = delete_snnwrapper;
  cryptonote::arqnet_relay_obligation_votes = relay_obligation_votes;

  SNNetwork::register_command("vote_ob", SNNetwork::command_type::quorum, handle_obligation_vote);
  SNNetwork::register_command("ping", SNNetwork::command_type::public_, handle_ping);
  SNNetwork::register_command("pong", SNNetwork::command_type::public_, handle_pong);
}

} // arqnet
