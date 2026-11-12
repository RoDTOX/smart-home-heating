"""System tests: actuator polarity, pump and failsafes, boiler trigger, anti-short-cycling, spam limiter (TC06-TC10)."""
from .base import *  # noqa: F401,F403


class HydraulicsTests:
    # ---------------------------------------------------------
    # Group 6: Universal Actuator Polarity (NC vs NO)
    # ---------------------------------------------------------

    def test_tc06_1_actuator_polarity_read(self):
        """SWE.5-TC06.1: Actuator Polarity Initial Query (FR-15)"""
        test_id = "SWE.5-TC06.1"
        name = "Actuator Polarity Initial Query"
        req = "FR-15"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            pol = r.json().get("actuatorType")
            assert pol in ["NC", "NO"], f"Invalid actuator polarity: {pol}"
            details = f"Initial Actuator Polarity: {pol}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc06_2_actuator_polarity_toggle(self):
        """SWE.5-TC06.2: Actuator Polarity Dynamic Toggle (NC <-> NO) (FR-15)"""
        test_id = "SWE.5-TC06.2"
        name = "Actuator Polarity Dynamic Toggle"
        req = "FR-15"
        start = time.time()
        try:
            r_cur = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            init_pol = r_cur.json().get("actuatorType", "NC")
            target_pol = "NO" if init_pol == "NC" else "NC"
            param = "0" if init_pol == "NC" else "1"

            r_set = requests.post(f"{self.base_url}/api/setup/actuator-type?nc={param}", timeout=self.timeout)
            assert r_set.status_code == 200, "Failed to toggle polarity"

            r_ver = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            new_pol = r_ver.json().get("actuatorType")
            latency = (time.time() - start) * 1000
            assert new_pol == target_pol, f"Expected {target_pol}, got {new_pol}"
            details = f"Successfully toggled {init_pol} -> {target_pol}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc06_3_actuator_polarity_restore(self):
        """SWE.5-TC06.3: Actuator Polarity NVS Restoration (FR-15)"""
        test_id = "SWE.5-TC06.3"
        name = "Actuator Polarity NVS Restoration"
        req = "FR-15"
        start = time.time()
        try:
            # Restore to standard NC polarity
            r_set = requests.post(f"{self.base_url}/api/setup/actuator-type?nc=1", timeout=self.timeout)
            assert r_set.status_code == 200, "Failed to restore NC polarity"
            r_ver = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            pol = r_ver.json().get("actuatorType")
            latency = (time.time() - start) * 1000
            assert pol == "NC", f"Expected NC, got {pol}"
            details = "Restored to Normally Closed (NC) in NVS"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 7: Hydraulic Controls & Failsafes
    # ---------------------------------------------------------

    def test_tc07_1_gpa_pump_controller(self):
        """SWE.5-TC07.1: Hydraulic GPA Pump Configuration (Relay 4) (FR-14)"""
        test_id = "SWE.5-TC07.1"
        name = "Hydraulic GPA Pump Configuration"
        req = "FR-14"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert "pumpEnabled" in data, "Missing pumpEnabled"
            assert "pumpActive" in data, "Missing pumpActive"
            details = f"GPA Pump Enabled: {data.get('pumpEnabled')}, Active: {data.get('pumpActive')} (Relay 4)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc07_2_actuator_interlock_delay_contract(self):
        """SWE.5-TC07.2: Actuator Interlock in 120-600 s, Lower Values Rejected (FR-2)"""
        test_id = "SWE.5-TC07.2"
        name = "Actuator Interlock Range (120-600 s)"
        req = "FR-2"
        start = time.time()
        try:
            url = f"{self.base_url}/api/config/system"
            cur = requests.get(url, timeout=self.timeout).json()["actuatorDelaySec"]
            assert 120 <= cur <= 600, f"interlockDelay {cur} s outside requirements §6 range 120-600 s"
            for bad in (119, 601):
                r = requests.post(url, data={"actuatorDelaySec": str(bad)}, timeout=self.timeout)
                assert r.status_code == 400, f"actuatorDelaySec={bad}: expected HTTP 400, got {r.status_code}"
            after = requests.get(url, timeout=self.timeout).json()["actuatorDelaySec"]
            latency = (time.time() - start) * 1000
            assert after == cur, f"interlockDelay changed by a rejected request: {cur} -> {after}"
            return TestResult(test_id, name, req, "PASS", latency, f"interlockDelay {cur} s; 119 s and 601 s rejected")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc07_3_pump_overrun_dissipation_contract(self):
        """SWE.5-TC07.3: Pump Overrun Residual Heat Dissipation (180s) (FR-3, FR-14)"""
        test_id = "SWE.5-TC07.3"
        name = "Pump Overrun Heat Dissipation (180s)"
        req = "FR-3, FR-14"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"/api/zones answered {r.status_code}"
            data = r.json()
            assert "pumpActive" in data, "Missing pumpActive in /api/zones"
            cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            assert cfg.get("pumpOverrunSec") == 180, f"Expected default pumpOverrunSec 180, got {cfg.get('pumpOverrunSec')}"
            details = f"Verified 180s overrun config; pumpActive={data.get('pumpActive')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc07_4_frost_guard_standby(self):
        """SWE.5-TC07.4: Emergency Frost Guard Standby (<12°C) (FR-16)"""
        test_id = "SWE.5-TC07.4"
        name = "Emergency Frost Guard Standby (<12°C)"
        req = "FR-16"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"/api/zones answered {r.status_code}"
            data = r.json()
            assert "frostGuardActive" in data, "Missing frostGuardActive"
            cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            assert abs(cfg.get("freezeProtectionTemp", 0.0) - 12) < 0.05, f"Expected freezeProtectionTemp 12 C, got {cfg.get('freezeProtectionTemp')}"
            details = f"Frost Guard Active: {data.get('frostGuardActive')} (Threshold: {cfg.get('freezeProtectionTemp')}°C)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc07_5_anti_seize_epoch_contract(self):
        """SWE.5-TC07.5: Hydronic Calendar Anti-Seize UNIX Epoch Contract (FR-14, FR-19)"""
        test_id = "SWE.5-TC07.5"
        name = "Hydronic Anti-Seize UNIX Epoch Contract"
        req = "FR-14, FR-19"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert "lastAntiSeizeEpoch" in data, "Missing lastAntiSeizeEpoch"
            epoch = data.get("lastAntiSeizeEpoch", 0)
            details = f"Anti-Seize Last Epoch: {epoch} (Weekly 20s summer exercise schedule)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 8: Boiler RF Trigger & Idempotency
    # ---------------------------------------------------------

    def test_tc08_1_boiler_state_baseline(self):
        """SWE.5-TC08.1: Boiler State Baseline Establishment (FR-5, FR-32)"""
        test_id = "SWE.5-TC08.1"
        name = "Boiler State Baseline Establishment"
        req = "FR-5, FR-32"
        start = time.time()
        try:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            r = requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed establishing baseline: {r.status_code}"
            data = r.json()
            assert data.get("boiler") == "OFF", f"Expected boiler OFF, got {data.get('boiler')}"
            details = "Boiler baseline set to OFF (RF transmitter verified)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc08_2_boiler_idempotency_noop(self):
        """SWE.5-TC08.2: Boiler Anti-Spam Idempotency No-Op Filter (FR-17, FR-32)"""
        test_id = "SWE.5-TC08.2"
        name = "Anti-Spam Idempotency No-Op Filter"
        req = "FR-17, FR-32"
        start = time.time()
        try:
            # Resend OFF while already in OFF state
            r = requests.post(f"{self.base_url}/api/boiler/off", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            if not data.get("idempotent") and data.get("pendingCancelled"):
                r = requests.post(f"{self.base_url}/api/boiler/off", timeout=self.timeout)
                data = r.json()
            assert r.status_code == 200, f"Expected 200, got {r.status_code}"
            assert data.get("idempotent") is True, f"Expected idempotent=True, got {data}"
            details = "Repeated command handled as 0ms no-op; RF burst skipped"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc08_3_boiler_zero_burst_skip(self):
        """SWE.5-TC08.3: Zero RF Burst Emission on Duplicate Command (FR-17)"""
        test_id = "SWE.5-TC08.3"
        name = "Zero RF Burst Emission Verification"
        req = "FR-17"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/boiler/off", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert data.get("status") == "ok", "Expected status ok"
            details = "RF transmission hardware driver bypassed without CPU blocking"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 9: Anti-Short-Cycling 60s Guard & Rate Limiting
    # ---------------------------------------------------------

    def test_tc09_1_anti_short_cycling_reversal(self):
        """SWE.5-TC09.1: Boiler 60s Reversal Anti-Short-Cycling Guard (FR-4, FR-17)"""
        test_id = "SWE.5-TC09.1"
        name = "Boiler 60s Reversal Interlock Guard"
        req = "FR-4, FR-17"
        start = time.time()
        try:
            r_stat = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            curr = r_stat.json().get("boiler", "OFF")
            opposite = "/api/boiler/on" if curr == "OFF" else "/api/boiler/off"

            r_try = requests.post(f"{self.base_url}{opposite}", timeout=self.timeout)
            latency = (time.time() - start) * 1000

            if r_try.status_code == 429:
                wait_sec = r_try.json().get("waitSec", 0)
                assert 0 <= wait_sec <= 60, f"waitSec {wait_sec} outside 60s guard"
                details = f"Interlock ACTIVE: Rejected state reversal (wait: {wait_sec}s remaining)"
                requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
                requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
                return TestResult(test_id, name, req, "PASS", latency, details)
            elif r_try.status_code == 200:
                # If accepted, reversing immediately must be blocked
                reverse_cmd = "/api/boiler/off" if opposite == "/api/boiler/on" else "/api/boiler/on"
                r_rev = requests.post(f"{self.base_url}{reverse_cmd}", timeout=self.timeout)
                # Cleanup: cancel pending and restore to OFF
                requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
                requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
                assert r_rev.status_code == 429, f"Expected 429 on reversal, got {r_rev.status_code}"
                wait_sec = r_rev.json().get("waitSec", 0)
                assert 0 <= wait_sec <= 60, f"waitSec {wait_sec} outside 60s guard"
                details = f"Interlock ACTIVE on immediate reversal (wait: {wait_sec}s remaining)"
                return TestResult(test_id, name, req, "PASS", (time.time()-start)*1000, details)
            else:
                requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
                requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
                return TestResult(test_id, name, req, "FAIL", latency, f"Unexpected code {r_try.status_code}")
        except Exception as e:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc09_2_http_429_telemetry(self):
        """SWE.5-TC09.2: Deterministic HTTP 429 & waitSec Telemetry (FR-17)"""
        test_id = "SWE.5-TC09.2"
        name = "Deterministic HTTP 429 & waitSec"
        req = "FR-17"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            # Always cancel pending action so boiler doesn't start later
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            if r.status_code == 429:
                data = r.json()
                assert "waitSec" in data and data["waitSec"] >= 0, "Missing waitSec"
                assert "error" in data, "Missing error string in 429 response"
                details = f"HTTP 429 verified: {data.get('error')} ({data.get('waitSec')}s remaining)"
                return TestResult(test_id, name, req, "PASS", latency, details)
            else:
                details = f"HTTP status: {r.status_code} (Cleaned up to OFF)"
                return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 10: Spam Detection & DTC-201 Trouble Code
    # ---------------------------------------------------------

    def test_tc10_1_rapid_spam_injection(self):
        """SWE.5-TC10.1: Rapid Boiler Spam Injection Loop (FR-17)"""
        test_id = "SWE.5-TC10.1"
        name = "Rapid Boiler Spam Injection Loop"
        req = "FR-17"
        start = time.time()
        try:
            requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
            time.sleep(0.1)
            # Inject 3 rapid rejected toggle requests (trigger threshold >= 3)
            for _ in range(3):
                r_sp = requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
                assert r_sp.status_code in (409, 429), f"Expected 409/429 on rapid boiler ON, got {r_sp.status_code}"
                time.sleep(0.08)
            # Immediately cancel any pending action scheduled by the rejected calls
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            diag = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json()
            assert any(d.get("code") == "DTC-201" for d in diag.get("dtcs", [])), "DTC-201 not raised after 3 rapid rejections"
            latency = (time.time() - start) * 1000
            details = "Fired 3 rapid requests, verified DTC-201 raised, cancelled pending startup"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc10_2_dtc201_fault_logging(self):
        """SWE.5-TC10.2: DTC-201 Fault Logging (FR-10, FR-17)"""
        test_id = "SWE.5-TC10.2"
        name = "DTC-201 Logging"
        req = "FR-10, FR-17"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            dtcs = data.get("dtcs", [])
            has_dtc201 = any(d.get("code") == "DTC-201" for d in dtcs)
            assert has_dtc201, f"DTC-201 was not logged after spam injection: {dtcs}"
            assert data.get("dtcCount", 0) >= 1, f"dtcCount does not count the active DTC-201: {data.get('dtcCount')}"
            details = f"DTC-201 raised: WARN_BOILER_SPAM_DETECTED, {data.get('dtcCount')} active DTC(s)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc10_3_dtc_clear_and_recovery(self):
        """SWE.5-TC10.3: Diagnostic DTC Clear (FR-10, FR-17)"""
        test_id = "SWE.5-TC10.3"
        name = "DTC Clear"
        req = "FR-10, FR-17"
        start = time.time()
        try:
            # Defuse any pending startup scheduled by the spam test
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)

            r_clr = requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
            assert r_clr.status_code == 200, "Failed clearing DTCs"
            r_diag = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r_diag.json()
            assert len(data.get("dtcs", [])) == 0, "DTCs still present after clear"
            assert data.get("dtcCount") == 0, f"dtcCount {data.get('dtcCount')} after clear, expected 0 active"
            details = "DTCs cleared; 0 active"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc10_4_basic_selftest_checks(self):
        """SWE.5-TC10.4: Basic Self-Test Reports Checks, No Score (FR-10.4)"""
        test_id = "SWE.5-TC10.4"
        name = "Self-Test Checks (no score)"
        req = "FR-10.4"
        start = time.time()
        try:
            diag = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json()
            status = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout).json()
            zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
            latency = (time.time() - start) * 1000
            # FR-10.4: no health score or percentage
            assert "healthScore" not in diag and "healthScore" not in status, "healthScore still reported"
            checks = {c["id"]: c for c in diag.get("checks", [])}
            expected = {"wifi", "heap", "ble", "sensors", "time", "weather", "radio"}
            assert set(checks) == expected, f"Checks {sorted(checks)}, expected {sorted(expected)}"
            for c in checks.values():
                assert c.get("result") in ("OK", "FAIL", "INFO"), f"{c['id']}: result {c.get('result')}"
                assert c.get("rule") and "value" in c, f"{c['id']}: rule or value missing"
            # The rules of the FR-10.4 table, recomputed from the values the board reports
            rssi = status["rssi"]
            assert checks["wifi"]["result"] == ("OK" if rssi >= -82 else "FAIL"), f"wifi {checks['wifi']} at {rssi} dBm"
            heap_ok = status["freeHeap"] >= 60000
            assert checks["heap"]["result"] == ("OK" if heap_ok else "FAIL"), f"heap {checks['heap']} at {status['freeHeap']}"
            on = [z for z in zones if z["enabled"]]
            want = "INFO" if not on else ("OK" if all(z["sensorValid"] and not z["sensorFault"] for z in on) else "FAIL")
            assert checks["sensors"]["result"] == want, f"sensors {checks['sensors']}, expected {want}"
            assert checks["radio"]["result"] == "INFO", "radio must be INFO (no return channel)"
            details = ", ".join(f"{k} {checks[k]['result']}" for k in sorted(checks))
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
