# Arqma Upgrade Roadmap

Branch: `upgrade`  
Goal: modernize Arqma to production-grade quality using mature Monero/Oxen
patterns while preserving Arqma identity — **fully functional on Linux,
Windows, and macOS**.

Platform matrix: [`docs/PLATFORM.md`](PLATFORM.md).

## Phase status

| Phase | Status | Notes |
|-------|--------|-------|
| 1 Analysis | **Done** | Architecture, deps, risks, Oxen gap analysis |
| 2 Core modernization | **Done** | C++20, CI, CLSAG tests, HF/Arq-Net fixes |
| 3 Oxen feature parity (selective) | **Foundation + native transport** | ArqMQ facade + `SocketStack` behind `--arqnet-backend=arqmq` |
| 4 Arq-Net evolution | **Foundation done** | Auth harden, ping, dual-stack plan |
| 5 Storage Server | **In-repo binary** | `arqma-storage` HTTP KV + TTL + swarm member fan-out + membership merge/push; daemon still probes |
| 6 Session-like modules | **In-repo CLI** | Envelope, onion peel/forward, swarm, sealed-box, `arqma-msg` |
| 7 RPC modernization | **Foundation done** | Validation, wallet caps/auth, OpenAPI 0.2.0 |
| 8 P2P improvements | **Foundation done** | Limits + Levin/preauth lock |
| 9 Performance | **Baseline** | Local measurement doc |
| 10 Testing | **Done (curated)** | **628** unit tests green; integration suites opt-in |
| 11 CI | **Done (3 OS)** | Linux/macOS native unit + Windows/macOS/Linux depends |
| 12 Final review | **Gate ready** | Checklist + completeness gate + platform doc |

## Cross-platform delivery map

```text
                    ┌─────────────────────────────────────┐
                    │         upgrade foundation PR         │
                    └─────────────────────────────────────┘
                                      │
          ┌───────────────────────────┼───────────────────────────┐
          ▼                           ▼                           ▼
   ┌─────────────┐            ┌─────────────┐            ┌─────────────┐
   │   Linux     │            │   macOS     │            │  Windows    │
   │ unit+ASan   │            │ macos-14    │            │ mingw x64   │
   │ depends x64 │            │ unit native │            │ depends     │
   │ depends arm │            │ depends x64 │            │ binaries    │
   │             │            │ depends arm │            │             │
   └─────────────┘            └─────────────┘            └─────────────┘
```

## Dependency map (summary)

| Dependency | Arqma today | Target |
|------------|-------------|--------|
| C++ standard | C++20 (upgrade) | Keep C++20 |
| CMake | ≥ 3.16 | Align README + CI |
| Boost | ≥ 1.66 | Track LTS |
| OpenSSL | ≥ 1.1.1 | Prefer 3.x in CI |
| libsodium | ≥ 1.0.18 | Keep |
| libzmq + cppzmq | ≥ 4.3.2 | Keep; evolve toward OxenMQ-style API |
| LMDB | vendored | Keep; audit wrappers |
| RandomARQ | submodule | Keep |
| easylogging++ | vendored | Evaluate structured logging later |

## Safe adaptations from Oxen (no new tokenomics)

1. C++20 / Ninja / ccache / LTO hygiene
2. Native CI with unit tests + sanitizers
3. Messaging library patterns (OxenMQ concepts → Arqma naming)
4. Operator RPC clarity and ping/reachability reporting
5. clang-format / clang-tidy baselines
6. Darwin depends via clang + lld (no legacy cctools for CI)

## Requires SN economics / product decision (Milestone C)

- Storage swarm replication for messaging (`arqma-storage` KV + TTL + inbox swarm fan-out is in-tree)
- Pulse-like PoS block production (**started:** HF20 hybrid / HF21 exclusive mesh +
  leader/quorum + miner extra + `pulse_rnd` collector + wait-windows + payload-bound extra +
  `get_pulse_status`;
  RandomARQ stays required — hybrid PoW, no PoW-off; SN idle `pulse_rnd`)
- Blink-like instant tx quorums (**started:** `arq_blink` collector + `get_blink_status`; does not replace Pulse/PoW)
- Privacy routing daemon (`arqma-router` HTTP peel/store/forward is in-tree)
- Session-class messenger clients (`arqma-msg` CLI is in-tree)

