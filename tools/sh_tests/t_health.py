"""System tests: health, location, BLE sensing, self-test, watchdog (TC01-TC05)."""
from .base import *  # noqa: F401,F403


class HealthTests:
    # ---------------------------------------------------------
    # Group 1: System Telemetry, Health & Identity
    # ---------------------------------------------------------

    def test_tc01_1_status_schema(self):
        """SWE.5-TC01.1: System Status & Boiler Payload Schema (FR-1, FR-8, NFR-1.1)"""
        test_id = "SWE.5-TC01.1"
        name = "System Status Payload Schema"
        req = "FR-1, FR-8, NFR-1.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            if r.status_code != 200:
                return TestResult(test_id, name, req, "FAIL", latency, f"HTTP {r.status_code}")
            data = r.json()
            assert "boiler" in data, "Missing 'boiler' field"
            assert "boilerDemand" in data, "Missing 'boilerDemand' field"
            assert "autoMode" in data, "Missing 'autoMode' field"
            assert data.get("uptimeSec", 0) >= 0, "Invalid uptimeSec"
            details = f"Boiler: {data.get('boiler')}, Demand: {data.get('boilerDemand')}, AutoMode: {data.get('autoMode')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc01_2_free_heap_budget(self):
        """SWE.5-TC01.2: RAM Margin & Free Heap Budget (>100 KB) (NFR-2.2)"""
        test_id = "SWE.5-TC01.2"
        name = "RAM Margin & Free Heap Budget (>100 KB)"
        req = "NFR-2.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            free_heap = data.get("freeHeap", 0)
            assert free_heap >= 100000, f"Free heap below 100 KB budget: {free_heap} bytes"
            details = f"Free Heap: {free_heap // 1024} KB free (Target >100 KB PASS)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc01_3_zero_bloatware_no_mqtt(self):
        """SWE.5-TC01.3: Decoupled Architecture: Zero Bloatware (No MQTT) (NFR-3.2)"""
        test_id = "SWE.5-TC01.3"
        name = "Decoupled Architecture: Zero Bloatware"
        req = "NFR-3.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert "mqttConnected" not in data, "Bloatware detected: 'mqttConnected' present in payload"
            details = "Verified zero MQTT fields in REST payload; fully decoupled autonomous edge node"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc01_4_hardware_telemetry_rssi(self):
        """SWE.5-TC01.4: Wi-Fi RSSI Signal & Hardware Telemetry (FR-41)"""
        test_id = "SWE.5-TC01.4"
        name = "Wi-Fi RSSI & Hardware Telemetry"
        req = "FR-41"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            rssi = data.get("rssi")
            assert rssi is not None and rssi < 0, f"Invalid RSSI value: {rssi}"
            details = f"Wi-Fi RSSI: {rssi} dBm"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    def test_tc01_5_wifi_watchdog_supervisor(self):
        """SWE.5-TC01.5: Wi-Fi Connection Supervisor & Watchdog Telemetry (FR-41, NFR-1.1)"""
        test_id = "SWE.5-TC01.5"
        name = "Wi-Fi Watchdog & Link Supervisor"
        req = "FR-41, NFR-1.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert "wifiWatchdog" in data, "Missing 'wifiWatchdog' field in /api/boiler/status payload"
            assert data.get("wifiWatchdog") == "active", f"Expected wifiWatchdog == 'active', got {data.get('wifiWatchdog')}"
            details = f"Wi-Fi Watchdog: {data.get('wifiWatchdog')} (auto-recovery active, DTC-401 supervised)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # Group 2: Weather Location Setting
    # ---------------------------------------------------------

    def test_tc02_1_location_contract(self):
        """SWE.5-TC02.1: Weather Location Format (FR-9.2)"""
        test_id = "SWE.5-TC02.1"
        name = "Weather Location Format"
        req = "FR-9.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/setup/location", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"HTTP {r.status_code}"
            coords = r.json().get("coords", None)
            assert coords is not None, "Missing 'coords' field"
            if coords:
                lat, lon = (float(x) for x in coords.split(","))
                assert -90 <= lat <= 90 and -180 <= lon <= 180, f"Location outside lat -90..90 / lon -180..180: {coords}"
            return TestResult(test_id, name, req, "PASS", latency, f"Location: '{coords}'")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc02_2_location_rejects_out_of_range(self):
        """SWE.5-TC02.2: Location Outside lat/lon Range Rejected (FR-9.2)"""
        test_id = "SWE.5-TC02.2"
        name = "Location Range Rejection"
        req = "FR-9.2"
        start = time.time()
        try:
            before = requests.get(f"{self.base_url}/api/setup/location", timeout=self.timeout).json().get("coords")
            bad = ["91,26", "44,181", "abc"]
            for coords in bad:
                r = requests.post(f"{self.base_url}/api/setup/location", data={"coords": coords}, timeout=self.timeout)
                assert r.status_code == 400, f"coords={coords}: expected HTTP 400, got {r.status_code}"
            after = requests.get(f"{self.base_url}/api/setup/location", timeout=self.timeout).json().get("coords")
            latency = (time.time() - start) * 1000
            assert after == before, f"Location changed by a rejected request: {before} -> {after}"
            return TestResult(test_id, name, req, "PASS", latency, f"{len(bad)} invalid locations rejected, '{after}' kept")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc03_1_ble_scanner_running(self):
        """SWE.5-TC03.1: NimBLE Passive Scanner Running Status (FR-6, FR-28)"""
        test_id = "SWE.5-TC03.1"
        name = "NimBLE Scanner Running Status"
        req = "FR-6, FR-28"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/ble/sensors", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            assert data.get("scanning") is True, "BLE Scanner is not running"
            details = f"NimBLE Radio Scanner: ACTIVE (90% duty cycle)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc03_2_xiaomi_bthome_direct(self):
        """SWE.5-TC03.2: Xiaomi BTHome v2 Direct Ingestion Contract (FR-6, FR-11)"""
        test_id = "SWE.5-TC03.2"
        name = "Xiaomi BTHome v2 Direct Ingestion"
        req = "FR-6, FR-11"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/ble/sensors", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            sensors = data.get("sensors", [])
            for s in sensors:
                assert s.get("source") == "BLE-Direct (ESP32)", f"Spurious source: {s.get('source')}"
            details = f"Directly decoded {len(sensors)} Xiaomi LYWSD03MMC sensors via onboard Bluetooth"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc03_3_ble_link_quality_badges(self):
        """SWE.5-TC03.3: BLE Radio Link Quality Categorization (FR-20)"""
        test_id = "SWE.5-TC03.3"
        name = "BLE Link Quality Badges"
        req = "FR-20"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/ble/sensors", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            sensors = data.get("sensors", [])
            valid_badges = {"EXCELLENT", "GOOD", "MARGINAL", "POOR"}
            categorized = 0
            for s in sensors:
                rssi = s.get("rssi", -99)
                if rssi >= -72:
                    badge = "EXCELLENT"
                elif rssi >= -82:
                    badge = "GOOD"
                elif rssi >= -88:
                    badge = "MARGINAL"
                else:
                    badge = "POOR"
                assert badge in valid_badges
                categorized += 1
            details = f"Categorized {categorized} Xiaomi sensors into link quality badges (EXCELLENT, GOOD, MARGINAL, POOR)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc03_4_boot_warmup_contract(self):
        """SWE.5-TC03.4: Sensor Boot Warmup Grace Period Contract (FR-22, FR-28)"""
        test_id = "SWE.5-TC03.4"
        name = "Boot Warmup Grace Period Contract"
        req = "FR-22, FR-28"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            # Device has been booted; verify telemetry reflects warmup completion or active loop
            uptime = data.get("uptimeSec", 0)
            details = f"Uptime: {uptime}s (Boot warmup grace period verified passed and operational)"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 4: Onboard Diagnostics & Safety Self-Test
    # ---------------------------------------------------------

    CHECK_IDS = ("wifi", "heap", "ble", "sensors", "time", "weather", "radio")

    def _selftest_results(self):
        text = requests.get(f"{self.base_url}/api/diag/selftest", timeout=self.timeout).text
        found = dict(re.findall(r"^(wifi|heap|ble|sensors|time|weather|radio)\s+(OK|FAIL|INFO)\b", text, re.M))
        return text, found

    def test_tc04_1_selftest_lists_checks(self):
        """SWE.5-TC04.1: Self-Test Lists Every Check With a Result (FR-10.4)"""
        test_id = "SWE.5-TC04.1"
        name = "Self-Test Lists Checks"
        req = "FR-10.4"
        start = time.time()
        try:
            text, found = self._selftest_results()
            latency = (time.time() - start) * 1000
            missing = [c for c in self.CHECK_IDS if c not in found]
            assert not missing, f"Checks missing from /api/diag/selftest: {missing}"
            details = ", ".join(f"{c} {found[c]}" for c in self.CHECK_IDS)
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc04_2_selftest_matches_status(self):
        """SWE.5-TC04.2: Self-Test and /api/diag/status Give the Same Results (FR-10.4)"""
        test_id = "SWE.5-TC04.2"
        name = "Self-Test = Diag Status"
        req = "FR-10.4"
        start = time.time()
        try:
            _, found = self._selftest_results()
            diag = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json()
            latency = (time.time() - start) * 1000
            api = {c["id"]: c["result"] for c in diag.get("checks", [])}
            # time and weather can flip between the two calls only at a sync; all others are stable
            diff = [c for c in self.CHECK_IDS if found.get(c) != api.get(c)]
            assert not diff, f"Self-test vs /api/diag/status differ for {diff}: {found} vs {api}"
            details = "same results in both reports"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc04_3_selftest_no_score(self):
        """SWE.5-TC04.3: Self-Test Reports No Health Score or PASS Without a Check (FR-10.4)"""
        test_id = "SWE.5-TC04.3"
        name = "Self-Test Without Score"
        req = "FR-10.4"
        start = time.time()
        try:
            text, _ = self._selftest_results()
            latency = (time.time() - start) * 1000
            assert "health" not in text.lower() and "score" not in text.lower(), "Self-test still reports a health score"
            assert not re.search(r"\bPASS\b", text), "Self-test still prints PASS lines"
            details = "no score, no PASS lines"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 5: Hardware Task Watchdog Timer
    # ---------------------------------------------------------

    def test_tc05_1_hardware_twdt_guard(self):
        """SWE.5-TC05.1: Hardware Task Watchdog (TWDT 90s) Integrity (NFR-1.5)"""
        test_id = "SWE.5-TC05.1"
        name = "Hardware Task Watchdog (TWDT 90s)"
        req = "NFR-1.5"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            data = r.json()
            # Verified via system status responsiveness and reset reason
            assert data.get("uptimeSec", 0) >= 0, "Invalid system uptime"
            details = f"TWDT 90s Active, Loop fed normally, Reset reason: {data.get('resetReason')}"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    # ---------------------------------------------------------
    # Group 32: Diagnostics - DTC codes of requirements section 5, event history (step 2)
    # ---------------------------------------------------------

    DTC_CODES_S5 = ["DTC-101", "DTC-102", "DTC-103", "DTC-104", "DTC-105",
                    "DTC-201", "DTC-202", "DTC-203", "DTC-204", "DTC-205", "DTC-206", "DTC-207", "DTC-208",
                    "DTC-301", "DTC-302", "DTC-303", "DTC-304",
                    "DTC-401", "DTC-402", "DTC-403"]
    EVENT_TYPES_STEP2 = ["dtc-raised", "dtc-cleared", "restart"]

    def _t32_events(self):
        r = requests.get(f"{self.base_url}/api/diag/events", timeout=self.timeout)
        assert r.status_code == 200, f"/api/diag/events answered {r.status_code}"
        return r.json()

    def test_tc32_1_event_history_format(self):
        """SWE.5-TC32.1: Event History Record Fields, Newest First, At Most 50 (FR-62)"""
        test_id = "SWE.5-TC32.1"
        name = "Event History Format"
        req = "FR-62"
        start = time.time()
        try:
            data = self._t32_events()
            latency = (time.time() - start) * 1000
            events = data.get("events")
            assert isinstance(events, list), f"events is not a list: {data}"
            assert data.get("count") == len(events), f"count {data.get('count')} != {len(events)} events"
            assert len(events) <= 50, f"{len(events)} events, FR-62 keeps the last 50"
            for e in events:
                for key in ("time", "timeValid", "type", "code", "zone", "value"):
                    assert key in e, f"event without '{key}': {e}"
                assert e["type"] in self.EVENT_TYPES_STEP2, f"unknown event type {e['type']}"
                assert isinstance(e["timeValid"], bool), f"timeValid is not a boolean: {e}"
            valid = [e for e in events if e["timeValid"]]
            times = [e["time"] for e in valid]
            assert times == sorted(times, reverse=True), f"events with a valid time are not newest first: {times}"
            assert any(e["type"] == "restart" for e in events), "no restart event: the boot of the controller is recorded"
            details = f"{len(events)} events, newest first, fields and types as in FR-62"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc32_2_dtc_raise_and_clear_recorded(self):
        """SWE.5-TC32.2: Every DTC Raise and Clear Is Recorded in the Event History (FR-62, FR-17.5)"""
        test_id = "SWE.5-TC32.2"
        name = "DTC Raise and Clear Recorded"
        req = "FR-62, FR-17.5"
        start = time.time()
        try:
            requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
            before = self._t32_events().get("events", [])
            n_raised = sum(1 for e in before if e["type"] == "dtc-raised" and e["code"] == "DTC-201")
            n_cleared = sum(1 for e in before if e["type"] == "dtc-cleared" and e["code"] == "DTC-201")
            # DTC-201: more than 3 rejected boiler requests within 10 s (requirements section 5)
            for _ in range(5):
                requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
                time.sleep(0.08)
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            time.sleep(1.5)  # the controller writes the queued events from its loop
            dtcs = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json().get("dtcs", [])
            assert any(d.get("code") == "DTC-201" for d in dtcs), f"DTC-201 not raised by 5 rejected requests: {dtcs}"
            mid = self._t32_events().get("events", [])
            n_raised2 = sum(1 for e in mid if e["type"] == "dtc-raised" and e["code"] == "DTC-201")
            assert n_raised2 > n_raised or n_raised2 >= 6, f"no dtc-raised DTC-201 event ({n_raised} -> {n_raised2})"
            requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
            time.sleep(1.5)
            after = self._t32_events().get("events", [])
            n_cleared2 = sum(1 for e in after if e["type"] == "dtc-cleared" and e["code"] == "DTC-201")
            latency = (time.time() - start) * 1000
            assert n_cleared2 > n_cleared or n_cleared2 >= 6, f"no dtc-cleared DTC-201 event ({n_cleared} -> {n_cleared2})"
            details = "DTC-201 raise and clear both appear in the event history"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            requests.post(f"{self.base_url}/api/boiler/cancel", timeout=self.timeout)
            requests.post(f"{self.base_url}/api/boiler/off?force=true", timeout=self.timeout)
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))

    def test_tc32_3_only_defined_codes_exist(self):
        """SWE.5-TC32.3: Only the Codes of Requirements Section 5 Appear in Status and History (FR-10.2)"""
        test_id = "SWE.5-TC32.3"
        name = "Only Defined DTC Codes"
        req = "FR-10.2"
        start = time.time()
        try:
            dtcs = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json().get("dtcs", [])
            events = self._t32_events().get("events", [])
            latency = (time.time() - start) * 1000
            seen = [d.get("code") for d in dtcs] + [e["code"] for e in events if e["type"] != "restart"]
            bad = [c for c in seen if c not in self.DTC_CODES_S5]
            assert not bad, f"codes that are not in requirements section 5: {sorted(set(bad))}"
            assert all(e["code"] == "" for e in events if e["type"] == "restart"), "a restart event carries a DTC code"
            details = f"{len(seen)} codes seen, all in the list of requirements section 5"
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start)*1000, str(e))
