# Platform Support Matrix (`upgrade`)

Arqma on the `upgrade` branch is built and verified for **Linux**, **Windows**,
and **macOS**. This page is the operator/developer map of how each platform is
covered in CI and how to reproduce builds locally.

## Supported platforms

| Platform | Architecture | How validated | What you get |
|----------|--------------|---------------|--------------|
| **Linux** | x86_64 | Native unit CI (`ubuntu-24.04`) + depends cross | Daemon, wallet, unit suite, ASan |
| **Linux** | aarch64 / RPi | Depends (`aarch64-linux-gnu`, `NO_AES` RPi) | Release binaries via contrib/depends |
| **macOS** | arm64 (Apple Silicon) | Native unit CI (`macos-14`) + depends `arm64-apple-darwin` | Unit suite + release binaries |
| **macOS** | x86_64 | Depends `x86_64-apple-darwin` (clang-19 + lld) | Release binaries |
| **Windows** | x86_64 | Depends mingw (`x86_64-w64-mingw32`) | Release binaries (`.exe`) |

Native MSVC unit CI on `windows-latest` is deferred until Boost/OpenSSL packages
are pinned; Windows product binaries are the depends/mingw artifacts.

## CI workflows

| Workflow | File | Jobs |
|----------|------|------|
| Unit / format / sanitize | `.github/workflows/ci.yml` | Linux Release+Debug unit, macOS-14 unit, ASan (`-DSANITIZE=ON` = address), clang-format, Windows depends gate |
| Cross binaries | `.github/workflows/depends.yml` | Windows x64, Linux x86_64, Linux armv8 (+ RPi), macOS x64, macOS arm64 |

Default test CMake flag: `-DBUILD_INTEGRATION_TESTS=OFF` (curated `unit_tests`
only). Legacy `core_tests` remain available with `-DBUILD_INTEGRATION_TESTS=ON`
after API migration.

## Local developer environment (match CI)

Install the same packages as `.github/workflows/ci.yml` (unit job) before
configure. Do **not** commit `build/` or binaries.

### Linux (native — `ubuntu-24.04` unit CI)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config ccache ninja-build \
  libboost-all-dev libssl-dev libzmq3-dev libunbound-dev \
  libsodium-dev libreadline-dev libhidapi-dev libusb-1.0-0-dev \
  libprotobuf-dev protobuf-compiler libgtest-dev
```

```bash
cmake -S . -B build/upgrade-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_INTEGRATION_TESTS=OFF \
  -DUSE_CCACHE=ON
cmake --build build/upgrade-release --parallel \
  --target unit_tests hash-target-tests daemon simplewallet wallet_rpc_server \
           arqma_storage arqma_router arqma_msg
ctest --test-dir build/upgrade-release -R 'unit_tests|hash-target' --output-on-failure
```

ASan (sanitize job): add `-DSANITIZE=ON` on a Debug tree (`build-san` in CI).

### macOS (native — `macos-14` unit CI)

```bash
brew update
brew install cmake ninja ccache pkg-config boost openssl@3 \
  zeromq unbound libsodium readline hidapi protobuf googletest
```

```bash
cmake -S . -B build/upgrade-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_INTEGRATION_TESTS=OFF \
  -DUSE_CCACHE=ON \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix);$(brew --prefix openssl@3)"
