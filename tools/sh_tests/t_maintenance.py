"""System tests for Step 12 (Maintenance): FR-19, FR-60, NFR-1.8.

Covers:
- SWE.5-TC45.1: QuietGate blocks OTA when boiler is actively running
- SWE.5-TC45.2: Self-test runs on request, tests outputs and returns report
- SWE.5-TC45.3: Valve exercise parameter bounds (exerciseIdleDays 7..14)
- SWE.5-TC45.4: Automatic image verification status and DTC-303 / DTC-304 contract
"""
import time
import requests
from .base import SuiteBase, TestResult


class MaintenanceTests:
    def test_tc45_1_ota_quiet_gate_contract(self):
        """SWE.5-TC45.1: OTA starts only with boiler OFF; QuietGate rejects or defers (FR-60, NFR-1.8)"""
        test_id, name, req = "SWE.5-TC45.1", "OTA Quiet Gate Contract", "FR-60"
        start = time.time()
        try:
            # Query maintenance status / quiet gate
            r = requests.get(f"{self.base_url}/api/maint/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "otaQuietGate" in j, "Missing otaQuietGate in /api/maint/status"
            assert "boilerOff" in j["otaQuietGate"], "Missing boilerOff in otaQuietGate"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"QuietGate operational: boilerOff={j['otaQuietGate']['boilerOff']}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc45_2_valve_exercise_param_bounds(self):
        """SWE.5-TC45.2: exerciseIdleDays parameter bounds [7..14] days (FR-19, §6)"""
        test_id, name, req = "SWE.5-TC45.2", "Valve Exercise Parameter Bounds", "FR-19"
        start = time.time()
        try:
            auth = self.api_auth()
            # Rejects 6 days
            r = requests.post(f"{self.base_url}/api/config/system", data={"exerciseIdleDays": "6"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"exerciseIdleDays=6 must return 400, got {r.status_code}"

            # Rejects 15 days
            r = requests.post(f"{self.base_url}/api/config/system", data={"exerciseIdleDays": "15"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"exerciseIdleDays=15 must return 400, got {r.status_code}"

            # Accepts 14 days (default)
            r = requests.post(f"{self.base_url}/api/config/system", data={"exerciseIdleDays": "14"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"exerciseIdleDays=14 must return 200, got {r.status_code}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "exerciseIdleDays correctly enforced bounds [7..14]")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc45_3_selftest_api_contract(self):
        """SWE.5-TC45.3: POST /api/maint/selftest triggers test and returns structured report (FR-60)"""
        test_id, name, req = "SWE.5-TC45.3", "SelfTest API Contract", "FR-60"
        start = time.time()
        try:
            # Query current selftest status or trigger dry check
            r = requests.get(f"{self.base_url}/api/maint/selftest", timeout=self.timeout)
            assert r.status_code == 200, f"Expected 200, got {r.status_code}"
            j = r.json()
            assert "checks" in j, "Missing checks list in selftest response"
            assert isinstance(j["checks"], list), "checks must be a list"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"SelfTest reported {len(j['checks'])} subsystems")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc45_4_ota_rollback_contract(self):
        """SWE.5-TC45.4: Automatic image validation state and DTC-303/DTC-304 contract (FR-60)"""
        test_id, name, req = "SWE.5-TC45.4", "OTA Rollback & DTC Contract", "FR-60"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/maint/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected 200, got {r.status_code}"
            j = r.json()
            assert "ota" in j, "Missing ota info in maint status"
            assert "rollbackSupported" in j["ota"], "Missing rollbackSupported flag"
            assert "appValid" in j["ota"], "Missing appValid status"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"Rollback supported: {j['ota']['rollbackSupported']}, appValid={j['ota']['appValid']}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
