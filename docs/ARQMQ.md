# ArqMQ Migration Plan

## Goal

Evolve Arq-Net messaging toward a maintainable command/ACL/worker model inspired
by OxenMQ, while preserving Arqma naming, ports and operator UX.

## Current state

`src/arqnet` provides the live Curve/ZMQ peer mesh (`SNNetwork`) used for
quorum vote relay.

`src/arqmq` is the Arqma-named facade **and** dedicated socket stack:

- `--arqnet-backend=legacy-arqnet` (default) — facade only; `transport=snnetwork`
- `--arqnet-backend=arqmq` — starts `arqmq::SocketStack` (ZMQ workers + ACL);
  RPC/status reports `transport=arqmq`
- command ACL registry + framing limits shared by both paths
- peer quorum mesh (`vote_ob`) still rides SNNetwork until dual-run cutover

## Migration steps

1. ~~Extract stable facade headers from `SNNetwork` usage sites.~~
2. ~~Add feature flag `--arqnet-backend=legacy-arqnet|arqmq` (daemon arg).~~
3. ~~Dedicated ArqMQ socket/worker internals under Arqma naming (`SocketStack`).~~
4. ~~Dual-run coexistence with SNNetwork peer mesh (compatibility mode).~~
5. Make dedicated `arqmq` **mesh** the default after stagenet parity; keep SNNetwork one release.
6. Optionally require Arq-Net ping for uptime proofs after operator notice.

## Target architecture

```
arqmad
  └── arqnet facade (stable C++ API for cryptonote_core / protocol)
        └── transport backend
              ├── legacy SNNetwork (default during migration)
              └── arqmq SocketStack (feature-flagged; Milestone B landed)
```

Public names remain **Arq-Net** / **arqnet**. Internal library is `arqmq`.

## ACL categories

| Category | Who | Examples |
|----------|-----|----------|
| `Denied` | rejected | unknown Curve peers |
| `Basic` | local admin / loopback helpers | diagnostics |
| `ServiceNode` | registered SN keys | quorum votes, pings |
| `Admin` | local control socket | privileged ops |

`arqmq::allows(required, granted)` encodes the privilege order
`Denied < Basic < ServiceNode < Admin`.

Unknown remote Curve keys stay denied (already enforced on SNNetwork).

### Framing limits (`message_limits.hpp`)

Aligned with `SN_ZMQ_MAX_MSG_SIZE` (1 MiB):

| Limit | Value |
|-------|-------|
| `max_message_bytes` | 1048576 |
| `max_command_name_bytes` | 64 |
| `max_payload_frames` | 16 |

`authorize_request` combines framing checks with ACL authorize; Arq-Net
`vote_ob` / `ping` / `pong` handlers use it, and so does `SocketStack::dispatch`.

## Non-goals

- Renaming the product mesh to Lokinet/Oxennet
- Breaking Levin P2P
- Shipping Session branding
- Flipping the operator default before dual-run exit criteria

## Success criteria

- Existing quorum vote relay works unchanged for default operators
- `--arqnet-backend=arqmq` reports `transport=arqmq` with a live worker stack
- Unit tests cover ACL deny path and native ping dispatch
- Docs updated in `docs/ARQNET.md` / `docs/ARQNET_DUAL_STACK.md`
