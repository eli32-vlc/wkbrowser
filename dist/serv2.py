#!/usr/bin/env python3
"""Phase-2 test server: tracks cookies, serves subresources."""
import http.server, socketserver, os, sys
ROOT=os.path.dirname(os.path.abspath(__file__))
HITS=[]
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*a,**k): super().__init__(*a,directory=ROOT,**k)
    def log_message(self,*a): pass
    def do_GET(self):
        p=self.path.split('?')[0]
        HITS.append(p)
        if p=='/setcookie':
            self.send_response(200)
            self.send_header('Set-Cookie','wkbtest=leaked; Path=/')
            self.send_header('Content-Length','2'); self.end_headers()
            self.wfile.write(b'ok'); return
        if p.endswith('.png'):
            body=b'\x89PNG\r\n\x1a\n'+b'0'*40; ct='image/png'
        elif p.endswith('.css'):
            body=b'body{color:red}'; ct='text/css'
        elif p.endswith('.js'):
            body=b'/*x*/'; ct='application/javascript'
        elif p.endswith('.woff2'):
            body=b'wOF2'; ct='font/woff2'
        elif p.endswith('.mp4'):
            body=b'\x00\x00\x00\x18ftypmp42'; ct='video/mp4'
        else:
            return super().do_GET()
        self.send_response(200)
        self.send_header('Content-Type', ct)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

if __name__=='__main__':
    port=int(sys.argv[1]) if len(sys.argv)>1 else 0
    with socketserver.TCPServer(('127.0.0.1',port),H) as srv:
        print(f"[s2] port={srv.server_address[1]}",flush=True)
        open('/tmp/wkb_port','w').write(str(srv.server_address[1]))
        srv.serve_forever()
