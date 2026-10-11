# Performance Baseline (Upgrade)

## Policy

Optimize only with measurements. This document records baselines and safe
instrumentation points; it does not ship speculative micro-optimizations.

## Local measurement checklist

```bash
# Wall time to configure + build (Release)
/usr/bin/time -p cmake --build build/upgrade-release --parallel

# Unit suite runtime
/usr/bin/time -p ./build/upgrade-release/tests/unit_tests/unit_tests --gtest_brief=1

# Optional sanitizers (slower, correctness first)
cmake -S . -B build-asan -G Ninja -DSANITIZE=ON -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan --parallel
```

## Observed (developer workstation, upgrade branch)

| Metric | Approx |
|--------|--------|
| Curated unit suite | ~2.0–2.1 s for **628** tests (Release, Apple Silicon) |
| `unit_tests` link | seconds-scale after incremental rebuild |

Re-run and replace these numbers on CI hardware before claiming regressions.

## Hotspots already known (not changed here)

- `wallet2.cpp` monolith
- `db_lmdb.cpp` / blockchain path
- Levin sync `.inl` handlers
- Large P2P packet max (documented in `P2P_HARDENING.md`)

## Next measurement targets

1. Daemon cold start to P2P handshake
2. IBD sync rate on testnet/stagenet
3. `get_service_nodes` with pagination vs full dump
4. CLSAG verify batch cost vs MLSAG legacy path
