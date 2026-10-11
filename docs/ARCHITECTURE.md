# Arqma Architecture Overview

This document describes the current architecture of the Arqma codebase on the
`upgrade` branch and how it maps to upstream Monero / Oxen-inspired subsystems.

## Identity

Arqma remains Arqma: public API, CLI, configuration keys, ports, branding and
operator-facing terminology use Arqma names. Design patterns may be adapted from
Monero, Oxen, Lokinet and Session, but the product must never read as an Oxen
rebrand.

## High-level layers

```
┌─────────────────────────────────────────────────────────────┐
│ Clients: simplewallet / wallet-rpc / operator tooling       │
├─────────────────────────────────────────────────────────────┤
│ RPC: HTTP JSON-RPC (19994) · ZMQ daemon RPC (19995)         │
├──────────────────────┬──────────────────────────────────────┤
│ cryptonote_core      │ Service Nodes · quorums · swarms     │
│ blockchain · tx_pool │ uptime proofs · obligation votes     │
├──────────────────────┼──────────────────────────────────────┤
│ P2P Levin (19993)    │ Arq-Net ZMQ mesh (19996)             │
│ Tor/I2P optional     │ Curve25519 SN auth · bt_serialize    │
├──────────────────────┴──────────────────────────────────────┤
│ blockchain_db (LMDB) · ringct/CLSAG · RandomARQ · crypto    │
└─────────────────────────────────────────────────────────────┘
```

## Modules (`src/`)

| Module | Role |
|--------|------|
| `crypto` / `ringct` | Ed25519, RandomARQ, MLSAG/CLSAG, Bulletproofs |
| `cryptonote_basic` | Blocks, tx, hardfork schedule |
| `cryptonote_core` | Chain state, tx pool, service node logic |
| `cryptonote_protocol` | Sync, Dandelion++, Arq-Net glue |
| `p2p` / `net` | Levin overlay, Tor/I2P address types |
| `arqnet` | SN mesh over ZMQ + Curve (not Lokinet) |
| `rpc` | HTTP + ZMQ daemon interfaces |
| `wallet` / `simplewallet` | Wallet logic and CLI |
| `blockchain_db` / `lmdb` | LMDB storage |
| `serialization` | Binary/JSON archives alongside epee KV |

## Consensus / hard forks

Software version: **10.0.0**. Highest protocol version: **network_version_19**.

HF19 enables:

- CLSAG ring signatures (`HF_VERSION_CLSAG`)
- amount burn (`HF_VERSION_BURN`)
- per-output fee (`HF_VERSION_PER_OUTPUT_FEE`)

Mainnet height for v19: **1886030** (2026-02-14 UTC). Testnet and stagenet
include matching schedule entries for local/testing parity.

## Arq-Net vs Lokinet

**Arq-Net** is an Arqma service-node messaging mesh (ZMQ, Curve25519, bt-style
serialization, quorum vote relay). It is **not** Lokinet/LLARP onion routing.

Future privacy-network work inspired by Lokinet must keep Arqma naming
(e.g. routing daemon, config keys, docs) and must not introduce Lokinet branding.

## Serialization stacks

The tree currently uses four serialization approaches:

1. epee portable_storage / `KV_SERIALIZE` (RPC + Levin)
2. `src/serialization` archives
3. Boost.Serialization (peerlist / some wallet paths)
4. Arq-Net `bt_serialize`

Long-term goal: shrink Boost.Serialization surface and migrate SN messaging to
a maintained MQ layer while preserving wire compatibility during transition.

## Security boundaries

- Default RPC bind is localhost; remote bind requires explicit confirmation.
- Arq-Net accepts authenticated **service nodes only** (unknown Curve keys denied).
- Storage Server ping is required for SN uptime proofs.
- Arq-Net ping is recorded for operators but is not yet a hard uptime gate.

## Related documents

- [UPGRADE_ROADMAP.md](UPGRADE_ROADMAP.md)
- [PRODUCT.md](PRODUCT.md)
- [PROCESS_BOUNDARIES.md](PROCESS_BOUNDARIES.md)
- [SECURITY.md](SECURITY.md)
- [ARQNET.md](ARQNET.md)
- [MIGRATION.md](MIGRATION.md)
