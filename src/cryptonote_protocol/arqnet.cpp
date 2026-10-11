#include "arqnet.h"
#include "cryptonote_core/cryptonote_core.h"
#include "cryptonote_core/pulse.h"
#include "cryptonote_core/service_node_voting.h"
#include "cryptonote_core/service_node_rules.h"
#include "cryptonote_core/tx_pool.h"
#include "arq_blink/blink.h"
#include "arqnet/sn_network.h"
#include "arqnet/conn_matrix.h"
#include "arqmq/command_registry.hpp"
#include "arqmq/mesh_bridge.hpp"
#include "arqmq/socket_stack.hpp"
#include "arqmq/arqmq.h"
#include "arqnet_auth.h"

#include <chrono>

#undef ARQMA_DEFAULT_LOG_CATEGORY
#define ARQMA_DEFAULT_LOG_CATEGORY "arqnet"

namespace arqnet {

namespace {

using namespace service_nodes;
using namespace std::string_literals;
using namespace std::chrono_literals;

size_t approx_payload_bytes(const std::vector<bt_value> &data)
{
  size_t bytes = 0;
  for (const auto &part : data)
  {
    if (const auto *s = boost::get<std::string>(&part))
      bytes += s->size();
    else
      bytes += 256; // bound non-string bt parts
  }
  return bytes;
}

struct SNNWrapper {
  SNNetwork snn;
  cryptonote::core &core;

  template <typename... Args>
  SNNWrapper(cryptonote::core &core, Args &&...args)
    : snn{std::forward<Args>(args)...}, core{core} {}

  static SNNWrapper &from(void* obj)
  {
    assert(obj);
    return *reinterpret_cast<SNNWrapper*>(obj);
  }
};

SNNWrapper *g_pulse_snw = nullptr;

void install_native_mesh_inbound_handlers(arqmq::SocketStack &stack, SNNWrapper &snw);
quorum_vote_t deserialize_vote(const bt_value &v);
bool try_decode_obligation_vote(std::string_view payload, quorum_vote_t *out) noexcept;
bool try_decode_pulse_vote(std::string_view payload, service_nodes::pulse::RelayVote &out) noexcept;
bool try_decode_blink_vote(std::string_view payload, arq_blink::RelayVote &out) noexcept;

template <typename T>
std::string get_data_as_string(const T &key)
{
  static_assert(std::is_trivial<T>(), "cannot safely copy non-trivial class to string");
  return {reinterpret_cast<const char *>(&key), sizeof(key)};
}

crypto::x25519_public_key x25519_from_string(std::string_view pubkey)
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
    const auto decision = decide_incoming_curve_peer_from_pubkey_size(static_cast<bool>(pubkey),
                                                                     x25519_pubkey_str.size());
    if (decision == IncomingCurveDecision::ServiceNode)
    {
      MINFO("Accepting incoming SN connection authentication from ip/x25519/pubkey: " << ip << "/" << x25519_pubkey << "/" << pubkey);
      return SNNetwork::allow::service_node;
    }

    // Quorum traffic is service-node only. Unknown Curve25519 identities must be
    // rejected to avoid an open ZMQ listener surface for DoS / command spam.
    MWARNING("Rejecting incoming Arq-Net connection from non-SN x25519 " << x25519_pubkey << " @ " << ip);
    return SNNetwork::allow::denied;
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
  g_pulse_snw = obj;

