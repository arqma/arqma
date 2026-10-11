# Operator Upgrade Guide (`upgrade` branch)

## Mainnet policy (production)

This branch is prepared for **mainnet** operators. Compatibility locks:

1. **Default** `--arqnet-backend=legacy-arqnet` — do not change on mainnet SNs.
2. **Peer mesh** (`vote_ob` / quorum Curve+ZMQ) stays on `arqnet::SNNetwork` unless
   **HF20+** and this node started `--arqnet-backend=arqmq` with CURVE identity
   (`native_mesh_live_at`). Mainnet HF20 is at height **4 000 000**, so until then
   the chain stays v19 and live mesh stays SNNetwork.
3. `--arqnet-backend=arqmq` is **refused on mainnet** unless you also pass
   `--arqnet-allow-experimental` (not recommended for production service nodes).
4. On testnet/stagenet, `arqmq` may be used to exercise the dedicated `SocketStack`.
5. Stagenet soak (shadow dual-write; live mesh SNNetwork until HF20 on `arqmq` nodes):

```text
arqmad --stagenet --arqnet-backend=arqmq --arqnet-mesh-shadow
```

   Watch `get_arqnet_status`:

   | Field | Meaning |
   |-------|---------|
   | `mesh_live_relays` / `mesh_vote_ob_live` | SNNetwork relays observed |
   | `mesh_vote_ob_shadow_ok` / `*_fail` | SocketStack dual-write results |
   | `mesh_vote_ob_shadow_in` | inbound shadow `vote_ob` frames (receivers) |
   | `mesh_vote_ob_shadow_parse_ok` / `*_parse_fail` | inbound payloads that decode as obligation-vote wire |
   | `mesh_pulse_rnd_live` / `mesh_pulse_rnd_shadow_*` | Pulse `pulse_rnd` dual-write + inbound parse (observability; not in `sample_ok`) |
   | `mesh_blink_tx_live` / `mesh_blink_tx_shadow_*` | Blink `blink_tx` dual-write + inbound parse (observability; not in `sample_ok`) |
   | `mesh_shadow_ok_rate_bps` | overall shadow success (0–10000) |
   | `mesh_shadow_parity_sample_ok` | ≥32 live `vote_ob` + ≥95% shadow send ok **and** inbound parse ok |
   | `native_mesh_ready` / `native_mesh_blocker` | implementation gate (stage 4 → `none`) |
   | `native_mesh_hf_permits` / `hard_fork_version` | HF20+ permit on this chain |
   | `mesh` | **live** quorum carrier (`arqmq` only at HF20+ with CURVE `arqmq` stack) |

   Soak re-check (implementation gate is already 4; confirm on a live SN quorum):

   1. ≥2 stagenet SNs run `arqmq` + mesh-shadow (legacy-only peers inflate fails).
   2. Open firewall for **ANET port + 10000** (CURVE listener). Before HF20, live
      mesh stays on ANET; after HF20, `arqmq` nodes send quorum on ANET+10000.
   3. `mesh_shadow_endpoint` populated; watch `mesh_vote_ob_shadow_in` **and**
      `mesh_vote_ob_shadow_parse_ok` on receivers (`parse_fail` must stay near 0).
      After HF20, also watch `mesh_pulse_rnd_shadow_in` / `mesh_pulse_rnd_shadow_parse_ok`
      (`pulse_parse_fail` near 0). `mesh_shadow_parity_sample_ok` still keys off `vote_ob`.
   4. `mesh_shadow_parity_sample_ok == true` across a multi-hour quorum window
      (requires both outbound send success and inbound vote-wire parse).
   5. No consensus / uptime regressions vs SNNetwork-only control nodes.

   Implementation cutover is already flipped:

```text
# src/arqmq/mesh_bridge.cpp
constexpr int k_native_mesh_port_stage = 4;
```

   Live `primary_mesh_send_to_peer` still requires HF20+ **and** `--arqnet-backend=arqmq`
   with CURVE keys. Do **not** change mainnet default `--arqnet-backend` in this release.

   Do **not** enable shadow on mainnet production SNs.

   Monitor soak progress (stdlib Python):

