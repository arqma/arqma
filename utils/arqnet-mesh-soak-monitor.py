#!/usr/bin/env python3
# Copyright (c) 2018 - 2026, The Arqma Network
#
# Poll one or more arqmad nodes for mesh-shadow soak parity + hybrid Pulse + storage gossip.
# Stdlib only (no third-party deps).
#
# Usage:
#   utils/arqnet-mesh-soak-monitor.py [host:rpc ...] [--interval SEC] [--once]
#   utils/arqnet-mesh-soak-monitor.py a:39994 b:39994 --require-all --once
#   utils/arqnet-mesh-soak-monitor.py a:39994 b:39994 --min-ok-minutes 120
#
# Exit codes:
#   0  success (--once all required nodes sample_ok, or --min-ok-minutes window met)
#   1  usage / RPC error
#   2  --once and parity sample not yet ok on a required node

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request


def rpc_call(url: str, method: str) -> dict:
    body = json.dumps({"jsonrpc": "2.0", "id": "0", "method": method, "params": {}}).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        payload = json.loads(resp.read().decode())
    if "error" in payload:
        raise RuntimeError(payload["error"])
    return payload.get("result") or {}


def fmt_pulse(result: dict) -> str:
    if not result:
        return "pulse=unavailable"
    return (
        f"pulse_mode={result.get('sn_operating_mode', '-')} "
        f"round={result.get('round', '-')} "
        f"lead={result.get('leader_index', '-')} "
        f"sigs={result.get('signature_count', 0)}/{result.get('majority_required', 0)} "
        f"maj={result.get('majority_ok')} "
        f"cert={result.get('certificate_ready')} "
        f"payload={(result.get('payload_hash') or '-')[:16]} "
        f"in_q={result.get('in_quorum')} "
        f"local={result.get('local_signature_ready')} "
        f"pow={result.get('pow_required')} "
        f"repl={result.get('pow_replacement_ready')} "
        f"blocker={result.get('pulse_blocker') or '-'}"
    )


def fmt_row(rpc: str, result: dict) -> str:
    return (
        f"node={rpc} "
        f"hf={result.get('hard_fork_version', '-')} "
        f"ready={result.get('native_mesh_ready')} "
        f"hf_ok={result.get('native_mesh_hf_permits')} "
        f"blocker={result.get('native_mesh_blocker') or '-'} "
        f"shadow={result.get('mesh_shadow')} "
        f"ep={result.get('mesh_shadow_endpoint') or '-'} "
        f"live_vote={result.get('mesh_vote_ob_live', 0)} "
        f"sh_ok={result.get('mesh_vote_ob_shadow_ok', 0)} "
        f"sh_fail={result.get('mesh_vote_ob_shadow_fail', 0)} "
        f"sh_in={result.get('mesh_vote_ob_shadow_in', 0)} "
        f"parse_ok={result.get('mesh_vote_ob_shadow_parse_ok', 0)} "
        f"parse_fail={result.get('mesh_vote_ob_shadow_parse_fail', 0)} "
        f"live_pulse={result.get('mesh_pulse_rnd_live', 0)} "
        f"pulse_ok={result.get('mesh_pulse_rnd_shadow_ok', 0)} "
        f"pulse_fail={result.get('mesh_pulse_rnd_shadow_fail', 0)} "
        f"pulse_in={result.get('mesh_pulse_rnd_shadow_in', 0)} "
        f"pulse_parse_ok={result.get('mesh_pulse_rnd_shadow_parse_ok', 0)} "
        f"pulse_parse_fail={result.get('mesh_pulse_rnd_shadow_parse_fail', 0)} "
        f"live_blink={result.get('mesh_blink_tx_live', 0)} "
        f"blink_ok={result.get('mesh_blink_tx_shadow_ok', 0)} "
        f"blink_fail={result.get('mesh_blink_tx_shadow_fail', 0)} "
        f"blink_in={result.get('mesh_blink_tx_shadow_in', 0)} "
        f"blink_parse_ok={result.get('mesh_blink_tx_shadow_parse_ok', 0)} "
        f"blink_parse_fail={result.get('mesh_blink_tx_shadow_parse_fail', 0)} "
        f"ok_bps={result.get('mesh_shadow_ok_rate_bps', 0)} "
        f"sample_ok={result.get('mesh_shadow_parity_sample_ok')}"
    )


