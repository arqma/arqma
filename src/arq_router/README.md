# Arq Router

`src/arq_router` is the in-repo privacy-router companion (`arqma-router`).

It is **not** Lokinet. The process stays separate from `arqmad` (see
`docs/PROCESS_BOUNDARIES.md`).

Current HTTP surface:

- `GET /` / `GET /status` — liveness
- `GET /v1/pubkey` — hop x25519 (hex)
- `POST /v1/peel` — peel one sealed onion layer
- `POST /v1/store?ns=&key=&ttl=&fwd=` — peel one layer, then either forward the
  leftover onion (`ARQH` frame) to the next hop or `PUT` into `--storage-url`

`fwd` counts hops already taken (max 3). Intermediate hops do not need
`--storage-url`; the last hop does.

`arqma-msg send --router` may be repeated (outermost first, max 3).
