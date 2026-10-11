# Arqma product

This repository ships the **consensus daemon and SN stack** (RandomARQ + hybrid
Pulse, Arq-Net mesh, RPC). Companion processes for storage and privacy routing
live in the same tree for operator probes; they stay **out of `arqmad`** (see
[`PROCESS_BOUNDARIES.md`](PROCESS_BOUNDARIES.md)).

Primary deliverable of the upgrade branch: **HF20 hybrid SN / HF21 exclusive
mesh**, with Pulse as a **hybrid producer** (PoW stays required). Messenger CLI
and local UI are **secondary probes**, not a Session-class client.

## Binaries

| Binary | Role |
|--------|------|
| `arqmad` | Consensus daemon (RandomARQ + hybrid Pulse, Arq-Net, RPC) |
| `arqma-wallet-rpc` / `arqma-wallet-cli` | Wallet processes |
| `arqma-storage` | Optional HTTP KV + TTL + anti-entropy (operator probe) |
| `arqma-router` | Optional privacy-router peel/store (operator probe) |
| `arqma-msg` | Minimal CLI probe over storage/router (`gen` / `send` / `inbox` / `open`) |

Blink lives **in-process** as `src/arq_blink` (quorum sign/verify + collector).
`get_blink_status` / `print_blink` report that collector plus `wire_connected`
and `mesh_blink_tx_*` soak counters. `blink_tx` mesh wire is connected for
observability; it does **not** replace Pulse or RandomARQ.

## Operator probes (secondary)

```text
utils/arqma-stack.sh                 # Linux / macOS — starts storage + router
utils/arqma-stack.cmd                # Windows
arqmad --storage-client-url=http://127.0.0.1:22021
# get_storage_status / print_storage
# get_pulse_status / print_pulse
# get_blink_status / print_blink
# get_arqnet_status
```

`get_storage_status` / `print_storage` talk to `arqma-storage` (`GET /status`
JSON: peers, snodes, KV count, gossip counters).

Optional local messenger probe (not product UX focus):

```text
utils/arqma-msg-ui.py    # http://127.0.0.1:8787/
arqma-msg gen | send | inbox | open
```

How to test core upgrade path: [`docs/OPERATOR_UPGRADE.md`](OPERATOR_UPGRADE.md)
(unit suite, daemon probes; messenger stack is optional).

## Locks that stay

1. Hybrid PoW + Pulse. RandomARQ remains required. Pulse stage stays **2**.
2. Default `--arqnet-backend=legacy-arqnet`.
3. Storage and router stay out of the `arqmad` process.
4. No Oxen / Session / Lokinet branding or tokenomics copied in.
5. Session-class messenger UX remains out of scope for this upgrade.
