# Developer Guide (Upgrade)

## Quick start

```bash
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R unit_tests
```

Makefile helpers:

- `make release-test` — Release build with tests
- `make debug-test` — Debug build with tests
- `make release-asan` / `make debug-asan` — sanitizer builds

## Standards

- Language: **C++20**, C11 (`-fno-char8_t` until epee `u8""` call sites migrate)
- CMake ≥ **3.16**
- Format: `.clang-format`
- Static analysis baseline: `.clang-tidy`

## NETWORK_ID (build-time)

P2P `NETWORK_ID` is produced every build
(`cmake/GenNetworkId.cmake` → `${CMAKE_BINARY_DIR}/network_id_generated.h`):

- **Mainnet** → historical fixed UUID (same as `master`). Tip/dirty state must
  never split live peers.
- **Testnet / stagenet** → commit-scoped seed (Zano-style dirty detection):
  - Clean tree → `git rev-parse HEAD`
  - Dirty (diff and/or untracked) →
    `<commit>-dirty-<sha256(diff+untracked)>`
  - Bytes: `SHA256("arqma-network-id|<net>|<seed>")[0..15]`

Daemon logs the seed at P2P init. Commit before sharing testnet/stagenet
binaries meant to peer with other clean tip builds.

## Branch workflow

1. Work on feature branches from `upgrade` (or `master` once merged).
2. Keep commits small and purposeful.
3. Do not add `Co-Authored-By` trailers.
4. Preserve copyright/license headers when adapting external code.
5. Update `CHANGELOG-upgrade.md` for user-visible changes.

## Tests expected for new modules

- Unit tests under `tests/unit_tests`
- Logging for failure paths
- Configuration / CLI flags documented
- Security notes if networking or crypto is touched

## Key documents

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [UPGRADE_ROADMAP.md](UPGRADE_ROADMAP.md)
- [SECURITY.md](SECURITY.md)
- [ARQNET.md](ARQNET.md)
- [ARQMQ.md](ARQMQ.md)
- [MIGRATION.md](MIGRATION.md)
