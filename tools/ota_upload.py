#!/usr/bin/env python3
"""
Web OTA Firmware Uploader for ESP32 Smart Heating Controller.
Flashes compiled binary over local Wi-Fi in ~6-7 seconds without USB cables.
"""

import os
import sys
import time
import argparse
import requests

def upload_ota(ip="192.168.1.20", bin_path=".pio/build/esp32dev/firmware.bin", username="admin", password=None):
    if password is None:
        password = os.environ.get("OTA_PASS", "21051990")

    bin_path = os.path.abspath(bin_path)
    if not os.path.isfile(bin_path):
        print(f"[-] Error: Firmware binary not found at '{bin_path}'")
        print("    Run 'pio run -e esp32dev' first to build the project.")
        sys.exit(1)

    file_size = os.path.getsize(bin_path)
    url = f"http://{ip}/update"
    auth = (username, password)

    print(f"[*] Uploading firmware: {bin_path} ({file_size} bytes)")
    print(f"[*] Target device: {url}")

    try:
        with open(bin_path, "rb") as f:
            files = {"firmware": ("firmware.bin", f, "application/octet-stream")}
            start = time.time()
            res = requests.post(url, auth=auth, files=files, timeout=60)
            elapsed = time.time() - start

        if res.status_code == 200:
            print(f"[+] OTA upload successful in {elapsed:.2f}s! Status: 200 OK")
            print("    Device is now rebooting into the new firmware...")
        else:
            print(f"[-] OTA failed with status {res.status_code}: {res.text}")
    except Exception as e:
        print(f"[-] OTA upload error: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="ESP32 Smart Heating Web OTA Uploader")
    parser.add_argument("--ip", default="192.168.1.20", help="ESP32 IP address (default: 192.168.1.20)")
    parser.add_argument("--bin", default=".pio/build/esp32dev/firmware.bin", help="Path to firmware.bin")
    parser.add_argument("--user", default="admin", help="OTA username (default: admin)")
    parser.add_argument("--pass", dest="password", default=None, help="OTA password")

    args = parser.parse_args()
    upload_ota(ip=args.ip, bin_path=args.bin, username=args.user, password=args.password)

if __name__ == "__main__":
    main()
