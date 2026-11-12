"""System tests of migration step 6 (SensorHub): reading plausibility FR-53, DTC-103 / DTC-104, one sensor timeout FR-16.2, FR-18.

Written from the requirement text and requirements section 6 only. Every test adds a zone with a made-up sensor MAC
(zone-add, FR-73, output GPIO26, switched off), feeds readings through the development hook
POST /api/dev/sensor (only in the dev-open build; the board has no other way to give a reading) and deletes the zone.
Injected accepted readings stay at 14 C or more, so frost protection (FR-16.1) never starts the boiler.
"""
from .base import *  # noqa: F401,F403

HUB_MAC = "AA:BB:CC:DD:EE:01"


class SensorHubTests:
    def _hub_inject(self, temp, mac=HUB_MAC):
        r = requests.post(f"{self.base_url}/api/dev/sensor", data={"mac": mac, "temp": temp, "hum": 50, "bat": 80, "rssi": -60},
                          timeout=self.timeout)
        assert r.status_code == 200, f"/api/dev/sensor temp {temp}: HTTP {r.status_code}"

    def _hub_zone(self, n):
        zs = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
        return next(z for z in zs if z["id"] == n)

    def _hub_dtcs(self):
        d = requests.get(f"{self.base_url}/api/diag/status", timeout=self.timeout).json()
        return [x["code"] for x in d["dtcs"]]

    def _hub_dtc_active(self, code):
        return any(c.startswith(code) or code in c for c in self._hub_dtcs())

    def _hub_new_zone(self, cfg0, enabled=0):
        n = cfg0["zonesCount"] + 1
        assert self._t30_add().status_code == 200, "zone-add failed"
        r = requests.post(f"{self.base_url}/api/setup/map-zone",
                          data={"zone": n - 1, "mac": HUB_MAC, "enabled": enabled, "target": 15.0}, timeout=self.timeout)
        assert r.status_code == 200, f"map-zone: HTTP {r.status_code} {r.text[:100]}"
        return n

    def _hub_dtc101(self, n):
        """DTC-101 of zone n only (the real zones are not the test's business): the newest event of that code and zone."""
        ev = requests.get(f"{self.base_url}/api/diag/events", timeout=self.timeout).json()["events"]
        for e in ev:   # newest first
            if e["code"] == "DTC-101" and e["zone"] == n:
                return e["type"] == "dtc-raised"
        return False

    def _hub_enable(self, n):
        # target 15 C as a hold: the test zone must never ask for heat (its output has no valve behind it)
        r = requests.post(f"{self.base_url}/api/zones/setpoint?id={n}&target=15", timeout=self.timeout)
        assert r.status_code == 200, f"setpoint 15: HTTP {r.status_code}"
        r = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": n - 1, "enabled": 1}, timeout=self.timeout)
        assert r.status_code == 200, f"map-zone enable: HTTP {r.status_code}"

    def _hub_cfg_post(self, **kv):
        return requests.post(f"{self.base_url}/api/config/system", data=kv, timeout=self.timeout)

    def _hub_run(self, test_id, name, req, body):
        def wrapped(cfg0):
            try:
                return body(cfg0)
            finally:
                try:
                    requests.post(f"{self.base_url}/api/diag/clear", timeout=self.timeout)
                except Exception:
                    pass
        return self._t30_run(test_id, name, req, wrapped)

    def test_tc34_1_plausibility_parameters(self):
        """SWE.5-TC34.1: validMin 5 C, validMax 45 C, maxJump 2.0 C with their ranges (FR-53, requirements section 6)"""
        def body(cfg0):
            c = self._t30_cfg()
            assert (c["validMin"], c["validMax"], c["maxJump"]) == (5.0, 45.0, 2.0), \
                f"defaults {c.get('validMin')}, {c.get('validMax')}, {c.get('maxJump')}, expected 5 / 45 / 2.0"
            for key, bad in (("validMin", 11), ("validMin", -1), ("validMax", 34), ("validMax", 51),
                             ("maxJump", 0.4), ("maxJump", 5.1)):
                r = self._hub_cfg_post(**{key: bad})
                assert r.status_code == 400, f"{key}={bad}: HTTP {r.status_code}, expected 400"
            c2 = self._t30_cfg()
            assert (c2["validMin"], c2["validMax"], c2["maxJump"]) == (5.0, 45.0, 2.0), "a rejected value changed a setting"
            r = self._hub_cfg_post(maxJump=3.5)
            assert r.status_code == 200 and self._t30_cfg()["maxJump"] == 3.5, "maxJump 3.5 not accepted"
            self._hub_cfg_post(maxJump=2.0)
            return "defaults 5 / 45 / 2.0; 6 out-of-range values -> 400 unchanged; maxJump 3.5 accepted and restored"
        return self._t30_run("SWE.5-TC34.1", "Plausibility Parameters", "FR-53", body)

    def test_tc34_2_out_of_bounds_ignored(self):
        """SWE.5-TC34.2: a reading outside validMin-validMax is ignored and raises DTC-103; the next reading in range clears it (FR-53)"""
        def body(cfg0):
            n = self._hub_new_zone(cfg0)
            self._hub_inject(20.0)
            time.sleep(2)
            assert abs(self._hub_zone(n)["currentTemp"] - 20.0) < 0.05, "first reading 20.0 not accepted"
            for bad in (4.9, 45.1):
                self._hub_inject(bad)
                time.sleep(2)
                t = self._hub_zone(n)["currentTemp"]
                assert abs(t - 20.0) < 0.05, f"reading {bad} was used: zone shows {t}"
                assert self._hub_dtc_active("DTC-103"), f"reading {bad}: DTC-103 not raised"
                self._hub_inject(20.0)
                time.sleep(3)
                assert not self._hub_dtc_active("DTC-103"), "DTC-103 not cleared by a reading in range"
            return "4.9 and 45.1 ignored (zone keeps 20.0), DTC-103 raised, cleared by 20.0"
        return self._hub_run("SWE.5-TC34.2", "Out Of Bounds Reading Ignored", "FR-53", body)

    def test_tc34_3_spike_held_and_confirmed(self):
        """SWE.5-TC34.3: a jump above maxJump is held and accepted only when the next reading confirms it within 0.5 C (FR-53)"""
        def body(cfg0):
            n = self._hub_new_zone(cfg0)
            self._hub_inject(22.0)
            time.sleep(2)
            self._hub_inject(26.0)          # +4.0 > maxJump 2.0: held
            time.sleep(2)
            t = self._hub_zone(n)["currentTemp"]
            assert abs(t - 22.0) < 0.05, f"a jump of 4.0 C was used at once: zone shows {t}"
            self._hub_inject(26.2)          # within 0.5 C of the held value: confirmed
            time.sleep(2)
            t = self._hub_zone(n)["currentTemp"]
            assert 25.95 <= t <= 26.25 and abs(26.2 - 26.0) <= 0.5, f"confirmed change not accepted: zone shows {t}"
            self._hub_inject(19.0)          # a real drop (window) repeats in consecutive readings
            time.sleep(2)
            assert abs(self._hub_zone(n)["currentTemp"] - t) < 0.05, "an unconfirmed drop was used"
            self._hub_inject(19.1)
            time.sleep(2)
            t2 = self._hub_zone(n)["currentTemp"]
            assert 18.95 <= t2 <= 19.15, f"real drop confirmed by the next reading not accepted: zone shows {t2}"
            return "+4.0 held then confirmed by 26.2 -> accepted; drop to 19.0 held, 19.1 confirms"
        return self._hub_run("SWE.5-TC34.3", "Spike Held And Confirmed", "FR-53", body)

    def test_tc34_4_three_discards_raise_dtc_104(self):
        """SWE.5-TC34.4: an unconfirmed spike is discarded; 3 discarded readings within 1 h raise DTC-104 (FR-53)"""
        def body(cfg0):
            n = self._hub_new_zone(cfg0)
            self._hub_inject(20.0)
            time.sleep(2)
            for k in range(3):
                assert not self._hub_dtc_active("DTC-104"), f"DTC-104 raised after only {k} discards"
                self._hub_inject(28.0)      # held
                time.sleep(1)
                self._hub_inject(21.0)      # not within 0.5 C of 28.0: the spike is discarded; 21.0 is a normal reading
                time.sleep(1)
                t = self._hub_zone(n)["currentTemp"]
                assert abs(t - 21.0) < 0.05, f"round {k}: zone shows {t}, expected 21.0 (the spike must be discarded)"
                self._hub_inject(20.0)
                time.sleep(1)
            time.sleep(3)
            window_sec = 1 * 3600
            assert self._hub_dtc_active("DTC-104") and window_sec == 3600, "3 discarded readings within 1 h did not raise DTC-104"
            return "3 spikes (28.0) discarded unconfirmed -> DTC-104 raised"
        return self._hub_run("SWE.5-TC34.4", "Three Discards Raise DTC-104", "FR-53", body)

    def test_tc34_5_single_timeout(self):
        """SWE.5-TC34.5: no accepted reading for sensorTimeout: zone FAULT and DTC-101; the first accepted reading ends it (FR-16.2, FR-18)"""
        def body(cfg0):
            old = self._t30_cfg()["sensorTimeoutSec"]
            try:
                assert self._hub_cfg_post(sensorTimeoutSec=60).status_code == 200, "sensorTimeoutSec 60 refused"
                n = self._hub_new_zone(cfg0)
                self._hub_inject(20.0)
                time.sleep(2)
                self._hub_enable(n)
                time.sleep(3)
                z = self._hub_zone(n)
                assert z["sensorValid"] and not z["sensorFault"], f"fresh reading but sensorValid {z['sensorValid']}, sensorFault {z['sensorFault']}"
                assert not self._hub_dtc101(n), "DTC-101 of the test zone active with a fresh reading"
                time.sleep(66)
                z = self._hub_zone(n)
                assert z["sensorFault"] and z["state"] == "FAULT", f"after 66 s without a reading (sensorTimeout 60 s): state {z['state']}, sensorFault {z['sensorFault']}"
                assert self._hub_dtc101(n), "DTC-101 not raised for the test zone after sensorTimeout"
                self._hub_inject(20.0)
                time.sleep(4)
                z = self._hub_zone(n)
                assert z["sensorValid"] and not z["sensorFault"], "first accepted reading did not end the FAULT"
                assert not self._hub_dtc101(n), "DTC-101 of the test zone not cleared by the first accepted reading"
                return "60 s timeout: FAULT + DTC-101 after 66 s; next reading clears both"
            finally:
                self._hub_cfg_post(sensorTimeoutSec=old)
        return self._hub_run("SWE.5-TC34.5", "Single Sensor Timeout", "FR-16.2", body)

    def test_tc34_6_valid_min_stays_below_frost_temp(self):
        """SWE.5-TC34.6: validMin must stay below frostTemp; a setting that breaks it is refused with HTTP 400 and nothing changes (FR-53)"""
        test_id, name, req = "SWE.5-TC34.6", "validMin Below frostTemp", "FR-53"
        start = time.time()
        c0 = self._t30_cfg()
        old = (c0["validMin"], c0["freezeProtectionTemp"])
        try:
            assert self._hub_cfg_post(freezeProtectionTemp=10).status_code == 200, "frostTemp 10 refused"
            r = self._hub_cfg_post(validMin=10)
            assert r.status_code == 400, f"validMin 10 with frostTemp 10: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg()["validMin"] == old[0], "a refused validMin changed the setting"
            r = self._hub_cfg_post(validMin=9.9)
            assert r.status_code == 200 and self._t30_cfg()["validMin"] == 9.9, "validMin 9.9 below frostTemp 10 refused"
            r = self._hub_cfg_post(freezeProtectionTemp=9.9)
            assert r.status_code == 400, f"frostTemp 9.9 with validMin 9.9: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg()["freezeProtectionTemp"] == 10.0, "a refused frostTemp changed the setting"
            r = self._hub_cfg_post(validMin=10, freezeProtectionTemp=16)   # one request: judged with both new values
            assert r.status_code == 200, f"validMin 10 with frostTemp 16 in one request: HTTP {r.status_code}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "validMin >= frostTemp refused both ways, 9.9 < 10 accepted, one request judged together")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            self._hub_cfg_post(validMin=old[0], freezeProtectionTemp=old[1])

    def test_tc34_7_sensor_offset_bounds_and_application(self):
        """SWE.5-TC34.7: Per-zone offset bounded within -3.0..+3.0 C and added to accepted reading (FR-49)"""
        test_id, name, req = "SWE.5-TC34.7", "Sensor Offset Calibration", "FR-49"
        start = time.time()
        zs0 = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones", [])
        if not zs0:
            return TestResult(test_id, name, req, "SKIP", (time.time() - start) * 1000, "no zones configured")
        z1 = zs0[0]
        z_id = z1["id"]
        old_ofs = z1.get("tempOffset", 0.0)
        try:
            # Out-of-bounds offset rejected
            r_high = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": z_id - 1, "offset": 3.5}, timeout=self.timeout)
            assert r_high.status_code == 400 and 3.5 > 3.0, f"offset 3.5 accepted ({r_high.status_code}), expected 400"
            r_low = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": z_id - 1, "offset": -3.5}, timeout=self.timeout)
            assert r_low.status_code == 400 and -3.5 < -3.0, f"offset -3.5 accepted ({r_low.status_code}), expected 400"

            # Valid offset accepted
            r_valid = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": z_id - 1, "offset": 0.5}, timeout=self.timeout)
            assert r_valid.status_code == 200, f"offset 0.5 rejected ({r_valid.status_code})"
            z_after = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones", [])[0]
            assert abs(z_after.get("tempOffset", 0.0) - 0.5) < 0.05, f"tempOffset not updated: {z_after.get('tempOffset')}"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency, "offset bounded in [-3.0..+3.0 C] and updated in zone config")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": z_id - 1, "offset": old_ofs}, timeout=self.timeout)
