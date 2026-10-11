# Binary launch guide (`upgrade`)

All commands assume binaries are on `PATH` or under `build/upgrade-release/bin/`
(Linux/macOS). On Windows depends builds, append `.exe` and use `\` paths.

Default datadir: `~/.arqma` (Linux/macOS) / `%USERPROFILE%\.arqma` (Windows).

Mainnet locks (do not flip in this release):

- `--arqnet-backend=legacy-arqnet` (default)
- Mainnet `--arqnet-backend=arqmq` needs `--arqnet-allow-experimental`
- Pulse stays hybrid (RandomARQ still required)

---

## Core

### `arqmad` — full node / service node daemon

```bash
# Mainnet full node (interactive console)
arqmad

# Detached / service-style
arqmad --detach --pidfile /var/run/arqmad.pid --non-interactive

# Custom datadir + config
arqmad --data-dir /var/lib/arqma --config-file /etc/arqma/arqma.conf

# Stagenet soak (experimental mesh dual-write; ≥2 SNs)
arqmad --stagenet --arqnet-backend=arqmq --arqnet-mesh-shadow

# With local companion stack probes
arqmad --storage-client-url=http://127.0.0.1:22021 --arq-router
```

Useful RPC (unrestricted): `get_info`, `get_arqnet_status`, `get_pulse_status`,
`get_blink_status`, `get_storage_status`. Console: `print_pulse`, `print_blink`, `print_storage`.

| Common flag | Purpose |
|-------------|---------|
| `--testnet` / `--stagenet` | Alternate chains |
| `--rpc-bind-ip` / `--rpc-bind-port` | Daemon JSON-RPC (default mainnet RPC **19994**) |
| `--confirm-external-bind` | Required if RPC binds outside localhost |
| `--arqnet-backend=legacy-arqnet\|arqmq` | Messaging backend |
| `--arqnet-allow-experimental` | Permit `arqmq` on mainnet |
| `--arqnet-mesh-shadow` | Shadow dual-write onto SocketStack |
| `--storage-client-url=` | Probe `arqma-storage` reachability |
| `--arq-router` | Hold experimental router lifecycle in-daemon |

---

### `arqma-wallet-cli` — interactive wallet

```bash
# New wallet (talks to local daemon)
arqma-wallet-cli --generate-new-wallet ~/wallets/mywallet --trusted-daemon

# Open existing wallet
arqma-wallet-cli --wallet-file ~/wallets/mywallet --trusted-daemon

# Remote daemon
arqma-wallet-cli --wallet-file ~/wallets/mywallet \
  --daemon-address node.example:19994

# Stagenet
arqma-wallet-cli --stagenet --generate-new-wallet ~/wallets/stage \
  --daemon-address 127.0.0.1:39994 --trusted-daemon
```

Restore: `--restore-deterministic-wallet` (Electrum-style seed; optional `--electrum-seed`).  
Must match daemon network flags (`--stagenet` / `--testnet`).

---

### `arqma-wallet-rpc` — wallet JSON-RPC server

```bash
arqma-wallet-rpc \
  --wallet-file ~/wallets/mywallet \
  --password-file ~/wallets/mywallet.pass \
  --rpc-bind-port 19991 \
  --daemon-address 127.0.0.1:19994 \
  --trusted-daemon \
  --disable-rpc-login
```

| Flag | Purpose |
|------|---------|
| `--wallet-file` / `--wallet-dir` | Single wallet or directory mode |
| `--rpc-bind-ip` / `--rpc-bind-port` | Wallet RPC listen (confirm external bind) |
| `--restricted-rpc` | View-oriented surface |
| `--daemon-address` | `arqmad` RPC endpoint |

---

## Optional operator probes (storage / router / msg)

Secondary to `arqmad` + wallets. Prefer the helper (writes env + starts both servers):

```bash
export ARQMA_BIN_DIR="$(pwd)/build/upgrade-release/bin"   # stack default is build/upgrade-test/bin
utils/arqma-stack.sh          # Linux/macOS → writes $ARQMA_STACK_DIR/env
# Windows: utils/arqma-stack.cmd
source "${ARQMA_STACK_DIR:-/tmp/arqma-stack}/env"
```

### `arqma-storage` — HTTP KV / inbox / swarm

```bash
arqma-storage --listen 127.0.0.1:22021 --data-dir ~/.arqma/storage

# With stack token + replica peer
arqma-storage --listen 127.0.0.1:22021 --data-dir ~/.arqma/storage \
  --token "$ARQMA_STACK_TOKEN" \
  --peer http://127.0.0.1:22022
```

| Flag | Default | Purpose |
|------|---------|---------|
| `--listen` | `127.0.0.1:22021` | `host:port` or `[ipv6]:port` |
| `--data-dir` | (memory only) | On-disk KV / snodes volume |
| `--token` | unset / `ARQMA_STACK_TOKEN` | Protects mutating routes |
| `--peer` | — | Replica base URL (repeatable) |

Daemon probe: `arqmad --storage-client-url=http://127.0.0.1:22021`.

