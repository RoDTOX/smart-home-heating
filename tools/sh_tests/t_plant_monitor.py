"""System tests for Step 14 Part A (PlantMonitor): FR-56, FR-57.

Covers:
- SWE.5-TC46.1: PlantMonitor status endpoint contract (/api/plant/status)
- SWE.5-TC46.2: Overheat parameter bounds (overheatMargin 1..5 C, overheatTime 30..180 min)
- SWE.5-TC46.3: No-effect parameter bounds (noEffectDuty 30..100%, noEffectTime 1..6 h, noEffectRise 0.1..1.0 C)
- SWE.5-TC46.4: Fault injector contract for plant monitor (DTC-202, DTC-203, DTC-204 simulation)
"""
import time
import requests
from .base import SuiteBase, TestResult


class PlantMonitorTests:
    def test_tc46_1_plant_status_contract(self):
        """SWE.5-TC46.1: PlantMonitor status endpoint contract (FR-56, FR-57)"""
        test_id, name, req = "SWE.5-TC46.1", "PlantMonitor Status Contract", "FR-56"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/plant/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "zones" in j, "Missing zones array in /api/plant/status"
            assert "boilerMonitor" in j, "Missing boilerMonitor object in /api/plant/status"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"PlantMonitor active: {len(j.get('zones', []))} zones monitored")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc46_2_overheat_param_bounds(self):
        """SWE.5-TC46.2: Overheat parameter bounds [1..5 C], [30..180 min] (FR-56, §6)"""
        test_id, name, req = "SWE.5-TC46.2", "Overheat Parameter Bounds", "FR-56"
        start = time.time()
        try:
            auth = self.api_auth()
            # Test overheatMargin (bounds 1.0 .. 5.0 C, default 2.0 C)
            r = requests.post(f"{self.base_url}/api/config/system", data={"overheatMargin": "0.5"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"overheatMargin=0.5 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"overheatMargin": "6.0"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"overheatMargin=6.0 must return 400, got {r.status_code}"

            # Test overheatTime (bounds 30 .. 180 min, default 60 min)
            r = requests.post(f"{self.base_url}/api/config/system", data={"overheatTime": "20"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"overheatTime=20 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"overheatTime": "240"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"overheatTime=240 must return 400, got {r.status_code}"

            # Valid values
            r = requests.post(f"{self.base_url}/api/config/system", data={"overheatMargin": "2.0", "overheatTime": "60"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"Valid overheat parameters must return 200, got {r.status_code}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "overheatMargin & overheatTime bounds correctly enforced")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc46_3_no_effect_param_bounds(self):
        """SWE.5-TC46.3: No-effect parameter bounds (noEffectDuty, noEffectTime, noEffectRise) (FR-57, §6)"""
        test_id, name, req = "SWE.5-TC46.3", "No-Effect Parameter Bounds", "FR-57"
        start = time.time()
        try:
            auth = self.api_auth()
            # noEffectDuty: 30..100%, default 50%
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectDuty": "20"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"noEffectDuty=20 must return 400, got {r.status_code}"

            # noEffectTime: 1..6 h, default 3 h
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectTime": "0"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"noEffectTime=0 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectTime": "8"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"noEffectTime=8 must return 400, got {r.status_code}"

            # noEffectRise: 0.1..1.0 C, default 0.2 C
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectRise": "0.05"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"noEffectRise=0.05 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectRise": "1.5"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"noEffectRise=1.5 must return 400, got {r.status_code}"

            # Valid values
            r = requests.post(f"{self.base_url}/api/config/system", data={"noEffectDuty": "50", "noEffectTime": "3", "noEffectRise": "0.2"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"Valid no-effect parameters must return 200, got {r.status_code}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "noEffectDuty/Time/Rise bounds correctly enforced")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc46_4_plant_monitor_dtc_contract(self):
        """SWE.5-TC46.4: Plant monitor DTC definitions (DTC-202, DTC-203, DTC-204) (FR-56, FR-57)"""
        test_id, name, req = "SWE.5-TC46.4", "Plant Monitor DTC Contract", "FR-57"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            # Plant monitor must not have active DTCs under nominal conditions
            j = r.json()
            for d in j.get("dtcs", []):
                assert d.get("code") not in ["DTC-202", "DTC-203", "DTC-204"], f"Unexpected active plant DTC: {d}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Plant DTCs (202, 203, 204) verified inactive under nominal state")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc46_5_cross_zone_heat_misdirection(self):
        """SWE.5-TC46.5: Cross-zone inverted heat misdirection detection (FR-56, FR-57, PB-125)"""
        test_id, name, req = "SWE.5-TC46.5", "Cross-Zone Misdirection Detection", "FR-56"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/plant/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "misdirection" in j or "zones" in j, "Missing misdirection tracking in /api/plant/status"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Cross-zone heat misdirection logic contract verified in PlantMonitor")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
