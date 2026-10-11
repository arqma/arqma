# Service Node Hygiene Audit (Upgrade)

Audit notes for service-node related paths touched by the `upgrade` branch.
This is not a claim that Pulse/Blink or full Oxen SN economics are shipped.

## Auth / mesh

| Item | Status | Notes |
|------|--------|-------|
| Unknown Curve peers on Arq-Net | Hardened | Denied (not treated as `client`) |
| Registered SN Curve access | Current | Allowed for SN path |
| Arq-Net ping observability | Restored | `arqnet_ping` + `last_arqnet_ping` |
| Arq-Net ping as uptime-proof hard gate | Deferred | Disabled with operator docs |

## RPC / DoS

| Item | Status | Notes |
|------|--------|-------|
| `get_service_nodes` pagination | Landed | Optional `offset`/`limit` |
| SN pubkey hex validation | Landed | Via `rpc_validation` |
| Batch RPC soft caps | Landed | txs / key images / blocks / headers |

## Messaging facade

| Item | Status | Notes |
|------|--------|-------|
| `--arqnet-backend` | Landed | Falls back to LegacyArqNet |
| ACL privilege ordering helper | Landed | `arqmq::allows` |
| Native ArqMQ transport | Landed (flagged) | `arqmq::SocketStack`; mesh cutover still dual-run |

## Remaining hygiene work

1. Inventory every SN command category and map to ACL levels.
2. Add integration coverage for deny-path under simulated unknown Curve peer.
3. Document operator rollout before enabling ping hard-gate for proofs.
4. Separate Storage Server incentives from core SN registration rules.
