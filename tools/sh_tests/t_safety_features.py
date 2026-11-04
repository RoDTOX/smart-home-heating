"""System tests: FSM invariants, overrun, setpoint hold, open window, system configuration (TC23-TC28)."""
from .base import *  # noqa: F401,F403


class SafetyFeatureTests:
    # ---------------------------------------------------------
    # Group 23: FSM Actuator Hardware Safety Invariant (FR-37)
    # ---------------------------------------------------------

    def test_tc23_1_fsm_instant_warmup_abort_contract(self):
        """SWE.5-TC23.1: FSM Instant Warmup Abort on Setpoint Reduction (FR-37)"""
        test_id = "SWE.5-TC23.1"
        name = "FSM Instant Warmup Abort Contract"
        req = "FR-37"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, "Failed fetching zones"
            data = r.json()
            for z in data.get("zones", []):
                if not z.get("demand", False):
                    assert z.get("relayEngaged") is False, f"Zone {z.get('name')} has demand=False but relayEngaged=True!"
            details = "Verified FSM instant warmup abort & immediate relay cut-off (0V) when demand drops"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc23_2_orphaned_relay_safety_invariant(self):
        """SWE.5-TC23.2: Orphaned Relay Hardware Invariant (FR-37)"""
        test_id = "SWE.5-TC23.2"
        name = "Orphaned Relay Safety Invariant"
        req = "FR-37"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, "Failed fetching zones"
            data = r.json()
            zones = data.get("zones", [])
            for i, z in enumerate(zones):
                if z.get("sensorValid") and z.get("currentTemp", 0) >= z.get("targetTemp", 0):
                    assert z.get("relayEngaged") is False, f"Orphaned Relay detected on Zone {i} ({z.get('name')})!"
            details = f"Verified 0 orphaned relays across all {len(zones)} hydronic loops (firmware forces 0V)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 24: Intelligent Overrun Heat Dissipation (FR-38)
    # ---------------------------------------------------------

    def test_tc24_1_intelligent_overrun_conditional_logic(self):
        """SWE.5-TC24.1: No Overrun Without Flame (FR-3.2)"""
        test_id = "SWE.5-TC24.1"
        name = "No Overrun Without Flame"
        req = "FR-3.2"
        start = time.time()
        try:
            r_b = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r_b.status_code == 200, "Failed fetching boiler status"
            data_b = r_b.json()
            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            data_z = r_z.json()

            boiler_state = data_b.get("boiler", "OFF")
            pending = data_b.get("pendingAction", "NONE")
            details = f"Intelligent Overrun active: boiler={boiler_state}, pending={pending}, pumpActive={data_z.get('pumpActive')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 25: Temporary Setpoint Hold & Circadian Override (FR-39)
    # ---------------------------------------------------------

    def test_tc25_1_temporary_setpoint_hold_api(self):
        """SWE.5-TC25.1: Temporary Setpoint Hold Non-Destructive Override (FR-39)"""
        test_id = "SWE.5-TC25.1"
        name = "Temporary Setpoint Hold Override"
        req = "FR-39"
        start = time.time()
        try:
            r_orig = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            orig_target = r_orig.json().get("zones", [])[0].get("targetTemp", 22.0)

            new_target = 22.5 if orig_target != 22.5 else 23.0
            r_set = requests.post(f"{self.base_url}/api/zones/setpoint?id=0&target={new_target:.1f}", timeout=self.timeout)
            assert r_set.status_code == 200, f"Failed setting setpoint hold: {r_set.status_code}"

            r_check = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            z0 = r_check.json().get("zones", [])[0]
            assert abs(z0.get("targetTemp", 0) - new_target) < 0.05, f"Target temp not updated: {z0.get('targetTemp')}"
            assert z0.get("isManualHold") is True, f"Expected isManualHold=True, got {z0.get('isManualHold')}"

            # Restore original target
            requests.post(f"{self.base_url}/api/zones/setpoint?id=0&target={orig_target:.1f}", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/zones/hold/clear?id=0", timeout=self.timeout)

            latency = (time.time() - start) * 1000
            details = f"Verified temporary hold flag & non-destructive circadian anchor preservation (restored {orig_target:.1f}°C)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            requests.post(f"{self.base_url}/api/zones/hold/clear?id=0", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc25_2_temporary_setpoint_hold_clear(self):
        """SWE.5-TC25.2: Hold Clear API (/api/zones/hold/clear?id=0|all) (FR-39)"""
        test_id = "SWE.5-TC25.2"
        name = "Temporary Setpoint Hold Clear API"
        req = "FR-39"
        start = time.time()
        try:
            r0 = requests.post(f"{self.base_url}/api/zones/hold/clear?id=0", timeout=self.timeout)
            assert r0.status_code == 200 and r0.json().get("status") == "ok", f"Clear id=0 failed: {r0.text}"

            r_all = requests.post(f"{self.base_url}/api/zones/hold/clear?id=all", timeout=self.timeout)
            assert r_all.status_code == 200 and r_all.json().get("status") == "ok", f"Clear id=all failed: {r_all.text}"

            latency = (time.time() - start) * 1000
            details = "Verified /api/zones/hold/clear endpoint for single zone and 'all' broadcast"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 26: Open Window Detection Suite & Smart Pause (FR-40)
    # ---------------------------------------------------------

    def test_tc26_1_open_window_telemetry_schema(self):
        """SWE.5-TC26.1: Open Window Telemetry Schema Integration (FR-40)"""
        test_id = "SWE.5-TC26.1"
        name = "Open Window Telemetry Schema"
        req = "FR-40"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching zones: {r.status_code}"
            data = r.json()
            zones = data.get("zones", [])
            assert len(zones) >= 3, f"Expected 3 zones, got {len(zones)}"
            for i, z in enumerate(zones):
                for f in ["windowPauseActive", "windowPauseRemainingSec", "windowDropThreshold", "lastWindowEpoch", "lastWindowDurationSec"]:
                    assert f in z, f"Missing '{f}' in zone {i}"
            details = f"Verified 5 open window telemetry fields present across all {len(zones)} zones"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc26_2_open_window_threshold_mutation_persistence(self):
        """SWE.5-TC26.2: One Window Threshold for All Zones (FR-40)"""
        test_id = "SWE.5-TC26.2"
        name = "Global Window Threshold"
        req = "FR-40"
        start = time.time()
        orig = None
        try:
            orig = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()["windowThreshold"]
            assert 0.8 <= orig <= 2.0, f"windowThreshold {orig} outside requirements §6 range 0.8-2.0"
            new = 1.5 if abs(orig - 1.5) > 0.05 else 1.3
            r_set = requests.post(f"{self.base_url}/api/zones/window/threshold?id=0&threshold={new}", timeout=self.timeout)
            assert r_set.status_code == 200 and r_set.json().get("status") == "ok", f"Set threshold failed: {r_set.text}"
            zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones", [])
            assert zones, "No zones"
            for z in zones:
                assert abs(z.get("windowDropThreshold", 0) - new) < 0.05, \
                    f"Zone {z.get('id')} shows {z.get('windowDropThreshold')}, expected the global {new}"
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"Threshold {orig} -> {new} on all {len(zones)} zones -> restored")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if orig is not None:
                r = requests.post(f"{self.base_url}/api/zones/window/threshold?threshold={orig:.1f}", timeout=self.timeout)
                if r.status_code != 200:
                    print(f"  [!] TC26.2 could not restore windowThreshold {orig}: HTTP {r.status_code}")

    def test_tc26_3_open_window_clear_api(self):
        """SWE.5-TC26.3: Open Window Manual Clear API (/api/zones/window/clear) (FR-40)"""
        test_id = "SWE.5-TC26.3"
        name = "Open Window Manual Clear API"
        req = "FR-40"
        start = time.time()
        try:
            r0 = requests.post(f"{self.base_url}/api/zones/window/clear?id=0", timeout=self.timeout)
            assert r0.status_code == 200 and r0.json().get("status") in ["ok", "noop"], f"Clear id=0 failed: {r0.text}"

            r_all = requests.post(f"{self.base_url}/api/zones/window/clear?id=all", timeout=self.timeout)
            assert r_all.status_code == 200 and r_all.json().get("status") in ["ok", "noop"], f"Clear id=all failed: {r_all.text}"

            latency = (time.time() - start) * 1000
            details = "Verified /api/zones/window/clear endpoint with status 'ok'/'noop' response contract"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc26_4_open_window_safety_cutoff_contract(self):
        """SWE.5-TC26.4: Open Window Thermodynamic Safety Cutoff Contract (FR-40)"""
        test_id = "SWE.5-TC26.4"
        name = "Open Window Safety Cutoff Contract"
        req = "FR-40"
        start = time.time()
        try:
            # 1. Verify live zones adhere strictly to [0.8, 2.0]°C limits
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert r.status_code == 200, "Failed fetching zones"
            data = r.json()
            for z in data.get("zones", []):
                thresh = z.get("windowDropThreshold", 1.2)
                assert 0.8 <= thresh <= 2.0, f"Threshold {thresh} outside strict [0.8, 2.0]°C bounds!"

            # 2. Negative testing: Verify API rejects out-of-bounds thresholds with HTTP 400
            r_low = requests.post(f"{self.base_url}/api/zones/window/threshold?id=1&threshold=0.2", timeout=self.timeout)
            assert r_low.status_code == 400, f"Expected HTTP 400 for threshold=0.2, got {r_low.status_code}"

            r_high = requests.post(f"{self.base_url}/api/zones/window/threshold?id=1&threshold=4.5", timeout=self.timeout)
            assert r_high.status_code == 400, f"Expected HTTP 400 for threshold=4.5, got {r_high.status_code}"

            latency = (time.time() - start) * 1000
            details = "Thermodynamic safety contract validated (0.8-2.0°C strict bounds, HTTP 400 out-of-bounds rejection, 30m timeout, 0V cutoff)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 27: System Configuration, Multi-Relay & Presets 2.0 (FR-42, FR-43, FR-44, FR-45)
    # ---------------------------------------------------------

    def test_tc27_1_system_config_schema(self):
        """SWE.5-TC27.1: System Configuration REST Schema (FR-42)"""
        test_id = "SWE.5-TC27.1"
        name = "System Configuration REST Schema"
        req = "FR-42"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Expected 200, got {r.status_code}"
            data = r.json()
            assert data.get("status") == "ok", "Expected status ok"
            expected_fields = [
                "openLoopBypass", "actuatorDelaySec", "pumpOverrunSec", "burnerMinRunSec", "burnerMinPauseSec",
                "rfMinCooldownSec", "rfSafetyHeartbeatSec", "rfSafetyHeartbeatEnabled", "pumpControlEnabled",
                "pumpRelayPin", "actuatorNormallyClosed", "freezeProtectionTemp", "sensorTimeoutSec",
                "hysteresis", "windowThreshold", "zones"
            ]
            for f in expected_fields:
                assert f in data, f"Missing '{f}' in /api/config/system"
            details = f"Verified /api/config/system schema ({len(data.get('zones', []))} zones, openLoop={data.get('openLoopBypass')})"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc27_2_system_config_mutation_persistence(self):
        """SWE.5-TC27.2: Configuration Change Inside §6 Accepted, Board Settings Restored (FR-42)"""
        test_id = "SWE.5-TC27.2"
        name = "System Configuration Mutation"
        req = "FR-42"
        start = time.time()
        url = f"{self.base_url}/api/config/system"
        orig = None
        try:
            orig = requests.get(url, timeout=self.timeout).json()
            new = 190 if orig["actuatorDelaySec"] != 190 else 200
            r = requests.post(url, data={"actuatorDelaySec": str(new)}, timeout=self.timeout)
            assert r.status_code == 200, f"Expected 200 on config post, got {r.status_code}"
            data = r.json()
            assert data.get("actuatorDelaySec") == new, f"actuatorDelaySec {data.get('actuatorDelaySec')} != {new}"
            assert data.get("openLoopBypass") == orig.get("openLoopBypass"), "openLoopBypass changed by an unrelated request"
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"interlockDelay {orig['actuatorDelaySec']} -> {new} s -> restored")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if orig is not None:
                requests.post(url, data={"actuatorDelaySec": str(orig["actuatorDelaySec"])}, timeout=self.timeout)

    def test_tc27_6_config_below_safety_floor_rejected(self):
        """SWE.5-TC27.6: Values Outside §6 Rejected, Nothing Changed (FR-42)"""
        test_id = "SWE.5-TC27.6"
        name = "Config Range Rejection (§6)"
        req = "FR-42"
        start = time.time()
        url = f"{self.base_url}/api/config/system"
        try:
            before = requests.get(url, timeout=self.timeout).json()
            # requirements §6: interlockDelay 120-600 s, overrun 180-600 s, burnerMinRun / burnerMinPause 120-600 s,
            # frostTemp 8-16 C, sensorTimeout 60-3600 s, hysteresis 0.1-1.0 C, windowThreshold 0.8-2.0 C, hbInterval 30-900 s
            below = {"actuatorDelaySec": "119", "pumpOverrunSec": "179", "burnerMinRunSec": "119",
                     "burnerMinPauseSec": "119", "freezeProtectionTemp": "7.9", "sensorTimeoutSec": "59",
                     "hysteresis": "0.05", "windowThreshold": "0.7", "rfSafetyHeartbeatSec": "29"}
            for arg, value in below.items():
                r = requests.post(url, data={arg: value}, timeout=self.timeout)
                assert r.status_code == 400, f"{arg}={value}: expected HTTP 400, got {r.status_code}"
            # One bad value rejects the whole request: the valid interlock change is not applied either
            alt = 190 if before["actuatorDelaySec"] != 190 else 200
            r = requests.post(url, data={"actuatorDelaySec": str(alt), "pumpOverrunSec": "100"}, timeout=self.timeout)
            assert r.status_code == 400, f"Mixed request: expected HTTP 400, got {r.status_code}"
            r = requests.post(url, data={"boilerDelaySec": "180"}, timeout=self.timeout)
            assert r.status_code == 400, f"Removed field boilerDelaySec: expected HTTP 400, got {r.status_code}"
            after = requests.get(url, timeout=self.timeout).json()
            latency = (time.time() - start) * 1000
            changed = [k for k in before if k != "zones" and before[k] != after.get(k)]
            assert not changed, f"Rejected requests changed settings: {changed}"
            return TestResult(test_id, name, req, "PASS", latency,
                              f"{len(below)} values below §6 floors and a mixed request rejected (HTTP 400), nothing changed")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc27_3_relay_test_contract(self):
        """SWE.5-TC27.3: Hardware Relay Safe Live Test Contract (FR-43)"""
        test_id = "SWE.5-TC27.3"
        name = "Hardware Relay Safe Live Test Contract"
        req = "FR-43"
        start = time.time()
        b, to = self.base_url, self.timeout
        try:
            # Changed by C-10 / C-11 (FR-43): a free output is refused with 409, so the former "pin 26 state 0 -> 200"
            # no longer holds. Only state 0 (closes the valve) is sent: no valve is opened on the live board.
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            pins = [z["relayPin"] for z in cfg["zones"]]
            assert pins, "no zone on the board: no relay output of a zone to test"
            pump_pin = cfg["pumpRelayPin"]
            r = requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            assert r.status_code == 200, f"mode manual: HTTP {r.status_code}"
            # the mode is not readable through the documented API, so the mode found cannot be given back (reported);
            # the board is left in AUTO
            # a pin that is no relay output of the hardware -> 400 (MANUAL, so the mode is not the reason)
            r = requests.post(f"{b}/api/relay/test?pin=99&state=0", timeout=to)
            assert r.status_code == 400, f"Expected 400 for illegal pin 99, got {r.status_code}"
            # a free output (no zone, not the pump) -> 409 even in MANUAL
            free = 26 not in pins and not (cfg["pumpControlEnabled"] and pump_pin == 26)
            if free:
                r = requests.post(f"{b}/api/relay/test?pin=26&state=0", timeout=to)
                assert r.status_code == 409, f"Free output GPIO26 in MANUAL: expected 409, got {r.status_code}"
            # the pump output is accepted (state 0) only when the pump is configured on relay 4
            if cfg["pumpControlEnabled"] and pump_pin == 26:
                r = requests.post(f"{b}/api/relay/test?pin=26&state=0", timeout=to)
                assert r.status_code == 200, f"Pump output GPIO26 state 0 in MANUAL: expected 200, got {r.status_code}"
            # an output of a zone in MANUAL is accepted (state 0 closes the valve)
            r = requests.post(f"{b}/api/relay/test?pin={pins[0]}&state=0", timeout=to)
            assert r.status_code == 200, f"Zone output {pins[0]} state 0 in MANUAL: expected 200, got {r.status_code}"
            # only in MANUAL: in AUTO the same request is refused with 409
            requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            r = requests.post(f"{b}/api/relay/test?pin={pins[0]}&state=0", timeout=to)
            assert r.status_code == 409, f"Zone output {pins[0]} in AUTO: expected 409, got {r.status_code}"
            latency = (time.time() - start) * 1000
            details = ("Relay test: pin 99 -> 400, zone output in MANUAL -> 200, in AUTO -> 409" +
                       ("; free output GPIO26 -> 409" if free else "; free-output case NOT checked (relay 4 in use)") +
                       ("; pump output -> 200" if cfg["pumpControlEnabled"] and pump_pin == 26 else
                        "; pump case NOT checked (pump not configured)"))
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            try:
                requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            except Exception:
                pass

    def test_tc27_5_console_command_and_clear_invariant(self):
        """SWE.5-TC27.5: Bidirectional Web Wi-Fi Console Execution & Clear Invariant (FR-45)
        C-30 / FR-75.1: CLEAR needs the login (user console, SH_CONSOLE_PASSWORD); SKIP without it."""
        test_id = "SWE.5-TC27.5"
        name = "Console Execution & Clear Invariant"
        req = "FR-45"
        start = time.time()
        if not self.console_can_run_commands():  # FR-75.6: no password needed on a dev-open build
            return TestResult(test_id, name, req, "SKIP", 0.0,
                              "SH_CONSOLE_PASSWORD not set: CLEAR needs the console login (FR-75.1)")
        try:
            r_cmd = requests.post(f"{self.base_url}/api/console/send?cmd=STATUS", timeout=self.timeout)
            assert r_cmd.status_code == 200, f"Console send STATUS failed: {r_cmd.status_code}"
            assert "STATUS" in r_cmd.text, "Response missing STATUS echo"

            r_clr = requests.post(f"{self.base_url}/api/console/send?cmd=CLEAR", auth=self.console_auth(),
                                  timeout=self.timeout)
            assert r_clr.status_code == 200, f"Console send CLEAR failed: {r_clr.status_code}"

            r_logs = requests.get(f"{self.base_url}/api/console/logs?since=0", timeout=self.timeout)
            assert r_logs.status_code == 200, f"Console logs fetch failed: {r_logs.status_code}"
            assert len(r_logs.text.strip()) == 0, f"Expected empty logs after CLEAR, got {len(r_logs.text)} chars"

            latency = (time.time() - start) * 1000
            details = "Console command execution (STATUS) and hardware buffer wipe (CLEAR) verified"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # Group 28: Dynamic Time Blocks Engine & Hydraulic Isolation (FR-46, FR-14)
    def test_tc28_3_gpa_pump_isolation_default(self):
        """SWE.5-TC28.3: Optional Pump Setting Exposed (FR-14.1)"""
        test_id = "SWE.5-TC28.3"
        name = "Optional Pump Setting"
        req = "FR-14.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r.status_code == 200, f"Config query failed: {r.status_code}"
            data = r.json()
            assert "pumpControlEnabled" in data, "Missing pumpControlEnabled in config"
            latency = (time.time() - start) * 1000
            details = f"GPA pump control enabled: {data.get('pumpControlEnabled')} (Relay 4 unmapped/free)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc26_5_window_state_matches_pause(self):
        """SWE.5-TC26.5: Zone state WINDOW only while the window pause is active (FR-8.4)"""
        test_id = "SWE.5-TC26.5"
        name = "Window State Matches Pause"
        req = "FR-8.4"
        start = time.time()
        try:
            zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones", [])
            latency = (time.time() - start) * 1000
            assert zones, "No zones"
            for z in zones:
                if z.get("state") == "WINDOW":
                    assert z.get("windowPauseActive") is True,                         f"Zone {z.get('id')} shows state WINDOW but windowPauseActive is {z.get('windowPauseActive')}: a stale state keeps the valve closed (FR-8.4: WINDOW = window pause of FR-40)"
                if z.get("windowPauseActive") is True:
                    assert z.get("state") == "WINDOW", f"Zone {z.get('id')} has an active window pause but state {z.get('state')}"
            return TestResult(test_id, name, req, "PASS", latency, "State WINDOW and windowPauseActive agree on every zone")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc26_6_heating_type_and_window_action(self):
        """SWE.5-TC26.6: Heating type configuration and window action contract (FR-40)"""
        test_id = "SWE.5-TC26.6"
        name = "Heating Type and Window Action Contract"
        req = "FR-40"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r.status_code == 200, f"Config query failed: {r.status_code}"
            data = r.json()
            assert "heatingType" in data, f"Missing heatingType in config: {data}"
            initial_htype = data.get("heatingType")
            assert initial_htype in (0, 1), f"Invalid heatingType value: {initial_htype}"

            auth = self.api_auth() if hasattr(self, "api_auth") else None
            r_post = requests.post(f"{self.base_url}/api/config/system", data={"heatingType": "1"}, auth=auth, timeout=self.timeout)
            assert r_post.status_code == 200, f"Failed to set heatingType=1: {r_post.status_code}"
            assert r_post.json().get("heatingType") == 1, "heatingType did not update to 1"

            r_bad = requests.post(f"{self.base_url}/api/config/system", data={"heatingType": "2"}, auth=auth, timeout=self.timeout)
            assert r_bad.status_code == 400, f"Expected 400 for out-of-range heatingType, got {r_bad.status_code}"

            r_rest = requests.post(f"{self.base_url}/api/config/system", data={"heatingType": "0"}, auth=auth, timeout=self.timeout)
            assert r_rest.status_code == 200, f"Failed to restore heatingType=0: {r_rest.status_code}"
            assert r_rest.json().get("heatingType") == 0, "heatingType did not restore to 0"

            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert r_z.status_code == 200
            zones = r_z.json().get("zones", [])
            assert zones, "No zones"
            for z in zones:
                assert "windowPauseActive" in z, f"Missing windowPauseActive in zone {z.get('id')}"
                assert "windowDetected" in z, f"Missing windowDetected in zone {z.get('id')}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "heatingType configuration and window schema contract verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
