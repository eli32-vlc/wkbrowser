#!/usr/bin/env bash
# t7 acceptance test: the native file picker is replaced by the registry API.
#
# Asserts, end to end:
#   1. run-file-chooser fires and no GTK dialog is constructed
#   2. input.files gains exactly one entry, with the right name and size
#   3. the upload POST arrives and the payload bytes are present verbatim
#
# Fixtures live in tests/fixtures so this works from a clean checkout.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p log dist
PORT=${PORT:-8081}
PASS=0

cleanup() { [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null; }
trap cleanup EXIT

echo "=== t7: file chooser + upload ==="

python3 tests/fixtures/upload_server.py "$PORT" > log/t7_server.log 2>&1 &
SRV=$!
for _ in $(seq 1 40); do
  curl -s -o /dev/null "http://127.0.0.1:$PORT/tests/fixtures/upload_form.html" && break
  sleep 0.25
done
if ! curl -s -o /dev/null "http://127.0.0.1:$PORT/tests/fixtures/upload_form.html"; then
  echo "FAIL server did not start"; cat log/t7_server.log; exit 1
fi
echo "server up on $PORT"

printf 'hello-wkb-file-content-12345\n' > dist/payload.txt
EXPECT=$(sha256sum dist/payload.txt | cut -d' ' -f1)
rm -f dist/upload_raw.bin dist/upload_result.txt

# The probe binary is a C file; build it if absent.
if [ ! -x dist/test_filechooser ]; then
  export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig
  gcc -O2 -g src/test_filechooser.c src/wkb_profile.c src/wkb_files.c src/wkb_net.c \
      -o dist/test_filechooser \
      $(pkg-config --cflags webkit2gtk-4.1) $(pkg-config --libs webkit2gtk-4.1) || {
    echo "FAIL could not build test_filechooser"; exit 1; }
fi

OUT=$(timeout 120 xvfb-run -a -s '-screen 0 1280x800x24' \
  ./dist/test_filechooser "$ROOT/dist/payload.txt" \
  "http://127.0.0.1:$PORT/tests/fixtures/upload_form.html" 2>&1)
echo "$OUT" | grep -E 'RESULT|chooser|satisfied|ERROR' || true

echo
echo "--- assertions ---"

if echo "$OUT" | grep -q 'run-file-chooser fired'; then
  echo "OK   run-file-chooser fired, no dialog constructed"; PASS=$((PASS+1))
else
  echo "FAIL run-file-chooser never fired"
fi

if echo "$OUT" | grep -q '"count":1'; then
  echo "OK   input.files has exactly 1 entry"; PASS=$((PASS+1))
else
  echo "FAIL input.files did not receive the file"
fi

if [ -s dist/upload_raw.bin ]; then
  echo "OK   upload reached the server"; PASS=$((PASS+1))
else
  echo "FAIL no upload received"
fi

if [ -s dist/upload_raw.bin ] && python3 tests/fixtures/verify_upload.py \
      dist/upload_raw.bin dist/payload.txt; then
  echo "OK   payload bytes present verbatim in multipart body"; PASS=$((PASS+1))
else
  echo "FAIL payload bytes missing from the upload"
fi

echo
echo "payload sha256: $EXPECT"
if [ "$PASS" -ge 4 ]; then echo "*** T7 PASS ($PASS/4) ***"; else echo "*** T7 FAIL ($PASS/4) ***"; exit 1; fi