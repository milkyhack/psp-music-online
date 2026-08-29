#!/bin/bash
# Start music server for PPSSPP bench if not already listening.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-8084}"
HOST="${HOST:-127.0.0.1}"
PIDFILE="${ROOT}/server/.ppsspp_bench_server.pid"
LOG="${ROOT}/server/.ppsspp_bench_server.log"

if curl -sf "http://${HOST}:${PORT}/api/status" >/dev/null 2>&1; then
  echo "Server already up on http://${HOST}:${PORT}"
  exit 0
fi

if [[ -f "$PIDFILE" ]]; then
  old_pid="$(cat "$PIDFILE" 2>/dev/null || true)"
  if [[ -n "${old_pid:-}" ]] && kill -0 "$old_pid" 2>/dev/null; then
    echo "Waiting for server pid ${old_pid}..."
    for _ in $(seq 1 30); do
      if curl -sf "http://${HOST}:${PORT}/api/status" >/dev/null 2>&1; then
        echo "Server ready on http://${HOST}:${PORT}"
        exit 0
      fi
      sleep 0.2
    done
  fi
fi

cd "${ROOT}/server"
if [[ ! -x .venv/bin/python ]]; then
  echo "Missing server/.venv — run: cd server && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt" >&2
  exit 1
fi

nohup .venv/bin/python -m app >>"$LOG" 2>&1 &
echo "$!" >"$PIDFILE"
echo "Starting server (pid $(cat "$PIDFILE")) → http://${HOST}:${PORT}"

for _ in $(seq 1 50); do
  if curl -sf "http://${HOST}:${PORT}/api/status" >/dev/null 2>&1; then
    echo "Server ready"
    exit 0
  fi
  sleep 0.2
done

echo "Server did not respond on port ${PORT} — see ${LOG}" >&2
exit 1
