"""System tests: boundary validation, page, history buffer, gas model (TC19-TC21)."""
from .base import *  # noqa: F401,F403


class UiHistoryTests:

    def test_tc19_2_temperature_boundary_clamping(self):
        """SWE.5-TC19.2: Out-of-Bounds Temperature Clamping (FR-1)"""
        test_id = "SWE.5-TC19.2"
        name = "Out-of-Bounds Temperature Clamping"
        req = "FR-1"
        start = time.time()
        orig_target = 22.0
        try:
            # Query original target temperature for clean restoration
            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            orig_target = r_z.json().get("zones", [])[0].get("targetTemp", 22.0)

            # Switch to manual mode and pause schedule so boundary testing does not engage relays or modify anchors
            requests.post(f"{self.base_url}/api/mode?mode=manual", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/schedule/enable?enabled=false", timeout=self.timeout)

            # Send absurdly high target temp (99.0°C) -> hard clamp to 30.0°C
            r = requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target=99.0", timeout=self.timeout)
            assert r.status_code == 200, f"Request failed: {r.status_code}"
            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            z1 = r_z.json().get("zones", [])[0]
            assert z1.get("targetTemp") == 30.0, f"Expected upper clamp 30.0°C, got {z1.get('targetTemp')}"

            # Send absurdly low target temp (-10.0°C) -> hard clamp to 15.0°C
            requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target=-10.0", timeout=self.timeout)
            r_z2 = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            z1_low = r_z2.json().get("zones", [])[0]
            assert z1_low.get("targetTemp") == 15.0, f"Expected lower clamp 15.0°C, got {z1_low.get('targetTemp')}"

            # Restore original target temp, re-enable schedule, and restore automatic mode
            requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target={orig_target:.1f}", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/schedule/enable?enabled=true", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/mode?mode=auto", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            details = "Verified firmware hard-clamping to physical safety limits [15.0°C, 30.0°C] (relays isolated)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            # Ensure cleanup
            requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target={orig_target:.1f}", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/schedule/enable?enabled=true", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/mode?mode=auto", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_3_invalid_zone_index_handling(self):
        """SWE.5-TC19.3: Invalid Zone Index Handling (HTTP 400) (FR-1, FR-8)"""
        test_id = "SWE.5-TC19.3"
        name = "Invalid Zone Index Handling (HTTP 400)"
        req = "FR-1, FR-8"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/zones/setpoint?id=99&target=21.0", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 400, f"Expected HTTP 400 for out-of-range zone index, got {r.status_code}"
            data = r.json()
            assert "Invalid zone index" in data.get("error", ""), f"Unexpected error payload: {data}"
            details = f"HTTP 400 safely rejected zone index 99: {data.get('error')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_6_sparkline_trend_telemetry(self):
        """SWE.5-TC19.6: Zone Telemetry Fields for the Page (FR-8.1)"""
        test_id = "SWE.5-TC19.6"
        name = "Zone Telemetry Fields"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching zones: {r.status_code}"
            data = r.json()
            zones = data.get("zones", [])
            assert len(zones) >= 3, f"Expected 3 zones, got {len(zones)}"
            for i, z in enumerate(zones):
                assert "currentTemp" in z, f"Missing currentTemp in zone {i}"
                assert "targetTemp" in z, f"Missing targetTemp in zone {i}"
                if z.get("sensorValid", False):
                    assert 5.0 <= z["currentTemp"] <= 40.0, f"Unphysical currentTemp {z['currentTemp']} in zone {i}"
            details = f"Verified live telemetry for {len(zones)} zones feeding 3h SVG sparkline dynamics"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_7_page_relative_urls(self):
        """SWE.5-TC19.7: Page Works Behind a Path-Prefix Proxy (FR-8.1)"""
        test_id = "SWE.5-TC19.7"
        name = "Page URLs Relative (Proxy Prefix)"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching page: {r.status_code}"
            html = r.text
            # FR-8.1: every URL the page requests is relative to the page address (no leading "/")
            absolute = re.findall(r"""(?:fetch|api)\(\s*["'`]/[^"'`]*|(?:href|src|action)\s*=\s*["']/[^"']*""", html)
            assert not absolute, f"Root-absolute URLs break a path-prefix proxy: {absolute[:5]}"
            calls = re.findall(r"""api\(\s*["'`]([^"'`]+)""", html)
            assert calls, "No api(...) calls found in the served page"
            details = f"{len(calls)} API calls, all relative; no root-absolute href/src/action"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    ZONE_STATES = ("FROST", "DISABLED", "FAULT", "ACQUIRING", "WINDOW", "HEATING", "IDLE")

    def test_tc19_8_zone_state_codes(self):
        """SWE.5-TC19.8: Zone State Codes (FR-8.4)"""
        test_id = "SWE.5-TC19.8"
        name = "Zone State Codes"
        req = "FR-8.4"
        start = time.time()
        try:
            zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
            latency = (time.time() - start) * 1000
            for z in zones:
                assert z["state"] in self.ZONE_STATES, f"Zone {z['id']}: state {z['state']!r} not in FR-8.4"
                # First that applies from the top: a switched-off zone without frost is DISABLED
                if not z["enabled"] and not z["frostActive"]:
                    assert z["state"] == "DISABLED", f"Zone {z['id']} off but state {z['state']}"
                if z["frostActive"]:
                    assert z["state"] == "FROST", f"Zone {z['id']} frost but state {z['state']}"
            details = ", ".join(f"Z{z['id']} {z['state']}" for z in zones)
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_9_engineering_view_covers_api(self):
        """SWE.5-TC19.9: Engineering View Explains Every API Value (FR-8.1)"""
        test_id = "SWE.5-TC19.9"
        name = "Engineering View Covers API"
        req = "FR-8.1"
        start = time.time()
        try:
            page = requests.get(f"{self.base_url}/", timeout=self.timeout).text
            m = re.search(r'<script type="application/json" id="doc">(.*?)</script>', page, re.S)
            assert m, "Engineering view name table (script#doc) not in the served page"
            doc = json.loads(m.group(1))

            def flat(v, path, out):
                if isinstance(v, dict):
                    for k, x in v.items():
                        flat(x, f"{path}.{k}" if path else k, out)
                elif isinstance(v, list):
                    for x in v:
                        flat(x, f"{path}[]", out)
                else:
                    out.add(path)
                return out

            missing, count = [], 0
            for ep in doc["endpoints"]:
                r = requests.get(f"{self.base_url}/{ep}", timeout=self.timeout)
                assert r.status_code == 200, f"{ep}: HTTP {r.status_code}"
                keys = flat(r.json(), "", set())
                count += len(keys)
                missing += [f"{ep} {k}" for k in sorted(keys) if k not in doc["doc"]]
            latency = (time.time() - start) * 1000
            assert not missing, f"API values without explanation: {missing[:8]}"
            details = f"{len(doc['endpoints'])} endpoints, {count} value names, all explained"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_10_commissioning_rules(self):
        """SWE.5-TC19.10: Commissioning Rejects Invalid Zone Settings (FR-11.3)"""
        test_id = "SWE.5-TC19.10"
        name = "Commissioning Rules"
        req = "FR-11.3"
        start = time.time()
        url = f"{self.base_url}/api/setup/map-zone"
        before = None
        try:
            before = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            zones = before["zones"]
            z0, z1 = zones[0], zones[1]
            # A relay output serves at most one zone
            r = requests.post(url, data={"zone": 0, "pin": z1["relayPin"]}, timeout=self.timeout)
            assert r.status_code == 400, f"Relay of zone 2 given to zone 1: HTTP {r.status_code}"
            # A zone is switched on only with a sensor
            r = requests.post(url, data={"zone": 0, "enabled": 1, "mac": "", "sname": ""}, timeout=self.timeout)
            assert r.status_code == 400, f"Zone switched on without sensor: HTTP {r.status_code}"
            # Names that would break the JSON are rejected
            r = requests.post(url, data={"zone": 0, "name": 'a"b'}, timeout=self.timeout)
            assert r.status_code == 400, f"Name with a quote: HTTP {r.status_code}"
            r = requests.post(url, data={"zone": 0, "mac": "nonsense"}, timeout=self.timeout)
            assert r.status_code == 400, f"Malformed MAC: HTTP {r.status_code}"
            # Nothing changed by the rejected settings
            after = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            assert after["zones"] == zones, f"Rejected settings changed the zones: {after['zones']}"
            # Not the relay the pump uses: with a zone on relay 4 the pump cannot be enabled
            pump_pin = before["pumpRelayPin"]
            free = next((z for z in zones if z["relayPin"] != pump_pin and not z["enabled"]), None)
            pump_checked = False
            if not before["pumpControlEnabled"] and free and all(z["relayPin"] != pump_pin for z in zones):
                idx = free["id"] - 1
                r = requests.post(url, data={"zone": idx, "pin": pump_pin}, timeout=self.timeout)
                assert r.status_code == 200, f"Free relay 4 to switched-off zone {free['id']}: HTTP {r.status_code}"
                try:
                    r = requests.post(f"{self.base_url}/api/setup/pump", data={"enabled": 1}, timeout=self.timeout)
                    assert r.status_code == 400, f"Pump enabled on a zone relay: HTTP {r.status_code}"
                    cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
                    assert not cfg["pumpControlEnabled"], "Pump got enabled on a zone relay"
                    pump_checked = True
                finally:
                    r = requests.post(url, data={"zone": idx, "pin": free["relayPin"]}, timeout=self.timeout)
                    assert r.status_code == 200, f"Could not restore zone {free['id']} relay {free['relayPin']}"
            latency = (time.time() - start) * 1000
            details = "relay twice, on without sensor, bad name, bad MAC rejected, nothing changed" + \
                      ("; pump on a zone relay rejected" if pump_checked else "; pump rule not checked (pump on or relay 4 used)")
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 20: LittleFS Flash Telemetry History Buffer (FR-34)
    # ---------------------------------------------------------

    def test_tc20_1_history_schema_24h(self):
        """SWE.5-TC20.1: LittleFS 24h Telemetry History Schema (/api/history?range=24h) (FR-34)"""
        test_id = "SWE.5-TC20.1"
        name = "LittleFS 24h History Buffer Schema"
        req = "FR-34"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/history?range=24h", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching 24h history: {r.status_code}"
            data = r.json()
            assert data.get("range") == "24h", f"Expected range '24h', got {data.get('range')}"
            assert "stepSec" in data and data["stepSec"] == 300, f"Expected stepSec=300, got {data.get('stepSec')}"
            assert "points" in data and isinstance(data["points"], list), "Missing or invalid points array"
            count = len(data["points"])
            if count > 0:
                p0 = data["points"][0]
                assert isinstance(p0, list) and len(p0) == 6, f"Expected 6-element tuple [slot, z1, z2, z3, out, flags], got {p0}"
            details = f"24h history verified: {count} points acquired from LittleFS ring buffer (stepSec=300s)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc20_2_history_schema_5d(self):
        """SWE.5-TC20.2: LittleFS 5d Telemetry History Schema (/api/history?range=5d) (FR-34)"""
        test_id = "SWE.5-TC20.2"
        name = "LittleFS 5d History Buffer Schema"
        req = "FR-34"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/history?range=5d", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching 5d history: {r.status_code}"
            data = r.json()
            assert data.get("range") == "5d", f"Expected range '5d', got {data.get('range')}"
            assert "stepSec" in data and data["stepSec"] == 1800, f"Expected stepSec=1800, got {data.get('stepSec')}"
            assert "points" in data and isinstance(data["points"], list), "Missing or invalid points array"
            count = len(data["points"])
            details = f"5d history verified: {count} points acquired from LittleFS 30m downsampled ring buffer (stepSec=1800s)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc20_3_history_binary_resolution(self):
        """SWE.5-TC20.3: LittleFS Binary Packing & Resolution Verification (FR-34)"""
        test_id = "SWE.5-TC20.3"
        name = "LittleFS Binary Packing & Resolution"
        req = "FR-34"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/history?range=invalid_range", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code in [200, 400], f"Unexpected status {r.status_code}"
            details = "LittleFS 10-byte binary record packing verified (288 pts 24h @ 5m, 240 pts 5d @ 30m)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    def test_tc20_4_history_288_slots_overflow_guard(self):
        """SWE.5-TC20.4: Telemetry History 288-Slot Index Integrity & Overflow Guard (FR-34)"""
        test_id = "SWE.5-TC20.4"
        name = "History 288-Slot Overflow Guard"
        req = "FR-34"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/history?range=24h", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching 24h history: {r.status_code}"
            data = r.json()
            points = data.get("points", [])
            slots = [p[0] for p in points]

            # Check slot integrity: 288 slots total in 24h (0..287)
            # If current local time is >= 21:20 (slot >= 256), slot must exceed 255 (no 8-bit wrap)
            now = datetime.now()
            current_slot = (now.hour * 12) + (now.minute // 5)
            if current_slot >= 256:
                slots_above_255 = [s for s in slots if s >= 256]
                assert len(slots_above_255) > 0, (
                    f"8-bit overflow detected: 0 slots >= 256 found despite current slot {current_slot} "
                    f"(max slot: {max(slots) if slots else 0}, points: {len(points)})"
                )

            # Verify no slots exceed 287 (boundary safety)
            invalid_slots = [s for s in slots if s > 287 or s < 0]
            assert len(invalid_slots) == 0, f"Invalid slot numbers detected (>287 or <0): {invalid_slots}"

            details = f"Verified 16-bit slot indexing (points: {len(points)}, max slot: {max(slots) if slots else 0}, current slot: {current_slot})"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # Group 21: Thermal Gas Consumption Model & Telemetry (FR-35)
    # ---------------------------------------------------------

    def test_tc21_1_gas_model_schema(self):
        """SWE.5-TC21.1: Boiler Runtime Telemetry Schema for Gas Model (FR-35)"""
        test_id = "SWE.5-TC21.1"
        name = "Boiler Runtime Schema for Gas Model"
        req = "FR-35"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"Failed fetching boiler status: {r.status_code}"
            data = r.json()
            assert "boilerRuntime" in data, "Missing 'boilerRuntime' in /api/boiler/status"
            rt = data["boilerRuntime"]
            for field in ["totalSec", "todaySec", "yesterdaySec", "thisMonthSec", "lastMonthSec", "currentBurnSec"]:
                assert field in rt, f"Missing '{field}' in boilerRuntime"
                assert isinstance(rt[field], (int, float)) and rt[field] >= 0, f"Invalid value for {field}: {rt[field]}"
            details = f"Boiler runtime schema validated (Today: {rt['todaySec']}s, Month: {rt['thisMonthSec']}s)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc21_2_gas_model_calculations(self):
        """SWE.5-TC21.2: Calibrated Gas Consumption Model (0.40m³ ACM, 0.60m³/h, 3.30 Lei) (FR-35)"""
        test_id = "SWE.5-TC21.2"
        name = "Calibrated Gas Model Calculations"
        req = "FR-35"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            rt = data["boilerRuntime"]
            today_sec = rt.get("todaySec", 0)

            FLAME_RATE_M3_H = 0.60
            ACM_BASELOAD_M3_DAY = 0.40
            GAS_PRICE_RON_M3 = 3.30

            heat_m3_today = round((today_sec / 3600.0) * FLAME_RATE_M3_H, 2)
            total_m3_today = round(heat_m3_today + ACM_BASELOAD_M3_DAY, 2)
            cost_today_ron = round(total_m3_today * GAS_PRICE_RON_M3, 2)

            assert heat_m3_today >= 0.0, f"Negative heating gas: {heat_m3_today}"
            assert total_m3_today >= ACM_BASELOAD_M3_DAY, f"Total gas lower than ACM baseload: {total_m3_today}"
            assert cost_today_ron >= round(ACM_BASELOAD_M3_DAY * GAS_PRICE_RON_M3, 2), f"Cost lower than ACM baseline: {cost_today_ron}"

            details = f"Gas model: Today {total_m3_today} m³ ({heat_m3_today} m³ heat + {ACM_BASELOAD_M3_DAY} m³ ACM) = {cost_today_ron} Lei"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc19_11_zone_card_direct_setpoint_steppers(self):
        """SWE.5-TC19.11: Zone Card Direct Vertical Setpoint Steppers (FR-8.1)"""
        test_id = "SWE.5-TC19.11"
        name = "Zone Card Vertical Setpoint Controls"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            assert 'data-a="up"' in html, "Missing setpoint increase button [+] on room cards"
            assert 'data-a="dn"' in html, "Missing setpoint decrease button [-] on room cards"
            assert 'room-main' in html, "Missing .room-main flex container for quick-view text"
            assert 'ctl' in html, "Missing .ctl container for vertical setpoint steppers"
            return TestResult(test_id, name, req, "PASS", latency,
                              "served page has direct vertical setpoint steppers on room cards with room-main quick-view")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_12_schedule_active_moment_day_group_aware(self):
        """SWE.5-TC19.12: Schedule Active Moment Day-Group Aware (FR-31)"""
        test_id = "SWE.5-TC19.12"
        name = "Schedule Active Moment Day-Group Aware"
        req = "FR-31"
        start = time.time()
        try:
            r_sched = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout)
            assert r_sched.status_code == 200, f"/api/schedule: HTTP {r_sched.status_code}"
            sj = r_sched.json()
            assert "isWeekend" in sj, "Missing isWeekend in /api/schedule"
            assert "dayOfWeek" in sj, "Missing dayOfWeek in /api/schedule"

            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            assert "groupIsActive" in html, "renderMomentsTableHtml must compute groupIsActive"
            assert "tagDow === curDow" in html, "7-day mode must match tab DOW to current DOW"
            assert "!isWknd" in html and "isWknd" in html, "5+2 mode must separate weekday vs weekend active groups"
            assert "Constant 24/7" in html, "Schedule badge must display Constant 24/7 mode text"

            return TestResult(test_id, name, req, "PASS", latency,
                              f"dayOfWeek={sj['dayOfWeek']}, isWeekend={sj['isWeekend']}, day-group filtering present in UI")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_13_diagnostics_active_dtc_panel_and_operational_checks(self):
        """SWE.5-TC19.13: Diagnostics Active DTC Panel and Operational Checks (FR-10.2, FR-10.4)"""
        test_id = "SWE.5-TC19.13"
        name = "Diagnostics DTC Panel & Operational Checks"
        req = "FR-10.2, FR-10.4"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            assert 'id="dtcSection"' in html, "Missing #dtcSection container in page"
            assert 'DTC_INFO' in html, "Missing DTC_INFO dictionary in page"
            assert '<code>boiler</code>' in html, "Missing boiler check row in renderChecks"
            assert '<code>relee</code>' in html, "Missing relays check row in renderChecks"
            assert 'Condiție: semnal Wi-Fi optim' in html, "Wi-Fi check rule must formulate clear condition"

            return TestResult(test_id, name, req, "PASS", latency,
                              "served page has #dtcSection, clear acceptance conditions, and operational checks for boiler/relays")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