```text
utils/arqnet-mesh-soak-monitor.py 127.0.0.1:39994
utils/arqnet-mesh-soak-monitor.py 127.0.0.1:39994 --once   # exit 0 when sample_ok
# Prints mesh-shadow parity plus Pulse round / signature_count (unrestricted RPC).
```

### HF20 / HF21 (native mesh + hybrid then exclusive SN)

| Network | HF20 | HF21 | Role |
|---------|------|------|------|
| Stagenet | 240 | 260 | Hybrid then exclusive (short window) |
| Testnet | 1300 | 1400 | Intermediate |
| Mainnet | **4 000 000** | **5 000 000** | v19 until 4M; hybrid POSPOW 4M–5M; exclusive after 5M |

- **Until 4 000 000:** major version 19, SNNetwork, miner PoW. Full compatibility with current nodes.
- **HF20 (hybrid):** miner RandomARQ **and** Pulse SN rounds are both permitted. Native mesh is live on `--arqnet-backend=arqmq` nodes; others stay SNNetwork.
- **HF21 (exclusive mesh):** intended native mesh for operators. Block production stays **hybrid**: miner RandomARQ **and** Pulse SN rounds. `get_pulse_status.pow_replacement_ready` stays false — Pulse does not replace PoW. From HF20, active SNs in the Pulse quorum sign and gossip `pulse_rnd` (idle loop + miner template). Miner extra is a **majority certificate** (exactly 7 of 11, lowest validator slots, increasing index) bound to the miner payload hash (timestamp, included txs, miner vout[0]); idle `pulse_rnd` votes may omit the payload. Extra `round` must match the parent→block 15s wait-window. If a round has no majority after **15 seconds**, the leader rotates (`round` 0–7, covering one 120s block target).

Until mainnet height 4 000 000, upgraded daemons still produce **major version 19**
blocks (they only vote 20 in `minor_version`). Pre-HF20 peers accept those blocks.
At 4 000 000, upgraded nodes require major version 20 — operators must upgrade
before that height. At 5 000 000 they must be ready for exclusive **mesh** (native
Arq-Net intent). Missing CURVE still falls back to SNNetwork; HF21 is not a
Pulse-only / PoW-off fork.

Wallets must be CLSAG-ready: from major **20**, MLSAG / Bulletproof2 transfers are
rejected. Upgrade `arqma-wallet-cli` / `arqma-wallet-rpc` with the daemon.

### Mainnet HF20 checklist (height 4 000 000)

1. Install this daemon **before** 4 000 000. Old v19 daemons reject v20 blocks.
2. Ship CLSAG wallets with the daemon. Pre-CLSAG spend txs will not confirm after the fork.
3. Keep `--arqnet-backend=legacy-arqnet` on production SNs.
4. Pulse stays **hybrid**: RandomARQ remains required. Do not expect PoS-only blocks.
5. `get_blink_status` is a local collector, not network pre-confirm.
6. Optional: `--arqnet-backend=arqmq` only with `--arqnet-allow-experimental` after soak.

Default `--arqnet-backend=legacy-arqnet` stays in this release. Pass `--arqnet-backend=arqmq`
before HF21 so exclusive mesh has a CURVE stack.

`get_arqnet_status` reports both:

| Field | Meaning |
|-------|---------|
| `transport` | Facade / native stack (`snnetwork` or `arqmq`) |
| `mesh` | Live peer quorum transport (`snnetwork` until HF20+ with a CURVE `arqmq` stack) |

## Before upgrading

1. Backup datadir (`~/.arqma` or custom `--data-dir`).
2. Confirm Storage Server is running and reachable (uptime proofs still require it).
3. Note current daemon version with `arqmad --version`.
4. Keep existing Arq-Net ports and SN keys; no live mesh protocol flip on mainnet
   until height **4 000 000**, and only on nodes that opt into `--arqnet-backend=arqmq`.

## Build / install notes

- Toolchain: **C++20**, CMake ≥ **3.16**
- Platforms: **Linux, macOS, Windows** — full package lists and commands in
  [`docs/PLATFORM.md`](PLATFORM.md) (mirrors `.github/workflows/ci.yml` /
  `depends.yml`).