  // Dual-run coexistence: dedicated SocketStack (if --arqnet-backend=arqmq) stays
  // attached for native ACL/framing checks, while peer Curve/ZMQ mesh remains
  // this SNNetwork instance for full wire compatibility.
  arqmq::attach_compatible_mesh_mirrors_if_active();
  if (auto *stack = arqmq::active_socket_stack(); stack && keys)
  {
    // Shadow CURVE identity for opt-in dual-write. Live arqnet port stays on
    // SNNetwork; SocketStack binds live_bind + k_mesh_shadow_port_offset when
    // --arqnet-mesh-shadow is enabled.
    arqmq::configure_mesh_shadow(
        *stack, get_data_as_string(keys->pub_x25519), get_data_as_string(keys->key_x25519.data),
        [&sn_list = core.get_service_node_list()](const std::string & /*ip*/, const std::string &x25519_pubkey_str) {
          auto x25519_pubkey = x25519_from_string(x25519_pubkey_str);
          auto pubkey = sn_list.get_pubkey_from_x25519(x25519_pubkey);
          const auto decision =
              decide_incoming_curve_peer_from_pubkey_size(static_cast<bool>(pubkey), x25519_pubkey_str.size());
          return decision == IncomingCurveDecision::ServiceNode ? arqmq::CurvePeerAllow::ServiceNode
                                                                : arqmq::CurvePeerAllow::Denied;
        });
    // Soak inbound uses the same decoder the stage-4 native handler will run.
    arqmq::set_vote_ob_payload_validator([](std::string_view payload) {
      return try_decode_obligation_vote(payload, nullptr);
    });
    arqmq::set_pulse_rnd_payload_validator([](std::string_view payload) {
      service_nodes::pulse::RelayVote vote{};
      return try_decode_pulse_vote(payload, vote);
    });
    arqmq::set_blink_tx_payload_validator([](std::string_view payload) {
      arq_blink::RelayVote vote{};
      return try_decode_blink_vote(payload, vote);
    });
    if (arqmq::native_mesh_shadow_relay_enabled() || arqmq::native_mesh_ready())
    {
      if (const auto ec = arqmq::start_mesh_shadow_listener(*stack, bind))
        MWARNING("Arq-Net mesh shadow CURVE bind failed on "
                 << arqmq::endpoint_with_port_offset(bind, arqmq::k_mesh_shadow_port_offset) << ": " << ec.message());
      else
        MINFO("Arq-Net mesh CURVE listening on " << arqmq::native_mesh_shadow_endpoint()
              << (arqmq::native_mesh_ready()
                      ? " (native mesh primary; SNNetwork retained for rollback)"
                      : " (shadow dual-write; live mesh remains SNNetwork @ " + bind + ")"));
    }
    install_native_mesh_inbound_handlers(*stack, *obj);
  }
  if (arqmq::native_transport_active())
  {
    MINFO("Arq-Net dual-run active: native transport=" << arqmq::transport_name()
          << ", peer mesh=" << arqmq::mesh_transport_name()
          << " (compatibility mode)");
  }

