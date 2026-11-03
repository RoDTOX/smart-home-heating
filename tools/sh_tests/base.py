"""Shared part of the system tests: HTTP helpers, requirement registry, TestResult, SuiteBase.

The tests live in `tools/sh_tests/t_<area>.py`; `tools/sh_test_system.py` runs them.

Automated System Verification Test Suite (ASPICE SWE.5)
ESP32 Multi-Zone Smart Heating & RF 433 MHz Controller

Executes automated end-to-end integration, atomic verification and safety tests
against the physical ESP32 board over REST and Network Console APIs.
Zero firmware mocks: all tests run against production endpoints with live hardware feedback.
"""

import sys
import os
import time
import json
import argparse
import re
from datetime import datetime

try:
    import requests
    # Single-threaded ESP32 WebServer requires explicit Connection: close to avoid socket exhaustion
    _orig_get = requests.get
    _orig_post = requests.post

    def safe_get(url, *args, **kwargs):
        headers = dict(kwargs.get("headers") or {})
        headers.setdefault("Connection", "close")
        kwargs["headers"] = headers
        time.sleep(0.1)
        for attempt in range(3):
            try:
                return _orig_get(url, *args, **kwargs)
            except (requests.ConnectionError, requests.Timeout):
                if attempt == 2:
                    raise
                time.sleep(0.25)

    def safe_post(url, *args, **kwargs):
        headers = dict(kwargs.get("headers") or {})
        headers.setdefault("Connection", "close")
        kwargs["headers"] = headers
        time.sleep(0.1)
        for attempt in range(3):
            try:
                return _orig_post(url, *args, **kwargs)
            except (requests.ConnectionError, requests.Timeout):
                if attempt == 2:
                    raise
                time.sleep(0.25)

    requests.get = safe_get
    requests.post = safe_post
except ImportError:
    print("[-] Error: 'requests' package required. Install via: pip install requests")
    sys.exit(1)

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

DEFAULT_IP = os.environ.get("ESP_HEATING_IP", "192.168.1.20")
DEFAULT_PORT = 80

# Requirement registry, read from docs/requirements_and_features.md - the only source (PB-11). Only active
# requirements verified on the board ({LEVEL; METHOD} with METHOD test or board) count for coverage.
REQ_DOC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "requirements_and_features.md")
REQ_LINE = re.compile(r"^- \*\*(N?FR-\d+(?:\.\d+)?) \(([^)]*)\):\*\*(.*)$")


def load_requirements(path=REQ_DOC):
    reqs = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                m = REQ_LINE.match(line.strip())  # retired items start with "- ~~" and do not match
                if not m:
                    continue
                attr = re.search(r"\{([^;}]*);\s*([^}]*)\}", m.group(3))
                methods = [x.strip() for x in attr.group(2).split(",")] if attr else []
                if any(x in ("test", "board") for x in methods):
                    reqs[m.group(1)] = m.group(2)
    except OSError:
        pass
    return reqs


SWE1_REQUIREMENTS = load_requirements()

class TestResult:
    def __init__(self, test_id, name, requirement, status="PASS", latency_ms=0.0, details=""):
        self.test_id = test_id
        self.name = name
        self.requirement = requirement
        self.status = status
        self.latency_ms = latency_ms
        self.details = details

class SuiteBase:
    def __init__(self, ip=DEFAULT_IP, port=DEFAULT_PORT, timeout=6.0):
        self.ip = ip
        self.port = port
        self.base_url = f"http://{ip}:{port}"
        self.timeout = timeout
        self.results = []

    def console_profile(self):
        """FR-75.6: `profile` of GET /api/status ("dev-open" or "release"); None if unreadable."""
        try:
            r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
            return r.json().get("profile")
        except Exception:
            return None

    def console_auth(self):
        """C-30 / FR-75.1: HTTP Basic login of the console (user `console`), password only from the
        environment variable SH_CONSOLE_PASSWORD; None when the variable is not set.
        FR-75.6: on a dev-open build the console is open, no login is sent (None)."""
        if self.console_profile() == "dev-open":
            return None
        pw = os.environ.get("SH_CONSOLE_PASSWORD")
        return ("console", pw) if pw else None

    def console_can_run_commands(self):
        """True when commands other than HELP/STATUS can run: dev-open build, or a password is available."""
        return self.console_profile() == "dev-open" or bool(os.environ.get("SH_CONSOLE_PASSWORD"))

    def notify_esp_console(self, msg):
        # Best effort (C-30): LOG needs the login; a 401 without the variable is ignored
        try:
            requests.post(f"{self.base_url}/api/console/send", data={"cmd": f"LOG {msg}"},
                          auth=self.console_auth(), timeout=2.0)
        except Exception:
            pass

    def log_header(self, text):
        print(f"\n{'='*74}\n  {text}\n{'='*74}")

    def log_test(self, test_id, name, status, latency_ms, details):
        status_tag = f"[\033[92mPASS\033[0m]" if status == "PASS" else f"[\033[91mFAIL\033[0m]" if status == "FAIL" else f"[\033[93mSKIP\033[0m]"
        print(f"  {status_tag} {test_id:<14} {name:<42} ({latency_ms:5.1f} ms)")
        if details:
            print(f"         \033[90m-> {details}\033[0m")
