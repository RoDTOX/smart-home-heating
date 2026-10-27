"""Upload Web UI directly to ESP32 LittleFS via HTTP POST.

No C++ compilation, no firmware flash, and NO ESP32 REBOOT required.
Usage:
    python tools/upload_ui.py [--page index|setup|console] [--host 192.168.1.20]
"""
import sys
import os
import gzip
import argparse
import requests
import time

# Ensure project root is in sys.path
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if PROJECT_DIR not in sys.path:
    sys.path.insert(0, PROJECT_DIR)

from tools.build_web import minify_html

PAGES = {
    "index": {
        "src": os.path.join(PROJECT_DIR, "web", "index.html"),
        "remote_name": "index.html.gz",
    },
    "setup": {
        "src": os.path.join(PROJECT_DIR, "web", "setup.html"),
        "remote_name": "setup.html.gz",
    },
    "console": {
        "src": os.path.join(PROJECT_DIR, "web", "console.html"),
        "remote_name": "console.html.gz",
    },
}


def upload(page_name="index", host="192.168.1.20"):
    if page_name not in PAGES:
        print(f"Unknown page: {page_name}. Choose from: {list(PAGES.keys())}")
        sys.exit(1)

    cfg = PAGES[page_name]
    src_path = cfg["src"]
    remote_name = cfg["remote_name"]

    if not os.path.exists(src_path):
        print(f"Error: {src_path} not found.")
        sys.exit(1)

    with open(src_path, "r", encoding="utf-8") as f:
        raw_text = f.read()

    print(f"[*] Minifying {os.path.basename(src_path)} ({len(raw_text)} chars)...")
    minified = minify_html(raw_text)
    gz_data = gzip.compress(minified.encode("utf-8"), compresslevel=9, mtime=0)
    print(f"[*] Compressed: {len(raw_text)} -> {len(gz_data)} bytes ({len(gz_data)/len(raw_text)*100:.1f}%)")

    url = f"http://{host}/api/upload-file"
    print(f"[*] Uploading to {url} as /{remote_name}...")
    t0 = time.time()

    files = {
        "file": (remote_name, gz_data, "application/octet-stream")
    }

    try:
        r = requests.post(url, files=files, timeout=10)
        dt = time.time() - t0
        if r.status_code == 200:
            speed = (len(gz_data) / 1024) / dt if dt > 0 else 0
            print(f"[+] SUCCESS! Uploaded in {dt:.2f}s ({speed:.1f} KB/s).")
            print(f"[+] Web page is LIVE at http://{host}/{'' if page_name == 'index' else page_name} (NO RESTART NEEDED!)")
        else:
            print(f"[-] Upload failed with status {r.status_code}: {r.text}")
    except Exception as e:
        print(f"[-] Connection error: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload UI directly to ESP32 LittleFS")
    parser.add_argument("--page", default="index", choices=["index", "setup", "console", "all"], help="Page to upload")
    parser.add_argument("--host", default="192.168.1.20", help="ESP32 IP address")
    args = parser.parse_args()

    if args.page == "all":
        for p in ["index", "setup", "console"]:
            upload(p, args.host)
    else:
        upload(args.page, args.host)

