# Phases 3 to 12 Status

Branch: `upgrade`

Foundation engineering for phases 3–12 is landed. Pulse SN hybrid/exclusive
gates, local signatures, `pulse_rnd` quorum gossip, 15s wait-windows, payload-bound
  majority extra, and idle-loop
SN participation are started (Milestone C). Companion binaries for Storage,
router, and messaging, plus in-daemon Blink quorum helpers, ship from this
repository. Hybrid PoW stays on (no Pulse PoW-off).

## Status Matrix

| Phase | Status | Implemented now | Still required (Milestone C / later) |
|-------|--------|-----------------|--------------------------------------|
| 3 MQ feature parity | Foundation + native transport + dual-run | facade, ACL, `SocketStack`; live `mesh=arqmq` at HF20+ with CURVE; exclusive intent at HF21 | Default backend flip |
| 4 Arq-Net evolution | Foundation done | SN-only auth, ping, dual-stack, CURVE ping/pong, stage-4, HF20/21 | Stagenet soak re-check |
| 5 Storage Server | In-repo binary | `arqma-storage` HTTP KV + TTL + digest/sync anti-entropy + `--gossip-interval` + `--peer` + inbox swarm | Full epidemic membership protocol |
| 6 Messaging modules | In-repo CLI | identity, onion, swarm, envelope, `arqma-msg` (`gen` / `send <hex> hello` / `inbox` / `open` + stack env + token + opaque inbox) | Session-class client UX |
| 7 RPC modernization | Foundation done | validation, pagination, DoS caps, auth helpers, OpenAPI | Generated OpenAPI + full auth middleware wiring |
| 8 P2P improvements | Foundation done | limit aliases, compile-time budget checks, unit coverage | Measured rollout tuning |
| 9 Performance | Baseline done | `docs/PERFORMANCE.md` local timings | CI hardware baselines / IBD profiles |
| 10 Testing | Foundation done | curated suite green (Linux/macOS CI) | `core_tests` under `BUILD_INTEGRATION_TESTS`; remaining API-drift fixtures |
| 11 CI | Done (3 OS) | Linux/macOS unit, ASan, format; Windows/macOS/Linux depends | Optional TSan; native MSVC unit job |
| 12 Final review | Gate ready | security checklist + completeness gate + `PLATFORM.md` | soak / signed release / stagenet |

## Recommended next milestone outputs

1. Optional live stagenet SN soak re-run (local loopback already passed; Pulse `pulse_rnd` soak counters are on `get_arqnet_status`). Keep HF21 block production hybrid (RandomARQ + Pulse).
2. Migrate legacy `core_tests` incrementally under `BUILD_INTEGRATION_TESTS=ON`.