def fmt_storage(result: dict) -> str:
    if not result:
        return "storage=unavailable"
    return (
        f"storage_reachable={result.get('client_reachable')} "
        f"service={result.get('service') or '-'} "
        f"peers={result.get('peer_count', 0)} "
        f"snodes={result.get('snode_count', 0)} "
        f"kv={result.get('kv_entries', 0)} "
        f"gossip_interval_sec={result.get('gossip_interval_sec', 0)} "
        f"gossip_rounds={result.get('gossip_rounds', 0)} "
        f"digest_ok={result.get('digest_ok', 0)} "
        f"sync_ok={result.get('sync_ok', 0)} "
        f"membership_ok={result.get('membership_ok', 0)} "
        f"gossip_fail={result.get('gossip_fail', 0)}"
    )


def fmt_blink(result: dict) -> str:
    if not result:
        return "blink=unavailable"
    return (
        f"blink_wire={result.get('wire_connected')} "
        f"sigs={result.get('signature_count', 0)}/{result.get('majority_required', 0)} "
        f"maj={result.get('majority_ok')} "
        f"quorum={result.get('quorum_size', 0)} "
        f"blocker={result.get('blink_blocker') or '-'} "
        f"live={result.get('mesh_blink_tx_live', 0)} "
        f"parse_ok={result.get('mesh_blink_tx_shadow_parse_ok', 0)} "
        f"parse_fail={result.get('mesh_blink_tx_shadow_parse_fail', 0)}"
    )


def poll_node(rpc: str) -> tuple[dict, dict, dict, dict, str | None]:
    url = f"http://{rpc}/json_rpc"
    try:
        arqnet = rpc_call(url, "get_arqnet_status")
    except (urllib.error.URLError, TimeoutError, RuntimeError, json.JSONDecodeError) as exc:
        return {}, {}, {}, {}, str(exc)
    try:
        pulse = rpc_call(url, "get_pulse_status")
    except (urllib.error.URLError, TimeoutError, RuntimeError, json.JSONDecodeError):
        pulse = {}
    try:
        storage = rpc_call(url, "get_storage_status")
    except (urllib.error.URLError, TimeoutError, RuntimeError, json.JSONDecodeError):
        storage = {}
    try:
        blink = rpc_call(url, "get_blink_status")
    except (urllib.error.URLError, TimeoutError, RuntimeError, json.JSONDecodeError):
        blink = {}
    return arqnet, pulse, storage, blink, None