- Recommended verify (native Linux/macOS; tree `build/upgrade-release` is fine):
  ```bash
  cmake -S . -B build/upgrade-release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_INTEGRATION_TESTS=OFF
  cmake --build build/upgrade-release --parallel \
    --target unit_tests hash-target-tests daemon simplewallet wallet_rpc_server \
             arqma_storage arqma_router arqma_msg
  ctest --test-dir build/upgrade-release -R 'unit_tests|hash-target' --output-on-failure
  ```
- Expect `[  PASSED  ] 628 tests.` plus `hash-target` via ctest.
- **Windows product binaries:** `make depends target=x86_64-w64-mingw32` (CI job
  `build-depends-windows-x64`). Native MSVC unit CI is deferred — see PLATFORM.md.
- **macOS product / cross:** native Homebrew unit build, or
  `make depends target=arm64-apple-darwin` / `x86_64-apple-darwin`.
- Do **not** commit `build/` or binaries.
- **NETWORK_ID:** mainnet keeps the historical fixed P2P UUID (upgrade nodes
  still peer with live mainnet). Testnet/stagenet IDs are commit-scoped; a dirty
  tree gets a distinct ID (daemon logs `NETWORK_ID seed=` at P2P init).

### Package inventory (ship from `bin/`)

Same names on depends artifacts (`.exe` on Windows):

| Binary | Required for |
|--------|----------------|
| `arqmad` | Service node / full node |
| `arqma-wallet-cli`, `arqma-wallet-rpc` | CLSAG wallets (upgrade before HF20) |
| `arqma-storage`, `arqma-router`, `arqma-msg` | Optional operator probes (`utils/arqma-stack`) |
| `arqma-blockchain-export`, `arqma-blockchain-import` | Datadir migration / backup |
| `arqma-blockchain-stats`, `arqma-blockchain-usage`, `arqma-blockchain-depth`, `arqma-blockchain-ancestry`, `arqma-blockchain-mark-spent-outputs` | Ops utilities |
| `arqma-generate-ssl-certificate` | Optional TLS helper |

**How to run each binary** (flags, examples, local topology):
[`docs/BINARIES.md`](BINARIES.md).

`gen_multisig` is intentionally not built (`src/CMakeLists.txt`).

**Debian/Ubuntu build packages (same as `ci.yml`):** `build-essential` `cmake`
`pkg-config` `ccache` `ninja-build` `libboost-all-dev` `libssl-dev` `libzmq3-dev`
`libunbound-dev` `libsodium-dev` `libreadline-dev` `libhidapi-dev`
`libusb-1.0-0-dev` `libprotobuf-dev` `protobuf-compiler` `libgtest-dev`.

**macOS Homebrew (same as `ci.yml`):** `cmake` `ninja` `ccache` `pkg-config`
`boost` `openssl@3` `zeromq` `unbound` `libsodium` `readline` `hidapi`
`protobuf` `googletest` (+ CMake `OPENSSL_ROOT_DIR` / `CMAKE_PREFIX_PATH`).

**Runtime shared libs (Linux native link of `arqmad`):** `libboost_*`, `libssl`/`libcrypto`,
`libzmq`, `libsodium`, `libunbound`, `libreadline`, `libstdc++`, plus transitive
(`libevent`, `libhidapi`, …). Prefer a depends-built tarball for self-contained trees.

**Cross / CI artifacts** (`.github/workflows/depends.yml`): Windows x64 mingw,
Linux x86_64, Linux aarch64 (+ RPi `NO_AES`), macOS x64, macOS arm64.

## How to test (before merge / before HF20)

Curated suite is **628** tests. Default CMake keeps `BUILD_INTEGRATION_TESTS=OFF`
(`core_tests` stay opt-in).

### 1. Unit tests (Linux / macOS)

```bash
cmake -S . -B build/upgrade-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_INTEGRATION_TESTS=OFF
cmake --build build/upgrade-release --parallel \
  --target unit_tests hash-target-tests daemon simplewallet wallet_rpc_server \
           arqma_storage arqma_router arqma_msg
build/upgrade-release/tests/unit_tests/unit_tests
```

Expect: `[  PASSED  ] 628 tests.`

Optional: `ctest --test-dir build/upgrade-release -R 'unit_tests|hash-target' --output-on-failure`

ASan (Linux CI equivalent): configure with `-DSANITIZE=ON` (address sanitizer only).

### 2. Daemon probes (core upgrade path)

