import socket
import time
import os
import sys

# Force unbuffered output so logs appear in real-time
try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass

HOST = "192.168.1.20"
PORT = 80
BIN_PATH = ".pio/build/esp32dev/firmware.bin"

if not os.path.exists(BIN_PATH):
    print(f"Error: {BIN_PATH} not found")
    sys.exit(1)

bin_size = os.path.getsize(BIN_PATH)
print(f"Firmware binary: {bin_size} bytes ({bin_size / 1024:.1f} KB)")

# Direct paced socket streaming to ensure ESP32 lwIP buffers never overflow


boundary = "---------------------------SmartHeatingOTA2026"
header_part = (
    f"--{boundary}\r\n"
    f'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
    f"Content-Type: application/octet-stream\r\n\r\n"
).encode('utf-8')

footer_part = f"\r\n--{boundary}--\r\n".encode('utf-8')
total_content_len = len(header_part) + bin_size + len(footer_part)

req_headers = (
    f"POST /update HTTP/1.1\r\n"
    f"Host: {HOST}\r\n"
    f"Authorization: Basic YWRtaW46MjEwNTE5OTA=\r\n"
    f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
    f"Content-Length: {total_content_len}\r\n"
    f"Connection: close\r\n\r\n"
).encode('utf-8')

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.settimeout(60)
print(f"Connecting to {HOST}:{PORT}...")
t0 = time.time()
s.connect((HOST, PORT))
print(f"Connected in {time.time() - t0:.3f}s. Sending headers...")
s.sendall(req_headers)
s.sendall(header_part)

chunk_size = 4096
sent = 0
last_report = time.time()

with open(BIN_PATH, "rb") as f:
    while True:
        chunk = f.read(chunk_size)
        if not chunk:
            break
        s.sendall(chunk)
        sent += len(chunk)
        time.sleep(0.003)  # Gentle pacing (~200 KB/s) gives SPI flash time to write without lwIP stall
        now = time.time()
        if now - last_report >= 2.0:
            pct = (sent / bin_size) * 100
            speed = (sent / 1024) / (now - t0)
            print(f"[{now - t0:.1f}s] Sent {sent}/{bin_size} ({pct:.1f}%) @ {speed:.1f} KB/s")
            last_report = now

s.sendall(footer_part)
t_upload = time.time() - t0
print(f"Upload finished in {t_upload:.2f}s. Waiting for board response...")

try:
    s.settimeout(12.0)
    resp = b""
    while True:
        data = s.recv(1024)
        if not data:
            break
        resp += data
        if b"200 OK" in resp or b"Update Success" in resp or b"<!DOCTYPE" in resp:
            break
    print(f"Response in {time.time() - t0:.2f}s: {resp.decode('utf-8', errors='ignore')[:300]}")
except Exception as e:
    print(f"Upload completed / Socket closed ({time.time() - t0:.2f}s): {e}")
finally:
    s.close()

print("[*] Waiting 7 seconds for ESP32 reboot...")
time.sleep(7)

import requests
try:
    r = requests.get(f"http://{HOST}:{PORT}/api/boiler/status", timeout=5)
    if r.status_code == 200:
        data = r.json()
        print(f"[+] SUCCESS: ESP32 is online! Firmware v{data.get('firmwareVersion')} (Build: {data.get('buildDate')})")
    else:
        print(f"[-] Status returned code {r.status_code}")
except Exception as e:
    print(f"[-] Reboot check warning: {e}")