### `arqma-router` — onion peel / store forward

```bash
arqma-router --listen 127.0.0.1:1090 --data-dir ~/.arqma/arq-router \
  --storage-url http://127.0.0.1:22021 \
  --token "$ARQMA_STACK_TOKEN"
```

| Flag | Default | Purpose |
|------|---------|---------|
| `--listen` | `127.0.0.1:1090` | HTTP status / peel API |
| `--data-dir` | `arq-router` | Router state directory |
| `--storage-url` | — | Last-hop `arqma-storage` base URL |
| `--token` | unset / `ARQMA_STACK_TOKEN` | Stack auth |

HTTP: `POST /v1/peel`, `POST /v1/store`.

### `arqma-msg` — minimal CLI probe (not Session-class UX)

```bash
# After utils/arqma-stack.sh (uses stack env automatically)
arqma-msg gen
arqma-msg send <64-hex-pubkey> hello
arqma-msg send alice=<64-hex> hello   # remembers ~/.arqma/msg/contacts
arqma-msg inbox
arqma-msg open

# Explicit endpoints
arqma-msg --url http://127.0.0.1:22021 --router http://127.0.0.1:1090 gen
arqma-msg --url http://127.0.0.1:22021 send <hex> "hello"

# Optional local UI shell (probe only): utils/arqma-msg-ui.py → http://127.0.0.1:8787/
```

| Subcommand | Purpose |
|------------|---------|
| `gen` | Create identity (`~/.arqma/msg/identity`); prints pubkey |
| `send` | Send plaintext to pubkey or remembered name |
| `inbox` | List inbox keys for local identity |
| `open` | Decrypt / open messages |
| `swarm` | List / announce swarm membership |

---

## Blockchain utilities

All need a datadir that already contains a blockchain DB (usually after `arqmad` sync).

### `arqma-blockchain-export`

```bash
arqma-blockchain-export --data-dir ~/.arqma --output-file ~/arqma-blockchain.raw
# optional: --block-stop N  --blocksdat
```

### `arqma-blockchain-import`

```bash
arqma-blockchain-import --data-dir ~/.arqma --input-file ~/arqma-blockchain.raw
# useful: --batch 1 --batch-size 20000
# skip checks only if you fully trust the file: --no-verify 1
```

### `arqma-blockchain-stats`

```bash
arqma-blockchain-stats --data-dir ~/.arqma \
  --block-start 0 --with-inputs --with-outputs --with-ringsize --with-hours
```

### `arqma-blockchain-usage`

```bash
arqma-blockchain-usage --data-dir ~/.arqma
# optional: --rct-only
```

### `arqma-blockchain-ancestry`

```bash
arqma-blockchain-ancestry --data-dir ~/.arqma --txid <64-hex>
arqma-blockchain-ancestry --data-dir ~/.arqma --height 1000000
# optional: --output amount/offset  --refresh
#           --cache-outputs --cache-txes --cache-blocks
#           --include-coinbase --show-cache-stats
```

### `arqma-blockchain-depth`

```bash
arqma-blockchain-depth --data-dir ~/.arqma --txid <64-hex>
arqma-blockchain-depth --data-dir ~/.arqma --height 1000000 --include-coinbase
```

### `arqma-blockchain-mark-spent-outputs`

```bash
arqma-blockchain-mark-spent-outputs \
  --inputs ~/.arqma \
  --spent-output-db-dir ~/.arqma/shared-ringdb
# optional: --rct-only  --export spent.list  --verbose
```

---

## TLS helper

### `arqma-generate-ssl-certificate`

```bash
arqma-generate-ssl-certificate \
  --certificate-filename ~/.arqma/rpc_cert.pem \
  --private-key-filename ~/.arqma/rpc_key.pem \
  --prompt-for-passphrase
```

Point daemon / wallet RPC at the PEM files via their SSL options
(`--rpc-ssl-certificate`, `--rpc-ssl-private-key`, … — see `--help`).

---

## Test binaries (not operator release)

```bash
build/upgrade-release/tests/unit_tests/unit_tests
ctest --test-dir build/upgrade-release -R 'unit_tests|hash-target' --output-on-failure
```

Expect **628** unit tests + `hash-target` pass.

---

## Typical local topology

```text
arqmad         :19994  ←── arqma-wallet-cli / arqma-wallet-rpc  (primary)
               └── --storage-client-url → storage (optional probe)
arqma-storage  :22021  ←── get_storage_status / print_storage / arqma-msg
arqma-router   :1090   ←── arqma-msg --router (optional onion probe)
```

See also: [`OPERATOR_UPGRADE.md`](OPERATOR_UPGRADE.md), [`PLATFORM.md`](PLATFORM.md),
`utils/arqma-stack.sh`.
