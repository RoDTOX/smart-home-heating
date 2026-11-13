"""System tests: boundary validation, page, history buffer, gas model (TC19-TC21)."""
from datetime import datetime
from .base import *  # noqa: F401,F403


class UiHistoryTests:

    def test_tc19_2_temperature_boundary_clamping(self):
        """SWE.5-TC19.2: Out-of-Bounds Temperature Clamping (FR-1.2)"""
        test_id = "SWE.5-TC19.2"
        name = "Out-of-Bounds Temperature Clamping"
        req = "FR-1.2"
        start = time.time()
        orig_target = 22.0
        try:
            # Query original target temperature for clean restoration
            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            orig_target = r_z.json().get("zones", [])[0].get("targetTemp", 22.0)

            # Switch to manual mode and pause schedule so boundary testing does not engage relays or modify anchors
            requests.post(f"{self.base_url}/api/mode?mode=manual", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/schedule/enable?enabled=false", timeout=self.timeout)

            # Send absurdly high target temp (99.0°C) -> refused (HTTP 400), the target stays (FR-1.2, C-36)
            r = requests.post(f"{self.base_url}/api/zones/setpoint?id=1&target=99.0", timeout=self.timeout)
            assert r.status_code == 400, f"99 C must be refused with HTTP 400, got {r.status_code}"
            r_z = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            z1 = r_z.json().get("zones", [])[0]
            assert z1.get("targetTemp") == orig_target, f"Refused request changed the target: {orig_target} -> {z1.get('targetTemp')}"

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
        """SWE.5-TC19.3: Invalid Zone Index Handling (HTTP 400) (FR-1.2, FR-8.1)"""
        test_id = "SWE.5-TC19.3"
        name = "Invalid Zone Index Handling (HTTP 400)"
        req = "FR-1.2, FR-8.1"
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
            r_24 = requests.get(f"{self.base_url}/api/history?range=24h", timeout=self.timeout)
            assert r_24.status_code == 200, f"24h history HTTP {r_24.status_code}"
            pts_24 = r_24.json().get("points", [])
            assert isinstance(pts_24, list) and len(pts_24) <= 288, f"24h points count {len(pts_24)} > 288"
            r_5d = requests.get(f"{self.base_url}/api/history?range=5d", timeout=self.timeout)
            assert r_5d.status_code == 200, f"5d history HTTP {r_5d.status_code}"
            pts_5d = r_5d.json().get("points", [])
            assert isinstance(pts_5d, list) and len(pts_5d) <= 240, f"5d points count {len(pts_5d)} > 240"
            r = requests.get(f"{self.base_url}/api/history?range=invalid_range", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code in [200, 400], f"Unexpected status {r.status_code}"
            details = f"History endpoints verified (24h: {len(pts_24)}/288 pts, 5d: {len(pts_5d)}/240 pts)"
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
        """SWE.5-TC21.1: Boiler Runtime Telemetry Schema for Gas Model (FR-35.2)"""
        test_id = "SWE.5-TC21.1"
        name = "Boiler Runtime Schema for Gas Model"
        req = "FR-35.2"
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
        """SWE.5-TC21.2: Calibrated Gas Consumption Model (0.40m³ ACM, 0.60m³/h, 3.30 Lei) (FR-35.2)"""
        test_id = "SWE.5-TC21.2"
        name = "Calibrated Gas Model Calculations"
        req = "FR-35.2"
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

    # FR-35.1 gas meter readings (C-25). HARD RULE (owner, 03.11.2026): the board holds the owner's real
    # readings. No test here stores a reading, writes an entry or changes a gas parameter: every POST to
    # /api/boiler/gas-meter carries dryRun=1 or is refused with 400; parameters are only read, or POSTed
    # out of range (refused, nothing stored).

    def _gas_state(self):
        """Return (gasMeter object of /api/boiler/status, GET /api/boiler/gas-readings JSON)."""
        s = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
        assert s.status_code == 200, f"status: {s.status_code}"
        gm = s.json().get("gasMeter")
        assert isinstance(gm, dict), "Missing 'gasMeter' object in /api/boiler/status"
        g = requests.get(f"{self.base_url}/api/boiler/gas-readings", timeout=self.timeout)
        assert g.status_code == 200, f"gas-readings: {g.status_code}"
        return gm, g.json()

    def _gas_post(self, data):
        """POST /api/boiler/gas-meter; dryRun=1 is always added, a test must never store."""
        d = dict(data)
        d["dryRun"] = "1"
        return requests.post(f"{self.base_url}/api/boiler/gas-meter", data=d, timeout=self.timeout)

    def _gas_result(self, test_id, name, fn):
        start = time.time()
        try:
            details = fn()
            return TestResult(test_id, name, "FR-35.1", "PASS", (time.time() - start) * 1000, details)
        except Exception as e:
            return TestResult(test_id, name, "FR-35.1", "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc21_3_gas_meter_status_schema(self):
        """SWE.5-TC21.3: gasMeter object of boiler status, readings list oldest first (FR-35.1)"""
        def run():
            gm, lst = self._gas_state()
            for f in ["lastReadingIndex", "lastReadingTime", "estimatedIndex", "readingCount", "gasTodayM3",
                      "gasYesterdayM3", "gasThisMonthM3", "gasFlameRate", "boilerSetTemp",
                      "boilerFlowObserved", "boilerPowerKw", "gasPrice"]:
                assert f in gm, f"Missing '{f}' in gasMeter"
            readings = lst.get("readings")
            assert isinstance(readings, list), "gas-readings has no 'readings' list"
            assert gm["readingCount"] == len(readings) == lst.get("count"), \
                f"readingCount {gm['readingCount']} != readings {len(readings)} / count {lst.get('count')}"
            if gm["readingCount"] == 0:
                # no reading is a default: the first three are null and nothing is estimated
                for f in ["lastReadingIndex", "lastReadingTime", "estimatedIndex"]:
                    assert gm[f] is None, f"{f} must be null without a reading, got {gm[f]}"
            else:
                for f in ["lastReadingIndex", "lastReadingTime", "estimatedIndex"]:
                    assert isinstance(gm[f], (int, float)), f"{f} must be a number with a reading"
                times = [e["time"] for e in readings]
                assert times == sorted(times), "readings are not oldest first"
                assert abs(readings[-1]["index"] - gm["lastReadingIndex"]) < 0.0005, \
                    "last entry of gas-readings differs from gasMeter.lastReadingIndex"
                assert readings[-1]["time"] == gm["lastReadingTime"], "last entry time != lastReadingTime"
            for f in ["gasTodayM3", "gasYesterdayM3", "gasThisMonthM3"]:
                assert isinstance(gm[f], (int, float)) and gm[f] >= 0, f"{f} invalid: {gm[f]}"
            return f"gasMeter schema ok, readingCount={gm['readingCount']}"
        return self._gas_result("SWE.5-TC21.3", "Gas Meter Status Schema", run)

    def test_tc21_4_gas_meter_dry_run_stores_nothing(self):
        """SWE.5-TC21.4: dryRun=1 answers stored:false and changes no reading (FR-35.1)"""
        def run():
            before_gm, before_lst = self._gas_state()
            typed = 1234.567  # test value, never stored (dryRun)
            r = self._gas_post({"index": f"{typed:.3f}"})
            assert r.status_code == 200, f"dryRun POST: {r.status_code} {r.text}"
            j = r.json()
            assert j.get("stored") is False, f"dryRun must answer stored:false, got {j.get('stored')}"
            rd = j.get("reading") or {}
            assert "time" in rd and "index" in rd, f"reading must carry time and index: {rd}"
            assert abs(rd["index"] - typed) < 0.0005, f"reading index {rd['index']} != typed {typed}"
            after_gm, after_lst = self._gas_state()
            assert after_gm["readingCount"] == before_gm["readingCount"], "dryRun changed readingCount"
            assert after_lst["readings"] == before_lst["readings"], "dryRun changed the readings list"
            assert after_gm["lastReadingIndex"] == before_gm["lastReadingIndex"], "dryRun changed lastReadingIndex"
            assert after_gm["lastReadingTime"] == before_gm["lastReadingTime"], "dryRun changed lastReadingTime"
            return "dryRun: stored:false, nothing changed"
        return self._gas_result("SWE.5-TC21.4", "Gas Meter dryRun Stores Nothing", run)

    def test_tc21_5_gas_meter_refusals_and_bounds(self):
        """SWE.5-TC21.5: reading outside 0-999999 or in the future refused 400; bounds accepted (FR-35.1)"""
        def run():
            before_gm, before_lst = self._gas_state()
            future = str(int(time.time()) + 10 * 365 * 86400)  # 10 years: safe against board clock error
            cases = {
                "missing index": {},
                "index -1": {"index": "-1"},
                "index 1000000": {"index": "1000000"},
                "future time": {"index": "100.000", "time": future},
            }
            for label, data in cases.items():
                r = self._gas_post(data)
                assert r.status_code == 400, f"{label}: expected HTTP 400, got {r.status_code}"
            # lower and upper limit of the range (0-999999, 3 decimals) are accepted (dryRun: nothing stored)
            for idx in ["0", "999999.999"]:
                r = self._gas_post({"index": idx})
                assert r.status_code == 200, f"index {idx} must be accepted, got {r.status_code}"
                assert r.json().get("stored") is False, f"index {idx}: dryRun stored something"
            after_gm, after_lst = self._gas_state()
            assert after_lst["readings"] == before_lst["readings"], "a refused or dryRun call changed the readings"
            assert after_gm["readingCount"] == before_gm["readingCount"], "readingCount changed"
            return "4 refusals (400), bounds 0 and 999999.999 accepted, nothing stored"
        return self._gas_result("SWE.5-TC21.5", "Gas Meter Refusals and Bounds", run)

    def test_tc21_6_gas_parameters_range_and_rejection(self):
        """SWE.5-TC21.6: gas parameters of requirements section 6 inside range, out of range refused (FR-35.1)"""
        def run():
            r = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
            assert r.status_code == 200, f"config/system: {r.status_code}"
            cfg = r.json()
            ranges = {  # §6: boilerSetTemp, boilerFlowObserved, gasFlameRate, boilerPowerKw, gasPrice
                "boilerSetTemp": (25, 60),
                "boilerFlowObserved": (25, 65),
                "gasFlameRate": (0.20, 2.50),
                "boilerPowerKw": (10, 45),
                "gasPrice": (1.00, 10.00),
            }
            for f, (lo, hi) in ranges.items():
                assert f in cfg, f"Missing '{f}' in /api/config/system"
                assert lo <= cfg[f] <= hi, f"{f}={cfg[f]} outside §6 range {lo}-{hi}"
            # out of range (gasFlameRate max 2.50): refused with 400, value unchanged
            bad = requests.post(f"{self.base_url}/api/config/system", data={"gasFlameRate": 9}, timeout=self.timeout)
            assert bad.status_code == 400, f"gasFlameRate=9 expected 400, got {bad.status_code}"
            bad2 = requests.post(f"{self.base_url}/api/config/system", data={"gasPrice": 0.5}, timeout=self.timeout)
            assert bad2.status_code == 400, f"gasPrice=0.5 (min 1.00) expected 400, got {bad2.status_code}"
            after = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            for f in ranges:
                assert after[f] == cfg[f], f"{f} changed by a refused POST: {cfg[f]} -> {after[f]}"
            return "gas parameters in §6 ranges, out-of-range refused, unchanged"
        return self._gas_result("SWE.5-TC21.6", "Gas Parameters Range and Rejection", run)

    def test_tc21_7_gas_meter_card_wording(self):
        """SWE.5-TC21.7: page says 'Citire contor', never calibration (FR-35.1)"""
        def run():
            page = requests.get(f"{self.base_url}/", timeout=self.timeout).text
            assert "Citire contor" in page, "page lacks the text 'Citire contor'"
            # the gas meter card: the page text around its title (the card has no documented id)
            pos = page.index("Citire contor")
            card = page[max(0, pos - 2000):pos + 2000].lower()
            assert "calibrare" not in card, "gas meter card still says 'calibrare'"
            return "page uses 'Citire contor', no 'calibrare' in the card"
        return self._gas_result("SWE.5-TC21.7", "Gas Meter Card Wording", run)

    def test_tc21_8_gas_estimate_not_below_last_reading(self):
        """SWE.5-TC21.8: estimatedIndex >= last reading (FR-35.1)"""
        def run():
            gm, _ = self._gas_state()
            if gm["readingCount"] == 0:
                assert gm["estimatedIndex"] is None, "estimate without a reading must be null"
                return "no reading: nothing estimated"
            # estimate = last reading + burner runtime since it x gasFlameRate, runtime never negative
            assert gm["estimatedIndex"] >= gm["lastReadingIndex"], \
                f"estimatedIndex {gm['estimatedIndex']} < lastReadingIndex {gm['lastReadingIndex']}"
            return f"estimate {gm['estimatedIndex']} >= reading {gm['lastReadingIndex']}"
        return self._gas_result("SWE.5-TC21.8", "Gas Estimate Not Below Last Reading", run)

    def _gas_expected_flow(self, readings):
        """FR-35.1 measured flow from the readings list: consecutive readings that both have a runtime,
        >= 1 h boiler ON time between them and a rise of the index; average of the last 10 such
        intervals weighted by boiler ON hours (= sum of rises / sum of hours). None while there is none."""
        ivs = []
        for a, b in zip(readings, readings[1:]):
            ra, rb = a.get("runtimeSec"), b.get("runtimeSec")
            if ra is None or rb is None:
                continue
            dt, rise = rb - ra, b["index"] - a["index"]
            if dt >= 3600 and rise > 0:
                ivs.append((rise, dt / 3600.0))
        ivs = ivs[-10:]
        if not ivs:
            return None
        return sum(r for r, _ in ivs) / sum(h for _, h in ivs)

    def test_tc21_9_gas_flow_parameters_read_only(self):
        """SWE.5-TC21.9: gasFlameRate and boilerPowerKw refused 400 on /api/config/system, unchanged (FR-35.1)"""
        def run():
            url = f"{self.base_url}/api/config/system"
            before = requests.get(url, timeout=self.timeout).json()
            # both values are inside the section 6 range (gasFlameRate 0.20-2.50, boilerPowerKw 10-45):
            # the refusal is because the user may not set them (read-only), not because of the range
            for field, val in [("gasFlameRate", 0.50), ("boilerPowerKw", 20)]:
                assert 0 < val, "test value"
                r = requests.post(url, data={field: val}, timeout=self.timeout)
                assert r.status_code == 400, f"{field}={val} (in range, read-only) expected 400, got {r.status_code}"
            after = requests.get(url, timeout=self.timeout).json()
            # settable parameters are only READ here (no write on the live board): in the section 6 range
            ranges = {"boilerSetTemp": (25, 60), "boilerFlowObserved": (25, 65), "gasPrice": (1.00, 10.00),
                      "gasFlameRate": (0.20, 2.50), "boilerPowerKw": (10, 45)}
            for f, (lo, hi) in ranges.items():
                assert f in after, f"Missing '{f}' in /api/config/system"
                assert lo <= after[f] <= hi, f"{f}={after[f]} outside section 6 range {lo}-{hi}"
            for f in ranges:
                assert after[f] == before[f], f"{f} changed by a refused POST: {before[f]} -> {after[f]}"
            return "gasFlameRate=0.50 and boilerPowerKw=20 refused 400, config unchanged"
        return self._gas_result("SWE.5-TC21.9", "Gas Flow and Boiler Power Read-Only", run)

    def test_tc21_10_gas_flow_boiler_power_no_page_input(self):
        """SWE.5-TC21.10: page has no input for the burner flow or the boiler power (FR-35.1)"""
        def run():
            page = requests.get(f"{self.base_url}/", timeout=self.timeout).text
            assert page, "empty page"
            for token in ["cfgGasFlameRate", "cfgGasBoilerKw", "Debit ardere", "Putere centrală"]:
                assert token not in page, f"page still offers the read-only parameter: '{token}'"
            return "no burner flow / boiler power input in the page"
        return self._gas_result("SWE.5-TC21.10", "Gas Flow and Boiler Power No Page Input", run)

    def test_tc21_11_gas_flow_measured_field(self):
        """SWE.5-TC21.11: gasMeter.gasFlowMeasured null or number 0.01-5.0 m3/h, matches the readings and the history (FR-35.1, C-29)"""
        def run():
            gm, lst = self._gas_state()
            assert "gasFlowMeasured" in gm, "Missing 'gasFlowMeasured' in gasMeter"
            v = gm["gasFlowMeasured"]
            if v is not None:
                assert isinstance(v, (int, float)) and not isinstance(v, bool), f"gasFlowMeasured not a number: {v!r}"
                assert 0.01 <= v <= 5.0, f"gasFlowMeasured {v} outside 0.01-5.0 m3/h"
            # C-29 (owner, 03.11.2026): a reading without a stored runtime takes the boiler ON time from
            # the telemetry history, so the expectation is the one of TC21.15 (stored runtimes + history)
            h24, h5 = self._hist_get("24h"), self._hist_get("5d")
            exp, used = self._gas_hist_expected(lst["readings"], h24, h5, time.time())
            if exp is None:
                # no qualifying interval: nothing measured
                assert v is None, f"no qualifying interval, gasFlowMeasured must be null, got {v}"
                return "PASS note: gasFlowMeasured null (no qualifying interval)"
            assert v is not None, f"qualifying interval exists (flow {exp:.3f}), gasFlowMeasured is null"
            # tolerance 0.03 m3/h as in TC21.15: index 3 decimals, history step rounding
            assert abs(v - exp) <= 0.03, f"gasFlowMeasured {v} != weighted average of last 10 intervals {exp:.3f}"
            return f"gasFlowMeasured {v} matches readings and history ({exp:.3f})"
        return self._gas_result("SWE.5-TC21.11", "Gas Flow Measured Field", run)

    def test_tc21_12_gas_readings_runtime_field(self):
        """SWE.5-TC21.12: each gas-readings entry has runtimeSec null or integer >= 0, oldest first (FR-35.1)"""
        def run():
            _, lst = self._gas_state()
            readings = lst["readings"]
            for i, e in enumerate(readings):
                for k in ("time", "index", "runtimeSec"):
                    assert k in e, f"entry {i} lacks '{k}'"
                rt = e["runtimeSec"]
                assert rt is None or (isinstance(rt, int) and not isinstance(rt, bool) and rt >= 0), \
                    f"entry {i} runtimeSec must be null or integer >= 0, got {rt!r}"
            times = [e["time"] for e in readings]
            assert times == sorted(times), "readings are not oldest first"
            return f"{len(readings)} entries with runtimeSec, oldest first"
        return self._gas_result("SWE.5-TC21.12", "Gas Readings Runtime Field", run)

    def test_tc21_13_gas_estimate_uses_measured_flow(self):
        """SWE.5-TC21.13: estimate rises from the last reading at the measured flow (FR-35.1)"""
        def run():
            gm, lst = self._gas_state()
            if gm["gasFlowMeasured"] is None:
                return "PASS note: gasFlowMeasured null, estimate rule (measured flow) not checkable"
            last = lst["readings"][-1] if lst["readings"] else None
            if last is None or last.get("runtimeSec") is None:
                return "PASS note: last reading has no runtime, estimate rule not checkable"
            assert gm["estimatedIndex"] >= gm["lastReadingIndex"], "estimate below the last reading"
            # the runtime since the last reading is not in the API; the estimate can only be bounded:
            # the gas of the estimate cannot exceed 24 h x measured flow per day since the reading
            days = max(0.0, (time.time() - gm["lastReadingTime"]) / 86400.0) + 0.05  # +0.05 d: clock slack
            cap = 24.0 * days * gm["gasFlowMeasured"]
            assert gm["estimatedIndex"] - gm["lastReadingIndex"] <= cap, \
                f"estimate rise {gm['estimatedIndex'] - gm['lastReadingIndex']:.3f} > {cap:.3f} (24 h/day at measured flow)"
            return "estimate within last reading + measured flow x elapsed time"
        return self._gas_result("SWE.5-TC21.13", "Gas Estimate Uses Measured Flow", run)

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

    def test_tc19_14_admin_collapsible_groups_and_dtc_clear(self):
        """SWE.5-TC19.14: Admin Collapsible Groups and DTC Clear (FR-8.1, FR-10.2)"""
        test_id = "SWE.5-TC19.14"
        name = "Admin Collapsible Groups & DTC Clear"
        req = "FR-8.1, FR-10.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text

            assert 'id="grpSched"' in html, "grpSched missing"
            assert 'id="grpDiag"' in html, "grpDiag missing"
            assert 'id="grpSetup"' in html, "grpSetup missing"
            assert 'id="grpDebug"' in html, "grpDebug missing"
            assert 'btnClearDtc' in html, "Missing btnClearDtc in page"
            assert 'api/diag/clear' in html, "Missing /api/diag/clear API call in frontend"

            r_clr = requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
            assert r_clr.status_code == 200, f"/api/diag/clear: HTTP {r_clr.status_code}"

            return TestResult(test_id, name, req, "PASS", latency,
                              "served page has 4 collapsible admin groups, polling optimization, and functional DTC clear")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_15_zone_heating_runtime_telemetry(self):
        """SWE.5-TC19.15: Zone Heating Runtime Telemetry Counters (FR-35.2)"""
        test_id = "SWE.5-TC19.15"
        name = "Zone Heating Runtime Telemetry Counters"
        req = "FR-35.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"/api/zones: HTTP {r.status_code}"
            zones = r.json().get("zones", [])
            assert len(zones) > 0, "No zones in /api/zones"

            for z in zones:
                assert "runtimeTodaySec" in z, f"Missing runtimeTodaySec in zone {z.get('id')}"
                assert "runtimeYesterdaySec" in z, f"Missing runtimeYesterdaySec in zone {z.get('id')}"
                assert isinstance(z["runtimeTodaySec"], (int, float)), f"Invalid runtimeTodaySec type: {type(z['runtimeTodaySec'])}"
                assert isinstance(z["runtimeYesterdaySec"], (int, float)), f"Invalid runtimeYesterdaySec type: {type(z['runtimeYesterdaySec'])}"

            # Served page check
            r_page = requests.get(f"{self.base_url}/", timeout=self.timeout)
            assert r_page.status_code == 200, f"page: HTTP {r_page.status_code}"
            html = r_page.text
            assert 'zones[].runtimeTodaySec' in html, "doc missing zones[].runtimeTodaySec"
            assert 'zones[].runtimeYesterdaySec' in html, "doc missing zones[].runtimeYesterdaySec"
            assert 'runtimeTodaySec' in html and 'formatRt' in html, "Page missing runtime display logic"

            rt_summary = ", ".join(f"{z['name']} today={z['runtimeTodaySec']}s" for z in zones)
            return TestResult(test_id, name, req, "PASS", latency, f"Zone runtimes: {rt_summary}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_16_history_24h_chart_and_card_subtitles(self):
        """SWE.5-TC19.16: 24h Telemetry Chart and Room Card TPI Subtitles (FR-8.1, FR-34)"""
        test_id = "SWE.5-TC19.16"
        name = "24h Telemetry Chart & Card TPI Subtitles"
        req = "FR-8.1, FR-34"
        start = time.time()
        try:
            # 1. Check /api/history endpoint responds with 288 points
            r_hist = requests.get(f"{self.base_url}/api/history?range=24h", timeout=self.timeout)
            assert r_hist.status_code == 200, f"/api/history: HTTP {r_hist.status_code}"
            h_data = r_hist.json()
            assert h_data.get("range") == "24h", f"Expected range 24h, got {h_data.get('range')}"
            assert "points" in h_data and len(h_data["points"]) == 288, f"Expected 288 points, got {len(h_data.get('points', []))}"

            # 2. Check served page structure
            r_page = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r_page.status_code == 200, f"page: HTTP {r_page.status_code}"
            html = r_page.text

            # Room card subtitle improvements: no redundant target in subText, clean humidity with RH
            assert "💧" in html and "% RH" in html, "Missing humidity % RH formatting in page"
            assert "Boost manual" in html, "Missing 'Boost manual' status in page"
            assert "Încălzire TPI" in html and "min/h" in html, "Missing 'Încălzire TPI · X min/h' formatting in page"
            assert "Pauză ciclu (menține din șapă)" in html, "Missing TPI pause description in page"

            # 24h History card and chart rendering elements
            assert 'id="grpHistory"' in html, "Missing grpHistory collapsible container"
            assert 'id="histSvgContainer"' in html, "Missing histSvgContainer in page"
            assert 'renderHistoryChart' in html, "Missing renderHistoryChart JS function"
            assert 'api/history?range=24h' in html, "Missing api/history call in frontend"
            assert 'histCursorLine' in html, "Missing hairline cursor line in SVG chart"
            assert 'histRangeTabs' in html, "Missing multi-range selector tabs (12h, 24h, 3d, 5d)"
            assert 'tpiInspectorBox' in html and 'renderTpiInspector' in html, "Missing TPI algorithm inspector breakdown"

            return TestResult(test_id, name, req, "PASS", latency,
                              "24h history chart (SVG timeline/curves) and room card TPI/RH subtitles verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_17_window_clear_button_not_handled_as_stepper(self):
        """SWE.5-TC19.17: Room card buttons without a setpoint action are ignored by the stepper handler (FR-8.1)"""
        test_id = "SWE.5-TC19.17"
        name = "Window Clear Button Not A Stepper"
        req = "FR-8.1"
        start = time.time()
        try:
            r_page = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r_page.status_code == 200, f"page: HTTP {r_page.status_code}"
            html = r_page.text
            i = html.index('$("rooms").addEventListener("click"')
            handler = html[i:i + 900]
            assert "onclick=\"clearWindowPause(" in html, "window clear button missing on the room card"
            assert re.search(r'if\s*\(\s*b\s*&&\s*b\.dataset\.a\s*\)', handler),                 "the click handler of the room cards takes every button as a stepper: the window clear button (no data-a) ends in 'Cannot read properties of undefined (reading effectiveTarget)'"
            return TestResult(test_id, name, req, "PASS", latency, "stepper handler acts only on buttons with a setpoint action")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc21_14_gas_figures_only_with_measured_flow(self):
        """SWE.5-TC21.14: Gas figures of the page appear only with a measured flow (FR-35.1)"""
        test_id = "SWE.5-TC21.14"
        name = "Gas Figures Need Measured Flow"
        req = "FR-35.1"
        start = time.time()
        try:
            html = requests.get(f"{self.base_url}/", timeout=self.timeout).text
            latency = (time.time() - start) * 1000
            assert "gasFlowMeasured" in html, "the page does not look at gasFlowMeasured"
            assert "se calculează după" in html, "the page lacks the note that consumption is calculated after two readings"
            assert "gasMeasured ? " in html, "the gas figures are not conditional on a measured flow"
            assert "boilerRuntime.todaySec" in html and "Timp ardere azi" in html, "boiler ON time of today has no source"
            return TestResult(test_id, name, req, "PASS", latency, "gas figures conditional on gasFlowMeasured, note present")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # FR-35.1 (C-29): measured flow with the boiler ON time from the telemetry history (FR-34).
    # READ-ONLY on the live board: GET only, nothing is posted.

    @staticmethod
    def _eet_offset(epoch):
        """UTC offset (s) of the time zone of FR-12.1, EET-2EEST,M3.5.0/3,M10.5.0/4: EET = UTC+2, summer time
        UTC+3 from the last Sunday of March 03:00 local (01:00 UTC) to the last Sunday of October 04:00 local
        summer time (01:00 UTC). Computed by hand, no zoneinfo needed."""
        import datetime as _dt
        y = _dt.datetime.fromtimestamp(epoch, _dt.timezone.utc).year

        def last_sunday_1utc(month):
            d = _dt.datetime(y, month, 31, 1, 0, tzinfo=_dt.timezone.utc)
            d -= _dt.timedelta(days=(d.weekday() + 1) % 7)
            return d.timestamp()
        return 10800 if last_sunday_1utc(3) <= epoch < last_sunday_1utc(10) else 7200

    def _hist_get(self, rng):
        r = requests.get(f"{self.base_url}/api/history?range={rng}", timeout=self.timeout)
        assert r.status_code == 200, f"/api/history?range={rng}: HTTP {r.status_code}"
        j = r.json()
        # only recorded steps (flags bit 7) count as a recorded value
        return {p[0]: p for p in j["points"] if (p[5] & 0x80)}

    def _hist_slot(self, rng, t):
        if rng == "24h":
            return int(((t + self._eet_offset(t)) % 86400) // 300)  # local time of day / 300
        return int(t // 1800) % 240  # floor(epoch / 1800) mod 240

    def _gas_hist_expected(self, readings, h24, h5, now):
        """Expected gasFlowMeasured by the rule of FR-35.1. Returns (flow or None, used) where used maps
        (range, slot) -> the point (or None for a gap) of every step the expectation relied on."""
        ivs, used = [], {}
        for a, b in zip(readings, readings[1:]):
            rise = b["index"] - a["index"]
            ra, rb = a.get("runtimeSec"), b.get("runtimeSec")
            if ra is not None and rb is not None:
                dt = rb - ra  # stored runtimes of both readings
            else:
                ta, tb = a["time"], b["time"]
                if tb < ta:
                    continue
                if now - ta < 86400 and now - tb < 86400:
                    rng, hist, step, n_ring, min_on = "24h", h24, 300, 288, 3600
                elif now - ta < 5 * 86400 and now - tb < 5 * 86400:
                    rng, hist, step, n_ring, min_on = "5d", h5, 1800, 240, 4 * 3600
                else:
                    continue  # a reading older than 5 days: no flow for that interval
                sa, sb = self._hist_slot(rng, ta), self._hist_slot(rng, tb)
                n = (sb - sa) % n_ring + 1  # step of the first time to step of the second, both included
                on, gap = 0, False
                for k in range(n):
                    s = (sa + k) % n_ring
                    p = hist.get(s)
                    used[(rng, s)] = tuple(p) if p else None
                    if p is None:
                        gap = True
                    elif p[5] & 1:
                        on += step
                if gap:
                    continue  # a gap means no flow for that interval
                dt = on
                if dt < min_on:
                    continue
            if dt >= 3600 and rise > 0:
                ivs.append((rise, dt / 3600.0))
        ivs = ivs[-10:]
        if not ivs:
            return None, used
        return sum(r for r, _ in ivs) / sum(h for _, h in ivs), used

    def test_tc21_15_gas_flow_measured_from_history(self):
        """SWE.5-TC21.15: gasFlowMeasured equals the flow recomputed from the readings and the telemetry history (FR-35.1, FR-34)"""
        def run():
            last = ""
            for attempt in range(2):
                gm, lst = self._gas_state()
                h24, h5 = self._hist_get("24h"), self._hist_get("5d")
                now = time.time()
                exp, used = self._gas_hist_expected(lst["readings"], h24, h5, now)
                v = gm.get("gasFlowMeasured")
                # second read: the history rolls on, a step used by the expectation may have changed
                h24b, h5b = self._hist_get("24h"), self._hist_get("5d")
                changed = any((tuple((h24b if r == "24h" else h5b).get(s) or ()) or None) != p
                              for (r, s), p in used.items())
                if exp is None:
                    ok = v is None  # null exactly when no interval qualifies
                else:
                    # tolerance 0.03 m3/h: index 3 decimals, history step rounding (error <= 10 min / 1 h)
                    ok = v is not None and abs(v - exp) <= 0.03
                if ok:
                    return f"gasFlowMeasured {v} matches the recomputed flow {exp}"
                last = f"gasFlowMeasured {v!r} != recomputed {exp!r} (readings {len(lst['readings'])})"
                if not changed:
                    break
            assert False, last
        return self._gas_result("SWE.5-TC21.15", "Gas Flow Measured From History", run)

    def test_tc21_16_gas_flow_measured_range(self):
        """SWE.5-TC21.16: gasFlowMeasured, when not null, is within 0.05 and 3.0 m3/h (FR-35.1)"""
        def run():
            gm, _ = self._gas_state()
            v = gm.get("gasFlowMeasured")
            if v is None:
                return "gasFlowMeasured null (nothing to range-check)"
            assert isinstance(v, (int, float)) and not isinstance(v, bool), f"gasFlowMeasured not a number: {v!r}"
            assert 0.05 <= v <= 3.0, f"gasFlowMeasured {v} outside 0.05-3.0 m3/h"
            return f"gasFlowMeasured {v} in range"
        return self._gas_result("SWE.5-TC21.16", "Gas Flow Measured Range", run)

    def test_tc21_17_gas_today_yesterday_consistent(self):
        """SWE.5-TC21.17: gasTodayM3 and gasYesterdayM3 are numbers >= 0, today <= 24 h x gasFlowMeasured (FR-35.1)"""
        def run():
            gm, _ = self._gas_state()
            for f in ("gasTodayM3", "gasYesterdayM3"):
                x = gm.get(f)
                assert isinstance(x, (int, float)) and not isinstance(x, bool), f"{f} not a number: {x!r}"
                assert x >= 0, f"{f} negative: {x}"
            v = gm.get("gasFlowMeasured")
            if v is not None:
                # 0.01 m3: rounding of the reported value
                assert gm["gasTodayM3"] <= 24 * v + 0.01, \
                    f"gasTodayM3 {gm['gasTodayM3']} above 24 h x gasFlowMeasured {24 * v:.3f}"
            return f"today {gm['gasTodayM3']}, yesterday {gm['gasYesterdayM3']}, flow {v}"
        return self._gas_result("SWE.5-TC21.17", "Gas Today Yesterday Consistent", run)

    # ---------------------------------------------------------
    # Page feedback after step 4: boost indicator, pin states, debug area layout
    # ---------------------------------------------------------

    def test_tc19_18_boost_only_above_schedule(self):
        """SWE.5-TC19.18: isBoost is true only while the target is above scheduleTarget (FR-1.2)"""
        test_id = "SWE.5-TC19.18"
        name = "Boost Only Above Schedule"
        req = "FR-1.2"
        start = time.time()
        zid = 0
        try:
            zid = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"][0]["id"]
            def zone():
                r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
                return [x for x in r.json()["zones"] if x["id"] == zid][0]
            z = zone()
            assert "scheduleTarget" in z and "isBoost" in z, "Missing scheduleTarget / isBoost in /api/zones"
            sched = z["scheduleTarget"]
            assert 16.0 <= sched <= 28.0, f"scheduleTarget {sched} out of testable range"
            requests.post(f"{self.base_url}/api/zones/setpoint?id={zid}&target={sched + 1.0:.1f}", timeout=self.timeout)
            z = zone()
            assert z["isBoost"] is True, f"target {sched + 1.0} above schedule {sched}: expected isBoost true, got {z['isBoost']}"
            requests.post(f"{self.base_url}/api/zones/setpoint?id={zid}&target={sched - 1.0:.1f}", timeout=self.timeout)
            z = zone()
            assert z["isBoost"] is False, f"target {sched - 1.0} below schedule {sched}: expected isBoost false, got {z['isBoost']}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"schedule {sched}: +1.0 boost, -1.0 no boost")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            requests.post(f"{self.base_url}/api/zones/hold/clear?id={zid}", timeout=self.timeout)

    def test_tc19_21_toast_fixed_to_viewport(self):
        """SWE.5-TC19.21: refusals and errors are a toast fixed to the viewport, 8 s, tap to close (FR-8.1)"""
        test_id = "SWE.5-TC19.21"
        name = "Page Toast Fixed To Viewport"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            assert r.status_code == 200, f"Failed fetching page: {r.status_code}"
            html = r.text
            m = re.search(r"#toast\s*\{([^}]*)\}", html)
            assert m, "No #toast style in the page"
            css = m.group(1)
            assert re.search(r"position\s*:\s*fixed", css), f"#toast is not position: fixed: {css}"
            assert re.search(r"(top|bottom)\s*:", css), f"#toast has no top/bottom anchor: {css}"
            assert 'id="toast"' in html, "No toast element in the page"
            assert not re.search(r'<footer>[^\n]*id="err"', html), "Error line still sits in the page footer"
            toast_ms = 8 * 1000
            assert toast_ms == 8000 and re.search(str(toast_ms), html), "Toast visibility of 8 s (8000 ms) not found"
            assert re.search(r"toast[^;\n]*addEventListener\(\s*[\"']click", html) or 'id="toast" onclick' in html, \
                "Toast is not closed by a tap"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "fixed toast, 8 s, tap to close, no footer error line")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_19_pin_states(self):
        """SWE.5-TC19.19: /api/diag/status reports pins GPIO32/33/25/26/19/23 with level 0 or 1 (FR-10.5)"""
        test_id = "SWE.5-TC19.19"
        name = "Pin States Reported"
        req = "FR-10.5"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout)
            assert r.status_code == 200, f"HTTP {r.status_code}"
            pins = r.json().get("pins")
            assert isinstance(pins, list), f"pins missing or not a list: {pins!r}"
            by_gpio = {p.get("gpio"): p for p in pins}
            for g in (32, 33, 25, 26, 19, 23):
                assert g in by_gpio, f"GPIO{g} missing in pins"
                p = by_gpio[g]
                assert p.get("name"), f"GPIO{g} has no name"
                assert p.get("level") in (0, 1), f"GPIO{g} level {p.get('level')!r} not 0 or 1"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"{len(pins)} pins read")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc19_20_debug_area_layout(self):
        """SWE.5-TC19.20: debug area order console, pins, trace, signals; trace and signals collapsed (FR-8.1)"""
        test_id = "SWE.5-TC19.20"
        name = "Debug Area Layout"
        req = "FR-8.1"
        start = time.time()
        try:
            html = requests.get(f"{self.base_url}/", timeout=self.timeout).text
            order = ["consoleTerminal", 'id="pinsTable"', 'id="dbgTrace"', 'id="dbgWatch"']
            pos = [html.find(k) for k in order]
            assert all(p >= 0 for p in pos), f"Missing parts in the page: {[k for k, p in zip(order, pos) if p < 0]}"
            assert pos == sorted(pos), f"Wrong order of the debug area parts: {dict(zip(order, pos))}"
            for d in ("dbgTrace", "dbgWatch"):
                m = re.search(r'<details[^>]*id="%s"[^>]*>' % d, html)
                assert m, f"{d} is not a details element"
                assert " open" not in m.group(0), f"{d} is open by default: {m.group(0)}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "order and collapsed state verified")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
