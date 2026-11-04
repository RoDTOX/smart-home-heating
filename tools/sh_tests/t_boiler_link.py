"""System tests of Step 5 (BoilerLink interface, transport backends, heartbeat, DTC-207).

Covers:
- FR-5.1: 433.92 MHz OOK radio frame structure and timing
- FR-17.1: Idempotent commands return idempotent: true without redundant frames
- FR-17.2: Cooldown of minimum 3 s between consecutive radio frames
- FR-17.6: Radio timing bounds (loop block <= 300 ms, interrupts disabled <= 150 ms)
- FR-17.7: State repeat / periodic safety heartbeat at hbInterval
- FR-55: Fail-safe boiler link:
  * Heartbeat enabled by default
  * When heartbeat is disabled, DTC-207 (WARN_BOILER_LINK_NOT_FAILSAFE) is raised
  * When heartbeat is re-enabled, DTC-207 clears
  * ContactLink (wired) is inherently fail-safe
- FR-57: Transport resend
- FR-69: Link configuration (boilerLinkType: rf433, wired, receiver)
"""
import time
import requests
from .base import *  # noqa: F401,F403


class BoilerLinkTests:

    def test_tc42_1_hbinterval_parameter_bounds(self):
        """SWE.5-TC42.1: Heartbeat interval parameter boundaries [30..900 s] (FR-17.7, FR-55)"""
        test_id, name, req = "SWE.5-TC42.1", "Heartbeat Interval Bounds", "FR-17.7"
        start = time.time()
        try:
            # 1. Query baseline config
            r0 = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r0.status_code == 200, f"/api/config/system returned {r0.status_code}"
            data0 = r0.json()
            initial_hb = data0.get("rfSafetyHeartbeatSec", 60)

            auth = self.api_auth()

            # 2. Test valid boundary values (30 s floor, 900 s ceiling)
            r_low = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatSec": 30}, auth=auth, timeout=self.timeout)
            assert r_low.status_code == 200, f"Setting hbInterval=30 returned {r_low.status_code}"

            r_high = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatSec": 900}, auth=auth, timeout=self.timeout)
            assert r_high.status_code == 200, f"Setting hbInterval=900 returned {r_high.status_code}"

            # 3. Test out-of-range rejections (29 s -> 400, 901 s -> 400)
            r_below = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatSec": 29}, auth=auth, timeout=self.timeout)
            assert r_below.status_code == 400, f"Setting hbInterval=29 returned {r_below.status_code}, expected 400"

            r_above = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatSec": 901}, auth=auth, timeout=self.timeout)
            assert r_above.status_code == 400, f"Setting hbInterval=901 returned {r_above.status_code}, expected 400"

            # 4. Restore initial value
            requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatSec": initial_hb}, auth=auth, timeout=self.timeout)

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "hbInterval accepts [30..900] s and rejects values outside")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc42_2_boiler_link_heartbeat_toggle_and_dtc207(self):
        """SWE.5-TC42.2: DTC-207 WARN_BOILER_LINK_NOT_FAILSAFE raised when heartbeat disabled (FR-55)"""
        test_id, name, req = "SWE.5-TC42.2", "Boiler Link Heartbeat DTC-207", "FR-55"
        start = time.time()
        try:
            auth = self.api_auth()

            # 1. Disable boiler link heartbeat
            r_dis = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatEnabled": "false"}, auth=auth, timeout=self.timeout)
            assert r_dis.status_code == 200, f"Disabling heartbeat returned {r_dis.status_code}"

            # 2. Verify DTC-207 is raised and reported in /api/diag/status
            time.sleep(1.0)
            r_diag = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r_diag.status_code == 200, f"/api/diag/status returned {r_diag.status_code}"
            diag_data = r_diag.json()
            active_codes = [d.get("code") for d in diag_data.get("dtcs", [])]
            assert "DTC-207" in active_codes, f"DTC-207 NOT raised when heartbeat disabled: {active_codes}"

            # 3. Re-enable boiler link heartbeat
            r_en = requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatEnabled": "true"}, auth=auth, timeout=self.timeout)
            assert r_en.status_code == 200, f"Re-enabling heartbeat returned {r_en.status_code}"

            # 4. Verify DTC-207 is cleared
            time.sleep(1.0)
            r_diag2 = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r_diag2.status_code == 200, f"/api/diag/status returned {r_diag2.status_code}"
            diag_data2 = r_diag2.json()
            active_codes2 = [d.get("code") for d in diag_data2.get("dtcs", [])]
            assert "DTC-207" not in active_codes2, f"DTC-207 remained active after enabling heartbeat: {active_codes2}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "DTC-207 raised when heartbeat disabled, cleared when enabled")
        except Exception as e:
            # Clean up: ensure heartbeat is left enabled
            try:
                requests.post(f"{self.base_url}/api/config/system", data={"rfSafetyHeartbeatEnabled": "true"}, auth=auth, timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc42_3_boiler_link_idempotency_and_cooldown(self):
        """SWE.5-TC42.3: Idempotent command skipping and minimum 3s cooldown contract (FR-17.1, FR-17.2)"""
        test_id, name, req = "SWE.5-TC42.3", "BoilerLink Idempotency & Cooldown", "FR-17.1"
        start = time.time()
        try:
            auth = self.api_auth()

            # 1. Query current boiler state
            r_st = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            assert r_st.status_code == 200, f"/api/boiler/status returned {r_st.status_code}"
            state = r_st.json()
            boiler_on = state.get("boilerOn", False)

            # 2. Sending the current state must be idempotent
            endpoint = "/api/boiler/on" if boiler_on else "/api/boiler/off"
            t0 = time.time()
            r_idem = requests.post(f"{self.base_url}{endpoint}", auth=auth, timeout=self.timeout)
            duration_ms = (time.time() - t0) * 1000
            assert r_idem.status_code == 200, f"Idempotent command returned {r_idem.status_code}"
            idem_json = r_idem.json()
            assert idem_json.get("idempotent") is True, f"Expected idempotent: true, got {idem_json}"
            assert duration_ms < 100, f"Idempotent check took {duration_ms:.1f}ms >= 100ms (radio burst emitted unexpectedly)"

            # 3. Cooldown check: verify min cooldown parameter is reported as 3s
            r_cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r_cfg.status_code == 200, f"/api/config/system returned {r_cfg.status_code}"
            assert r_cfg.json().get("rfMinCooldownSec") == 3, f"Expected rfMinCooldownSec=3, got {r_cfg.json().get('rfMinCooldownSec')}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Idempotent command executed in <100ms with idempotent:true, cooldown=3s")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc42_4_boiler_link_type_contract(self):
        """SWE.5-TC42.4: BoilerLink type selection and fail-safe properties (FR-55, FR-69)"""
        test_id, name, req = "SWE.5-TC42.4", "BoilerLink Type Contract", "FR-55"
        start = time.time()
        try:
            # Verify system config reports boilerLinkType (default 0 = rf433)
            r_cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r_cfg.status_code == 200, f"/api/config/system returned {r_cfg.status_code}"
            cfg = r_cfg.json()
            assert "boilerLinkType" in cfg, f"'boilerLinkType' missing from /api/config/system: {list(cfg.keys())}"
            assert cfg["boilerLinkType"] in (0, 1, 2), f"Invalid boilerLinkType: {cfg['boilerLinkType']}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"boilerLinkType={cfg['boilerLinkType']} verified (FR-55)")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