def aggregate(rows: list[tuple[str, dict, dict, dict, dict, str | None]]) -> dict:
    reachable = [
        (rpc, arq, pulse, storage, blink)
        for rpc, arq, pulse, storage, blink, err in rows
        if err is None
    ]
    sample_ok = [rpc for rpc, arq, *_rest in reachable if arq.get("mesh_shadow_parity_sample_ok")]
    parse_fail = sum(int(arq.get("mesh_vote_ob_shadow_parse_fail", 0) or 0) for _, arq, *_ in reachable)
    pulse_parse_fail = sum(int(arq.get("mesh_pulse_rnd_shadow_parse_fail", 0) or 0) for _, arq, *_ in reachable)
    blink_parse_fail = sum(int(arq.get("mesh_blink_tx_shadow_parse_fail", 0) or 0) for _, arq, *_ in reachable)
    gossip_fail = sum(int((storage or {}).get("gossip_fail", 0) or 0) for _, _, _, storage, _ in reachable)
    return {
        "nodes": len(rows),
        "reachable": len(reachable),
        "sample_ok": len(sample_ok),
        "sample_ok_nodes": sample_ok,
        "all_sample_ok": bool(reachable) and len(sample_ok) == len(reachable),
        "parse_fail_sum": parse_fail,
        "pulse_parse_fail_sum": pulse_parse_fail,
        "blink_parse_fail_sum": blink_parse_fail,
        "gossip_fail_sum": gossip_fail,
        "errors": [(rpc, err) for rpc, *_, err in rows if err is not None],
    }


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Monitor Arq-Net mesh-shadow soak parity on one or more SN RPC endpoints"
    )
    ap.add_argument(
        "rpc",
        nargs="*",
        default=["127.0.0.1:39994"],
        help="arqmad RPC host:port list (default stagenet 39994)",
    )
    ap.add_argument("--interval", type=float, default=30.0, help="seconds between polls (default 30)")
    ap.add_argument("--once", action="store_true", help="single poll; exit 0 only if required nodes sample_ok")
    ap.add_argument(
        "--require-all",
        action="store_true",
        help="with multiple nodes, every reachable node must be sample_ok (default: all listed nodes)",
    )
    ap.add_argument(
        "--min-ok-minutes",
        type=float,
        default=0.0,
        help="keep polling until all required nodes stay sample_ok for this many minutes",
    )
    args = ap.parse_args()

    rpcs: list[str] = []
    for item in args.rpc:
        for part in item.split(","):
            part = part.strip()
            if not part:
                continue
            if ":" not in part:
                print(f"error: rpc must look like host:port ({part!r})", file=sys.stderr)
                return 1
            rpcs.append(part)
    if not rpcs:
        print("error: need at least one host:port", file=sys.stderr)
        return 1

    require_all = args.require_all or len(rpcs) > 1
    ok_since: float | None = None
    need_seconds = max(0.0, args.min_ok_minutes) * 60.0

    while True:
        rows: list[tuple[str, dict, dict, dict, dict, str | None]] = []
        for rpc in rpcs:
            arqnet, pulse, storage, blink, err = poll_node(rpc)
            rows.append((rpc, arqnet, pulse, storage, blink, err))
            if err:
                print(f"RPC error @ {rpc}: {err}", file=sys.stderr)
                continue
            print(fmt_row(rpc, arqnet), flush=True)
            print(f"node={rpc} {fmt_pulse(pulse)}", flush=True)
            print(f"node={rpc} {fmt_blink(blink)}", flush=True)
            print(f"node={rpc} {fmt_storage(storage)}", flush=True)

        summary = aggregate(rows)
        print(
            "aggregate "
            f"nodes={summary['nodes']} reachable={summary['reachable']} "
            f"sample_ok={summary['sample_ok']} all_ok={summary['all_sample_ok']} "
            f"vote_parse_fail_sum={summary['parse_fail_sum']} "
            f"pulse_parse_fail_sum={summary['pulse_parse_fail_sum']} "
            f"blink_parse_fail_sum={summary['blink_parse_fail_sum']} "
            f"gossip_fail_sum={summary['gossip_fail_sum']}",
            flush=True,
        )

        if summary["errors"] and (args.once or need_seconds > 0):
            # Hard fail on unreachable when an operator asked for a decisive window.
            if require_all or args.once:
                if args.once:
                    return 1
                ok_since = None

        quorum_ok = summary["all_sample_ok"] if require_all else summary["sample_ok"] > 0
        if not summary["reachable"]:
            quorum_ok = False

        if args.once:
            return 0 if quorum_ok else 2

        now = time.time()
        if quorum_ok:
            if ok_since is None:
                ok_since = now
            held = now - ok_since
            print(
                f"parity sample OK on {summary['sample_ok']}/{summary['reachable']} "
                f"reachable node(s) for {held:.0f}s"
                + (f" (need {need_seconds:.0f}s)" if need_seconds else "")
                + " — do not flip cutover without a multi-hour window",
                flush=True,
            )
            if need_seconds > 0 and held >= need_seconds:
                print("min-ok window satisfied", flush=True)
                return 0
        else:
            ok_since = None

        time.sleep(args.interval)


if __name__ == "__main__":
    sys.exit(main())
