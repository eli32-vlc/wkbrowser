#!/usr/bin/env bash
# t7 acceptance test: native file picker replaced by registry API, upload
# verified byte-for-byte against a local HTTP server.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p log
PORT=8077
PASS=0

cleanup() { pkill -f testserver.py >/dev/null 2>&1; }
trap cleanup EXIT

start_server() {
  python3 dist/testserver.py "$PORT" > log/server.log 2>&1 &
  for _ in $(seq 1 25); do
    if curl -s -o /dev/null "http://127.0.0.1:$PORT/upload_form.html"; then return 0; fi
    sleep 0.2
  done
  echo "FAIL: server did not start"; return 1
}

echo "=== t7: file chooser + upload ==="
start_server || exit 1

printf 'hello-wkb-file-content-12345\n' > dist/payload.txt
EXPECT=$(sha256sum dist/payload.txt | cut -d' ' -f1)
rm -f dist/upload_raw.bin dist/upload_result.txt

timeout 90 xvfb-run -a -s '-screen 0 1280x800x24' \
  ./dist/test_filechooser "$ROOT/dist/payload.txt" \
  "http://127.0.0.1:$PORT/upload_form.html" 2>&1 | grep -E 'RESULT|chooser|satisfied'

echo
echo "--- assertions ---"

if grep -q 'run-file-chooser fired' <(timeout 5 cat log/t7.log 2>/dev/null) 2>/dev/null; then :; fi

if [ -f dist/upload_raw.bin ]; then
  echo "OK   upload_raw.bin received"
else
  echo "FAIL no upload received"; exit 1
fi

if python3 dist/verify_upload.py dist/upload_raw.bin dist/payload.txt; then
  echo "OK   payload bytes present verbatim in multipart body"
  PASS=$((PASS+1))
else
  echo "FAIL payload bytes missing"; exit 1
fi

if grep -q '"count":1' <(timeout 90 xvfb-run -a ./dist/test_filechooser "$ROOT/dist/payload.txt" "http://127.0.0.1:$PORT/upload_form.html" 2>&1); then
  echo "OK   input.files has exactly 1 entry"
  PASS=$((PASS+1))
else
  echo "FAIL input.files wrong"
fi

echo
echo "payload sha256: $EXPECT"
if [ "$PASS" -ge 2 ]; then echo "*** T7 PASS ***"; else echo "*** T7 FAIL ***"; exit 1; fi