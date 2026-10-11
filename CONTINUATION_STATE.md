# CONTINUATION_STATE

Branch: `upgrade` · Remote: `origin/upgrade` · Authorship: ArqTras only  
Hooks: `git -c core.hooksPath=/tmp/empty-git-hooks`  
Author: ArqTras `<33489188+ArqTras@users.noreply.github.com>`

## Tip

- Stage 4 mesh gate on; HF20 **4 000 000** hybrid SN; HF21 **5 000 000** exclusive
- Pulse Milestone C: hybrid producer; majority certificate extra bound to miner
  payload hash; weight-neutral miner extra; round bound to block timestamp
- PR notes: GitHub PR https://github.com/ArqTras/arqma/pull/3 (EN + PL)
- Release prep (Linux native): `build/upgrade-release` Release + `BUILD_TESTS=ON`
  → **628** unit tests + hash-target green; companion `stop()` accept/join hang fixed
- Local env docs: `docs/PLATFORM.md` lists Linux apt / macOS brew / Windows mingw
  packages matching `ci.yml` + `depends.yml` (3-OS configure+build+unit_tests)

## Mainnet locks until 4 000 000 (do **not** flip)

- Default `--arqnet-backend=legacy-arqnet`
- Mainnet `arqmq` needs `--arqnet-allow-experimental`
- `k_native_mesh_port_stage = 4`
- Mainnet HF20=**4 000 000**, HF21=**5 000 000**
- `k_pulse_pow_stage = 2` (hybrid; do **not** bump to 3 / PoW-off)
- v19 wire / miner RandomARQ

## SN operating modes

| HF | Heights (mainnet) | Mode | Mesh | Block production |
|----|-------------------|------|------|------------------|
| 19 | now → 3 999 999 | legacy | SNNetwork | RandomARQ |
| 20 | 4 000 000 → 4 999 999 | hybrid | native if `arqmq`+CURVE else SNNetwork | RandomARQ + Pulse signatures via `pulse_rnd` (PoW still required) |
| 21 | ≥ 5 000 000 | exclusive (mesh) | native intended; SNNetwork fallback if no CURVE | RandomARQ **+** Pulse (hybrid; PoW not dropped) |

## Local release build (this VM)

Config: `/workspace/build/upgrade-release` · Release · gcc/g++ · `BUILD_TESTS=ON`

Binaries in `build/upgrade-release/bin/` (do **not** commit).
Launch examples for every binary: [`docs/BINARIES.md`](docs/BINARIES.md).

| Binary | Role |
|--------|------|
| `arqmad` | Daemon |
| `arqma-wallet-cli` / `arqma-wallet-rpc` | Wallets |
| `arqma-storage` / `arqma-router` / `arqma-msg` | Optional operator probes |
| `arqma-blockchain-*` | Import/export/stats/usage/depth/ancestry/mark-spent |
| `arqma-generate-ssl-certificate` | TLS helper |

Linux system deps (native link): full apt list in `docs/PLATFORM.md` (matches
`ci.yml`: Boost, OpenSSL, libzmq, libsodium, unbound, readline, hidapi, usb,
protobuf, gtest, …). macOS: Homebrew list in PLATFORM.md. Windows product:
`make depends target=x86_64-w64-mingw32` (MSVC unit CI deferred). Cross
Windows/macOS/Linux arm: `make depends` / `.github/workflows/depends.yml`
(too heavy for this VM — use CI artifacts).

`gen_multisig` stays commented out in `src/CMakeLists.txt`.

## Next

