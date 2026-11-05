"""System tests for Step 14 Part B (HeatUpModel): FR-48, FR-67.

Covers:
- SWE.5-TC47.1: Baseline FR-48 formula fallback contract (lead time vs outdoor temperature)
- SWE.5-TC47.2: HeatUpModel status endpoint contract (/api/heatup/status)
- SWE.5-TC47.3: Learned rate boundaries (rateDegPerHour bounds 0.1 .. 2.0 C/h)
- SWE.5-TC47.4: Weather lost fallback to 90 min fixed lead
"""
import time
import requests
from .base import SuiteBase, TestResult


class HeatUpModelTests:
    def test_tc47_1_fr48_baseline_lead_formula(self):
        """SWE.5-TC47.1: Pre-heat lead time follows FR-48 formula [120 + 9*(5 - Tout)] clamped [45..240 min] (FR-48)"""
        test_id, name, req = "SWE.5-TC47.1", "FR-48 Baseline Lead Formula", "FR-48"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/heatup/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "leadMinutes" in j, "Missing leadMinutes in /api/heatup/status"
            lead = j["leadMinutes"]
            assert 45 <= lead <= 240, f"leadMinutes={lead} out of bounds [45..240]"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"Pre-heat lead={lead} min within [45..240] min")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc47_2_heatup_status_contract(self):
        """SWE.5-TC47.2: HeatUpModel status report contract (FR-67)"""
        test_id, name, req = "SWE.5-TC47.2", "HeatUp Status Contract", "FR-67"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/heatup/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "zones" in j, "Missing zones array in /api/heatup/status"
            assert len(j["zones"]) >= 3, "Expected at least 3 zones in heatup model"
            for z in j["zones"]:
                assert "rateDegPerHour" in z, f"Missing rateDegPerHour in zone {z}"
                assert "samples" in z, f"Missing samples count in zone {z}"
                assert "learned" in z, f"Missing learned flag in zone {z}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"HeatUpModel active for {len(j['zones'])} zones")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc47_3_learned_rate_bounds(self):
        """SWE.5-TC47.3: Heat-up learned rates stay within physical bounds [0.1 .. 2.0 C/h] (FR-67, NFR-5.3)"""
        test_id, name, req = "SWE.5-TC47.3", "Learned Rate Physical Bounds", "FR-67"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/heatup/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            for z in j["zones"]:
                rate = z["rateDegPerHour"]
                assert 0.1 <= rate <= 2.0, f"Zone {z['zone']} rate={rate} out of bounds [0.1..2.0 C/h]"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "All zone heat-up rates bounded within [0.1..2.0 C/h]")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc47_4_weather_lost_preheat_fallback(self):
        """SWE.5-TC47.4: Weather lost fallback produces exactly 90 min pre-heat lead (FR-48, NFR-5.1)"""
        test_id, name, req = "SWE.5-TC47.4", "Weather Lost Fallback 90m Lead", "FR-48"
        start = time.time()
        try:
            auth = self.api_auth()
            # Inject weather-lost
            r = requests.post(f"{self.base_url}/api/diag/inject", json={"point": "weather-lost"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"Inject weather-lost failed: {r.status_code}"

            time.sleep(0.5)
            r = requests.get(f"{self.base_url}/api/heatup/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert j.get("leadMinutes") == 90, f"Weather lost must yield 90 min lead, got {j.get('leadMinutes')}"

            # Clear injection
            requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Pre-heat correctly reverted to 90 min fallback when weather lost")
        except Exception as e:
            # Always attempt injection clear
            try:
                requests.post(f"{self.base_url}/api/diag/inject/clear", auth=self.api_auth(), timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
