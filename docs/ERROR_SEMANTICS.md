# Shared Error Semantics (Upgrade Modules)

Cross-module upgrade components (`arqmq`, `arq_storage`, `arq_messaging`,
`arq_router`, RPC validation) should converge on the following categories.

## Categories

| Category | Meaning | Typical caller action |
|----------|---------|------------------------|
| `ok` | Success | Continue |
| `invalid_argument` | Malformed input (hex, size, range) | Fix request; do not retry blindly |
| `not_supported` | Feature/backend not available yet | Fall back or disable feature |
| `unavailable` | Dependency temporarily down | Retry with backoff |
| `denied` | Auth / ACL / peer policy rejected | Do not retry without credentials |
| `internal` | Unexpected failure | Log and alert |

## Mapping today

- RPC string `status` / JSON-RPC `error` remain the public transport.
- Batch DoS caps return additive status strings such as
  `Too many transaction hashes requested` (`invalid_argument`).
- Restricted daemon operator methods return JSON-RPC
  `CORE_RPC_ERROR_CODE_RESTRICTED` (-14) or a `Failed, restricted RPC…`
  status string (`denied`).
- Wallet-rpc restricted mode returns `WALLET_RPC_ERROR_CODE_DENIED` (-7);
  oversized batches return `WALLET_RPC_ERROR_CODE_TOO_MANY_ENTRIES` (-45).
- `arqmq::Backend::ArqMq` init failure maps to `not_supported`.
- Legacy Arq-Net peer rejection maps to `denied`.

## Rules

1. Prefer additive RPC status messages over breaking schema changes.
2. Do not leak secrets or full peer keys in operator-visible errors.
3. Keep category names stable when wiring future native ArqMQ / storage RPCs.
