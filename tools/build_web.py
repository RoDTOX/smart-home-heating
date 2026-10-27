"""PlatformIO pre-build script: gzip web/*.html into PROGMEM headers.

Generates:
    - include/dashboard_html_gz.h (from web/index.html)
    - include/setup_html_gz.h     (from web/setup.html)
    - include/console_html_gz.h   (from web/console.html)

Can be run automatically via PlatformIO (extra_scripts = pre:tools/build_web.py)
or standalone: python tools/build_web.py
"""
import gzip
import os
import re

try:
    Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
    PROJECT_DIR = env["PROJECT_DIR"]  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PAGES = [
    {
        "src": os.path.join(PROJECT_DIR, "web", "index.html"),
        "dst": os.path.join(PROJECT_DIR, "include", "dashboard_html_gz.h"),
        "var": "DASHBOARD_HTML_GZ",
        "len_var": "DASHBOARD_HTML_GZ_LEN",
    },
    {
        "src": os.path.join(PROJECT_DIR, "web", "setup.html"),
        "dst": os.path.join(PROJECT_DIR, "include", "setup_html_gz.h"),
        "var": "SETUP_HTML_GZ",
        "len_var": "SETUP_HTML_GZ_LEN",
    },
    {
        "src": os.path.join(PROJECT_DIR, "web", "console.html"),
        "dst": os.path.join(PROJECT_DIR, "include", "console_html_gz.h"),
        "var": "CONSOLE_HTML_GZ",
        "len_var": "CONSOLE_HTML_GZ_LEN",
    },
]


def strip_js_comments_and_whitespace(js_code):
    """Accurately strip single-line and multi-line comments while preserving string literals."""
    out = []
    i = 0
    n = len(js_code)
    state = "NORMAL"

    while i < n:
        c = js_code[i]
        nxt = js_code[i + 1] if i + 1 < n else ""

        if state == "NORMAL":
            if c == "'":
                state = "SINGLE_QUOTE"
                out.append(c)
                i += 1
            elif c == '"':
                state = "DOUBLE_QUOTE"
                out.append(c)
                i += 1
            elif c == '`':
                state = "TEMPLATE_STRING"
                out.append(c)
                i += 1
            elif c == '/' and nxt == '/':
                state = "LINE_COMMENT"
                i += 2
            elif c == '/' and nxt == '*':
                state = "BLOCK_COMMENT"
                i += 2
            else:
                out.append(c)
                i += 1

        elif state == "SINGLE_QUOTE":
            out.append(c)
            if c == '\\':
                if i + 1 < n:
                    out.append(js_code[i + 1])
                    i += 2
                else:
                    i += 1
            elif c == "'":
                state = "NORMAL"
                i += 1
            else:
                i += 1

        elif state == "DOUBLE_QUOTE":
            out.append(c)
            if c == '\\':
                if i + 1 < n:
                    out.append(js_code[i + 1])
                    i += 2
                else:
                    i += 1
            elif c == '"':
                state = "NORMAL"
                i += 1
            else:
                i += 1

        elif state == "TEMPLATE_STRING":
            out.append(c)
            if c == '\\':
                if i + 1 < n:
                    out.append(js_code[i + 1])
                    i += 2
                else:
                    i += 1
            elif c == '`':
                state = "NORMAL"
                i += 1
            else:
                i += 1

        elif state == "LINE_COMMENT":
            if c == '\n':
                out.append('\n')
                state = "NORMAL"
            i += 1

        elif state == "BLOCK_COMMENT":
            if c == '*' and nxt == '/':
                state = "NORMAL"
                i += 2
            else:
                i += 1

    cleaned = "".join(out)
    clean_lines = []
    for line in cleaned.splitlines():
        s = line.strip()
        if not s:
            continue
        s = re.sub(r' {2,}', ' ', s)
        clean_lines.append(s)
    return "\n".join(clean_lines)


def minify_html(text):
    # 1. Strip HTML comments
    text = re.sub(r'<!--(?!\s*slide)[^>]*?-->', '', text)

    # 2. Minify CSS blocks
    def min_css(m):
        c = m.group(1)
        c = re.sub(r'/\*.*?\*/', '', c, flags=re.S)
        c = re.sub(r'\s+', ' ', c)
        c = re.sub(r'\s*([\{\}:;,>])\s*', r'\1', c)
        c = re.sub(r';\}', '}', c)
        return f"<style>{c.strip()}</style>"

    text = re.sub(r'<style>(.*?)</style>', min_css, text, flags=re.S)

    # 3. Minify JS blocks
    def min_js(m):
        js = m.group(1)
        return f"<script>\n{strip_js_comments_and_whitespace(js)}\n</script>"

    text = re.sub(r'<script>(.*?)</script>', min_js, text, flags=re.S)

    # 4. Clean line breaks and extra whitespace
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    return "\n".join(lines)


def build(force=False):
    try:
        script_path = os.path.abspath(__file__)
    except NameError:
        script_path = os.path.join(PROJECT_DIR, "tools", "build_web.py")
    script_mtime = os.path.getmtime(script_path) if os.path.exists(script_path) else 0
    for page in PAGES:
        src = page["src"]
        dst = page["dst"]
        var_name = page["var"]
        len_name = page["len_var"]

        if not os.path.exists(src):
            print(f"[build_web] Warning: {src} not found, skipping.")
            continue

        if not force and os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src) and os.path.getmtime(dst) >= script_mtime:
            continue

        with open(src, "r", encoding="utf-8") as f:
            text = f.read()

        minified = minify_html(text)
        minified_bytes = minified.encode("utf-8")

        # mtime=0 keeps the output deterministic between builds
        data = gzip.compress(minified_bytes, compresslevel=9, mtime=0)
        raw = minified_bytes

        lines = []
        for i in range(0, len(data), 20):
            lines.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 20]) + ",")

        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(f"// AUTO-GENERATED by tools/build_web.py from {os.path.relpath(src, PROJECT_DIR)} - DO NOT EDIT\n")
            f.write("#pragma once\n#include <Arduino.h>\n\n")
            f.write(f"// Uncompressed: {len(raw)} bytes, gzip: {len(data)} bytes ({len(data)/len(raw)*100:.1f}%)\n")
            f.write(f"const uint8_t {var_name}[] PROGMEM = {{\n")
            f.write("\n".join(lines))
            f.write(f"\n}};\nconst size_t {len_name} = {len(data)};\n")

        pct = len(data) / len(raw) * 100
        print(f"[build_web] {os.path.relpath(dst, PROJECT_DIR)}: {len(raw)} -> {len(data)} bytes ({pct:.1f}% gzip)")


if __name__ == "__main__":
    build(force=True)
else:
    build()
