#!/usr/bin/env python3
# Copyright (c) 2018 - 2026, The Arqma Network
#
# Local Session-class messenger UI using the modern Arqma wallet palette
# (Arqma-GUI-MM / arqma-electron-wallet: GUI-MM muted champagne/bronze gold on pure black).
# Arqma-GUI-MM itself is private; gold accents (goldenrod/gold as in the electron wallet).
#
# Usage:
#   utils/arqma-stack.sh
#   utils/arqma-msg-ui.py [--port 8787] [--bin DIR]
#   open http://127.0.0.1:8787/

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_DIR = ROOT / "src" / "arq_messaging" / "ui"


def find_msg_bin(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
        if path.is_dir():
            for name in ("arqma-msg", "arqma-msg.exe"):
                candidate = path / name
                if candidate.exists():
                    return candidate
            return path / "arqma-msg"
        return path
    env = os.environ.get("ARQMA_BIN_DIR") or os.environ.get("ARQMA_BIN")
    candidates = []
    if env:
        candidates.append(Path(env) / "arqma-msg")
        candidates.append(Path(env))
    candidates.extend(
        [
            ROOT / "build" / "upgrade-release" / "bin" / "arqma-msg",
            ROOT / "build" / "release" / "bin" / "arqma-msg",
            Path("arqma-msg"),
        ]
    )
    for c in candidates:
        if c.is_file():
            return c
        if c.is_dir() and (c / "arqma-msg").exists():
            return c / "arqma-msg"
    return Path("arqma-msg")


def run_msg(bin_path: Path, args: list[str]) -> tuple[int, str, str]:
    proc = subprocess.run(
        [str(bin_path), *args],
        capture_output=True,
        text=True,
        env=os.environ.copy(),
        check=False,
    )
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def read_stack_env() -> dict:
    out = {"storage_url": "", "router_url": "", "has_token": False}
    path = Path(os.environ.get("ARQMA_STACK_DIR", "/tmp/arqma-stack")) / "env"
    if not path.exists():
        return out
    for line in path.read_text().splitlines():
        if "=" not in line:
            continue
        key, val = line.split("=", 1)
        key = key.strip()
        val = val.strip().strip('"')
        if key in ("ARQMA_STORAGE_URL", "ARQMA_STORAGE"):
            out["storage_url"] = val
        elif key in ("ARQMA_ROUTER_URL", "ARQMA_ROUTER"):
            out["router_url"] = val
        elif key == "ARQMA_STACK_TOKEN" and val:
            out["has_token"] = True
    return out


def identity_hex() -> str:
    contacts = Path.home() / ".arqma" / "msg" / "contacts"
    if contacts.exists():
        for line in contacts.read_text().splitlines():
            bits = line.split()
            if len(bits) >= 2 and bits[0] == "me" and len(bits[1]) == 64:
                return bits[1].lower()
    # Fallback: identity file may store hex pubkey on its own line.
    path = Path.home() / ".arqma" / "msg" / "identity"
    if path.exists():
        for line in path.read_text(errors="ignore").splitlines():
            line = line.strip()
            if len(line) == 64 and all(c in "0123456789abcdefABCDEF" for c in line):
                return line.lower()
    return ""


def load_contacts() -> dict[str, str]:
    path = Path.home() / ".arqma" / "msg" / "contacts"
    out: dict[str, str] = {}
    if not path.exists():
        return out
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 2 and len(parts[1]) == 64:
            out[parts[0]] = parts[1].lower()
    return out


def save_contact(name: str, hex_key: str) -> None:
    path = Path.home() / ".arqma" / "msg" / "contacts"
    path.parent.mkdir(parents=True, exist_ok=True)
    contacts = load_contacts()
    contacts[name] = hex_key.lower()
    path.write_text("".join(f"{k} {v}\n" for k, v in sorted(contacts.items())))


class Handler(BaseHTTPRequestHandler):
    bin_path: Path = Path("arqma-msg")

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, code: int, payload: dict) -> None:
        self._send(code, json.dumps(payload).encode(), "application/json")

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length) if length else b"{}"
        if not raw:
            return {}
        return json.loads(raw.decode())

    def do_GET(self) -> None:  # noqa: N802
        path = urllib.parse.urlparse(self.path).path
        if path in ("/", "/index.html"):
            self._send(200, (UI_DIR / "index.html").read_bytes(), "text/html; charset=utf-8")
            return
        if path == "/ui.css":
            self._send(200, (UI_DIR / "ui.css").read_bytes(), "text/css; charset=utf-8")
            return
        if path == "/ui.js":
            self._send(200, (UI_DIR / "ui.js").read_bytes(), "application/javascript; charset=utf-8")
            return
        if path == "/api/status":
            stack = read_stack_env()
            self._json(
                200,
                {
                    "identity": identity_hex(),
                    "storage_url": stack["storage_url"],
                    "router_url": stack["router_url"],
                    "has_token": stack["has_token"],
                },
            )
            return
        if path == "/api/inbox":
            code, out, err = run_msg(self.bin_path, ["inbox"])
            if code != 0:
                self._json(400, {"error": err or out or "inbox failed"})
                return
            keys = [ln.strip() for ln in out.splitlines() if ln.strip()]
            self._json(200, {"keys": keys})
            return
        if path == "/api/contacts":
            # Prefer CLI when present.
            code, out, err = run_msg(self.bin_path, ["contacts"])
            if code == 0 and out is not None:
                contacts: dict[str, str] = {}
                for line in out.splitlines():
                    parts = line.split()
                    if len(parts) >= 2:
                        contacts[parts[0]] = parts[1]
                self._json(200, {"contacts": contacts})
                return
            self._json(200, {"contacts": load_contacts()})
            return
        self._json(404, {"error": "not found"})

    def do_POST(self) -> None:  # noqa: N802
        path = urllib.parse.urlparse(self.path).path
        try:
            payload = self._read_json()
        except json.JSONDecodeError:
            self._json(400, {"error": "invalid json"})
            return

        if path == "/api/gen":
            code, out, err = run_msg(self.bin_path, ["gen"])
            if code != 0:
                self._json(400, {"error": err or out or "gen failed"})
                return
            self._json(200, {"identity": out.strip()})
            return

        if path == "/api/send":
            to = (payload.get("to") or "").strip()
            text = (payload.get("text") or "").strip()
            if not to or not text:
                self._json(400, {"error": "to and text required"})
                return
            code, out, err = run_msg(self.bin_path, ["send", to, *text.split()])
            if code != 0:
                self._json(400, {"error": err or out or "send failed"})
                return
            self._json(200, {"key": out.strip(), "output": out})
            return

        if path == "/api/open":
            key = (payload.get("key") or "").strip()
            args = ["open"]
            if key:
                args.extend(["--key", key])
            code, out, err = run_msg(self.bin_path, args)
            if code != 0:
                self._json(400, {"error": err or out or "open failed"})
                return
            self._json(200, {"plaintext": out, "output": out})
            return

        if path == "/api/contacts":
            name = (payload.get("name") or "").strip()
            hex_key = (payload.get("hex") or "").strip()
            if not name or len(hex_key) != 64:
                self._json(400, {"error": "name and 64-hex required"})
                return
            code, out, err = run_msg(self.bin_path, ["name", name, hex_key])
            if code != 0:
                save_contact(name, hex_key)
                self._json(200, {"ok": True, "warning": err or out or "saved locally"})
                return
            self._json(200, {"ok": True})
            return

        self._json(404, {"error": "not found"})


def main() -> int:
    ap = argparse.ArgumentParser(description="Arqma messenger UI (arqma-gui look)")
    ap.add_argument("--port", type=int, default=8787)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--bin", default=None, help="arqma-msg path or bin directory")
    args = ap.parse_args()

    if not UI_DIR.exists():
        print(f"UI assets missing: {UI_DIR}", file=sys.stderr)
        return 1

    Handler.bin_path = find_msg_bin(args.bin)
    server = ThreadingHTTPServer((args.bind, args.port), Handler)
    print(f"Arqma messenger UI on http://{args.bind}:{args.port}/")
    print(f"using arqma-msg: {Handler.bin_path}")
    print("Start utils/arqma-stack.sh first for storage/router.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nbye")
    return 0


if __name__ == "__main__":
    sys.exit(main())
