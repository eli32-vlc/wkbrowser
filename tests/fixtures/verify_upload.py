#!/usr/bin/env python3
"""Verify the multipart body contains the payload bytes verbatim."""
import sys, re
body = open(sys.argv[1], 'rb').read()
payload = open(sys.argv[2], 'rb').read()
print(f"multipart body: {len(body)} bytes")
print(f"payload file  : {len(payload)} bytes")
print(f"payload present verbatim: {payload in body}")
sys.exit(0 if payload in body else 1)