cmake --build build/upgrade-release --parallel --target unit_tests hash-target-tests
ctest --test-dir build/upgrade-release -R 'unit_tests|hash-target' --output-on-failure
```

macOS **release product** binaries (static/depends tree) also use
`make depends target=arm64-apple-darwin` or `x86_64-apple-darwin` (see below).

### Windows

| Path | When to use |
|------|-------------|
| **Depends / mingw** (`x86_64-w64-mingw32`) | Supported product path — same as `build-depends-windows-x64` in `.github/workflows/depends.yml` |
| **Native MSVC** | Not in CI yet (Boost/OpenSSL pin deferred). Experimental local only; prefer depends artifacts |

From a Linux (or WSL) host with mingw tools:

```bash
# CI also runs: update-alternatives …-posix for mingw gcc/g++
make depends target=x86_64-w64-mingw32
# artifacts: build/x86_64-w64-mingw32/release/bin/arqma*.exe
```

Companion stack on Windows: `utils/arqma-stack.cmd` then `arqma-msg.exe …`.

## Local verification (flags shared with CI)

Native unit flags (identical to `ci.yml` Configure step):

`-DCMAKE_BUILD_TYPE=Release|Debug` · `-DBUILD_TESTS=ON` ·
`-DBUILD_INTEGRATION_TESTS=OFF` · optional `-DUSE_CCACHE=ON`

Build targets used by CI: `unit_tests` `hash-target-tests`. Operator verify also
builds `daemon` `simplewallet` `wallet_rpc_server` `arqma_storage` `arqma_router`
`arqma_msg`.

### Cross binaries (Linux host → Windows / macOS / Linux targets)

Same host triplets as `.github/workflows/depends.yml`:

```bash
make depends target=x86_64-w64-mingw32          # Windows
make depends target=x86_64-apple-darwin         # macOS Intel (needs clang-19 + lld-19)
make depends target=arm64-apple-darwin          # macOS Apple Silicon
make depends target=x86_64-unknown-linux-gnu    # Linux x64
make depends target=aarch64-linux-gnu           # Linux ARM
# RPi: same aarch64 host with cmake_opts -DNO_AES=ON (see depends.yml matrix)
```

macOS cross from Linux requires LLVM 19 tools (`clang-19`, `lld-19`) as in
`.github/workflows/depends.yml`. Use CI artifacts when `contrib/depends` is too
heavy on the local machine.

## Functional surface (this PR)

Working end-to-end on the platforms above:

- Consensus / wallet HF19 (CLSAG, burns, fees)
- Daemon + wallet RPC with validation, DoS soft-caps, restricted-RPC gates
- Arq-Net SN-only auth, ping observability; ArqMQ facade + optional SocketStack
- **Mainnet:** keep `--arqnet-backend=legacy-arqnet` (default). Peer mesh stays
  `snnetwork`. Experimental `arqmq` is refused on mainnet without
  `--arqnet-allow-experimental`.
- Storage client TCP/HTTP GET probe; messaging envelope / onion / swarm helpers
- Optional operator probes: `arqma-storage`, `arqma-router`, `arqma-msg` (IPv6 listen,
  1 MiB HTTP body cap, socket timeouts on Windows and POSIX; router multi-hop
  onion forward, max 3; storage swarm membership merge/push, cap 32)
- Experimental `arq_router` lifecycle (scaffold; not a Lokinet product)

See `docs/UPGRADE_ROADMAP.md` and `docs/OPERATOR_UPGRADE.md` for mainnet locks.

## Documentation index

| Doc | Purpose |
|-----|---------|
| `docs/UPGRADE_ROADMAP.md` | Phase / milestone map |
| `docs/PR_COMPLETENESS_GATE.md` | What this PR claims vs defers |
| `docs/OPERATOR_UPGRADE.md` | Operator knobs and upgrade steps |
| `docs/BINARIES.md` | Launch commands / flags for every shipped binary |
| `docs/MIGRATION.md` | Developer and SN migration notes |
| `docs/ARCHITECTURE.md` | Module layout |
| `docs/PROCESS_BOUNDARIES.md` | Where MQ / storage / router live |
| `docs/openapi/arqmad-rpc.openapi.yaml` | RPC OpenAPI stub |
| `CHANGELOG-upgrade.md` | User-visible change log |
| `CONTINUATION_STATE.md` | Agent / resume handoff |

## Known gaps (documented, not blockers for foundation merge)

1. Legacy Monero-era `core_tests` / some unit fixtures need API migration.
2. Native Windows MSVC unit job not yet in CI (Windows product binaries are mingw depends).
3. Full Storage swarm gossip protocol and Session-class clients remain follow-ups.
   Companion binaries, KV TTL, and inbox swarm fan-out are already in-tree.
4. Multi-SN stagenet soak needs a live quorum (steps in `docs/OPERATOR_UPGRADE.md`).
5. Cross builds: use CI `depends.yml` artifacts when `contrib/depends` is too heavy locally.

Release packaging (binary list, Linux deps, soak how-to): `docs/OPERATOR_UPGRADE.md`
and `docs/RELEASE_NOTES_upgrade_A.md`.
