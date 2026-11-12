"""
ASPICE SWE.5 Test Framework Base Classes & Global Types
"""

import sys
import os
import re
import json
import time
import requests

DEFAULT_IP = os.environ.get("ESP_HEATING_IP", "192.168.1.20")
DEFAULT_PORT = 80

def load_requirements():
    req_file = os.path.join(os.path.dirname(__file__), "..", "..", "docs", "requirements_and_features.md")
    reqs = {}
    if not os.path.exists(req_file):
        return reqs
    try:
        with open(req_file, "r", encoding="utf-8") as f:
            for line in f:
                # Match both root features: "- **FR-14 (GPA pump control):**"
                # and sub-items: "  - **FR-14.1 (Relay assignment):**"
                m = re.match(r'^\s*-\s+\*\*(FR-[0-9]+(?:\.[0-9]+)?)\s*(?:\(([^)]+)\))?:\*\*', line)
                if not m:
                    # Match NFRs: "- **NFR-1.1 (Autonomous control loop):**"
                    m = re.match(r'^\s*-\s+\*\*(NFR-[0-9]+(?:\.[0-9]+)?)\s*(?:\(([^)]+)\))?:\*\*', line)
                if m:
                    r_id = m.group(1)
                    title = m.group(2) if m.group(2) else ""
                    # Check for [phase 4], [later], [nice to have] tags in title or on the line
                    is_p4 = "[phase 4]" in line.lower() or "[phase 4]" in title.lower()
                    is_later = "[later]" in line.lower() or "[later]" in title.lower()
                    is_nice = "[nice to have]" in line.lower() or "[nice to have]" in title.lower()
                    
                    # Clean tags from title
                    clean_title = re.sub(r'\[.*?\]', '', title).strip()
                    
                    reqs[r_id] = {
                        "title": clean_title,
                        "phase_4": is_p4,
                        "later": is_later,
                        "nice_to_have": is_nice
                    }
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
    only = []   # --only: test groups to run (empty = all)

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

    def api_auth(self):
        """FR-58: Credentials for authenticated API endpoints (same credentials as OTA)."""
        u = os.environ.get("SH_API_USER")
        p = os.environ.get("SH_API_PASSWORD")
        if u and p:
            return (u, p)
        # Check active custom credentials in NVS fallback
        candidates = [("bogdan", "bogdan123"), ("admin", "admin123")]
        sec_path = os.path.join(os.path.dirname(__file__), "..", "..", "include", "secrets.h")
        if os.path.exists(sec_path):
            try:
                with open(sec_path, encoding="utf-8") as f:
                    sec = dict(re.findall(r'#define\s+(\w+)\s+"([^"]*)"', f.read()))
                    if "OTA_USER" in sec and "OTA_PASS" in sec:
                        candidates.insert(0, (sec["OTA_USER"], sec["OTA_PASS"]))
            except Exception:
                pass
        for cu, cp in candidates:
            try:
                r = requests.post(f"{self.base_url}/api/auth/verify", auth=(cu, cp), timeout=1.5)
                if r.status_code == 200:
                    return (cu, cp)
            except Exception:
                pass
        return candidates[0]

    def notify_esp_console(self, msg):
        # Best effort (C-30): LOG needs the login; a 401 without the variable is ignored
        try:
            requests.post(f"{self.base_url}/api/console/send", data={"cmd": f"LOG {msg}"},
                          auth=self.console_auth(), timeout=2.0)
        except Exception:
            pass

    def log_header(self, text):
        print(f"\n{'='*74}\n  {text}\n{'='*74}")

    def log_test(self, test_id, name, status, latency_ms, details, idx=None, total=None):
        status_tag = f"[\033[92mPASS\033[0m]" if status == "PASS" else f"[\033[91mFAIL\033[0m]" if status == "FAIL" else f"[\033[93mSKIP\033[0m]"
        prefix = f"[{idx:>3}/{total}] " if idx is not None and total is not None else ""
        print(f"  {prefix}{status_tag} {test_id:<14} {name:<42} ({latency_ms:5.1f} ms)", flush=True)
        if details:
            print(f"         \033[90m-> {details}\033[0m", flush=True)


# Step 9 (FR-58): Auto-supply credentials for protected endpoints called by existing test suites,
# unless opted out via X-No-Auth header.
_orig_post = requests.post

def _auto_auth_post(url, *args, **kwargs):
    headers = kwargs.get("headers")
    if headers and headers.get("X-No-Auth") == "1":
        headers.pop("X-No-Auth")
        return _orig_post(url, *args, **kwargs)
    if "auth" not in kwargs:
        for ep in ("/api/boiler/", "/api/mode", "/api/relay/test", "/api/diag/clear",
                  "/api/diag/exercise", "/api/config/import", "/api/setup/actuator",
                  "/api/setup/pump", "/api/setup/sensor-timeout", "/api/config/system",
                  "/api/web/"):
            if ep in url:
                kwargs["auth"] = SuiteBase().api_auth()
                break
    return _orig_post(url, *args, **kwargs)

requests.post = _auto_auth_post
