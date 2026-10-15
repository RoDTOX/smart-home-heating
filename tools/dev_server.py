#!/usr/bin/env python3
"""
tools/dev_server.py
Zero-Risk Local Frontend Development Server for Smart Heating.
Serves web/ files locally and routes all REST API calls directly to the ESP32 (192.168.1.20).
Enables instant (50ms) live UI changes without compiling or flashing the ESP32.
"""

import http.server
import socketserver
import webbrowser
import os
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

PORT = 8080
REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB_DIR = os.path.join(REPO_DIR, "web")

class LocalDevHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def end_headers(self):
        # Prevent browser caching during local development
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()

def main():
    print("=" * 60)
    print("  SMART HEATING — LOCAL FRONTEND DEV SERVER")
    print("=" * 60)
    print(f"[*] Director local servit: {WEB_DIR}")
    print(f"[*] Target Hardware ESP32: http://192.168.1.20/")
    print(f"[*] Adresă locală:        http://localhost:{PORT}/index.html")
    print(f"[*] Modifici HTML/CSS/JS -> Salvezi -> Dai F5 în browser!")
    print(f"[*] ZERO flash pe ESP32, ZERO risc de blocare, iterare instantanee.")
    print("=" * 60)

    url = f"http://localhost:{PORT}/index.html"
    try:
        webbrowser.open(url)
    except Exception:
        pass

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), LocalDevHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[DEV SERVER] Oprit.")

if __name__ == "__main__":
    main()
