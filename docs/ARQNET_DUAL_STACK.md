# Arq-Net Dual-Stack Migration

Plan for evolving from `LegacyArqNet` (`SNNetwork`) to a native `ArqMq`
backend without breaking operators.

## Goals

- Keep `--arqnet-backend=legacy-arqnet` as the default until one stable release cycle
  after the native backend is production-ready.
- Allow `--arqnet-backend=arqmq` on testnet/stagenet first.
- Preserve Arqma naming, ports and Curve25519 SN identity.

## Phases

1. **Facade only (landed)** — `arqmq::init`, ACL helpers, RPC status, fallback.
2. **Command registry (landed)** — map existing quorum/ping commands to ACL categories.
3. **Native transport (landed)** — `arqmq::SocketStack` ZMQ worker path under Arqma naming;
   selected by `--arqnet-backend=arqmq` (`transport=arqmq`). Peer mesh still SNNetwork.
4. **Dual-run coexistence (landed, compatibility mode)** — native stack + SNNetwork mesh
   run together; RPC reports `transport` vs `mesh`; peer wire protocol unchanged.
5. **Cutover (implementation on)** — `k_native_mesh_port_stage = 4` so
   `native_mesh_ready()` is true. Live relay uses `primary_mesh_send_to_peer`
   only at HF20+ on nodes with a CURVE `arqmq` stack (`native_mesh_live_at`).
   Default `--arqnet-backend` stays `legacy-arqnet`; SNNetwork is the fallback
   so votes are not dropped. Re-run stagenet soak when a live SN quorum exists.
6. **Optional hard gate** — Arq-Net ping for uptime proofs after operator notice.

## Operator flags

| Flag | Meaning |
|------|---------|
| `--arqnet-backend=legacy-arqnet` | Current production path (default; **required for mainnet**) |
| `--arqnet-backend=arqmq` | Experimental SocketStack; allowed on testnet/stagenet; mainnet needs `--arqnet-allow-experimental` |
| `--arqnet-allow-experimental` | Explicit mainnet override for `arqmq` / mesh-shadow (not for production SNs) |
| `--arqnet-mesh-shadow` | Opt-in SocketStack dual-write of peer commands (needs `arqmq` stack; stagenet/testnet soak; binds ANET+10000) |

## Hard fork gate (HF20)

Following Monero/Oxen practice, native mesh cutover is tied to a **network
version**, not only an operator flag:

| Net | HF20 height | HF21 height | Notes |
|-----|-------------|-------------|-------|
| Stagenet | **240** | **260** | Hybrid then exclusive |
| Testnet | **1300** | **1400** | Intermediate |
| Mainnet | **4 000 000** | **5 000 000** | v19 until 4M; hybrid 4M–5M |

Runtime helpers (`arqmq`):

- `hf_permits_native_mesh(hf)` — true at `network_version_20+`
- `native_mesh_implementation_ready()` — true at stage ≥4
- `native_mesh_ready_at(hf)` — HF + implementation (does not require a running stack)
- `native_mesh_live_at(hf)` — also requires an active CURVE SocketStack (daemon live path)
- `native_mesh_blocker()` — `none` at stage 4

Alias: `HF_VERSION_NATIVE_ARQNET_MESH` → `network_version_20`.

## Exit criteria before default flip

- [x] Native backend initializes on Linux CI (unit coverage for `SocketStack`)
- [x] Dual-run coexistence with peer mesh locked on SNNetwork (full wire compatibility)
- [x] Deny path for unknown Curve peers covered by unit tests (`arqnet_auth` /
      `decide_incoming_curve_peer*`; full ZAP two-process integration still later)
- [x] HF20 scaffold (stagenet/testnet heights; mainnet **4 000 000**)
- [x] Curve/ZAP allow + CURVE bind on SocketStack (shadow; SNNetwork still live mesh)
- [x] Peer endpoint bookkeeping (`PeerTable`)
- [x] Outbound CURVE peer send + inbound `vote_ob` handler path (unit-tested; not live-wired)
- [x] Daemon `--arqnet-mesh-shadow` + RPC counters (`mesh_shadow_*` on `get_arqnet_status`)
- [x] Live vs shadow parity telemetry + local dual-write `vote_ob` unit coverage
- [x] Shadow CURVE listener on ANET+10000 + outbound hint rewrite
- [x] Soak monitor script (`utils/arqnet-mesh-soak-monitor.py`)
- [x] Cutover relay (`primary_mesh_send_to_peer` / HF gate; stage 4)
- [x] RPC cutover gate fields on `get_arqnet_status`
- [x] Native inbound `vote_ob` processing (installed at stage ≥4)
- [x] Soak inbound `vote_ob` wire parse (`mesh_vote_ob_shadow_parse_*`; same decoder as stage 4)
- [x] Soak inbound `pulse_rnd` wire parse (`mesh_pulse_rnd_shadow_*`; observability only, not in `sample_ok`)
- [x] Native CURVE ping/pong reply path on SocketStack (unit-tested)
- [x] `native_mesh_implementation_ready()` (stage 4); live path `native_mesh_live_at`
- [x] Mainnet HF20 height **4 000 000** (v19 compatible until then)
- [x] HF21 **5 000 000** exclusive mesh intent + Pulse `get_pulse_status`
- [x] Pulse local round signatures + miner-tx extra (`TX_EXTRA_TAG_PULSE`)
- [x] Pulse `pulse_rnd` Arq-Net collector (hybrid PoW; no PoW-off scheduled)
- [x] Pulse wait-windows (15s) + leader rotation (`round` 0–7); RandomARQ still required
- [x] Pulse majority certificate extra (fixed 7/11, canonical order, round bound to timestamps)
- [x] Pulse extra payload binding (`pulse_rnd` v2; idle null payload; miner extra requires signed template hash)
- [x] Weight-neutral Pulse extra on miner templates; `get_pulse_status` certificate/payload fields
- [x] Daemon `print_pulse`; block-header Pulse extra fields
- [x] Pulse idle-loop participation (`pulse_rnd` without mining); hybrid PoW
- [x] Pulse vote retransmit + collector drop on new block / reorg
- [ ] Re-run stagenet soak; default `--arqnet-backend` flip later
