#!/usr/bin/env bash
# Start companion processes for a local Arqma stack (same repo, separate PIDs).
set -euo pipefail

BIN_DIR="${ARQMA_BIN_DIR:-$(cd "$(dirname "$0")/.." && pwd)/build/upgrade-test/bin}"
STORAGE_LISTEN="${1:-127.0.0.1:22021}"
ROUTER_LISTEN="${2:-127.0.0.1:1090}"
STACK_DIR="${ARQMA_STACK_DIR:-${TMPDIR:-/tmp}/arqma-stack}"

if [[ ! -x "${BIN_DIR}/arqma-storage" || ! -x "${BIN_DIR}/arqma-router" ]]; then
  echo "Build companions first, e.g.:" >&2
  echo "  cmake --build build/upgrade-test --parallel --target arqma_storage arqma_router arqma_msg daemon" >&2
  exit 1
fi

mkdir -p "${STACK_DIR}/storage" "${STACK_DIR}/router"
TOKEN="${ARQMA_STACK_TOKEN:-$(openssl rand -hex 16 2>/dev/null || python3 -c 'import secrets; print(secrets.token_hex(16))')}"
export ARQMA_STACK_TOKEN="${TOKEN}"

cat > "${STACK_DIR}/env" <<EOF
ARQMA_STORAGE_URL=http://${STORAGE_LISTEN}
ARQMA_ROUTER_URL=http://${ROUTER_LISTEN}
ARQMA_STACK_TOKEN=${TOKEN}
EOF

"${BIN_DIR}/arqma-storage" --listen "${STORAGE_LISTEN}" --data-dir "${STACK_DIR}/storage" --token "${TOKEN}" &
STORAGE_PID=$!
"${BIN_DIR}/arqma-router" --listen "${ROUTER_LISTEN}" --data-dir "${STACK_DIR}/router" \
  --storage-url "http://${STORAGE_LISTEN}" --token "${TOKEN}" &
ROUTER_PID=$!

cleanup() {
  kill "${STORAGE_PID}" "${ROUTER_PID}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "arqma-storage pid=${STORAGE_PID} http://${STORAGE_LISTEN} data=${STACK_DIR}/storage"
echo "arqma-router  pid=${ROUTER_PID} http://${ROUTER_LISTEN} data=${STACK_DIR}/router"
echo "env           ${STACK_DIR}/env"
echo
echo "Messenger:"
echo "  ${BIN_DIR}/arqma-msg gen"
echo "  ${BIN_DIR}/arqma-msg send <hex> hello"
echo "  ${BIN_DIR}/arqma-msg inbox"
echo "  ${BIN_DIR}/arqma-msg open"
echo
echo "Daemon (separate process):"
echo "  ${BIN_DIR}/arqmad --storage-client-url=http://${STORAGE_LISTEN} --arq-router"
echo
wait
