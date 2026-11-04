"""PlatformIO pre-build script: minify and gzip web/index.html into .pio/build/index.html.gz.

The page is no longer part of the firmware image (FR-8.5): it is uploaded to LittleFS with
`python tools/sh_ota_upload.py assets` (FR-8.6).

Runs automatically via PlatformIO (extra_scripts = pre:tools/sh_build_web.py) or standalone: python tools/sh_build_web.py
"""
import gzip
import os
import re

try:
    Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
    PROJECT_DIR = env["PROJECT_DIR"]  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = os.path.join(PROJECT_DIR, "web", "index.html")
DST = os.path.join(PROJECT_DIR, ".pio", "build", "index.html.gz")


def build():
    if not os.path.exists(SRC):
        print(f"[sh_build_web] Warning: {SRC} not found, skipping.")
        return
    if os.path.exists(DST) and os.path.getmtime(DST) >= os.path.getmtime(SRC):
        return
    with open(SRC, "r", encoding="utf-8") as f:
        text = f.read()
    text = re.sub(r'<!--(?!\s*slide)[^>]*?-->', '', text)  # strip HTML comments
    raw = "\n".join(l.strip() for l in text.splitlines() if l.strip()).encode("utf-8")
    data = gzip.compress(raw, compresslevel=9, mtime=0)  # mtime=0 keeps the output deterministic
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    with open(DST, "wb") as f:
        f.write(data)
    print(f"[sh_build_web] {os.path.relpath(DST, PROJECT_DIR)}: {len(raw)} -> {len(data)} bytes ({len(data) / len(raw) * 100:.1f}% gzip)")


build()
