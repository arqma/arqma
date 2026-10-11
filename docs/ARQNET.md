# Arq-Net

Arq-Net is Arqma’s service-node messaging mesh. It carries quorum-oriented
traffic (for example obligation votes and reachability) using ZeroMQ with
Curve25519 authentication.

## What Arq-Net is

- In-process / co-located SN messaging component of `arqmad`
- ZMQ ROUTER/worker design (`src/arqnet/sn_network.*`)
- Compact bt-style serialization (`src/arqnet/bt_serialize.*`)
- Quorum connection matrices (`src/arqnet/conn_matrix.h`)
- Daemon glue in `src/cryptonote_protocol/arqnet.cpp`

Default listen port: **19996** (`cryptonote_config.h`).

## What Arq-Net is not

- Not Lokinet / LLARP
- Not a general-purpose onion router
- Not a replacement for Tor/I2P anonymity zones used by ordinary P2P/RPC

Future Lokinet-inspired routing, if pursued, must ship under Arqma naming and
documentation, without Lokinet branding.

## Authentication policy

Incoming Curve25519 identities are resolved against the service node list:

- Known SN pubkey → accepted as service node
- Unknown identity → **denied**

Unauthenticated client access to the mesh is intentionally closed.

## Operator ping

JSON-RPC method `arqnet_ping` records the last successful ping timestamp
(`last_arqnet_ping` in `get_info`). Storage Server ping remains a hard
requirement for uptime proofs. Arq-Net ping is currently **observability only**
and will become a hard gate only after a coordinated operator release.

Minimum advertised component version: `MIN_ARQNET_VERSION` `{1,0,0}`.
