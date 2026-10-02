#!/usr/bin/env bash
# Phase-2 acceptance test: ephemeral context, asset blocking, domain blocklist.
# Runs its own server and browser in one process group so nothing dangles.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p log
PORT=${PORT:-8177}
FAIL=0

pass() { echo "OK   $1"; }
fail() { echo "FAIL $1"; FAIL=1; }

cleanup() { [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null; }
trap cleanup EXIT

python3 dist/serv2.py "$PORT" > log/s2.log 2>&1 &
SRV=$!
for _ in $(seq 1 40); do
  curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html" && break
  sleep 0.25
done
if ! curl -s -o /dev/null "http://127.0.0.1:$PORT/testpage.html"; then
  echo "FAIL server did not start"; cat log/s2.log; exit 1
fi
echo "server up on $PORT"

run() {  # run <label> <extra args...>
  local label="$1"; shift
  timeout 90 xvfb-run -a -s '-screen 0 1280x800x24' \
    ./dist/wkbrowser --url "http://127.0.0.1:$PORT/testpage.html" \
    --timeout 25 "$@" 2>&1
}

echo
echo "=== P2-1 ephemeral context (default) ==="
OUT=$(run ephemeral --dump --no-blocklist)
echo "$OUT" | grep -E 'ephemeral|persistent' | head -2
if echo "$OUT" | grep -q 'ephemeral navigation'; then
  pass "context is ephemeral by default"
else
  fail "expected an ephemeral context"
fi

echo
echo "=== P2-1 --persist opt-out ==="
OUT=$(run persist --dump --no-blocklist --persist)
if echo "$OUT" | grep -q 'persistent navigation'; then
  pass "--persist selects a persistent context"
else
  fail "--persist did not switch context"
fi

echo
echo "=== P2-2 asset blocking ==="
OUT=$(run blockassets --dump --block-assets --stats --timeout 30)
STATS=$(echo "$OUT" | grep 'resources:' | tail -1)
echo "  $STATS"
BLOCKED=$(echo "$STATS" | sed -n 's/.*blocked=\([0-9]*\).*/\1/p')
if [ "${BLOCKED:-0}" -gt 0 ]; then
  pass "blocked $BLOCKED subresources with --block-assets"
else
  fail "--block-assets blocked nothing"
fi

echo
echo "=== P2-2 control: no blocking ==="
OUT=$(run noblock --dump --no-blocklist --stats --timeout 30)
STATS=$(echo "$OUT" | grep 'resources:' | tail -1)
echo "  $STATS"
BLOCKED=$(echo "$STATS" | sed -n 's/.*blocked=\([0-9]*\).*/\1/p')
if [ "${BLOCKED:-0}" -eq 0 ]; then
  pass "control run blocked nothing"
else
  fail "control run unexpectedly blocked $BLOCKED"
fi

echo
echo "=== P2-3 domain blocklist ==="
OUT=$(run blocklist --dump --blocklist share/wkb-blocklist.txt --stats --timeout 30)
STATS=$(echo "$OUT" | grep 'resources:' | tail -1)
LOADED=$(echo "$OUT" | grep 'blocklist entries' | head -1)
echo "  $LOADED"
echo "  $STATS"
if echo "$OUT" | grep -q 'loaded [0-9]* blocklist entries'; then
  pass "blocklist parsed"
else
  fail "blocklist did not load"
fi

echo
echo "=== P2-5 fingerprint probe ==="
OUT=$(run probe --probe --no-blocklist --timeout 25)
PROBE=$(echo "$OUT" | grep -E '^\{' | head -1)
if [ -n "$PROBE" ]; then
  echo "$PROBE" | python3 -m json.tool 2>/dev/null | head -30 || echo "$PROBE"
  pass "probe returned a JSON fingerprint"
else
  fail "probe returned nothing"
  echo "$OUT" | tail -5
fi

echo
if [ "$FAIL" -eq 0 ]; then echo "*** PHASE 2 ACCEPTANCE: PASS ***"; else echo "*** PHASE 2 ACCEPTANCE: FAIL ***"; fi
exit $FAIL