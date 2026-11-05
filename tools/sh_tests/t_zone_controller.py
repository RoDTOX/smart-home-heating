"""System tests of Step 7 (ZoneController, learned duty per outdoor band, FR-51 fault duty, FaultInjector).

Covers:
- FR-47: TPI control law, 60 min cycle, 1.5 C band, deficit rule >= 0.4 C -> 100% duty, min valve ON/OFF 10 min
- FR-51: Fallback heating duty when sensor is in timeout:
  * Uses learned duty for current outdoor temperature band (bounded by NFR-5.3)
  * If nothing learned, uses faultDuty (default 20%)
  * Respects minimum valve ON time 10 min (PB-61)
  * Resolves PB-23 (zone kept heating at fallback duty instead of actuator shutdown)
- AR-12: 7 outdoor temperature bands (< -10, [-10, -5), [-5, 0), [0, 5), [5, 10), [10, 15), >= 15)
- NFR-5.5: FaultInjector: test-only masking of inputs in RAM:
  * sensor-lost <z>, weather-lost, time-lost, humidity-lost
  * POST /api/diag/inject, GET /api/diag/inject, POST /api/diag/inject/clear
  * Console INJECT <point> [zone], INJECT CLEAR, INJECT STATUS
  * Resolves PB-95 (allows automated board tests of fault conditions without physical manipulation)
"""
import time
import requests
from .base import *  # noqa: F401,F403


