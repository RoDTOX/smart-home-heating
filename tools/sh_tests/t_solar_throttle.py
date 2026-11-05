"""System tests for Step 14 Part C (SolarThrottle): FR-70.

Covers:
- SWE.5-TC48.1: SolarThrottle parameter bounds (solarLimit 200..800 W/m2, solarReduction 10..60%)
- SWE.5-TC48.2: Solar status contract (/api/solar/status) reporting irradiance forecast and zone factors
- SWE.5-TC48.3: Zone solarGain flag contract via /api/zones and setup
- SWE.5-TC48.4: Weather lost fallback disables solar reduction completely (0% reduction)
"""
import time
import requests
from .base import SuiteBase, TestResult


class SolarThrottleTests:
    def test_tc48_1_solar_param_bounds(self):
        """SWE.5-TC48.1: solarLimit [200..800 W/m2] & solarReduction [10..60%] bounds (FR-70, §6)"""
        test_id, name, req = "SWE.5-TC48.1", "Solar Parameter Bounds", "FR-70"
        start = time.time()
        try:
            auth = self.api_auth()
            # solarLimit: 200..800, default 400
            r = requests.post(f"{self.base_url}/api/config/system", data={"solarLimit": "100"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"solarLimit=100 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"solarLimit": "900"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"solarLimit=900 must return 400, got {r.status_code}"

            # solarReduction: 10..60, default 30
            r = requests.post(f"{self.base_url}/api/config/system", data={"solarReduction": "5"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"solarReduction=5 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"solarReduction": "75"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"solarReduction=75 must return 400, got {r.status_code}"

            # Valid values
            r = requests.post(f"{self.base_url}/api/config/system", data={"solarLimit": "400", "solarReduction": "30"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"Valid solar parameters must return 200, got {r.status_code}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "solarLimit and solarReduction bounds correctly enforced")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc48_2_solar_status_contract(self):
        """SWE.5-TC48.2: Solar status report contract (/api/solar/status) (FR-70)"""
        test_id, name, req = "SWE.5-TC48.2", "Solar Status Contract", "FR-70"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/solar/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "forecastIrradiance" in j, "Missing forecastIrradiance in /api/solar/status"
            assert "solarLimit" in j, "Missing solarLimit in /api/solar/status"
            assert "solarReduction" in j, "Missing solarReduction in /api/solar/status"
            assert "zones" in j, "Missing zones array in /api/solar/status"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"Solar status active: forecast={j['forecastIrradiance']} W/m2, zones={len(j['zones'])}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc48_3_zone_solargain_flag(self):
        """SWE.5-TC48.3: Zone solarGain exposed and persisted (FR-70)"""
        test_id, name, req = "SWE.5-TC48.3", "Zone SolarGain Flag Contract", "FR-70"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "zones" in j and len(j["zones"]) > 0, "No zones in /api/zones"
            for z in j["zones"]:
                assert "solarGain" in z, f"Missing solarGain flag in zone {z.get('id')}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "solarGain field present in all zone status entries")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc48_4_weather_lost_disables_reduction(self):
        """SWE.5-TC48.4: Weather lost fallback disables solar reduction completely (FR-70, NFR-5.1)"""
        test_id, name, req = "SWE.5-TC48.4", "Weather Lost Solar Fallback", "FR-70"
        start = time.time()
        try:
            auth = self.api_auth()
            # Inject weather-lost
            r = requests.post(f"{self.base_url}/api/diag/inject", json={"point": "weather-lost"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"Inject weather-lost failed: {r.status_code}"

            time.sleep(0.5)
            r = requests.get(f"{self.base_url}/api/solar/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            # Under weather lost, throttling must be inactive for all zones
            for z in j.get("zones", []):
                assert z.get("throttled") is False, f"Zone {z.get('zone')} should not be throttled during weather loss"

            # Clear injection
            requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Solar reduction safely bypassed on weather-lost")
        except Exception as e:
            try:
                requests.post(f"{self.base_url}/api/diag/inject/clear", auth=self.api_auth(), timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
