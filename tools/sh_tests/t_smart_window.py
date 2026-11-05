"""System tests for Step 14 Part F (SmartWindow): FR-40.2.

Covers:
- SWE.5-TC51.1: SmartWindow status contract (/api/window/status) reporting slope, absolute humidity, and dT
- SWE.5-TC51.2: Absolute humidity computation contract (Magnus formula) from room temperature and relative humidity
- SWE.5-TC51.3: dT < 5 C suspension contract: window detection suspended when room - outdoor < 5 C (FR-40.2)
- SWE.5-TC51.4: Fallback contract to FR-40 when outdoor or humidity data is missing (FR-40.2, NFR-5.1)
"""
import time
import requests
from .base import SuiteBase, TestResult


class SmartWindowTests:
    def test_tc51_1_smart_window_status_contract(self):
        """SWE.5-TC51.1: SmartWindow status report contract (/api/window/status) (FR-40.2)"""
        test_id, name, req = "SWE.5-TC51.1", "Smart Window Status Contract", "FR-40.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/window/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "outdoorTemp" in j, "Missing outdoorTemp in /api/window/status"
            assert "zones" in j and isinstance(j["zones"], list), "Missing zones list in /api/window/status"
            for z in j.get("zones", []):
                assert "index" in z, "Zone missing index"
                assert "temp" in z, "Zone missing temp"
                assert "humidity" in z, "Zone missing humidity"
                assert "absoluteHumidity" in z, "Zone missing absoluteHumidity"
                assert "tempSlope" in z, "Zone missing tempSlope"
                assert "deltaOutdoor" in z, "Zone missing deltaOutdoor"
                assert "windowOpen" in z, "Zone missing windowOpen"
                assert "suspended" in z, "Zone missing suspended"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "SmartWindow status report contract verified (/api/window/status)")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc51_2_absolute_humidity_magnus_formula(self):
        """SWE.5-TC51.2: Absolute humidity computation from T and RH (FR-40.2)"""
        test_id, name, req = "SWE.5-TC51.2", "Absolute Humidity Computation", "FR-40.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/window/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            for z in j.get("zones", []):
                t = z.get("temp", 0.0)
                rh = z.get("humidity", 0.0)
                ah = z.get("absoluteHumidity", 0.0)
                if t > 15.0 and rh > 30.0:
                    # At ~23 C and 60% RH, absolute humidity is typically ~12..14 g/m3
                    assert 5.0 <= ah <= 25.0, f"Absolute humidity out of physical bounds: {ah} g/m3"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Absolute humidity values validated against physical bounds")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc51_3_dt_suspension_rule(self):
        """SWE.5-TC51.3: Detection suspended while room - outdoor < 5 C (FR-40.2)"""
        test_id, name, req = "SWE.5-TC51.3", "dT Suspension Rule", "FR-40.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/window/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            for z in j.get("zones", []):
                dT = z.get("deltaOutdoor", 0.0)
                susp = z.get("suspended", False)
                if dT < 5.0:
                    assert susp is True, f"Expected suspended=True when deltaOutdoor < 5 C, got {susp}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Detection correctly suspended when room - outdoor < 5 C")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc51_4_fallback_to_fr40_without_data(self):
        """SWE.5-TC51.4: Fallback contract to FR-40 baseline when outdoor/humidity missing (FR-40.2, NFR-5.1)"""
        test_id, name, req = "SWE.5-TC51.4", "SmartWindow Fallback to FR-40", "FR-40.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/window/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "fallbackRule" in j, "Missing fallbackRule in /api/window/status"
            assert j["fallbackRule"] == "FR-40", f"Expected fallbackRule FR-40, got {j.get('fallbackRule')}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Fallback to FR-40 rule contract verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
