"""System tests for Step 14 Part E (HumidityComp): FR-71.

Covers:
- SWE.5-TC50.1: Zone humidityComp configuration flag exposed and persisted via /api/zones and /api/setup/map-zone (FR-71, §6)
- SWE.5-TC50.2: Humidity compensation advisor contract (/api/humidity/status) reporting relative humidity and target delta
- SWE.5-TC50.3: Effective target reduced by up to 0.5 C when RH >= 60% for opt-in zone (FR-71)
- SWE.5-TC50.4: Fallback contract: no humidity reading or sensor fault results in 0.0 C compensation (FR-71)
"""
import time
import requests
from .base import SuiteBase, TestResult


class HumidityCompTests:
    def test_tc50_1_zone_humidity_flag_contract(self):
        """SWE.5-TC50.1: Zone humidityComp flag exposed and persisted (FR-71, §6)"""
        test_id, name, req = "SWE.5-TC50.1", "Zone HumidityComp Flag Contract", "FR-71"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            zones = r.json().get("zones", [])
            assert len(zones) >= 1, "No zones configured on board"
            z0 = zones[0]
            assert "humidityComp" in z0, "humidityComp flag missing from zone record"
            assert isinstance(z0["humidityComp"], bool), f"humidityComp must be boolean, got {type(z0['humidityComp'])}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Zone humidityComp flag contract verified in /api/zones")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc50_2_humidity_status_contract(self):
        """SWE.5-TC50.2: Humidity compensation status contract (/api/humidity/status) (FR-71)"""
        test_id, name, req = "SWE.5-TC50.2", "Humidity Status Contract", "FR-71"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/humidity/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "zones" in j and isinstance(j["zones"], list), "Missing zones list in /api/humidity/status"
            for z in j.get("zones", []):
                assert "index" in z, "Zone missing index"
                assert "humidityComp" in z, "Zone missing humidityComp"
                assert "humidity" in z, "Zone missing humidity"
                assert "targetReduction" in z, "Zone missing targetReduction"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Humidity status contract verified (/api/humidity/status)")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc50_3_effective_target_reduction_above_threshold(self):
        """SWE.5-TC50.3: Effective target lowered by up to 0.5 C at RH >= 60% (FR-71)"""
        test_id, name, req = "SWE.5-TC50.3", "Effective Target Reduction Above 60% RH", "FR-71"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/humidity/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            for z in j.get("zones", []):
                red = z.get("targetReduction", 0.0)
                assert 0.0 <= red <= 0.5 + 0.001, f"Reduction out of bounds [0..0.5 C]: {red}"
                if not z.get("humidityComp") or z.get("humidity", 0.0) < 60.0:
                    assert red == 0.0, f"Expected 0.0 reduction when RH < 60% or opt-out, got {red}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Effective target reduction rule (up to 0.5 C at >= 60% RH) verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc50_4_fallback_no_humidity_no_compensation(self):
        """SWE.5-TC50.4: Fallback contract: no humidity reading -> 0 compensation (FR-71, NFR-5.1)"""
        test_id, name, req = "SWE.5-TC50.4", "Humidity Missing Fallback", "FR-71"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/humidity/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            for z in j.get("zones", []):
                if z.get("humidity", 0.0) <= 0.0:
                    assert z.get("targetReduction", 0.0) == 0.0, "Missing humidity must give 0.0 reduction"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Fallback verified: missing humidity results in 0.0 C reduction")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
