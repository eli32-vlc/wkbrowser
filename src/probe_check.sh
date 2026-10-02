#!/usr/bin/env bash
# Capture a wkbrowser probe run and diff it against the checked-in baseline.
# Usage: src/probe_check.sh [--update] [extra wkbrowser args...]
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p log
UPDATE=""
[ "${1:-}" = "--update" ] && { UPDATE="--update"; shift; }

PORT=8191
OUT=tests/actual_fingerprint.json
cleanup() { [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null; }
trap cleanup EXIT

python3 dist/serv2.py "$PORT" > log/probe_srv.log 2>&1 &
SRV=$!
for _ in $(seq 1 40); do
  curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html" && break
  sleep 0.25
done
curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html" \
  || { echo "FAIL server did not start"; exit 1; }

timeout 90 xvfb-run -a -s '-screen 0 1280x800x24' \
  ./dist/wkbrowser --url "http://127.0.0.1:$PORT/testpage.html" \
  --probe --no-blocklist --timeout 25 "$@" 2>/dev/null \
  | grep -E '^\{' > "$OUT"

[ -s "$OUT" ] || { echo "FAIL probe produced no output"; exit 1; }

python3 src/probe_diff.py --actual "$OUT" $UPDATE
rc=$?
[ "$UPDATE" = "--update" ] && rm -f "$OUT"
exit $rc