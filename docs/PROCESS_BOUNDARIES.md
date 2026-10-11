# Process vs Separate Binary Boundaries

Decision record for upgrade-module placement. Companion binaries live **in this
repository** ([`docs/PRODUCT.md`](PRODUCT.md)); they are still separate PIDs.

## Keep in-process (`arqmad`)

| Component | Why |
|-----------|-----|
| `arqmq` facade + ACL registry + framing limits | Thin control plane over existing SNNetwork |
| `arq_storage::StorageClient` | Client stubs / RPC status / reachability probes only |
| RPC validation / pagination / DoS caps / `rpc_auth` | Daemon request path |
| Messaging envelope / sealed-box / onion peel helpers | Shared crypto primitives used by `arqma-msg` |
| `arq_blink` quorum + `get_blink_status` | Instant-tx signatures; does not replace Pulse or RandomARQ |
| Wallet-rpc validation + restricted auth | Operator-facing wallet process (`arqma-wallet-rpc`) |

## Separate binary / service (same repo)

| Component | Binary | Why |
|-----------|--------|-----|
| Storage Server | `arqma-storage` | Replication/IO isolation; HTTP KV + TTL + swarm membership merge/push |
| Privacy router | `arqma-router` | Onion peel + optional multi-hop forward; separate from consensus |
| Messenger CLI probe | `arqma-msg` | Minimal smoke over storage + envelopes (not Session UX) |

`utils/arqma-stack.sh` (Linux/macOS) or `utils/arqma-stack.cmd` (Windows) starts
storage (`--data-dir`) + router (`--storage-url`).
Point `arqmad` at them with `--storage-client-url` and `--arq-router`.

## Reachability vs full APIs

| Probe | In-process meaning |
|-------|--------------------|
| Storage TCP / cleartext HTTP GET | Daemon can report `arqma-storage` liveness without embedding the server |
| Storage TLS | TCP connect only until a TLS client is deliberately linked |
| Arq-Net ping/pong | Mesh liveness; not a substitute for Storage Server |

## Rule of thumb

If a feature needs its own process supervision, storage volume, or upgrade cadence
independent of consensus, it stays out of the `arqmad` **process**. Facades and
status RPCs may live in-process as long as they do not pretend to be the full
service. The source and binaries still ship from this tree.