  return obj;
}

void delete_snnwrapper(void *&obj)
{
  auto *snn = reinterpret_cast<SNNWrapper *>(obj);
  MINFO("Shutting down arqnet listener");
  if (g_pulse_snw == snn)
    g_pulse_snw = nullptr;
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
    : snn{snw.snn}, core{snw.core}
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
          my_pos = static_cast<int>(i);
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
  cryptonote::core &core;

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
            MTRACE("Inter-quorum relay from Q to Q' service node " << next_validators[half + my_position[i]]);
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
    const uint8_t hf = core.get_blockchain_storage().get_current_hard_fork_version();
    // Native is primary only when HF20+, stage ≥4, and this node has a CURVE stack.
    // Default `--arqnet-backend=legacy-arqnet` keeps SNNetwork so votes are not dropped.
    const bool native_primary = arqmq::native_mesh_live_at(hf);

    for (auto &peer : peers)
    {
      MTRACE("Relaying " << cmd << " to peer " << as_hex(peer.first) << (peer.second.empty() ? " (if connected)"s : " @ " + peer.second)
             << (native_primary ? " [native-mesh]" : " [snnetwork]"));

      if (native_primary)
      {
        // HF20 hybrid / HF21 exclusive: SocketStack is primary when CURVE is up.
        if constexpr (N >= 1)
          arqmq::primary_mesh_send_to_peer(peer.first, cmd, relay_data[0].data, peer.second);
        continue;
      }

      if (arqmq::hf_requires_native_mesh_exclusive(hf))
        MWARNING("HF21 exclusive mesh requested but this node has no CURVE SocketStack; "
                 "falling back to SNNetwork so votes are not dropped. Pass --arqnet-backend=arqmq.");

      if (peer.second.empty())
        snn.send(peer.first, cmd, relay_data[I]..., send_option::optional{});
      else
        snn.send(peer.first, cmd, relay_data[I]..., send_option::hint{peer.second});

      // Live SNNetwork relay accounted for soak parity vs opt-in SocketStack dual-write.
      arqmq::note_live_mesh_relay(cmd);
      if constexpr (N >= 1)
        arqmq::shadow_send_to_peer(peer.first, cmd, relay_data[0].data, peer.second);
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

bool try_decode_obligation_vote(std::string_view payload, quorum_vote_t *out) noexcept
{
  try
  {
    if (payload.empty())
      return false;
    bt_value decoded;
    bt_deserialize(payload.data(), payload.size(), decoded);
    auto vote = deserialize_vote(decoded);
    if (vote.type != quorum_type::obligations)
      return false;
    if (out)
      *out = vote;
    return true;
  }
  catch (...)
  {
    return false;
  }
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

  // Group votes by quorum to avoid recreating peer_info for the same quorum
  std::map<std::pair<quorum_type, uint64_t>, std::vector<const service_nodes::quorum_vote_t*>> votes_by_quorum;
  for (const auto& vote : votes)
  {
    if (vote.type != quorum_type::obligations)
    {
      MERROR("Internal logic error: arqnet asked to relay a " << vote.type << " vote, but should only be called with obligations votes");
      continue;
    }
    votes_by_quorum[{vote.type, vote.block_height}].push_back(&vote);
  }

  MDEBUG("Grouped " << votes.size() << " votes info " << votes_by_quorum.size() << " quorum groups");

  size_t vote_index = 0;
  for (auto &[quorum_key, quorum_votes] : votes_by_quorum)
  {
    auto [q_type, block_height] = quorum_key;
    vote_index += quorum_votes.size();
    if (vote_index % 50 == 0 || vote_index == votes.size())
      MDEBUG("Processing vote group " << vote_index - quorum_votes.size() + 1 << "-" << vote_index << " of " << votes.size() << " (quorum at height " << block_height << ")");

    auto quorum = snw.core.get_service_node_list().get_quorum(q_type, block_height);
    if (!quorum)
    {
      MWARNING("Unable to relay " << quorum_votes.size() << " votes: no " << q_type << " quorum available for height " << block_height);
      continue;
    }

    auto &quorum_voters = quorum->validators;
    if (quorum_voters.size() < service_nodes::min_votes_for_quorum_type(q_type))
    {
      MWARNING("Invalid vote relay: " << q_type << " quorum @ height " << block_height << " does not have enough validators ("
               << quorum_voters.size() << ") to reach the minimum required votes ("
               << service_nodes::min_votes_for_quorum_type(q_type) << ")");
    }

    MTRACE("Creating peer_info for quorum at height " << block_height << " with " << quorum_votes.size() << " votes");
    peer_info pinfo{snw, q_type, quorum};
    if (!pinfo.my_position_count)
    {
      MWARNING("Invalid vote relay: quorum at height " << block_height << " does not include this service node");
      continue;
    }

    MTRACE("Relaying " << quorum_votes.size() << " votes to " << pinfo.peers.size() << " peers");
    for (const auto *vote_ptr : quorum_votes)
    {
      pinfo.relay_to_peers("vote_ob", serialize_vote(*vote_ptr));
      relayed_votes.push_back(*vote_ptr);
    }
  }
  MDEBUG("Relayed " << relayed_votes.size() << " votes");
  snw.core.set_service_node_votes_relayed(relayed_votes);
}

void handle_obligation_vote(SNNetwork::message &m, void *self)
{
  const auto peer_acl = m.sn ? arqmq::CategoryAcl::ServiceNode : arqmq::CategoryAcl::Denied;
  if (!arqmq::authorize_request("vote_ob", peer_acl, approx_payload_bytes(m.data), m.data.size()))
  {
    MWARNING("Dropping vote_ob from unauthorized peer " << as_hex(m.pubkey));
    return;
  }

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

bool try_decode_pulse_vote(const std::string_view payload, service_nodes::pulse::RelayVote &out) noexcept
{
  if (service_nodes::pulse::decode_relay_vote(payload, out))
    return true;
  try
  {
    if (payload.empty())
      return false;
    bt_value decoded;
    bt_deserialize(payload.data(), payload.size(), decoded);
    const auto *s = boost::get<std::string>(&decoded);
    if (!s)
      return false;
    return service_nodes::pulse::decode_relay_vote(*s, out);
  }
  catch (const std::exception &)
  {
    return false;
  }
}

bool try_decode_blink_vote(const std::string_view payload, arq_blink::RelayVote &out) noexcept
{
  if (arq_blink::decode_relay_vote(payload, out))
    return true;
  try
  {
    if (payload.empty())
      return false;
    bt_value decoded;
    bt_deserialize(payload.data(), payload.size(), decoded);
    const auto *s = boost::get<std::string>(&decoded);
    if (!s)
      return false;
    return arq_blink::decode_relay_vote(*s, out);
  }
  catch (const std::exception &)
  {
    return false;
  }
}

void relay_pulse_vote(const service_nodes::pulse::RelayVote &vote)
{
  if (!g_pulse_snw || !g_pulse_snw->core.get_service_node_keys())
    return;
  const auto active = g_pulse_snw->core.get_service_node_list().get_active_service_node_pubkeys();
  auto q = std::make_shared<quorum>();
  q->validators = service_nodes::pulse::quorum_pubkeys(vote.height, active, vote.round);
  if (q->validators.empty())
    return;
  std::string blob;
  if (!service_nodes::pulse::encode_relay_vote(vote, blob))
    return;
  std::shared_ptr<const quorum> cq = std::move(q);
  // Peer list comes from `cq->validators` (Pulse quorum). `quorum_type` is only
  // used for Q/Q' trace labels in peer_info.
  peer_info pinfo{*g_pulse_snw, quorum_type::obligations, cq};
  if (!pinfo.my_position_count)
    return;
  pinfo.relay_to_peers("pulse_rnd", blob);
}

void relay_blink_vote(const arq_blink::RelayVote &vote)
{
  if (!g_pulse_snw || !g_pulse_snw->core.get_service_node_keys())
    return;
  const auto active = g_pulse_snw->core.get_service_node_list().get_active_service_node_pubkeys();
  const auto indices = arq_blink::quorum_indices(vote.height, active.size());
  if (indices.empty())
    return;
  auto q = std::make_shared<quorum>();
  q->validators.reserve(indices.size());
  for (const auto i : indices)
    q->validators.push_back(active[i]);
  std::string blob;
  if (!arq_blink::encode_relay_vote(vote, blob))
    return;
  std::shared_ptr<const quorum> cq = std::move(q);
  peer_info pinfo{*g_pulse_snw, quorum_type::obligations, cq};
  if (!pinfo.my_position_count)
    return;
  pinfo.relay_to_peers("blink_tx", blob);
}

bool apply_pulse_vote(SNNWrapper &snw, const service_nodes::pulse::RelayVote &vote, const bool relay_if_new)
{
  const uint8_t hf = snw.core.get_blockchain_storage().get_current_hard_fork_version();
  if (!service_nodes::pulse::hybrid_sn_permitted(hf))
    return false;
  const uint64_t chain_h = snw.core.get_current_blockchain_height();
  if (vote.height < chain_h)
    return false;
  if (vote.height > chain_h + 1)
    return false;
  const auto &bc = snw.core.get_blockchain_storage();
  uint64_t parent_ts = 0;
  if (bc.get_db().height() > 0)
    parent_ts = bc.get_db().get_block_timestamp(bc.get_db().height() - 1);
  const uint64_t now = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
  const uint8_t now_round = service_nodes::pulse::round_from_timestamps(parent_ts, now);
  if (vote.round > now_round + 1)
    return false;
  if (now_round > 0 && vote.round + 1 < now_round)
    return false;
  const auto active = snw.core.get_service_node_list().get_active_service_node_pubkeys();
  const bool added = service_nodes::pulse::collector().add_vote(
      vote, service_nodes::pulse::quorum_pubkeys(vote.height, active, vote.round), active.size(), relay_if_new);
  if (!added)
    return false;
  if (vote.payload_hash != crypto::null_hash)
  {
    if (const auto *keys = snw.core.get_service_node_keys())
    {
      if (snw.core.is_service_node(keys->pub, true))
        service_nodes::pulse::participate_round(vote.height, vote.prev_id, vote.round, keys->pub, keys->key,
                                                active, relay_if_new, vote.payload_hash);
    }
  }
  return true;
}

void handle_pulse_round(SNNetwork::message &m, void *self)
{
  const auto peer_acl = m.sn ? arqmq::CategoryAcl::ServiceNode : arqmq::CategoryAcl::Denied;
  if (!arqmq::authorize_request("pulse_rnd", peer_acl, approx_payload_bytes(m.data), m.data.size()))
  {
    MWARNING("Dropping pulse_rnd from unauthorized peer " << as_hex(m.pubkey));
    return;
  }
  if (m.data.size() != 1)
    return;
  const auto *blob = boost::get<std::string>(&m.data[0]);
  if (!blob)
    return;
  service_nodes::pulse::RelayVote vote{};
  if (!service_nodes::pulse::decode_relay_vote(*blob, vote))
    return;
  apply_pulse_vote(SNNWrapper::from(self), vote, true);
}

void handle_blink_tx(SNNetwork::message &m, void * /*self*/)
{
  const auto peer_acl = m.sn ? arqmq::CategoryAcl::ServiceNode : arqmq::CategoryAcl::Denied;
  if (!arqmq::authorize_request("blink_tx", peer_acl, approx_payload_bytes(m.data), m.data.size()))
  {
    MWARNING("Dropping blink_tx from unauthorized peer " << as_hex(m.pubkey));
    return;
  }
  if (m.data.size() != 1)
    return;
  const auto *blob = boost::get<std::string>(&m.data[0]);
  if (!blob)
    return;
  arq_blink::RelayVote vote{};
  if (!arq_blink::decode_relay_vote(*blob, vote))
    return;
  arq_blink::apply_relay_vote(vote, true);
}

/// Native-mesh inbound path (SocketStack). Active only after cutover stage ≥4.
/// Overwrites soak listener handlers; still records inbound parse counters via
/// `note_inbound_mesh_shadow` (`vote_ob` + `pulse_rnd` + `blink_tx`). SNNetwork stays live.
void install_native_mesh_inbound_handlers(arqmq::SocketStack &stack, SNNWrapper &snw)
{
  if (!arqmq::native_mesh_ready())
    return;

  stack.register_handler("vote_ob", arqmq::CategoryAcl::ServiceNode, [&snw](const arqmq::InboundRequest &req) {
    arqmq::note_inbound_mesh_shadow("vote_ob", req.payload);
    if (!arqmq::authorize_request("vote_ob", req.peer_acl, req.payload.size(), 1))
    {
      MWARNING("Dropping native-mesh vote_ob from unauthorized peer "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
      return std::string{};
    }

    quorum_vote_t vote{};
    if (!try_decode_obligation_vote(req.payload, &vote))
    {
      MWARNING("Native-mesh vote_ob deserialize failed from "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
      return std::string{};
    }
    if (vote.block_height > snw.core.get_current_blockchain_height())
      return std::string{};

    std::vector<quorum_vote_t> vvote;
    vvote.push_back(vote);
    cryptonote::vote_verification_context vvc{};
    snw.core.add_service_node_vote(vote, vvc);
    if (vvc.m_verification_failed)
      return std::string{};
    if (vvc.m_added_to_pool)
      relay_obligation_votes(&snw, std::move(vvote));
    return std::string{};
  });

  stack.register_handler("pulse_rnd", arqmq::CategoryAcl::ServiceNode, [&snw](const arqmq::InboundRequest &req) {
    arqmq::note_inbound_mesh_shadow("pulse_rnd", req.payload);
    if (!arqmq::authorize_request("pulse_rnd", req.peer_acl, req.payload.size(), 1))
    {
      MWARNING("Dropping native-mesh pulse_rnd from unauthorized peer "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
      return std::string{};
    }
    service_nodes::pulse::RelayVote vote{};
    if (!try_decode_pulse_vote(req.payload, vote))
      return std::string{};
    apply_pulse_vote(snw, vote, true);
    return std::string{};
  });

  stack.register_handler("blink_tx", arqmq::CategoryAcl::ServiceNode, [](const arqmq::InboundRequest &req) {
    arqmq::note_inbound_mesh_shadow("blink_tx", req.payload);
    if (!arqmq::authorize_request("blink_tx", req.peer_acl, req.payload.size(), 1))
    {
      MWARNING("Dropping native-mesh blink_tx from unauthorized peer "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
      return std::string{};
    }
    arq_blink::RelayVote vote{};
    if (!try_decode_blink_vote(req.payload, vote))
      return std::string{};
    arq_blink::apply_relay_vote(vote, true);
    return std::string{};
  });

  stack.register_handler("ping", arqmq::CategoryAcl::Basic, [](const arqmq::InboundRequest &req) {
    if (!arqmq::authorize_request("ping", req.peer_acl, req.payload.size(), req.payload.empty() ? 0 : 1))
    {
      MWARNING("Dropping native-mesh ping from unauthorized peer "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
      return std::string{};
    }
    return std::string{"pong"};
  });
  stack.register_handler("pong", arqmq::CategoryAcl::Basic, [](const arqmq::InboundRequest &req) {
    if (!arqmq::authorize_request("pong", req.peer_acl, req.payload.size(), req.payload.empty() ? 0 : 1))
    {
      MWARNING("Dropping native-mesh pong from unauthorized peer "
               << (req.peer_pubkey.size() == 32 ? as_hex(req.peer_pubkey) : "unknown"));
    }
    return std::string{};
  });

  MINFO("Arq-Net native mesh inbound vote_ob/pulse_rnd/blink_tx/ping handlers installed (cutover active)");
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
  const auto peer_acl = m.sn ? arqmq::CategoryAcl::ServiceNode : arqmq::CategoryAcl::Basic;
  if (!arqmq::authorize_request("ping", peer_acl, approx_payload_bytes(m.data), m.data.size()))
  {
    MWARNING("Dropping ping from unauthorized peer " << as_hex(m.pubkey));
    return;
  }

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
  const auto peer_acl = m.sn ? arqmq::CategoryAcl::ServiceNode : arqmq::CategoryAcl::Basic;
  if (!arqmq::authorize_request("pong", peer_acl, approx_payload_bytes(m.data), m.data.size()))
  {
    MWARNING("Dropping pong from unauthorized peer " << as_hex(m.pubkey));
    return;
  }

  MINFO("Received pong from " << (m.sn ? "SN" : "non-SN") << " " << as_hex(m.pubkey));
}

} // end empty namespace

void init_core_callbacks()
{
  cryptonote::arqnet_new = new_snnwrapper;
  cryptonote::arqnet_delete = delete_snnwrapper;
  cryptonote::arqnet_relay_obligation_votes = relay_obligation_votes;
  service_nodes::pulse::set_relay_new_vote(relay_pulse_vote);
  arq_blink::set_wire_connected(true);
  arq_blink::set_relay_new_vote(relay_blink_vote);

  SNNetwork::register_command("vote_ob", SNNetwork::command_type::quorum, handle_obligation_vote);
  SNNetwork::register_command("pulse_rnd", SNNetwork::command_type::quorum, handle_pulse_round);
  SNNetwork::register_command("blink_tx", SNNetwork::command_type::quorum, handle_blink_tx);
  SNNetwork::register_command("ping", SNNetwork::command_type::public_, handle_ping);
  SNNetwork::register_command("pong", SNNetwork::command_type::public_, handle_pong);
}

} // arqnet