```bash
build/upgrade-release/bin/arqmad --storage-client-url=http://127.0.0.1:22021 --arq-router
# unrestricted RPC / console:
# get_pulse_status / print_pulse  — hybrid, pow_replacement_ready=false
# get_blink_status / print_blink  — wire_connected + mesh_blink_tx_*
# get_storage_status / print_storage — gossip counters when storage is up
# get_arqnet_status — mesh=snnetwork on default backend
```

Keep `--arqnet-backend=legacy-arqnet` on mainnet SNs.

### 3. Optional messenger probe (not product UX focus)

Secondary smoke path for companions. Session-class UX is out of scope.

```bash
export ARQMA_BIN_DIR="$(pwd)/build/upgrade-release/bin"
utils/arqma-stack.sh          # writes $ARQMA_STACK_DIR/env including ARQMA_STACK_TOKEN
# other terminal:
$ARQMA_BIN_DIR/arqma-msg gen
PUB=$($ARQMA_BIN_DIR/arqma-msg gen | awk '{print $1}')
$ARQMA_BIN_DIR/arqma-msg send "$PUB" hello
$ARQMA_BIN_DIR/arqma-msg inbox
$ARQMA_BIN_DIR/arqma-msg open
# optional local UI shell: utils/arqma-msg-ui.py → http://127.0.0.1:8787/
```

Checks: `gen` prints **one** hex (pubkey). `inbox`/`open` work without `--to`/`--secret`.
`GET /status` on storage stays open without a token; `PUT /v1/kv` without token is 401.

Windows: `utils/arqma-stack.cmd` then `arqma-msg.exe gen` / `send <hex> hello` / `inbox` / `open`.

### 4. Network soak (multi-SN quorum — not available in a single VM)

Operators with ≥2 stagenet SNs should run the soak before relying on `arqmq` mesh:

```text
# On each soak SN (≥2 nodes; open ANET and ANET+10000):
arqmad --stagenet --arqnet-backend=arqmq --arqnet-mesh-shadow

# Poll one SN:
utils/arqnet-mesh-soak-monitor.py <sn-ip>:39994
utils/arqnet-mesh-soak-monitor.py <sn-ip>:39994 --once

# Poll the whole soak set (required for Milestone C multi-SN gate):
utils/arqnet-multi-sn-soak.sh <sn1-ip>:39994 <sn2-ip>:39994
utils/arqnet-multi-sn-soak.sh --once <sn1-ip>:39994 <sn2-ip>:39994
MIN_OK_MINUTES=120 utils/arqnet-multi-sn-soak.sh <sn1-ip>:39994 <sn2-ip>:39994
# Equivalent:
utils/arqnet-mesh-soak-monitor.py --require-all --min-ok-minutes 120 \
  <sn1-ip>:39994 <sn2-ip>:39994
```

Pass criteria (multi-hour window):

1. `mesh_shadow_parity_sample_ok == true` on **every** participating soak SN.
2. Aggregate `vote_ob` / `pulse_rnd` shadow `parse_fail` near 0 on each node.
3. After stagenet HF20 (height 240): `get_pulse_status` → hybrid mode,
   `pow_replacement_ready=false`; `print_pulse` shows rising `signature_count`.
4. No consensus / uptime regressions vs SNNetwork-only control peers.

Mainnet production SNs stay on `--arqnet-backend=legacy-arqnet` until after soak
and an explicit operator decision (still needs `--arqnet-allow-experimental` for `arqmq`).

## New / restored operator knobs

