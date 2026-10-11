# RPC Modernization Plan

## Goals

- tighten authentication for privileged daemon operations
- normalize request validation and error reporting
- add predictable pagination to large list endpoints
- publish a stable operator-facing OpenAPI description

## Authentication Plan

1. Keep read-only informational RPCs available without elevated access where
   safe.
2. Require authenticated access for administrative, service-node and future
   storage-routing operations.
3. Support a layered model:
   - loopback/local operator access
   - credential or token-based remote admin access
   - explicit role mapping for future module APIs such as storage and messaging
4. Log denied requests with enough metadata for operators without leaking
   sensitive material.

## Validation Plan

- reject unknown required-field omissions with structured errors
- validate numeric ranges before handler execution
- validate hashes, pubkeys and namespace identifiers with reusable helpers
- centralize pagination validation so limits/cursors are enforced consistently
- keep transport-level parse failures distinct from business-logic failures

### Soft batch caps (landed on `upgrade`)

| Endpoint | Cap constant | Limit |
|----------|--------------|-------|
| `get_transactions` | `max_tx_hashes_per_request` | 100 |
| `is_key_image_spent` | `max_key_images_per_request` | 1000 |
| `get_blocks_by_height` | `max_block_heights_per_request` | 100 |
| `get_block_header_by_hash` (batch `hashes`) | `max_block_hashes_per_request` | 100 |
| `get_block_headers_range` | `max_block_headers_range` | 1000 |

Oversized requests return a status / JSON-RPC error without executing the heavy
handler path. See `docs/ERROR_SEMANTICS.md`.

### Wallet RPC soft caps (`wallet_rpc_validation.h`)

| Surface | Cap | Limit |
|---------|-----|-------|
| transfer destinations | `max_transfer_destinations` | 100 |
| `get_bulk_payments` payment IDs | `max_payment_ids_per_request` | 100 |
| address-book index lists | `max_address_book_indices_per_request` | 1000 |
| subaddress index filters | `max_subaddr_indices_per_request` | 1000 |

Payment ID hex is validated with the shared daemon `validate_nonempty_hex`
helper (8 or 32 bytes). Oversized requests return
`WALLET_RPC_ERROR_CODE_TOO_MANY_ENTRIES` (-45).

### Wallet RPC restricted mode (`wallet_rpc_auth.h`)

`--restricted-rpc` denies spend, key-export, wallet-control and staking methods
listed in `method_requires_full_access`. Handlers call `deny_if_restricted`
for defense-in-depth; read helpers such as `get_balance` / `get_height` /
`validate_address` remain available.

### Daemon restricted mode (`rpc_auth.h`)

`k_operator_rpc_methods` is the single catalog for `--restricted-rpc`.
`MAP_*_IF` uses `allow_restricted_map`, and handlers call `deny_restricted_rpc`
for defense-in-depth. Public reads such as `get_info` / `get_height` stay mapped.

## Pagination Plan

Prefer cursor-based pagination for dynamic collections and bounded page sizes for
legacy list endpoints.

Recommended common fields:

- `limit`
- `cursor`
- `next_cursor`
- `has_more`

Recommended default rules:

- default `limit` per endpoint with explicit maximum
- deterministic sort order
- cursor encoding opaque to clients
- stable partial responses when a page boundary is reached

## OpenAPI Stub Outline

The initial stub should remain deliberately small and hand-maintained:

- `/json_rpc` with `get_info`
- `/storage_server/ping`
- `/arqnet/ping`

Future generated or expanded descriptions should add:

- reusable schemas for status/error envelopes
- auth scheme definitions
- tagged separation for daemon, service-node, storage and messaging endpoints
- examples for common operator flows

## Suggested implementation sequence

1. Introduce shared request/response validation helpers.
2. Add auth checks to the privileged RPC surface.
3. Add paginated wrappers for high-cardinality list endpoints.
4. Expand OpenAPI coverage and keep it reviewed with code changes.
