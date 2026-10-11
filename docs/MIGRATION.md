# Migration Notes — Upgrade Branch

## For developers

1. Checkout `upgrade` and update submodules:
   ```bash
   git fetch origin
   git checkout upgrade
   git submodule update --init --recursive
   ```
2. Requires **CMake ≥ 3.16** and a **C++20** compiler.
3. Recommended local verification (Linux / macOS):
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DBUILD_TESTS=ON -DBUILD_INTEGRATION_TESTS=OFF
   cmake --build build --parallel --target unit_tests
   ctest --test-dir build -R unit_tests --output-on-failure
   ```
   Cross binaries (Windows / macOS / Linux targets): see `docs/PLATFORM.md`.
4. Formatting baseline: `.clang-format` (C++20). Do not mass-reformat unrelated
   files in feature PRs.

## For service node operators

- **Mainnet:** keep the default `--arqnet-backend=legacy-arqnet`. Do not enable
  experimental `arqmq` on production SNs (refused unless
  `--arqnet-allow-experimental`). Peer quorum mesh stays on SNNetwork.
- Continue running Storage Server; uptime proofs still require SS pings.
- `arqnet_ping` / `get_arqnet_status` (`transport` + `mesh`) are observability
  helpers. Missing Arq-Net pings do **not** currently block uptime proofs.
- Expect stronger Arq-Net peer authentication: only registered SN keys are
  accepted on the mesh port.

## For wallet developers

- HF19 enables CLSAG, burns and per-output fees.
- Burn construction is allowed at HF ≥ 19 in both wallet and core
  (`construct_tx` gate corrected on this branch).
- Testnet now includes `network_version_19` at height **1200** for testing.

## Compatibility

| Area | Impact |
|------|--------|
| Mainnet consensus rules | Unchanged schedule; burn gate bugfix aligns core with intended HF19 behaviour |
| P2P wire | No intentional breaking change in this milestone |
| RPC | Additive: `arqnet_ping`, `get_arqnet_status` (`backend`/`transport`/`mesh`), `get_storage_status`; restricted denials for operator/SN-key methods |
| Arq-Net mesh | Unchanged wire path on mainnet (`mesh=snnetwork`); experimental SocketStack opt-in only |
| Wallet cache | No format bump in this milestone |
| Build | C++20; Linux/macOS/Windows via native + depends (see `docs/PLATFORM.md`) |
| Platforms | Linux x64/arm, macOS x64/arm64, Windows x64 |

## Rollback

Reverting the `upgrade` branch commits restores previous burn rejection at
HF19 and previous Arq-Net accept-all client behaviour. Prefer forward fixes
over silent rollback once HF19 burn usage exists on network.
