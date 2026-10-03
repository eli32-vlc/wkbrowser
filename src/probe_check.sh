#!/usr/bin/env bash
# Capture a wkbrowser probe run and diff it against the checked-in baseline.
#
# Usage: src/probe_check.sh [--update] [extra wkbrowser args...]
#
# The server binds port 0 so the OS assigns a free one; a hardcoded port kept
# colliding with servers orphaned by earlier runs, which surfaced as a
# confusing "server did not start" failure.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p log

UPDATE=""
[ "${1:-}" = "--update" ] && { UPDATE="--update"; shift; }

OUT=tests/actual_fingerprint.json
PORTFILE=/tmp/wkb_port
cleanup() { [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null; }
trap cleanup EXIT

rm -f "$PORTFILE"
python3 tests/fixtures/serv2.py 0 > log/probe_srv.log 2>&1 &
SRV=$!

PORT=""
for _ in $(seq 1 40); do
  PORT=$(cat "$PORTFILE" 2>/dev/null)
  if [ -n "$PORT" ] && curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html"; then
    break
  fi
  sleep 0.25
done

if [ -z "$PORT" ] || ! curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html"; then
  echo "FAIL server did not start"; tail -5 log/probe_srv.log; exit 1
fi

timeout 90 xvfb-run -a -s '-screen 0 1280x800x24' \
  ./dist/wkbrowser --url "http://127.0.0.1:$PORT/testpage.html" \
  --probe --no-blocklist --timeout 25 "$@" 2>/dev/null \
  | grep -E '^\{' > "$OUT"

[ -s "$OUT" ] || { echo "FAIL probe produced no output"; exit 1; }

python3 src/probe_diff.py --actual "$OUT" $UPDATE
rc=$?
[ "$UPDATE" = "--update" ] && rm -f "$OUT"
exit $rc