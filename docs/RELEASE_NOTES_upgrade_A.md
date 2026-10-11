# Release notes — upgrade foundation (Milestone A → C prep)

## Highlights

- Correct HF19 burn construction so burns work from hard fork 19 onward.
- Harden Arq-Net: only registered service nodes may authenticate to the mesh.
- Restore `arqnet_ping` / `last_arqnet_ping` for operator visibility.
- Raise the build baseline to C++20 (with `-fno-char8_t` compatibility).
- Curated unit suite (**628** tests) green on Linux native Release verify
  (`ctest -R 'unit_tests|hash-target'`); macOS + ASan covered in CI.
- Cross-compile release binaries for **Windows x64**, **macOS x64/arm64**, and
  **Linux x64/arm** via `contrib/depends` / `.github/workflows/depends.yml`.
- Primary: Pulse hybrid producer + Arq-Net mesh telemetry; optional probes
  `arqma-storage` / `arqma-router` / `arqma-msg`; Blink collector (local only).
- RPC validation, DoS soft-caps, unified restricted-RPC catalog, OpenAPI stub.
- Companion HTTP servers shut down cleanly (accept/join wakeup on `stop()`).

## Platforms

| OS | Unit tests | Release binaries |
|----|------------|------------------|
| Linux | Native CI + ASan + local Release verify | depends x86_64 / armv8 |
| macOS | Native `macos-14` | depends x64 + arm64 (clang+lld) |
| Windows | Gate → mingw depends | depends `x86_64-w64-mingw32` |

Details: `docs/PLATFORM.md`. Binary list + Linux deps: `docs/OPERATOR_UPGRADE.md`.

## Release readiness

| Gate | Status |
|------|--------|
| Unit + hash-target (628) | Green on Linux Release |
| Mainnet locks (legacy backend, HF heights, Pulse hybrid, stage 4) | Unchanged |
| Multi-SN stagenet soak | **Operator follow-up** (needs live quorum) |
| depends cross builds | CI `depends.yml` (not run in this VM) |

Do **not** flip: default `--arqnet-backend`, `k_pulse_pow_stage` → PoW-off,
HF20/HF21 heights, or `k_native_mesh_port_stage`.

## Operator impact

- Storage Server ping remains required for uptime proofs.
- Missing Arq-Net pings do **not** yet block uptime proofs.
- Unknown remote Curve keys are rejected on the Arq-Net port.
- Restricted RPC denials return `CORE_RPC_ERROR_CODE_RESTRICTED` for operator
  and SN-key methods.
- New flags: `--arqnet-backend`, `--arq-router`, `--storage-client-url`,
  `--arqnet-mesh-shadow`, `--arqnet-allow-experimental`.
- Keep `--arqnet-backend=legacy-arqnet` on mainnet SNs until after soak.

## Developer impact

- CMake ≥ 3.16, C++20 compiler required.
- Default: `-DBUILD_INTEGRATION_TESTS=OFF` (curated `unit_tests`).
- Prefer `ctest -R 'unit_tests|hash-target'`.
- See `docs/MIGRATION.md`, `docs/UPGRADE_ROADMAP.md`, `docs/OPERATOR_UPGRADE.md`.

## Known limitations / deferred

- Legacy `core_tests` / some Monero-era fixtures remain behind
  `BUILD_INTEGRATION_TESTS=ON` until API migration.
- Multi-SN mesh/Pulse soak needs a live stagenet quorum (document steps in
  `docs/OPERATOR_UPGRADE.md`).
- Full Storage swarm gossip / Session-class clients remain follow-ups.
- Default `--arqnet-backend` stays `legacy-arqnet`; Pulse stays hybrid (PoW required).
- Native MSVC unit job deferred (Windows via mingw depends).