| Flag / RPC | Meaning |
|------------|---------|
| `--arqnet-backend=legacy-arqnet\|arqmq` | Messaging selection (default `legacy-arqnet`; mainnet refuses `arqmq` without override) |
| `--arqnet-allow-experimental` | Permit `arqmq` / mesh-shadow on mainnet (dev/soak only; peer mesh still SNNetwork) |
| `--arqnet-mesh-shadow` | Dual-write peer commands onto SocketStack (needs `arqmq`; default off) |
| `--arq-router` | Hold experimental router lifecycle in the daemon (companion `arqma-router` is the HTTP process) |
| `--storage-client-url=<url>` | Outbound Storage Server reachability probe (`http://` GET / `https://` TCP) — point at `arqma-storage` |
| `arqnet_ping` | Records Arq-Net reachability (not yet a hard uptime gate) |
| `get_arqnet_status` | `backend`, `transport`, `mesh`, shadow counters (`vote_ob` + `pulse_rnd` + `blink_tx`), `native_mesh_*`, `sn_operating_mode`, `pulse_*` |
| `get_pulse_status` | Hybrid/exclusive SN mode, PoW gate, Pulse `round` / leader / quorum / collector `signature_count` / `majority_ok` / `certificate_ready` / `payload_hash` / `local_signature_ready` |
| `print_pulse` | Daemon console dump of `get_pulse_status` |
| `get_blink_status` / `print_blink` | In-daemon Blink quorum (7 of 10) + `wire_connected` + `mesh_blink_tx_*`. Does not replace Pulse or RandomARQ |
| `get_block_header_*` | Optional `pulse_certificate`, `pulse_round`, `pulse_signature_count`, `pulse_payload_hash` when miner extra has Pulse |
| `get_storage_status` / `print_storage` | Storage client reachability + `GET /status` telemetry (`gossip_interval_sec`, `peer_count`, `snode_count`, `kv_entries`, `gossip_rounds`, `digest_ok` / `sync_ok` / `membership_ok` / `gossip_fail`) |
| `get_service_nodes` `offset`/`limit` | Optional pagination |

Optional companion probes (same repo, separate PIDs — [`docs/PRODUCT.md`](PRODUCT.md)).
Not the primary upgrade deliverable; use for storage/router smoke tests:

```text
utils/arqma-stack.sh
utils/arqma-stack.cmd
arqmad --storage-client-url=http://127.0.0.1:22021 --arq-router
arqma-msg gen | send <hex> hello | inbox | open
```

The stack writes `$ARQMA_STACK_DIR/env` so `arqma-msg` needs no `--url` / `--router`.
`gen` saves `~/.arqma/msg/identity`; `inbox` and `open` use it.
`send bob=<hex> hello` remembers the name in `~/.arqma/msg/contacts`.
The stack token is in `$ARQMA_STACK_DIR/env`.

Operator binaries if you are not using the stack:

```text
arqma-storage --listen 127.0.0.1:22021 --data-dir ~/.arqma/storage --peer http://127.0.0.1:22022
arqma-router --listen 127.0.0.1:1090 --data-dir ~/.arqma/arq-router --storage-url http://127.0.0.1:22021
```

`arqma-storage` honors `PUT /v1/kv?ttl=` (max 14 days). Inbox keys also copy to
URLs listed in `PUT /v1/snodes?pubkey=`. `GET /v1/swarm?pubkey=` returns swarm id
plus those members. `arqma-msg` get / inbox / open also read those members when
the node in `--url` has no local copy. Listen on IPv6 as `[::1]:22021`. HTTP
bodies are capped at 1 MiB on Linux, macOS, and Windows. Repeat `--router`
(outermost first, max 3); intermediate hops forward leftover onion, the last hop
needs `--storage-url`. `PUT /v1/snodes` merges unique HTTP member URLs (cap 32)
and pushes the list onto those members. `arqma-msg swarm` lists or announces
membership. Anti-entropy repair: `GET /v1/digest?ns=` + `POST /v1/sync?ns=` and `arqma-storage --gossip-interval=15` (0 disables). Still not a full Oxen swarm.
Probe gossip health via `GET http://127.0.0.1:22021/status` or daemon `get_storage_status` / `print_storage` (`peer_count`, `gossip_rounds`, `digest_ok`, `sync_ok`, `membership_ok`, `gossip_fail`). Blink mesh soak: watch `mesh_blink_tx_shadow_parse_ok` / `*_parse_fail` on `get_blink_status` / `get_arqnet_status` (observability only).

## Compatibility

- HF19 burn construction now allowed at HF ≥ 19 (core/wallet aligned).
- Arq-Net accepts only registered service-node Curve keys.
- Missing Arq-Net pings do **not** currently block uptime proofs.
- Mainnet quorum wire path stays SNNetwork until HF20 on `arqmq` nodes; HF21 is exclusive native mesh when CURVE is configured.

## Rollback

Revert to the previous release binaries and restore datadir backup if required.
Prefer forward fixes once HF19 burn transactions exist on-network.
