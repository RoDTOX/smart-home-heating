"""PlatformIO pre-build script: assemble web/src/* and gzip into .pio/build/index.html.gz.

The page is no longer part of the firmware image (FR-8.5): it is uploaded to LittleFS with
`python tools/sh_ota_upload.py assets` (FR-8.6).

Runs automatically via PlatformIO (extra_scripts = pre:tools/sh_build_web.py) or standalone:
  python tools/sh_build_web.py          # build .pio/build/index.html.gz
  python tools/sh_build_web.py --sync   # also update web/index.html if assembled output changed
"""
import base64
import gzip
import os
import re
import sys

try:
    Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
    PROJECT_DIR = env["PROJECT_DIR"]  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC_DIR = os.path.join(PROJECT_DIR, "web", "src")
SHELL = os.path.join(SRC_DIR, "shell.html")
LEGACY_SRC = os.path.join(PROJECT_DIR, "web", "index.html")
DST = os.path.join(PROJECT_DIR, ".pio", "build", "index.html.gz")

JS_ORDER = (
    "i18n.ro.js",
    "core.js",
    "rooms.js",
    "schedule.js",
    "diagnostics.js",
    "history.js",
    "gas.js",
    "setup.js",
    "console.js",
    "app.js",
)


# FR-8.1 (C-47): the page loads nothing from another host, so its fonts are embedded as base64 woff2
FONTS = (
    ("Plus Jakarta Sans", "400 800", "jakarta-latin.woff2",
     "U+0000-00FF,U+0131,U+0152-0153,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+2000-206F,U+20AC,U+2122,U+2212,U+2215"),
    ("Plus Jakarta Sans", "400 800", "jakarta-latin-ext.woff2",
     "U+0100-02BA,U+02BD-02C5,U+02C7-02CC,U+02CE-02D7,U+02DD-02FF,U+1E00-1E9F,U+20A0-20AB,U+20AD-20C0,U+2113"),
    ("JetBrains Mono", "400 700", "jetbrains-latin.woff2",
     "U+0000-00FF,U+0131,U+0152-0153,U+02BB-02BC,U+02C6,U+02DA,U+02DC,U+2000-206F,U+20AC,U+2122,U+2212,U+2215"),
)


def _fonts_css(project_dir):
    out = []
    for family, weight, name, urange in FONTS:
        with open(os.path.join(project_dir, "web", "src", "fonts", name), "rb") as f:
            b64 = base64.b64encode(f.read()).decode("ascii")
        out.append("@font-face{font-family:'%s';font-style:normal;font-weight:%s;font-display:swap;"
                   "src:url(data:font/woff2;base64,%s) format('woff2');unicode-range:%s}" % (family, weight, b64, urange))
    return "\n".join(out)


def _read_part(project_dir, name):
    path = os.path.join(project_dir, "web", "src", name)
    with open(path, "r", encoding="utf-8") as f:
        return f.read().rstrip("\n")


def assemble_html(project_dir=PROJECT_DIR):
    """Assemble web/src/* into a single HTML document string."""
    shell_path = os.path.join(project_dir, "web", "src", "shell.html")
    if not os.path.exists(shell_path):
        with open(os.path.join(project_dir, "web", "index.html"), "r", encoding="utf-8") as f:
            return f.read()

    html = _read_part(project_dir, "shell.html") + "\n"
    scripts = "\n".join(_read_part(project_dir, name) for name in JS_ORDER)
    replacements = {
        "{{STYLES}}": _read_part(project_dir, "styles.css"),
        "{{FONTS}}": _fonts_css(project_dir),
        "{{CARD_ROOMS}}": _read_part(project_dir, "rooms.html"),
        "{{CARD_SCHEDULE}}": _read_part(project_dir, "schedule.html"),
        "{{CARD_DIAGNOSTICS}}": _read_part(project_dir, "diagnostics.html"),
        "{{CARD_HISTORY}}": _read_part(project_dir, "history.html"),
        "{{CARD_GAS}}": _read_part(project_dir, "gas.html"),
        "{{CARD_SETUP}}": _read_part(project_dir, "setup.html"),
        "{{CARD_CONSOLE}}": _read_part(project_dir, "console.html"),
        "{{DOC_JSON}}": _read_part(project_dir, "doc.json"),
        "{{SCRIPTS}}": scripts,
    }
    # Replace CARD_HISTORY before CARD_GAS so CARD_GAS inside history.html resolves cleanly
    for key, val in replacements.items():
        html = html.replace(key, val)
    return html


def source_files(project_dir=PROJECT_DIR):
    src_dir = os.path.join(project_dir, "web", "src")
    if not os.path.isdir(src_dir):
        legacy = os.path.join(project_dir, "web", "index.html")
        return [legacy] if os.path.exists(legacy) else []
    script_path = os.path.join(project_dir, "tools", "sh_build_web.py")
    files = [script_path] if os.path.exists(script_path) else []
    for name in sorted(os.listdir(src_dir)):
        p = os.path.join(src_dir, name)
        if os.path.isfile(p):
            files.append(p)
    files += [os.path.join(src_dir, "fonts", f[2]) for f in FONTS]
    return files


def build(force=False, sync_index=False):
    inputs = source_files(PROJECT_DIR)
    if not inputs:
        print(f"[sh_build_web] Warning: no web sources found, skipping.")
        return
    latest_mtime = max(os.path.getmtime(p) for p in inputs)
    if not force and not sync_index and os.path.exists(DST) and os.path.getmtime(DST) >= latest_mtime:
        return

    text = assemble_html(PROJECT_DIR)
    if sync_index:
        old_text = None
        if os.path.exists(LEGACY_SRC):
            with open(LEGACY_SRC, "r", encoding="utf-8") as f:
                old_text = f.read()
        if old_text != text:
            with open(LEGACY_SRC, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            print(f"[sh_build_web] synced {os.path.relpath(LEGACY_SRC, PROJECT_DIR)}")

    stripped = re.sub(r"<!--(?!\s*slide)[^>]*?-->", "", text)
    raw = "\n".join(l.strip() for l in stripped.splitlines() if l.strip()).encode("utf-8")
    data = gzip.compress(raw, compresslevel=9, mtime=0)  # mtime=0 keeps the output deterministic
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    with open(DST, "wb") as f:
        f.write(data)
    print(f"[sh_build_web] {os.path.relpath(DST, PROJECT_DIR)}: {len(raw)} -> {len(data)} bytes ({len(data) / len(raw) * 100:.1f}% gzip)")


if __name__ == "__main__":
    build(force=("--force" in sys.argv), sync_index=("--sync" in sys.argv))
else:
    build()
