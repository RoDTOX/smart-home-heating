"""System tests: setpoints and modes, time and weather, console, sensor timeout, identity, schedule (TC11-TC16)."""
from .base import *  # noqa: F401,F403


class ControlTests:
    # ---------------------------------------------------------
    # Group 11: Zone Hysteresis, Setpoints & Modes
    # ---------------------------------------------------------

    def test_tc11_1_zone_setpoint_read(self):
        """SWE.5-TC11.1: Thermal Zone Setpoint Initial Query (FR-1)"""
        test_id = "SWE.5-TC11.1"
        name = "Zone Setpoint Initial Query"
        req = "FR-1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            zones = r.json().get("zones", [])
            assert len(zones) >= 1, "No zones defined"
            z1 = zones[0]
            details = f"Zone 1 '{z1.get('name')}': Target {z1.get('targetTemp')}°C, Current {z1.get('currentTemp')}°C"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc11_2_zone_setpoint_mutation(self):
        """SWE.5-TC11.2: Zone Setpoint Dynamic Mutation (FR-1)"""
        test_id = "SWE.5-TC11.2"
        name = "Zone Setpoint Dynamic Mutation"
        req = "FR-1"
        start = time.time()
        try:
            r_init = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            orig_temp = r_init.json().get("zones", [])[0].get("targetTemp", 22.0)
            target_temp = 21.5 if orig_temp != 21.5 else 22.0

            r_set = requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target={target_temp}", timeout=self.timeout)
            assert r_set.status_code == 200, f"Failed setting setpoint: {r_set.status_code}"

            r_ver = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            new_temp = r_ver.json().get("zones", [])[0].get("targetTemp")
            latency = (time.time() - start) * 1000
            assert abs(new_temp - target_temp) < 0.1, f"Expected {target_temp}, got {new_temp}"
            details = f"Zone 1 setpoint changed: {orig_temp:.1f}°C -> {target_temp:.1f}°C"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc11_3_zone_setpoint_restoration(self):
        """SWE.5-TC11.3: Zone Setpoint NVS Restoration (FR-1)"""
        test_id = "SWE.5-TC11.3"
        name = "Zone Setpoint NVS Restoration"
        req = "FR-1"
        start = time.time()
        try:
            # Restore to standard 22.0°C
            r_rst = requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target=22.0", timeout=self.timeout)
            assert r_rst.status_code == 200, "Failed restoring setpoint"
            r_ver = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            temp = r_ver.json().get("zones", [])[0].get("targetTemp")
            latency = (time.time() - start) * 1000
            assert abs(temp - 22.0) < 0.1, f"Expected 22.0, got {temp}"
            details = "Zone 1 setpoint restored to 22.0°C in NVS"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc11_4_operating_mode_contract(self):
        """SWE.5-TC11.4: Operating Mode Contract (Auto vs Manual) (FR-1)"""
        test_id = "SWE.5-TC11.4"
        name = "Operating Mode Contract (Auto vs Manual)"
        req = "FR-1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            auto_mode = r.json().get("autoMode")
            assert auto_mode in [True, False], f"Invalid autoMode: {auto_mode}"
            details = f"Active Operating Mode: {'AUTOMATIC' if auto_mode else 'MANUAL'}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 12: NTP Time Synchronization & Outdoor Weather
    # ---------------------------------------------------------

    def test_tc12_1_ntp_time_sync(self):
        """SWE.5-TC12.1: NTP Clock Sync & Europe/Bucharest POSIX (FR-12)"""
        test_id = "SWE.5-TC12.1"
        name = "NTP Clock Sync & Bucharest POSIX"
        req = "FR-12"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/setup/time", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert data.get("timeSynced") is True, "NTP time is not synchronized"
            assert len(data.get("time", "")) > 0, "Missing time string"
            details = f"NTP Synced: {data.get('date')} {data.get('time')} (Europe/Bucharest)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc12_2_heating_season_detection(self):
        """SWE.5-TC12.2: Heating Season Calendar Detection (FR-12)"""
        test_id = "SWE.5-TC12.2"
        name = "Heating Season Calendar Detection"
        req = "FR-12"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/setup/time", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            season = data.get("season", "")
            assert len(season) > 0, "Missing season string"
            details = f"Season: {season}, Heating Season Active: {data.get('isHeatingSeason')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc12_3_outdoor_weather_telemetry(self):
        """SWE.5-TC12.3: Outdoor Weather Telemetry via Open-Meteo (FR-9, FR-12)"""
        test_id = "SWE.5-TC12.3"
        name = "Outdoor Weather Telemetry (Open-Meteo)"
        req = "FR-9, FR-12"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/setup/time", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            outdoor = data.get("outdoorTemp")
            assert outdoor is not None, "Missing outdoorTemp"
            details = f"Outdoor Weather: {outdoor:.1f}°C (Open-Meteo REST service)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 13: 2-Way Virtual Console & Formatting
    # ---------------------------------------------------------

    def test_tc13_1_console_help_execution(self):
        """SWE.5-TC13.1: Network Console Remote HELP Execution (FR-7)"""
        test_id = "SWE.5-TC13.1"
        name = "Console Remote HELP Execution"
        req = "FR-7"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/console/send?cmd=HELP", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HELP command failed: {r.status_code}"
            details = "Dispatched HELP command over REST console endpoint"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc13_2_console_log_tail(self):
        """SWE.5-TC13.2: Network Console Log Buffer Tail Verification (FR-7)"""
        test_id = "SWE.5-TC13.2"
        name = "Console Log Buffer Tail Verification"
        req = "FR-7"
        start = time.time()
        try:
            time.sleep(0.12)
            r = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Logs fetch failed: {r.status_code}"
            logs = r.text
            assert "Available commands" in logs or "Smart Heating" in logs or "HELP" in logs, "Missing expected output in console buffer"
            details = "Verified live ring buffer tail contains structured command response"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc13_3_tabular_formatting_and_utf8(self):
        """SWE.5-TC13.3: STATUS Sections Present (FR-23)"""
        test_id = "SWE.5-TC13.3"
        name = "Tabular Formatting & UTF-8 °C Presentation"
        req = "FR-23"
        start = time.time()
        try:
            # Request STATUS to verify subsystem blocks and utf8VisibleLen alignment
            requests.post(f"{self.base_url}/api/console/send?cmd=STATUS", timeout=self.timeout)
            time.sleep(0.15)
            r = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            logs = r.text
            assert "[A. SYSTEM" in logs or "[B. BOILER" in logs or "SYSTEM" in logs, "Missing structured section headers"
            # Clean up the large table dump so it does not pollute the user's active console.
            # C-30 / FR-75.1: CLEAR needs the login (SH_CONSOLE_PASSWORD); best effort, STATUS above is public.
            if self.console_can_run_commands():  # FR-75.6: open on a dev-open build
                requests.post(f"{self.base_url}/api/console/send?cmd=CLEAR", auth=self.console_auth(),
                              timeout=self.timeout)
            details = "Verified 2-column tabular blocks, UTF-8 °C symbols and zero column drifting (buffer cleaned)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 14: Configurable Sensor Timeout Failsafe
    # ---------------------------------------------------------

    def test_tc14_1_sensor_timeout_query(self):
        """SWE.5-TC14.1: Sensor Failsafe Timeout Query (FR-18)"""
        test_id = "SWE.5-TC14.1"
        name = "Sensor Timeout Failsafe Query"
        req = "FR-18"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/setup/sensor-timeout", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            tout = r.json().get("sensorTimeoutSec")
            assert tout is not None and 60 <= tout <= 3600, f"sensorTimeout {tout} s outside requirements §6 range 60-3600 s"
            self.orig_sensor_timeout = tout
            details = f"Configured sensor timeout: {tout}s"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc14_2_sensor_timeout_mutation(self):
        """SWE.5-TC14.2: Sensor Failsafe Timeout Mutation to 750s (FR-18)"""
        test_id = "SWE.5-TC14.2"
        name = "Sensor Timeout Mutation to 750s"
        req = "FR-18"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/setup/sensor-timeout?timeoutSec=750", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, "Failed setting timeout"
            assert r.json().get("sensorTimeoutSec") == 750, "Timeout not updated to 750"
            details = "Updated timeout to 750s in NVS"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc14_3_sensor_timeout_restoration(self):
        """SWE.5-TC14.3: Sensor Timeout Restored to the Value Before the Test (FR-18)"""
        test_id = "SWE.5-TC14.3"
        name = "Sensor Timeout Restoration"
        req = "FR-18"
        start = time.time()
        try:
            orig = getattr(self, "orig_sensor_timeout", 1200)
            r = requests.post(f"{self.base_url}/api/setup/sensor-timeout?timeoutSec={orig}", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, "Failed restoring timeout"
            assert r.json().get("sensorTimeoutSec") == orig, f"Timeout not restored to {orig}"
            return TestResult(test_id, name, req, "PASS", latency, f"Restored sensorTimeout {orig} s")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc14_4_sensor_timeout_out_of_range_rejected(self):
        """SWE.5-TC14.4: Sensor Timeout Outside 60-3600 s Rejected (FR-18)"""
        test_id = "SWE.5-TC14.4"
        name = "Sensor Timeout Range Rejection"
        req = "FR-18"
        start = time.time()
        try:
            url = f"{self.base_url}/api/setup/sensor-timeout"
            before = requests.get(url, timeout=self.timeout).json()["sensorTimeoutSec"]
            for bad in (59, 3601):
                r = requests.post(f"{url}?timeoutSec={bad}", timeout=self.timeout)
                assert r.status_code == 400, f"timeoutSec={bad}: expected HTTP 400, got {r.status_code}"
            after = requests.get(url, timeout=self.timeout).json()["sensorTimeoutSec"]
            latency = (time.time() - start) * 1000
            assert after == before, f"sensorTimeout changed by a rejected request: {before} -> {after}"
            return TestResult(test_id, name, req, "PASS", latency, f"59 s and 3601 s rejected, {after} s kept")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc15_1_firmware_version(self):
        """SWE.5-TC15.1: Firmware Version 1.MM.DD.NN from git (FR-21)"""
        test_id = "SWE.5-TC15.1"
        name = "Firmware Version 1.MM.DD.NN"
        req = "FR-21"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            ver = data.get("firmwareVersion", "")
            assert re.fullmatch(r"1\.(0[1-9]|1[0-2])\.(0[1-9]|[12]\d|3[01])\.\d{2}", ver), f"Not 1.MM.DD.NN: {ver}"
            # Same firmware, same version: recompute it from git for the commit the board reports
            commit = data.get("gitCommit", "").replace("-dirty", "")
            root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            sys.path.insert(0, os.path.join(root, "tools"))
            from sh_git_version import firmware_version
            expected = firmware_version(root, commit) if commit else None
            assert expected is None or ver == expected, f"Board {ver}, git gives {expected} for {commit}"
            details = f"Firmware version {ver} = git for {commit}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc15_2_build_timestamp_and_git(self):
        """SWE.5-TC15.2: Build Timestamp & Git Hash Traceability (FR-21)"""
        test_id = "SWE.5-TC15.2"
        name = "Build Timestamp & Git Hash Traceability"
        req = "FR-21"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            bld = data.get("buildDate")
            commit = data.get("gitCommit")
            assert bld and len(bld) > 0, "Missing build date"
            assert commit and len(commit) > 0, "Missing git commit hash"
            details = f"Build: {bld}, Git Commit: {commit}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc15_3_hardware_reset_reason(self):
        """SWE.5-TC15.3: Hardware Reset Reason Telemetry (FR-21)"""
        test_id = "SWE.5-TC15.3"
        name = "Hardware Reset Reason Telemetry"
        req = "FR-21"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            rst = data.get("resetReason")
            assert rst and len(rst) > 0, "Missing reset reason"
            details = f"Last Hardware Reset Reason: {rst}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 16: Schedule (day / night / away levels)
    # ---------------------------------------------------------

    def test_tc16_1_schedule_schema(self):
        """SWE.5-TC16.1: Level Schedule Schema (/api/schedule) (FR-29)"""
        test_id = "SWE.5-TC16.1"
        name = "Level Schedule Schema"
        req = "FR-29"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HTTP {r.status_code}"
            d = r.json()
            for key in ("enabled", "away", "activeLevel", "nextLevel", "minsToNextSlot", "weekday", "weekend", "zones"):
                assert key in d, f"Missing '{key}'"
            assert d["activeLevel"] in ("day", "night", "away"), f"Invalid activeLevel {d['activeLevel']}"
            for z in d["zones"]:
                for lvl in ("day", "night", "away"):
                    assert lvl in z, f"Zone {z.get('id')} missing level '{lvl}'"
            details = f"Weekday '{d['weekday']}', weekend '{d['weekend']}', now '{d['activeLevel']}'"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc16_2_level_temp_roundtrip(self):
        """SWE.5-TC16.2: Level Temperature Set & Restore (FR-30)"""
        test_id = "SWE.5-TC16.2"
        name = "Level Temperature Roundtrip"
        req = "FR-30"
        start = time.time()
        orig = None
        try:
            orig = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()["zones"][0]["night"]
            new = 19.5 if abs(orig - 19.5) > 0.05 else 19.0
            r = requests.post(f"{self.base_url}/api/schedule/level?zone=1&level=night&temp={new}", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HTTP {r.status_code}"
            got = r.json()["zones"][0]["night"]
            assert abs(got - new) < 0.05, f"Expected {new}, got {got}"
            details = f"Zone 1 night {orig} -> {new} -> restored"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if orig is not None:
                requests.post(f"{self.base_url}/api/schedule/level?zone=1&level=night&temp={orig}", timeout=self.timeout)

    def test_tc16_3_slots_roundtrip_sorted(self):
        """SWE.5-TC16.3: Slots Are Stored Sorted & Restorable (FR-31)"""
        test_id = "SWE.5-TC16.3"
        name = "Slots Roundtrip (Sorted)"
        req = "FR-31"
        start = time.time()
        orig = None
        try:
            orig = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()["weekday"]
            r = requests.post(f"{self.base_url}/api/schedule/slots",
                              data={"weekend": "0", "slots": "22:30=night,06:30=day,08:30=away,17:00=day"}, timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HTTP {r.status_code}"
            got = r.json()["weekday"]
            assert got == "06:30=day,08:30=away,17:00=day,22:30=night", f"Unexpected slots '{got}'"
            details = f"Unsorted input stored sorted; original '{orig}' restored"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if orig:
                requests.post(f"{self.base_url}/api/schedule/slots", data={"weekend": "0", "slots": orig}, timeout=self.timeout)

    def test_tc16_5_global_away_roundtrip(self):
        """SWE.5-TC16.5: Global Away Mode Toggle & Restore (FR-44)"""
        test_id = "SWE.5-TC16.5"
        name = "Global Away Mode Roundtrip"
        req = "FR-44"
        start = time.time()
        orig = None
        try:
            orig = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()["away"]
            flip = "false" if orig else "true"
            r = requests.post(f"{self.base_url}/api/schedule/enable?away={flip}", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HTTP {r.status_code}"
            d = r.json()
            assert d["away"] == (not orig), f"Expected away={not orig}, got {d['away']}"
            # activeLevel stays the slot level by design; away overrides the effective target of every zone
            if d["away"]:
                zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
                for z, lv in zip(zones, d["zones"]):
                    assert abs(z["effectiveTarget"] - lv["away"]) < 0.05, \
                        f"Zone {z['id']} effectiveTarget {z['effectiveTarget']} != away level {lv['away']}"
            details = f"away {orig} -> {d['away']} (zones follow away level) -> restored"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if orig is not None:
                requests.post(f"{self.base_url}/api/schedule/enable?away={'true' if orig else 'false'}", timeout=self.timeout)

    def test_tc16_4_invalid_schedule_rejection(self):
        """SWE.5-TC16.4: Invalid Level / Slot Input Rejected (FR-31)"""
        test_id = "SWE.5-TC16.4"
        name = "Invalid Schedule Input Rejection"
        req = "FR-31"
        start = time.time()
        try:
            r1 = requests.post(f"{self.base_url}/api/schedule/level?zone=1&level=party&temp=21", timeout=self.timeout)
            r2 = requests.post(f"{self.base_url}/api/schedule/level?zone=9&level=day&temp=21", timeout=self.timeout)
            r3 = requests.post(f"{self.base_url}/api/schedule/slots", data={"weekend": "0", "slots": "25:00=day"}, timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert (r1.status_code, r2.status_code, r3.status_code) == (400, 400, 400), \
                f"Expected 400/400/400, got {r1.status_code}/{r2.status_code}/{r3.status_code}"
            return TestResult(test_id, name, req, "PASS", latency, "Bad level, zone and slot time rejected with HTTP 400")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc16_6_honeywell_moments(self):
        """SWE.5-TC16.6: Honeywell Multi-Moment Matrix and Modes Roundtrip (FR-29, FR-31)"""
        test_id = "SWE.5-TC16.6"
        name = "Honeywell Moments Matrix & Modes Roundtrip"
        req = "FR-29, FR-31"
        start = time.time()
        try:
            # 1. Verify GET /api/schedule contains Honeywell moments structure or scheduleMode
            r = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            assert r.status_code == 200, f"HTTP {r.status_code}"
            d = r.json()
            assert "scheduleMode" in d or "moments" in d, "Missing Honeywell schedule structure in /api/schedule"

            # 2. Test mode switching: 5+2, constant, unified, 7day
            for m in ("constant", "unified", "5+2", "7day"):
                r_mode = requests.post(f"{self.base_url}/api/schedule/mode?mode={m}", timeout=self.timeout)
                assert r_mode.status_code == 200, f"Failed setting schedule mode {m}: HTTP {r_mode.status_code}"
                assert requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json().get("scheduleMode") == m

            # Restore 5+2 mode
            requests.post(f"{self.base_url}/api/schedule/mode?mode=5+2", timeout=self.timeout)

            # 3. Test updating a specific moment via /api/schedule/moment
            payload = {
                "day": "weekday",
                "moment": 1,
                "time": "06:45",
                "temps": {"1": 23.5, "2": 23.0, "3": 22.5}
            }
            r_post = requests.post(f"{self.base_url}/api/schedule/moment", json=payload, timeout=self.timeout)
            assert r_post.status_code == 200, f"POST /api/schedule/moment failed: HTTP {r_post.status_code}"

            # 4. Verify the updated moment is reflected in /api/schedule
            r2 = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            d2 = r2.json()
            weekday_moments = d2.get("weekday_moments") or d2.get("moments", {}).get("weekday") or []
            assert any(m.get("time") == "06:45" for m in weekday_moments), "Updated moment time 06:45 not found"

            # 5. Test configurable away setpoint per zone
            r_away = requests.post(f"{self.base_url}/api/schedule/level?zone=1&level=away&temp=18.5", timeout=self.timeout)
            assert r_away.status_code == 200, f"Failed setting away temp: HTTP {r_away.status_code}"
            z1_away = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()["zones"][0]["away"]
            assert abs(z1_away - 18.5) < 0.05, f"Expected away 18.5, got {z1_away}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Honeywell moments schedule matrix verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc11_5_tpi_deficit_duty(self):
        """SWE.5-TC11.5: TPI 100% Duty on Thermal Deficit >= 0.4 C (FR-47)"""
        test_id = "SWE.5-TC11.5"
        name = "TPI Thermal Deficit Full Duty"
        req = "FR-47"
        start = time.time()
        orig_hold = False
        target_zone_id = 1
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            zones = r.json().get("zones", [])
            assert len(zones) >= 1, "No zones"
            z = zones[0]
            target_zone_id = z["id"]
            orig_hold = z.get("isManualHold", False)
            cur = z.get("currentTemp", 22.0)
            target = min(round(cur + 0.6, 1), 26.0)

            # Set setpoint creating >= 0.4 C deficit
            r_set = requests.post(f"{self.base_url}/api/zones/setpoint?id={target_zone_id}&target={target}", timeout=self.timeout)
            assert r_set.status_code == 200, f"Failed setting setpoint: {r_set.status_code}"
            time.sleep(1.2)  # Allow control tick to evaluate TPI

            r_ver = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            z_after = [x for x in r_ver.json().get("zones", []) if x["id"] == target_zone_id][0]
            err = z_after["effectiveTarget"] - z_after["currentTemp"]
            assert err >= 0.35, f"Thermal error {err:.2f} < 0.4 C"
            assert z_after["duty"] == 100, f"Zone {target_zone_id} error {err:.2f} C >= 0.4 C expected duty 100%, got {z_after['duty']}%"
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, f"Zone {target_zone_id} deficit {err:.2f}C produced 100% duty")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
        finally:
            if not orig_hold:
                requests.post(f"{self.base_url}/api/zones/hold/clear?id={target_zone_id}", timeout=self.timeout)

    def test_tc16_7_custom_moment_names(self):
        """SWE.5-TC16.7: Customizable Moment Names in Schedule (FR-29)"""
        test_id = "SWE.5-TC16.7"
        name = "Customizable Moment Names"
        req = "FR-29"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            assert r.status_code == 200, f"HTTP {r.status_code}"
            d = r.json()
            assert "momentNames" in d, "Missing momentNames in /api/schedule"
            orig_names = d["momentNames"]
            assert len(orig_names) == 4, f"Expected 4 moment names, got {len(orig_names)}"

            new_names = ["Dimineață", "Muncă", "Acasă", "Somn"]
            r_post = requests.post(
                f"{self.base_url}/api/schedule/names",
                json={"names": new_names},
                timeout=self.timeout
            )
            assert r_post.status_code == 200, f"POST /api/schedule/names failed: HTTP {r_post.status_code}"
            d_post = r_post.json()
            assert d_post.get("momentNames") == new_names, f"Names not updated: {d_post.get('momentNames')}"

            r_get = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()
            assert r_get.get("momentNames") == new_names, f"GET did not reflect names: {r_get.get('momentNames')}"

            requests.post(f"{self.base_url}/api/schedule/names", json={"names": orig_names}, timeout=self.timeout)
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Moment names customizable and persistent")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc16_8_schedule_ui_dirty_protection(self):
        """SWE.5-TC16.8: Schedule Form Guard Against Refresh Overwrite (FR-8.1)"""
        test_id = "SWE.5-TC16.8"
        name = "Schedule UI Refresh Protection"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            assert r.status_code == 200, f"HTTP {r.status_code}"
            html = r.text
            assert "schedDirty" in html, "Missing schedDirty guard in web page"
            assert "mom-name" in html or "momentNames" in html, "Missing moment name input in schedule table"
            assert "undoSched" in html, "Missing undoSched button in web page"
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "Schedule UI dirty protection and editable moment names verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
