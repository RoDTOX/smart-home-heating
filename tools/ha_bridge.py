#!/usr/bin/env python3
"""
tools/ha_bridge.py
Zero-privilege TCP Bridge forwarding incoming Wi-Fi connections:
  - 192.168.1.13:8123 (standard Home Assistant port for mobile app)
  - 0.0.0.0:8124      (alternate fallback port)
directly to WSL2 Home Assistant IP (172.27.207.7:8123).
Allows phone / tablet on the same Wi-Fi network to connect to Home Assistant on this PC.
"""

import socket
import subprocess
import threading
import sys
import time

def get_wsl_ip():
    try:
        out = subprocess.check_output(['wsl', '-d', 'Ubuntu', '--', 'hostname', '-I'], timeout=5).decode()
        return out.split()[0]
    except Exception:
        return "172.27.207.7"

def forward(src, dst):
    try:
        while True:
            data = src.recv(16384)
            if not data:
                break
            dst.sendall(data)
    except Exception:
        pass
    finally:
        try: src.close()
        except: pass
        try: dst.close()
        except: pass

def handle_client(client_sock, target_host, target_port):
    try:
        backend_sock = socket.create_connection((target_host, target_port), timeout=5)
    except Exception:
        client_sock.close()
        return
    t1 = threading.Thread(target=forward, args=(client_sock, backend_sock), daemon=True)
    t2 = threading.Thread(target=forward, args=(backend_sock, client_sock), daemon=True)
    t1.start()
    t2.start()

def listen_on(host, port, target_host, target_port):
    try:
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((host, port))
        server.listen(100)
        print(f"[*] Bridge active: http://{host}:{port}/ -> {target_host}:{target_port}", flush=True)
        while True:
            client, addr = server.accept()
            threading.Thread(target=handle_client, args=(client, target_host, target_port), daemon=True).start()
    except Exception as ex:
        print(f"[!] Bridge error on {host}:{port} - {ex}", flush=True)

def main():
    sys.stdout.reconfigure(encoding='utf-8')
    wsl_ip = get_wsl_ip()
    print(f"[*] Discovered WSL2 IP: {wsl_ip}", flush=True)

    threads = []
    # Primary: Standard HA port 8123 on Wi-Fi IP
    t1 = threading.Thread(target=listen_on, args=("192.168.1.13", 8123, wsl_ip, 8123), daemon=True)
    t1.start()
    threads.append(t1)

    # Fallback: Port 8124 on all interfaces
    t2 = threading.Thread(target=listen_on, args=("0.0.0.0", 8124, wsl_ip, 8123), daemon=True)
    t2.start()
    threads.append(t2)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("[*] Bridge stopped.", flush=True)

if __name__ == "__main__":
    main()
