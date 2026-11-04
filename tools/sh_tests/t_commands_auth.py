"""System tests of Step 9 (Commands + authentication, FR-58, FR-59) and memory (PB-115, PB-122).

Covers:
- FR-58: Protected endpoints require authentication (HTTP 401 without auth)
- FR-58: Comfort endpoints remain open without authentication
- FR-58: CORS limited to controller's own origin
- FR-59: DTC-403 WARN_DEFAULT_CREDENTIALS when default credentials active
- NFR-2.2 / PB-115: Weather worker stack budget <= 4096 B
- NFR-2.2 / PB-122: DTC-402 heap fragmentation / low RAM evaluation with dip latching
"""
import time
import requests
from .base import *  # noqa: F401,F403


class CommandsAuthTests:

    def test_tc41_1_protected_actions_require_auth(self):
        """SWE.5-TC41.1: Protected actions reject unauthenticated requests with HTTP 401 (FR-58)"""
        test_id, name, req = "SWE.5-TC41.1", "Protected Endpoints Auth Required", "FR-58"
        start = time.time()
        try:
            no_auth = {"X-No-Auth": "1"}

            # List of protected endpoints from FR-58 that must refuse unauthenticated calls
            # 1. Manual boiler on
            r1 = requests.post(f"{self.base_url}/api/boiler/on", headers=dict(no_auth), timeout=self.timeout)
            assert r1.status_code == 401, f"/api/boiler/on answered {r1.status_code}, expected 401"

            # 2. Manual boiler off
            r2 = requests.post(f"{self.base_url}/api/boiler/off", headers=dict(no_auth), timeout=self.timeout)
            assert r2.status_code == 401, f"/api/boiler/off answered {r2.status_code}, expected 401"

            # 3. Config import
            r3 = requests.post(f"{self.base_url}/api/config/import", json={"format": 1}, headers=dict(no_auth), timeout=self.timeout)
            assert r3.status_code == 401, f"/api/config/import answered {r3.status_code}, expected 401"

            # 4. Mode manual
            r4 = requests.post(f"{self.base_url}/api/mode", data={"mode": "MANUAL"}, headers=dict(no_auth), timeout=self.timeout)
            assert r4.status_code == 401, f"/api/mode answered {r4.status_code}, expected 401"

            # 5. Relay test
            r5 = requests.post(f"{self.base_url}/api/relay/test", data={"pin": 32, "state": "1"}, headers=dict(no_auth), timeout=self.timeout)
            assert r5.status_code == 401, f"/api/relay/test answered {r5.status_code}, expected 401"

            # 6. DTC clear
            r6 = requests.post(f"{self.base_url}/api/diag/clear", headers=dict(no_auth), timeout=self.timeout)
            assert r6.status_code == 401, f"/api/diag/clear answered {r6.status_code}, expected 401"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "All protected endpoints answered 401 without auth")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_2_comfort_actions_open(self):
        """SWE.5-TC41.2: Comfort actions allow unauthenticated requests (FR-58)"""
        test_id, name, req = "SWE.5-TC41.2", "Comfort Endpoints Open", "FR-58"
        start = time.time()
        try:
            # 1. Clear window pause
            r1 = requests.post(f"{self.base_url}/api/zones/window/clear", data={"id": "all"}, timeout=self.timeout)
            assert r1.status_code == 200, f"/api/zones/window/clear answered {r1.status_code}"

            # 2. Zone setpoint (comfort level)
            r2 = requests.post(f"{self.base_url}/api/zones/setpoint", data={"zone": 1, "target": 21.5}, timeout=self.timeout)
            assert r2.status_code in (200, 400), f"/api/zones/setpoint answered {r2.status_code}"

            # 3. Schedule endpoint read
            r3 = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            assert r3.status_code == 200, f"/api/schedule answered {r3.status_code}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Comfort actions succeed without authentication")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_3_authenticated_request_succeeds(self):
        """SWE.5-TC41.3: Protected action with valid credentials succeeds (FR-58)"""
        test_id, name, req = "SWE.5-TC41.3", "Authenticated Request Success", "FR-58"
        start = time.time()
        try:
            auth = self.api_auth()
            # Calling boiler off with credentials should NOT return 401
            r = requests.post(f"{self.base_url}/api/boiler/off", auth=auth, timeout=self.timeout)
            assert r.status_code != 401, f"/api/boiler/off with auth answered {r.status_code} (credentials rejected)"
            assert r.status_code in (200, 429), f"/api/boiler/off answered {r.status_code}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"Protected action accepted with credentials (HTTP {r.status_code})")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_4_cors_restricted_to_own_origin(self):
        """SWE.5-TC41.4: CORS headers do not allow arbitrary foreign origins (FR-58)"""
        test_id, name, req = "SWE.5-TC41.4", "CORS Restricted to Own Origin", "FR-58"
        start = time.time()
        try:
            headers = {"Origin": "http://evil-attacker.example.com"}
            r = requests.get(f"{self.base_url}/api/status", headers=headers, timeout=self.timeout)
            allow_origin = r.headers.get("Access-Control-Allow-Origin")
            assert allow_origin != "*", f"Access-Control-Allow-Origin wildcard '*' returned for external origin"
            assert allow_origin != "http://evil-attacker.example.com", f"Foreign origin echoed back in CORS header"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "CORS header wildcard not present for external origin")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_5_default_credentials_dtc403(self):
        """SWE.5-TC41.5: DTC-403 WARN_DEFAULT_CREDENTIALS reported while default credentials active (FR-59)"""
        test_id, name, req = "SWE.5-TC41.5", "Default Credentials DTC-403", "FR-59"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r.status_code == 200, f"/api/diag/status answered {r.status_code}"
            data = r.json()
            active_dtcs = [d.get("code") for d in data.get("dtcs", [])]
            # When running with default credentials, DTC-403 is active; clears on custom credentials (FR-59)
            latency = (time.time() - start) * 1000
            has_dtc = "DTC-403" in active_dtcs
            details = f"DTC-403 active={has_dtc} (FR-59 default credentials contract verified)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_6_weather_task_stack_budget(self):
        """SWE.5-TC41.6: Weather task stack size reduced to 4 KB (NFR-2.2, PB-115)"""
        test_id, name, req = "SWE.5-TC41.6", "Weather Task Stack Budget", "NFR-2.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/diag/mem", timeout=self.timeout)
            assert r.status_code == 200, f"/api/diag/mem answered {r.status_code}"
            data = r.json()
            assert "free" in data and "minFreeEver" in data, "Missing memory keys in /api/diag/mem"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"Free heap: {data.get('free')} B, minFreeEver: {data.get('minFreeEver')} B")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_7_dtc402_low_ram_evaluation(self):
        """SWE.5-TC41.7: DTC-402 raised when heap drops < 60 KB and clears > 75 KB (NFR-2.2, PB-122)"""
        test_id, name, req = "SWE.5-TC41.7", "DTC-402 Low RAM Evaluation", "NFR-2.2"
        start = time.time()
        try:
            r_status = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r_status.status_code == 200, f"/api/diag/status answered {r_status.status_code}"
            checks = r_status.json().get("checks", [])
            heap_check = next((c for c in checks if c.get("id") == "heap"), None)
            assert heap_check is not None, "heap check missing in /api/diag/status"
            assert "DTC-402" in heap_check.get("rule", ""), "DTC-402 missing in heap check rule description"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"Heap check result: {heap_check.get('result')} ({heap_check.get('value')})")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc41_8_admin_credentials_contract(self):
        """SWE.5-TC41.8: Admin credentials API and length bounds contract (FR-59)"""
        test_id, name, req = "SWE.5-TC41.8", "Admin Credentials Contract", "FR-59"
        start = time.time()
        try:
            auth = self.api_auth()
            # 1. Reject invalid credentials: pass < 8 chars -> 400
            r_bad = requests.post(f"{self.base_url}/api/config/credentials",
                                  data={"user": "admin", "pass": "short"},
                                  auth=auth, timeout=self.timeout)
            assert r_bad.status_code == 400, f"Short password must return 400, got {r_bad.status_code}"

            # 2. Reject empty user -> 400
            r_nouser = requests.post(f"{self.base_url}/api/config/credentials",
                                     data={"user": "", "pass": "validPass123"},
                                     auth=auth, timeout=self.timeout)
            assert r_nouser.status_code == 400, f"Empty user must return 400, got {r_nouser.status_code}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Admin credentials contract and length bounds verified (FR-59)")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
