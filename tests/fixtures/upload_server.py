#!/usr/bin/env python3
"""Upload target for the t7 file-picker acceptance test.

Serves the repo root (so tests/fixtures/upload_form.html and dist/ both
resolve) and records any multipart POST it receives.
"""
import http.server, socketserver, os, sys, hashlib

# three levels up: tests/fixtures/<file> -> repo root
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RECEIVED = {}

class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=ROOT, **kw)

    def log_message(self, *a):
        pass

    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(n)
        RECEIVED['bytes'] = len(body)
        RECEIVED['sha'] = hashlib.sha256(body).hexdigest()
        RECEIVED['type'] = self.headers.get('Content-Type', '')
        RECEIVED['raw'] = body[:400]
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'ok')
        OUTDIR = os.path.join(ROOT, 'dist')
        os.makedirs(OUTDIR, exist_ok=True)
        with open(os.path.join(OUTDIR, 'upload_result.txt'), 'w') as f:
            f.write(f"bytes={RECEIVED['bytes']}\nsha={RECEIVED['sha']}\n"
                    f"type={RECEIVED['type']}\n")
        with open(os.path.join(OUTDIR, 'upload_raw.bin'), 'wb') as f:
            f.write(body)
        print(f"[server] upload received: {RECEIVED['bytes']} bytes "
              f"sha={RECEIVED['sha'][:16]}", flush=True)

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8077
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(('127.0.0.1', port), H) as httpd:
        print(f"[server] listening on {port}", flush=True)
        httpd.serve_forever()