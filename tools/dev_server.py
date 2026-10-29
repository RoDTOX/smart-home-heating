#!/usr/bin/env python3
"""
tools/dev_server.py
Zero-Risk Local Frontend Development Server for Smart Heating.
Serves web/ files locally and proxies all REST & Console API calls directly to ESP32 (192.168.1.20).
Enables instant live UI changes and full console integration on http://localhost:8080/
"""

import http.server
import socketserver
import urllib.request
import urllib.error
import webbrowser
import os
import sys

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

PORT = 8080
ESP32_HOST = os.environ.get("ESP_HEATING_IP", "192.168.1.20")
ESP32_PORT = 80
REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB_DIR = os.path.join(REPO_DIR, "web")

def generate_mock_history(range_param="24h"):
    import math
    import json
    is_5d = (range_param == "5d")
    count = 240 if is_5d else 288
    step_sec = 1800 if is_5d else 300
    points = []

    for i in range(count):
        if is_5d:
            t_frac = (i % 48) / 48.0
            ambient_wave = math.sin((t_frac - 0.35) * 2 * math.pi)
            out_temp = round(8.0 + 5.0 * ambient_wave, 1)

            is_morning_burn = (12 <= (i % 48) <= 17)
            is_evening_burn = (36 <= (i % 48) <= 41)
            flame = is_morning_burn or is_evening_burn

            z1 = round(23.6 + (0.5 if flame else -0.3) + math.sin(i * 0.1) * 0.2, 1)
            z2 = round(23.2 + (0.4 if flame else -0.2) + math.cos(i * 0.1) * 0.15, 1)
            z3 = round(23.1 + (0.4 if flame else -0.2) + math.sin(i * 0.1 + 1) * 0.2, 1)

            flags = 0x80  # valid bit
            if flame:
                flags |= 0x01 | 0x02 | 0x04 | 0x08 | 0x10
            points.append([i, z1, z2, z3, out_temp, flags])
        else:
            t_frac = i / 288.0
            ambient_wave = math.sin((t_frac - 0.35) * 2 * math.pi)
            out_temp = round(6.5 + 6.0 * ambient_wave, 1)

            is_morning = (69 <= i <= 96)
            is_evening = (210 <= i <= 237)
            flame = is_morning or is_evening

            z1 = round(23.2 + (0.7 if (75 <= i <= 120 or 215 <= i <= 260) else 0.0) + math.sin(i * 0.05) * 0.15, 1)
            z2 = round(22.8 + (0.6 if (75 <= i <= 115 or 215 <= i <= 255) else 0.0) + math.cos(i * 0.05) * 0.12, 1)
            z3 = round(22.7 + (0.5 if (75 <= i <= 115 or 215 <= i <= 255) else 0.0) + math.sin(i * 0.05 + 2) * 0.1, 1)

            flags = 0x80  # valid bit
            if flame:
                flags |= 0x01 | 0x02 | 0x04 | 0x08 | 0x10
            points.append([i, z1, z2, z3, out_temp, flags])

    return json.dumps({
        "range": "5d" if is_5d else "24h",
        "stepSec": step_sec,
        "currentSlot": 180 if not is_5d else 210,
        "points": points
    })

class LocalDevHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB_DIR, **kwargs)

    def end_headers(self):
        # Prevent browser caching during local development
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Authorization, X-Log-Seq')
        self.send_header('Access-Control-Expose-Headers', 'X-Log-Seq, Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def _proxy_to_esp(self, method="GET", body=None):
        target_url = f"http://{ESP32_HOST}:{ESP32_PORT}{self.path}"
        headers = {}
        for k, v in self.headers.items():
            if k.lower() not in ['host', 'content-length', 'connection']:
                headers[k] = v
        headers['Connection'] = 'close'
        if body:
            headers['Content-Length'] = str(len(body))

        req = urllib.request.Request(target_url, data=body, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=6.0) as resp:
                resp_data = resp.read()
                self.send_response(resp.status)
                for header, val in resp.getheaders():
                    if header.lower() not in ['transfer-encoding', 'content-length', 'connection']:
                        self.send_header(header, val)
                self.send_header('Content-Length', str(len(resp_data)))
                self.end_headers()
                self.wfile.write(resp_data)
        except urllib.error.HTTPError as e:
            if self.path.startswith('/api/history') and e.code == 404:
                range_param = "5d" if "range=5d" in self.path else "24h"
                mock_json = generate_mock_history(range_param).encode('utf-8')
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(mock_json)))
                self.end_headers()
                self.wfile.write(mock_json)
                return
            err_data = e.read()
            self.send_response(e.code)
            for header, val in e.headers.items():
                if header.lower() not in ['transfer-encoding', 'content-length', 'connection']:
                    self.send_header(header, val)
            self.send_header('Content-Length', str(len(err_data)))
            self.end_headers()
            self.wfile.write(err_data)
        except Exception as ex:
            if self.path.startswith('/api/history'):
                range_param = "5d" if "range=5d" in self.path else "24h"
                mock_json = generate_mock_history(range_param).encode('utf-8')
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(mock_json)))
                self.end_headers()
                self.wfile.write(mock_json)
                return
            err_msg = f'{{"error":"ESP32 Proxy Error","details":"{str(ex)}","target":"{target_url}"}}'.encode('utf-8')
            try:
                self.send_response(502)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(err_msg)))
                self.end_headers()
                self.wfile.write(err_msg)
            except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, Exception):
                pass

    def do_GET(self):
        if self.path.startswith('/api/') or self.path == '/api':
            self._proxy_to_esp("GET")
            return
        if self.path == '/' or self.path == '':
            self.path = '/index.html'
        elif self.path == '/setup' or self.path == '/setup/':
            self.path = '/setup.html'
        super().do_GET()

    def do_POST(self):
        if self.path.startswith('/api/') or self.path == '/api':
            content_length = int(self.headers.get('Content-Length', 0))
            post_body = self.rfile.read(content_length) if content_length > 0 else None
            self._proxy_to_esp("POST", post_body)
            return
        super().do_POST()

class ThreadedDevServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True

    def handle_error(self, request, client_address):
        ex_type, ex_val, _ = sys.exc_info()
        if ex_type in (ConnectionResetError, ConnectionAbortedError, BrokenPipeError):
            return
        super().handle_error(request, client_address)

def main():
    print("=" * 65)
    print("  SMART HEATING — LOCAL FRONTEND DEV SERVER WITH REVERSE PROXY")
    print("=" * 65)
    print(f"[*] Director local servit: {WEB_DIR}")
    print(f"[*] Target Hardware ESP32: http://{ESP32_HOST}/")
    print(f"[*] Adresă locală:        http://localhost:{PORT}/index.html")
    print(f"[*] Proxy activ:          /api/* -> http://{ESP32_HOST}/api/*")
    print(f"[*] Modifici HTML/CSS/JS -> Salvezi -> Dai F5 în browser!")
    print(f"[*] ZERO flash pe ESP32, ZERO risc de blocare, iterare instantanee.")
    print("=" * 65)

    with ThreadedDevServer(("", PORT), LocalDevHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n[DEV SERVER] Oprit.")

if __name__ == "__main__":
    main()