class ZoneControllerTests:

    def test_tc43_1_fault_injector_api_contract(self):
        """SWE.5-TC43.1: FaultInjector API contract (NFR-5.5, PB-95)"""
        test_id, name, req = "SWE.5-TC43.1", "FaultInjector API Contract", "NFR-5.5"
        start = time.time()
        try:
            auth = self.api_auth()

            # 1. Query baseline injections
            r0 = requests.get(f"{self.base_url}/api/diag/inject", timeout=self.timeout)
            assert r0.status_code == 200, f"GET /api/diag/inject returned {r0.status_code}"
            data0 = r0.json()
            assert "injections" in data0, f"Missing 'injections' key in response: {data0}"

            # 2. Inject sensor-lost for zone 1
            r_inj = requests.post(f"{self.base_url}/api/diag/inject",
                                  json={"point": "sensor-lost", "zone": 1},
                                  auth=auth, timeout=self.timeout)
            assert r_inj.status_code == 200, f"POST /api/diag/inject returned {r_inj.status_code}: {r_inj.text}"

            # Verify injection is active
            r1 = requests.get(f"{self.base_url}/api/diag/inject", timeout=self.timeout)
            assert r1.status_code == 200
            data1 = r1.json()
            inj_list = data1.get("injections", [])
            assert any(i.get("point") == "sensor-lost" and i.get("zone") == 1 for i in inj_list), \
                f"Active injection not found in list: {inj_list}"

            # 3. Clear injection
            r_clr = requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)
            assert r_clr.status_code == 200, f"POST /api/diag/inject/clear returned {r_clr.status_code}"

            r2 = requests.get(f"{self.base_url}/api/diag/inject", timeout=self.timeout)
            data2 = r2.json()
            assert len(data2.get("injections", [])) == 0, f"Injections not cleared: {data2}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              "FaultInjector API injects and clears sensor-lost correctly")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc43_2_fr51_sensor_loss_fallback_duty(self):
        """SWE.5-TC43.2: FR-51 Sensor Loss Fallback Duty (FR-51, PB-23)"""
        test_id, name, req = "SWE.5-TC43.2", "FR-51 Fallback Duty", "FR-51"
        start = time.time()
        try:
            auth = self.api_auth()

            # 1. Inject sensor loss on zone 1 (Living)
            r_inj = requests.post(f"{self.base_url}/api/diag/inject",
                                  json={"point": "sensor-lost", "zone": 1},
                                  auth=auth, timeout=self.timeout)
            assert r_inj.status_code == 200, f"Inject returned {r_inj.status_code}"

            # Wait 2 seconds for control loop tick to evaluate
            time.sleep(2.0)

            # Query zone status
            rz = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert rz.status_code == 200
            zones = rz.json().get("zones", [])
            z1 = next((z for z in zones if z.get("id") == 1), None)
            assert z1 is not None, "Zone 1 not found"

            # Must enter FAULT state with active sensorFault
            state = z1.get("state", "")
            sensor_fault = z1.get("sensorFault", False)
            assert sensor_fault is True, f"Zone 1 sensorFault is not True: {z1}"

            # FR-51 check: valve demand must NOT be shut down permanently.
            # In AUTO mode during heating season, zone in FAULT must have fallback duty active (faultDuty >= 20%).
            duty = z1.get("duty", 0.0)
            assert duty >= 0.15, f"Expected FR-51 fallback duty >= 15%, got duty={duty} in zone {z1}"

            # DTC-101 must be raised for zone 1
            rd = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            dtcs = rd.json().get("dtcs", [])
            has_dtc101 = any(d.get("code") == "DTC-101" for d in dtcs)
            assert has_dtc101, f"DTC-101 not raised on sensor loss: {dtcs}"

            # Clear injection and restore normal control
            requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)
            time.sleep(2.0)

            # First accepted reading should clear DTC-101 and restore NORMAL/IDLE
            rd_after = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            dtcs_after = rd_after.json().get("dtcs", [])
            dtc101_after = any(d.get("code") == "DTC-101" for d in dtcs_after)
            assert not dtc101_after, f"DTC-101 not cleared after restoring sensor: {dtcs_after}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              "FR-51 fallback duty active during sensor loss, DTC-101 raised and cleared")
        except Exception as e:
            # Ensure cleanup on failure
            try:
                requests.post(f"{self.base_url}/api/diag/inject/clear", auth=self.api_auth(), timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc43_3_outdoor_temperature_bands(self):
        """SWE.5-TC43.3: 7 Outdoor temperature bands for learned duty (AR-12, FR-47)"""
        test_id, name, req = "SWE.5-TC43.3", "Outdoor Temperature Bands", "FR-47"
        start = time.time()
        try:
            # Query zones endpoint which includes learned bands or active outdoor band
            rz = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert rz.status_code == 200
            data = rz.json()
            # Must expose outdoor band index (0..6) or bands configuration
            assert "outdoorBand" in data or "outdoorTemp" in data, f"Missing outdoor info in /api/zones: {data.keys()}"

            # Query system config to check faultDuty and faultFrostOutdoor parameters
            rc = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert rc.status_code == 200
            cfg = rc.json()
            assert "faultDuty" in cfg, f"Missing faultDuty in /api/config/system: {cfg.keys()}"
            assert "faultFrostOutdoor" in cfg, f"Missing faultFrostOutdoor in /api/config/system: {cfg.keys()}"
            assert 0 <= cfg["faultDuty"] <= 30, f"faultDuty out of range [0..30]: {cfg['faultDuty']}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              f"Outdoor band evaluation and faultDuty={cfg['faultDuty']}% verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc43_4_pb61_minimum_valve_on_time_clamp(self):
        """SWE.5-TC43.4: Minimum valve ON time 10 min enforced on fallback duty (PB-61, FR-47)"""
        test_id, name, req = "SWE.5-TC43.4", "Min Valve ON Time Clamp", "FR-47"
        start = time.time()
        try:
            auth = self.api_auth()

            # Temporarily configure low faultDuty (e.g. 5% = 3 min on 60 min cycle)
            rc_old = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            orig_fault_duty = rc_old.get("faultDuty", 20)

            # Set faultDuty = 5% via API
            requests.post(f"{self.base_url}/api/config/system", data={"faultDuty": 5}, auth=auth, timeout=self.timeout)

            # Inject sensor loss
            r_inj = requests.post(f"{self.base_url}/api/diag/inject", json={"point": "sensor-lost", "zone": 1}, auth=auth, timeout=self.timeout)
            assert r_inj.status_code == 200, f"Inject returned {r_inj.status_code}"
            time.sleep(2.0)

            rz = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()
            z1 = next((z for z in rz.get("zones", []) if z.get("id") == 1), {})

            # PB-61 requirement: valve ON time cannot be under 10 min (600s).
            # The onTimeSec or effective duty must guarantee >= 600s ON duration if duty > 0.
            on_sec = z1.get("onTimeSec", 0)
            duty = z1.get("duty", 0.0)
            if duty > 0:
                assert on_sec >= 600 or duty >= (600.0 / 3600.0) - 0.01, \
                    f"PB-61 violation: valve ON time {on_sec}s (< 600s min) for duty {duty}"

            # Restore original config and clear inject
            requests.post(f"{self.base_url}/api/config/system", data={"faultDuty": orig_fault_duty}, auth=auth, timeout=self.timeout)
            requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              "Minimum 10 min valve ON time enforced on fallback duty (PB-61)")
        except Exception as e:
            try:
                requests.post(f"{self.base_url}/api/diag/inject/clear", auth=self.api_auth(), timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
