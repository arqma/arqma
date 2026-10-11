# P2P Hardening Recommendations

This document ties near-term hardening work to current constants in
`src/cryptonote_config.h` so upgrades stay grounded in the existing daemon
behavior.

## Implemented in upgrade branch

- Named aliases for connection totals, rate ceilings and packet budgets.
- Compile-time assertions aligning documented budgets with wire ceilings.
- `P2P_PREAUTH_PACKET_MAX_SIZE` documenting the Levin initial (pre-auth) budget.
- Runtime truncation of oversized handshake peerlists to `P2P_MAX_PEERS_IN_HANDSHAKE`.
- Unit coverage in `tests/unit_tests/p2p_limits.cpp`.

## Current limits worth revisiting

- `P2P_DEFAULT_PACKET_MAX_SIZE` is `50000000` bytes: large packets increase
  memory pressure and DoS amplification risk.
- `P2P_DEFAULT_PEERS_IN_HANDSHAKE` and `P2P_MAX_PEERS_IN_HANDSHAKE` are `250`:
  large peer lists help bootstrap but expand parsing and peer-poisoning surface.
- `P2P_DEFAULT_CONNECTION_TIMEOUT` is `5000` ms and
  `P2P_DEFAULT_PING_CONNECTION_TIMEOUT` is `2000` ms: tune carefully to avoid
  abuse without harming slow links.
- `P2P_IP_FAILS_BEFORE_BLOCK` is `10` and `P2P_IP_BLOCKTIME` is `60*60*24`:
  useful baseline for abuse throttling, but block policies should distinguish
  malformed traffic from transient failures.
- `BLOCKS_SYNCHRONIZING_MAX_COUNT` is `2048` and
  `COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT` is `1000`: these bounds should
  stay aligned with sync and RPC resource ceilings.
- `CRYPTONOTE_MAX_TX_SIZE` is `1000000` and `CRYPTONOTE_MAX_FRAGMENTS` is `20`:
  input fragmentation and oversized transaction handling should be validated
  early.

## DoS resistance

1. Lower effective packet ceilings for untrusted pre-auth traffic, even if the
   full protocol maximum remains higher internally.
2. Apply stricter parsing budgets to handshake, peerlist and ping paths.
3. Rate-limit repeated failures per IP and per logical peer role before work is
   dispatched deeper into the daemon.
4. Prefer bounded allocations and incremental decoding for network packets.

## Peer selection

1. Bias toward proven-good peers instead of treating graylist candidates as
   equal during initial selection.
2. Keep anchor peers limited and periodically reevaluate them.
3. Randomize within safe bounds to reduce eclipse susceptibility.
4. Separate service-node-oriented reachability heuristics from generic peer
   discovery logic where possible.

## Packet and sync handling

1. Reject obviously oversized packet claims before full buffering.
2. Add per-command payload ceilings instead of relying only on one global packet
   limit.
3. Audit block-sync batch sizes to ensure worst-case CPU and memory remain
   bounded.
4. Ensure fragment count validation happens before expensive deserialization.

## Recommended next implementation steps

1. Introduce smaller pre-auth command-specific size caps.
2. Add metrics/logging around packet drops, handshake rejects and peer bans.
3. Review peerlist import/export paths for poisoning resistance.
4. Add focused tests for oversized packet rejection and repeated-failure bans.
