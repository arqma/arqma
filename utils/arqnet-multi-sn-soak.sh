#!/usr/bin/env bash
# Copyright (c) 2018 - 2026, The Arqma Network
#
# Operator helper: poll ≥2 stagenet SN RPC endpoints for mesh-shadow parity.
# Does not start daemons — point at already-running soak SNs.
#
# Example:
#   utils/arqnet-multi-sn-soak.sh 10.0.0.1:39994 10.0.0.2:39994
#   utils/arqnet-multi-sn-soak.sh --once sn1:39994 sn2:39994
#   MIN_OK_MINUTES=120 utils/arqnet-multi-sn-soak.sh sn1:39994 sn2:39994

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MONITOR="${ROOT}/utils/arqnet-mesh-soak-monitor.py"
INTERVAL="${INTERVAL:-30}"
MIN_OK_MINUTES="${MIN_OK_MINUTES:-0}"
ONCE=0
RPCS=()

usage() {
  cat <<EOF
Usage: $(basename "$0") [--once] [--interval SEC] [--min-ok-minutes N] host:rpc [host:rpc ...]

Polls get_arqnet_status + get_pulse_status on each SN. Exit 0 when every listed
node reports mesh_shadow_parity_sample_ok (and optional min-ok window).

Environment:
  INTERVAL          poll interval seconds (default 30)
  MIN_OK_MINUTES    hold sample_ok this many minutes before exit 0 (default 0 = run forever unless --once)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --once)
      ONCE=1
      shift
      ;;
    --interval)
      INTERVAL="${2:?}"
      shift 2
      ;;
    --min-ok-minutes)
      MIN_OK_MINUTES="${2:?}"
      shift 2
      ;;
    --)
      shift
      RPCS+=("$@")
      break
      ;;
    -*)
      echo "unknown flag: $1" >&2
      usage >&2
      exit 1
      ;;
    *)
      RPCS+=("$1")
      shift
      ;;
  esac
done

if [[ ${#RPCS[@]} -lt 1 ]]; then
  usage >&2
  exit 1
fi

if [[ ${#RPCS[@]} -lt 2 ]]; then
  echo "warning: multi-SN soak expects ≥2 RPC endpoints (got ${#RPCS[@]})" >&2
fi

ARGS=(--interval "${INTERVAL}" --require-all --min-ok-minutes "${MIN_OK_MINUTES}")
if [[ "${ONCE}" -eq 1 ]]; then
  ARGS=(--interval "${INTERVAL}" --require-all --once)
fi

exec python3 "${MONITOR}" "${ARGS[@]}" "${RPCS[@]}"
