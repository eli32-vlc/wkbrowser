#!/usr/bin/env bash
# Build wkbrowser against Debian's WebKitGTK 4.1 (2.50.6).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig
cd "$ROOT"

CFLAGS=$(pkg-config --cflags webkit2gtk-4.1)
LIBS=$(pkg-config --libs webkit2gtk-4.1)
SRC="src/wkbrowser.c src/wkb_profile.c src/wkb_files.c src/wkb_net.c src/wkb_probe.c"

gcc -O2 -g -Wall -Wextra -Wno-unused-parameter $SRC \
    -o dist/wkbrowser $CFLAGS $LIBS

echo "built: $ROOT/dist/wkbrowser"
ls -la dist/wkbrowser