- [x] Local loopback mesh/Pulse soak (operator may re-run live stagenet later)
- [x] Shadow `pulse_rnd` parse telemetry on `get_arqnet_status` / soak monitor
- [x] Pulse extra only after majority; honest `get_pulse_status.signature_count`
- [x] Canonical Pulse extra vote order (`validator_index` strictly increasing)
- [x] Fixed-size majority certificate extra + round bound to block wait-window
- [x] Pulse extra payload binding (quorum signs miner template hash; idle votes stay round-only)
- [x] Weight-neutral Pulse extra on miner templates + honest `get_pulse_status` certificate fields
- [x] Daemon `print_pulse` + Pulse fields on block headers
- [x] In-repo companions: `arqma-storage`, `arqma-router`, `arqma-msg` + Blink collector/`get_blink_status`
- [x] Storage `--data-dir` volume + router `POST /v1/peel` + `arqma-msg inbox`
- [x] Storage `--peer` replica fan-out + router `POST /v1/store` + `arqma-msg open`
- [x] Storage TTL (`?ttl=`) + inbox swarm fan-out (`/v1/snodes` members) + `GET /v1/swarm`
- [x] Cross-platform companion HTTP (IPv6 listen, 1 MiB body cap, socket timeouts, Windows stack.cmd)
- [x] Inbox swarm read fallback (`retrieve` / `list_keys` try `/v1/snodes` members)
- [x] Multi-hop onion forward (`ARQH` frame, `fwd` cap, repeatable `--router`)
- [x] Swarm membership merge + push to listed members (`arqma-msg swarm`)
- [x] Simple messenger defaults (stack env file; `send`/`inbox`/`open` without extra flags)
- [x] Local messenger identity (`gen` saves `~/.arqma/msg/identity`; `inbox`/`open` use it)
- [x] Everyday `send <hex> hello` (positional) + remembered names (`name=hex` → `~/.arqma/msg/contacts`)
- [x] Mainnet HF20 prep: weight-neutral Pulse splice, stack token, opaque inbox, honest Blink/UBSan docs, CLSAG operator checklist
- [x] Unified daemon restricted-RPC catalog (`rpc_auth.h` drives `MAP_*_IF` + handler denials); storage quota per namespace
- [x] Fix companion `StorageServer` / `RouterServer` `stop()` accept/join hang (self-connect wakeup)
- [x] Local Release verify: 628 unit + hash-target; binary inventory + packaging docs
- [x] Document 3-OS local env (Linux apt / macOS brew / Windows mingw) aligned with CI
- [x] Binary launch guide (`docs/BINARIES.md`) for every shipped binary
- [x] Messenger UI styled like Arqma-GUI-MM (black + muted gold from screenshot) (`utils/arqma-msg-ui.py`)
- [x] Storage anti-entropy gossip (`/v1/digest`, `/v1/sync`, `--gossip-interval`)
- [x] Blink Arq-Net wire (`blink_tx`) + mesh shadow handlers
- [x] Blink mesh RPC telemetry (`mesh_blink_tx_*` on `get_arqnet_status` / `get_blink_status`)
- [x] Storage epidemic snode membership gossip (`GET /v1/snodes` catalog + peer merge)
- [x] Storage `/status` + `get_storage_status` gossip telemetry
- [x] Blink `blink_tx` mesh shadow unit tests (parse ok / parse fail)
- [x] Daemon `print_storage` + soak monitor storage gossip line + counter asserts
- [x] Docs: demote companion/messenger UX narrative (PRODUCT / OPERATOR / BINARIES)
- [x] Operator surface polish: OpenAPI Blink schema, soak `get_blink_status` +
  `gossip_interval_sec`, daemon help for storage/print_*, CHANGELOG/gate tone
- [x] README: upgrade-branch overview, locks, docs index, clone/build verify path
- [ ] Keep Pulse hybrid (do **not** flip `k_pulse_pow_stage` to 3 / PoW-off)
- [ ] Default `--arqnet-backend` flip (later; not required at HF20)
- [ ] Multi-SN stagenet soak (needs live quorum; see `docs/OPERATOR_UPGRADE.md`)

## Quality

- Local `unit_tests` → **628** passed; `ctest -R 'unit_tests|hash-target'` green
- Test steps: `docs/OPERATOR_UPGRADE.md` (unit / daemon probes / optional messenger / network soak)
- Primary upgrade story: HF20/HF21 + Pulse hybrid + Arq-Net mesh; companions are probes
- Release readiness: code + unit gate ready; network soak deferred to operators with SN quorum