Do not implement those as drive-by copies; schedule behind an explicit product
milestone.

## Milestone A — platform (**complete**)

- [x] Architectural analysis + docs
- [x] Fix HF19 burn gate off-by-one
- [x] Deny non-SN Arq-Net connections
- [x] Restore Arq-Net ping RPC (non-blocking for proofs)
- [x] CLSAG unit tests
- [x] Testnet HF19 schedule
- [x] Native GitHub Actions CI + sanitizer job
- [x] `.clang-format` / `.clang-tidy`
- [x] CMake ≥ 3.16, C++20 (`-fno-char8_t` bridge)
- [x] Green curated unit suite (**628** tests)
- [x] Restore legacy fixtures (base58/uri/parse_amount/sha256/mul_div/fee/…)
- [x] Hardfork version + serialization basic unit coverage
- [x] ArqMQ dual-backend facade (transport remains SNNetwork)
- [x] RPC validation / pagination / DoS caps / restricted SN-key RPCs
- [x] P2P limit aliases + packet-budget unit coverage
- [x] Messaging envelope + onion/swarm helpers
- [x] Linux + macOS native unit CI; Windows/macOS/Linux depends binaries
- [x] Platform matrix documentation (`docs/PLATFORM.md`)

## Milestone B — MQ / SN hygiene

- [x] `arqmq` facade under Arqma naming
- [x] Command ACL registry + authorize semantics
- [x] Dedicated transport internals under Arqma naming (`arqmq::SocketStack`)
- [x] Feature-flag dual-stack (`--arqnet-backend`)
- [x] SN hygiene audit notes
- [x] Operator upgrade path docs

Remaining before default flip (not blocking Milestone B; preserves compatibility):

- [x] Dual-run coexistence: SocketStack + SNNetwork mesh (`mesh=snnetwork` always today)
- [x] Deny-path unit coverage for unknown Curve peers (`arqnet_auth`)
- [x] HF20 scaffold (`HF_VERSION_NATIVE_ARQNET_MESH`; stagenet 240 / testnet 1300 / mainnet 4 000 000)
- [x] Native mesh Stage A/B (Curve/ZAP allow + `PeerTable` on SocketStack)
- [x] Native mesh Stage C send path (CURVE `send_to_peer` unit-tested)
- [x] Native mesh Stage C daemon flag + RPC shadow counters (`--arqnet-mesh-shadow`)
- [x] Live/shadow parity telemetry + dual-write unit coverage
- [x] Soak monitor script (`utils/arqnet-mesh-soak-monitor.py`)
- [x] Cutover relay scaffold (HF-gated; stage 4)
- [x] Soak inbound vote_ob wire parse + RPC parse counters
- [x] Soak inbound pulse_rnd wire parse + RPC counters (observability; not in `sample_ok`)
- [x] Native CURVE ping→pong reply path on SocketStack
- [x] Cutover gate (`k_native_mesh_port_stage = 4`); live mesh still SNNetwork without `arqmq`+CURVE+HF20
- [x] Mainnet HF20 at **4 000 000** (compatible v19 until then)
- [x] HF21 at **5 000 000** (exclusive SN) + Pulse Milestone C start (`get_pulse_status`)
- [ ] Re-verify stagenet soak; keep Pulse hybrid PoW (no stage-3 PoW-off); default backend flip later

## Milestone C — product fork-in-the-road

**Requires human product decision.** Choose one primary value-add:

1. Storage swarm replication (HTTP KV + TTL + inbox `/v1/snodes` fan-out + membership merge/push is in-tree)
2. Blink-like fast confirmation (collector/RPC in-tree; not a Pulse/PoW replacement)
3. Pulse-like consensus change (hybrid producer started; PoW stays)
4. Privacy routing daemon (HTTP companion is in-tree: `arqma-router`)

See [`docs/PRODUCT.md`](PRODUCT.md). Full epidemic swarm gossip and Session-class
clients remain follow-ups (`docs/PR_COMPLETENESS_GATE.md`).

## Explicit non-goals (near term)

- Rebranding to Oxen/Session/Lokinet
- SESH / L2 / BLS tokenomics copy
- Wallet3 wholesale import without API plan
- Co-authored commit trailers / unnecessary contributor metadata
- Enabling broken legacy `core_tests` in default CI before API migration
