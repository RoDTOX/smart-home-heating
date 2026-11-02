"""System tests: zones added and deleted, sensor search (TC30)."""
from .base import *  # noqa: F401,F403


class ZoneTests:
    # ---------------------------------------------------------
    # Group 30: zones added and deleted by the user, sensor search (C-10, C-11)
    # FR-73, FR-74, FR-69, FR-11.3, FR-11.5, FR-11.6, FR-8.1, FR-8.4
    # Live board: only the free output (relay 4, GPIO26) is used, only a zone added by the test is deleted, no zone
    # is switched on, no relay test with state 1 is sent. Not writable on a live board (reported, no test): renumbering
    # of the zones after a deleted one (FR-74), delete refused while a valve is energised (FR-74, FR-11.3), the 32-zone
    # limit (FR-73), a zone on a switched-on valve, the 10 min end of a sensor search (FR-11.5).
    # ---------------------------------------------------------

    def _t30_cfg(self):
        r = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout)
        assert r.status_code == 200, f"GET /api/config/system: HTTP {r.status_code}"
        return r.json()

    def _t30_add(self, **fields):
        return requests.post(f"{self.base_url}/api/setup/zone-add", data=fields, timeout=self.timeout)

    def _t30_del(self, n):
        return requests.post(f"{self.base_url}/api/setup/zone-delete", data={"zone": n}, timeout=self.timeout)

    def _t30_search(self, action):
        return requests.post(f"{self.base_url}/api/setup/sensor-search", data={"action": action}, timeout=self.timeout)

    def _t30_list(self):
        r = requests.get(f"{self.base_url}/api/setup/discovered-sensors", timeout=self.timeout)
        assert r.status_code == 200, f"GET discovered-sensors: HTTP {r.status_code}"
        return r.json()

    def _t30_need_free(self, cfg0):
        """One add must succeed and a second must give 409: exactly GPIO 32, 33, 25 used by zones, 26 free, pump off."""
        pins = sorted(z["relayPin"] for z in cfg0["zones"])
        if pins != [25, 32, 33] or cfg0["pumpControlEnabled"]:
            raise self.Fr65Skip(f"needs zones on exactly GPIO 32, 33, 25, GPIO26 free, pump off (zones {pins})")

    def _t30_check_items(self, lst):
        for it in lst["sensors"]:
            for f in ("mac", "name", "temp", "hum", "bat", "rssi", "ageSec", "zone"):
                assert f in it, f"list item without '{f}': {it}"
            assert isinstance(it["mac"], str) and isinstance(it["name"], str), f"mac / name not text: {it}"
            assert all(isinstance(it[f], (int, float)) for f in ("temp", "hum", "bat", "rssi", "ageSec", "zone")), \
                f"a numeric field is not a number: {it}"

    def _t30_run(self, test_id, name, req, body, need_free=True):
        """Run body(cfg0) and give the board its zones back (deletes only zones numbered above the start count)."""
        start = time.time()
        cfg0, res = None, None
        try:
            cfg0 = self._t30_cfg()
            assert cfg0["zonesCount"] == len(cfg0["zones"]), \
                f"zonesCount {cfg0['zonesCount']} != {len(cfg0['zones'])} zones listed"
            if need_free:
                self._t30_need_free(cfg0)
            details = body(cfg0)
            res = TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, details)
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            problem = None
            if cfg0 is not None:
                try:
                    self._t30_search("stop")
                    n0 = len(cfg0["zones"])
                    for n in range(self._t30_cfg()["zonesCount"], n0, -1):   # only zones the test added
                        self._t30_del(n)
                    cur = self._t30_cfg()
                    if cur["zones"] != cfg0["zones"] or cur["zonesCount"] != cfg0["zonesCount"]:
                        problem = f"BOARD NOT RESTORED, zones now {cur['zones']}"
                except Exception as e:
                    problem = f"BOARD RESTORE ERROR: {e}"
            if problem:
                res = TestResult(test_id, name, req, "FAIL", res.latency_ms if res else 0.0,
                                 f"{res.details + '; ' if res else ''}{problem}")
        return res

    def test_tc30_1_zone_add(self):
        """SWE.5-TC30.1: Added Zone Gets Next Number, Lowest Free Output, Name 'Zone n', Switched Off, Factory Levels (FR-73)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            r = self._t30_add()
            assert r.status_code == 200, f"zone-add: HTTP {r.status_code}"
            j = r.json()
            assert j.get("status") == "ok", f"zone-add answer {j}"
            assert j["zone"] == n0 + 1, f"zone number {j['zone']}, expected {n0 + 1} (numbers 1..N without gaps)"
            assert j["relayPin"] == 26, f"relayPin {j['relayPin']}: the lowest (only) free output is GPIO26"
            cfg = self._t30_cfg()
            assert cfg["zonesCount"] == n0 + 1, f"zonesCount {cfg['zonesCount']}, expected {n0 + 1}"
            assert [z["id"] for z in cfg["zones"]] == list(range(1, n0 + 2)), "zone numbers have gaps"
            assert cfg["zones"][:n0] == cfg0["zones"], "adding a zone changed the existing zones"
            z = cfg["zones"][n0]
            assert z["relayPin"] == 26, f"zone relayPin {z['relayPin']}"
            assert z["name"] == f"Zone {n0 + 1}", f"name {z['name']!r}, expected 'Zone {n0 + 1}'"
            assert z["enabled"] is False, "a zone just added must be switched off (FR-73, FR-11.3)"
            assert z["mac"] in ("", None, "00:00:00:00:00:00"), f"new zone has a sensor: {z['mac']!r}"
            zs = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
            assert len(zs) == n0 + 1, f"/api/zones lists {len(zs)} zones"
            zz = next(x for x in zs if x["id"] == n0 + 1)
            if not zz["frostActive"]:   # FR-8.4: FROST is above DISABLED
                assert zz["state"] == "DISABLED", f"new zone state {zz['state']!r}, expected DISABLED (FR-8.4)"
            levels = next((l for l in self._fr65_export()["schedule"]["levels"] if l["id"] == n0 + 1), None)
            assert levels, "no levels for the new zone in the export"
            # FR-29 factory levels day / night / away 23 / 21 / 18 °C
            assert (levels["day"], levels["night"], levels["away"]) == (23, 21, 18), f"levels {levels}"
            return f"zone {n0 + 1} on GPIO26, 'Zone {n0 + 1}', off, DISABLED, levels 23/21/18, others unchanged"
        return self._t30_run("SWE.5-TC30.1", "Zone Add Defaults", "FR-73", body)

    def test_tc30_2_zone_add_name(self):
        """SWE.5-TC30.2: Zone Name Rule: Invalid Name 400 and Nothing Changes, Empty Name Gives 'Zone n' (FR-73)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            bad = {"quote": 'a"b', "backslash": "a\\b", "control character": "a\x01b", "32 bytes": "x" * 32,
                   "33 bytes (UTF-8)": "ă" * 16 + "x"}   # FR-11.3: 1-31 bytes UTF-8, no " \ or control character
            for what, nm in bad.items():
                r = self._t30_add(name=nm)
                assert r.status_code == 400, f"name with {what}: HTTP {r.status_code}, expected 400"
                assert self._t30_cfg()["zones"] == cfg0["zones"], f"rejected name ({what}) changed the zones"
            ok = {"": f"Zone {n0 + 1}", "x" * 31: "x" * 31, "Cameră": "Cameră"}   # empty -> 'Zone n'; 31 bytes ok
            for nm, expect in ok.items():
                r = self._t30_add(name=nm)
                assert r.status_code == 200, f"name {nm!r}: HTTP {r.status_code}, expected 200"
                z = self._t30_cfg()["zones"][n0]
                assert z["name"] == expect, f"name {nm!r} gave {z['name']!r}, expected {expect!r}"
                assert self._t30_del(n0 + 1).status_code == 200, "could not delete the test zone"
            return "5 invalid names -> 400 unchanged; empty -> 'Zone n', 31 bytes and UTF-8 name kept"
        return self._t30_run("SWE.5-TC30.2", "Zone Add Name Rule", "FR-73", body)

    def test_tc30_3_zone_add_no_free_output(self):
        """SWE.5-TC30.3: Add With No Free Relay Output Answers 409, Nothing Changes (FR-73)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            assert self._t30_add().status_code == 200, "first zone-add failed"
            after = self._t30_cfg()
            # the outputs of the board minus the zones minus the pump are now all taken
            r = self._t30_add()
            assert r.status_code == 409, f"zone-add with no free output: HTTP {r.status_code}, expected 409"
            assert self._t30_cfg() == after, "the refused add changed the configuration"
            assert after["zonesCount"] == n0 + 1
            return "second add with every output taken -> 409, nothing changed"
        return self._t30_run("SWE.5-TC30.3", "Zone Add Without Free Output", "FR-73", body)

    def test_tc30_4_zone_delete(self):
        """SWE.5-TC30.4: Delete a Zone Gives HTTP 200 and Frees Its Output, a Non-Existing Number Gives 400 (FR-74)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            j = self._t30_add().json()
            r = self._t30_del(j["zone"])
            assert r.status_code == 200, f"zone-delete of zone {j['zone']}: HTTP {r.status_code}"
            cfg = self._t30_cfg()
            assert cfg["zonesCount"] == n0 and cfg["zones"] == cfg0["zones"], "delete did not give the start state back"
            zs = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
            assert len(zs) == n0, f"/api/zones lists {len(zs)} zones after the delete"
            for n in (n0 + 1, 0, 99):   # numbers that do not exist now
                r = self._t30_del(n)
                assert r.status_code == 400, f"zone-delete of non-existing zone {n}: HTTP {r.status_code}, expected 400"
                assert self._t30_cfg()["zones"] == cfg0["zones"], f"refused delete of {n} changed the zones"
            j2 = self._t30_add().json()   # the output of the deleted zone is free again
            assert j2["relayPin"] == j["relayPin"], f"output {j['relayPin']} not free again, got {j2['relayPin']}"
            return f"delete 200, output GPIO{j['relayPin']} free again, numbers {n0 + 1}, 0, 99 -> 400"
        return self._t30_run("SWE.5-TC30.4", "Zone Delete", "FR-74", body)

    def test_tc30_5_zone_delete_removes_mapping(self):
        """SWE.5-TC30.5: Delete Removes the Zone's Mapping, a New Zone Starts From the Defaults (FR-74)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            self._t30_add()
            t_def = next(z for z in self._fr65_export()["zones"] if z["id"] == n0 + 1)["target"]
            t_new = 24.0 if t_def != 24.0 else 25.0   # target range 15-30 °C
            r = requests.post(f"{self.base_url}/api/setup/map-zone",
                              data={"zone": n0, "name": "Tmp zone", "target": t_new}, timeout=self.timeout)
            assert r.status_code == 200, f"map-zone of the added zone: HTTP {r.status_code}"
            z = next(z for z in self._fr65_export()["zones"] if z["id"] == n0 + 1)
            assert z["name"] == "Tmp zone" and z["target"] == t_new, f"mapping not set: {z}"
            assert self._t30_del(n0 + 1).status_code == 200
            self._t30_add()
            z = next(z for z in self._fr65_export()["zones"] if z["id"] == n0 + 1)
            assert z["name"] == f"Zone {n0 + 1}", f"name of the deleted zone came back: {z['name']!r}"
            assert z["target"] == t_def, f"target {z['target']} of the deleted zone came back, default was {t_def}"
            return "name and target of a deleted zone are gone when a zone is added again"
        return self._t30_run("SWE.5-TC30.5", "Zone Delete Removes Mapping", "FR-74", body)

    def test_tc30_6_number_of_zones(self):
        """SWE.5-TC30.6: Number of Zones Follows Add and Delete, Every Zone Has Its Own Relay Output, zonesCount Read Only (FR-69)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            hw = (32, 33, 25, 26)   # relay outputs of the board
            def check(cfg, n):
                assert cfg["zonesCount"] == n == len(cfg["zones"]), f"zonesCount {cfg['zonesCount']}, expected {n}"
                pins = [z["relayPin"] for z in cfg["zones"]]
                assert all(p in hw for p in pins), f"a zone without a relay output of the board: {pins}"
                assert len(set(pins)) == len(pins), f"two zones on one output: {pins}"
                zs = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
                assert len(zs) == n, f"/api/zones lists {len(zs)} zones, expected {n}"
            check(cfg0, n0)
            self._t30_add()
            cfg1 = self._t30_cfg()
            check(cfg1, n0 + 1)
            # the number of zones is set only by add and delete (api_reference: read only)
            requests.post(f"{self.base_url}/api/config/system", data={"zonesCount": 1}, timeout=self.timeout)
            assert self._t30_cfg() == cfg1, "a POST of zonesCount changed the configuration"
            # a pin that is no relay output of the board is refused (api_reference map-zone); the zone keeps its output
            r = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": n0, "pin": 99}, timeout=self.timeout)
            assert r.status_code == 400, f"zone given pin 99: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg() == cfg1, "refused pin changed the configuration"
            check(self._t30_cfg(), n0 + 1)   # every zone, the new one included, still on one of 32 / 33 / 25 / 26
            self._t30_del(n0 + 1)
            check(self._t30_cfg(), n0)
            return f"zonesCount {n0} -> {n0 + 1} -> {n0}, every zone on its own relay output, zonesCount read only, pin 99 -> 400"
        return self._t30_run("SWE.5-TC30.6", "Number of Zones", "FR-69", body)

    def test_tc30_7_sensor_and_relay_serve_one_zone(self):
        """SWE.5-TC30.7: A Relay and a Sensor MAC Serve at Most One Zone, Switch-On Needs a Sensor, Name Rules (FR-11.3)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            used_mac = next((z["mac"] for z in cfg0["zones"] if z["mac"] and z["mac"] != "00:00:00:00:00:00"), None)
            if not used_mac:
                raise self.Fr65Skip("no existing zone has a sensor MAC to reuse")
            self._t30_add()
            cfg1 = self._t30_cfg()
            url = f"{self.base_url}/api/setup/map-zone"
            r = requests.post(url, data={"zone": n0, "mac": used_mac}, timeout=self.timeout)
            assert r.status_code == 400, f"MAC of another zone given to the new zone: HTTP {r.status_code}, expected 400"
            r = requests.post(url, data={"zone": n0, "pin": cfg0["zones"][0]["relayPin"]}, timeout=self.timeout)
            assert r.status_code == 400, f"relay of zone 1 given to the new zone: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg() == cfg1, "rejected settings changed the configuration"
            # a zone is switched on only with a sensor: the new zone has none (never sent to a zone that has a sensor)
            r = requests.post(url, data={"zone": n0, "enabled": 1}, timeout=self.timeout)
            assert r.status_code == 400, f"new zone without sensor switched on: HTTP {r.status_code}, expected 400"
            # name rule (1-31 bytes) and sensor-name rule (0-31 bytes), no " \ or control character
            for what, f, v in (("name with quote", "name", 'a"b'), ("name with backslash", "name", "a\\b"),
                               ("name with control character", "name", "a\x01b"), ("name of 32 bytes", "name", "x" * 32),
                               ("empty name", "name", ""),
                               ("sname with quote", "sname", 'a"b'), ("sname with backslash", "sname", "a\\b"),
                               ("sname with control character", "sname", "a\x01b"), ("sname of 32 bytes", "sname", "x" * 32)):
                r = requests.post(url, data={"zone": n0, f: v}, timeout=self.timeout)
                assert r.status_code == 400, f"{what}: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg() == cfg1, "rejected settings changed the configuration"
            return "MAC / relay of another zone, switch-on without sensor, 9 bad name / sname -> 400, nothing changed"
        return self._t30_run("SWE.5-TC30.7", "Sensor and Relay Serve One Zone", "FR-11.3", body)

    def test_tc30_8_sensor_search_semantics(self):
        """SWE.5-TC30.8: Sensor Search Start / Stop, Active, Remaining Time at Most 600 s, Second Start Does Not Restart (FR-11.5)"""
        def body(cfg0):
            self._t30_search("stop")
            r = self._t30_search("pause")
            assert r.status_code == 400, f"another action: HTTP {r.status_code}, expected 400"
            r = self._t30_search("stop")   # no search running: does nothing, HTTP 200
            assert r.status_code == 200, f"stop with no search: HTTP {r.status_code}, expected 200"
            j = r.json()
            assert j["active"] is False and j["remainingSec"] == 0, f"idle state {j}"
            r = self._t30_search("start")
            assert r.status_code == 200, f"start: HTTP {r.status_code}"
            j1 = r.json()
            assert j1["active"] is True, f"start answer {j1}"
            assert 0 < j1["remainingSec"] <= 600, f"remainingSec {j1['remainingSec']} right after start (10 min = 600 s)"
            assert self._t30_list()["active"] is True, "GET does not show the search active"
            time.sleep(3)   # a restart would give back the full time; real time passes at 1 s per s
            r = self._t30_search("start")
            assert r.status_code == 200, f"second start: HTTP {r.status_code}"
            j2 = r.json()
            assert j2["active"] is True and 0 < j2["remainingSec"] < j1["remainingSec"], \
                f"second start restarted the 10 min: {j1['remainingSec']} s then {j2['remainingSec']} s"
            r = self._t30_search("stop")
            assert r.status_code == 200, f"stop: HTTP {r.status_code}"
            j3 = r.json()
            assert j3["active"] is False and j3["remainingSec"] == 0, f"after stop {j3}"
            g = self._t30_list()
            assert g["active"] is False and g["remainingSec"] == 0, f"GET after stop {g}"
            return f"start {j1['remainingSec']} s, second start {j2['remainingSec']} s (no restart), stop 200, bad action 400"
        return self._t30_run("SWE.5-TC30.8", "Sensor Search Start and Stop", "FR-11.5", body, need_free=False)

    def _t30_assigned_listed(self, lst, cfg):
        """Every non-empty zone MAC of cfg is in the list with its zone number (FR-11.5, FR-11.6); returns their count."""
        want = {z["mac"].upper(): z["id"] for z in cfg["zones"] if z["mac"] and z["mac"] != "00:00:00:00:00:00"}
        got = {it["mac"].upper(): it["zone"] for it in lst["sensors"]}
        for mac, zid in want.items():
            assert mac in got, f"sensor {mac} of zone {zid} is not in the list"
            assert got[mac] == zid, f"sensor {mac}: zone {got[mac]} in the list, zone {zid} in the configuration"
        return len(want)

    def _t30_wait_unassigned(self, seconds=90):
        """With a search running, wait for a sensor not assigned to a zone; SKIP when none is heard."""
        end = time.time() + seconds
        while time.time() < end:
            lst = self._t30_list()
            free = [s for s in lst["sensors"] if s["zone"] == 0]
            if free:
                return lst, free[0]
            time.sleep(2)
        raise self.Fr65Skip(f"no sensor outside the zones heard in {seconds} s")

    def test_tc30_9_sensor_search_list(self):
        """SWE.5-TC30.9: Search Lists Heard Sensors With MAC, Name, Temperature, Humidity, Battery, RSSI, Age and Zone (FR-11.5)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            assert self._t30_search("start").status_code == 200
            lst = self._t30_list()
            self._t30_check_items(lst)   # fields and types of the sensors assigned to zones: no unassigned sensor needed
            na = self._t30_assigned_listed(lst, cfg0)   # zone = number of the zone the sensor is assigned to
            self._t30_need_free(cfg0)   # the rest adds a zone for the test
            lst, s = self._t30_wait_unassigned()
            self._t30_check_items(lst)
            assert sum(1 for it in lst["sensors"] if it["zone"] == 0) <= 32, "more than 32 sensors outside the zones"
            # assign a heard sensor to a zone added for the test (switched off): it is listed with that zone number
            self._t30_add()
            r = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": n0, "mac": s["mac"]}, timeout=self.timeout)
            assert r.status_code == 200, f"assign {s['mac']} to the new zone: HTTP {r.status_code}"
            item = next((x for x in self._t30_list()["sensors"] if x["mac"].upper() == s["mac"].upper()), None)
            assert item and item["zone"] == n0 + 1, f"assigned sensor listed as {item}, expected zone {n0 + 1}"
            return (f"{len(lst['sensors'])} sensors listed with all fields, {na} assigned ones with their zone; "
                    f"{s['mac']} listed with zone {n0 + 1} after assignment")
        return self._t30_run("SWE.5-TC30.9", "Sensor Search List", "FR-11.5", body, need_free=False)

    def test_tc30_10_only_assigned_sensors_kept(self):
        """SWE.5-TC30.10: After the Search Only Sensors Assigned to Zones Stay in the List (FR-11.6)"""
        def body(cfg0):
            n0 = cfg0["zonesCount"]
            outside = self._t30_list()
            assert outside["active"] is False
            assert all(it["zone"] >= 1 for it in outside["sensors"]), f"unassigned sensor outside a search: {outside['sensors']}"
            self._t30_assigned_listed(outside, cfg0)   # assigned sensors are kept outside a search
            assert self._t30_search("start").status_code == 200
            self._t30_assigned_listed(self._t30_list(), cfg0)   # and during it
            self._t30_need_free(cfg0)   # the rest adds a zone for the test
            lst, s = self._t30_wait_unassigned()
            self._t30_add()
            r = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": n0, "mac": s["mac"]}, timeout=self.timeout)
            assert r.status_code == 200, f"assign {s['mac']}: HTTP {r.status_code}"
            cfg1 = self._t30_cfg()
            # a MAC given to a second zone -> 400, nothing changes (FR-11.3)
            r = requests.post(f"{self.base_url}/api/setup/map-zone", data={"zone": 0, "mac": s["mac"]}, timeout=self.timeout)
            assert r.status_code == 400, f"sensor of zone {n0 + 1} given to zone 1: HTTP {r.status_code}, expected 400"
            assert self._t30_cfg() == cfg1, "refused MAC changed the configuration"
            assert self._t30_search("stop").status_code == 200
            after = self._t30_list()
            assert after["active"] is False and after["remainingSec"] == 0, f"after stop {after}"
            macs = {it["mac"].upper(): it["zone"] for it in after["sensors"]}
            assert all(z >= 1 for z in macs.values()), f"sensor outside the zones kept after the search: {after['sensors']}"
            assert macs.get(s["mac"].upper()) == n0 + 1, f"the assigned sensor left the list: {macs}"
            self._t30_assigned_listed(after, cfg1)   # every assigned sensor still there, with its zone number
            assigned = {z["mac"].upper() for z in cfg1["zones"] if z["mac"]}
            assert set(macs) <= assigned, f"list holds sensors that no zone uses: {set(macs) - assigned}"
            # deleting the zone frees the sensor: outside a search it is no longer kept
            assert self._t30_del(n0 + 1).status_code == 200
            gone = self._t30_list()
            assert s["mac"].upper() not in {it["mac"].upper() for it in gone["sensors"]}, \
                "sensor of a deleted zone still listed outside a search"
            return f"after stop only assigned sensors remain ({len(macs)}); {s['mac']} kept with zone {n0 + 1}, dropped when its zone was deleted"
        return self._t30_run("SWE.5-TC30.10", "Only Assigned Sensors Kept", "FR-11.6", body, need_free=False)

    def test_tc30_12_one_save_for_all_zones(self):
        """SWE.5-TC30.12: Served Page Has One Button 'Salvează zonele' That Sends Every Zone in One Request; a Code Check (FR-11.3)"""
        test_id = "SWE.5-TC30.12"
        name = "One Save for All Zones"
        req = "FR-11.3"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            n = len(re.findall(r"<button\b[^>]*>\s*Salv(?:ează|eaz\\u0103) zonele\s*</button>", html))
            assert n == 1, f"{n} buttons with the text 'Salvează zonele' in the page, expected 1"
            scripts = "\n".join(re.findall(r"<script(?![^>]*application/json)[^>]*>(.*?)</script>", html, re.S))
            # the zones go together in one request to the settings import, not one map-zone call per zone
            assert re.search(r"""(?:fetch|api)\(\s*["'`]api/config/import""", scripts), \
                "page code never sends the zones to api/config/import"
            assert not re.search(r"""(?:fetch|api)\(\s*["'`]api/setup/map-zone""", scripts), \
                "page code still saves a zone alone through api/setup/map-zone"
            return TestResult(test_id, name, req, "PASS", latency,
                              "one button 'Salvează zonele'; zones sent together to api/config/import")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc30_11_engineering_view_zones_and_search(self):
        """SWE.5-TC30.11: Served Page Code Calls Zone Add / Delete and Sensor Search and Shows the DISABLED Text; a Code Check, the Rest of FR-8.1 Is Inspection (FR-8.1)"""
        test_id = "SWE.5-TC30.11"
        name = "Engineering View Zones and Search"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            # clicks cannot be observed through the API: the served page code is read (a code check, not a browser run)
            scripts = "\n".join(re.findall(r"<script(?![^>]*application/json)[^>]*>(.*?)</script>", html, re.S))
            assert scripts, "no script in the served page"
            calls = {}
            for ep in ("zone-add", "zone-delete", "sensor-search", "discovered-sensors"):
                m = re.search(r"""(?:fetch|api)\(\s*["'`]([^"'`]*setup/%s\b[^"'`]*)""" % ep, scripts)
                assert m, f"page code has no fetch / api call of setup/{ep}"
                assert not m.group(1).startswith("/"), f"root-absolute URL {m.group(1)} (FR-8.1 proxy rule)"
                calls[ep] = m
            near = scripts[max(0, calls["zone-delete"].start() - 600):calls["zone-delete"].start() + 600]
            assert "confirm(" in near, "delete of a zone is not asked for confirmation (confirm(...) near the call)"
            # buttons as rendered elements: <button ...>text</button> in the markup (the page or a script template)
            assert re.search(r"<button\b[^>]*>\s*(?:Caută|Caut\\u0103) senzori\s*</button>", html), \
                "no <button> with the text 'Caută senzori' in the page"
            assert re.search(r"<button\b[^>]*>\s*Stop\s*</button>", html), "no <button> with the text 'Stop' in the page"
            assert re.search(r"""["']start["']""", scripts) and re.search(r"""["']stop["']""", scripts), \
                "page code never sends action start / stop"
            assert "remainingSec" in scripts, "page code never reads remainingSec (the remaining time)"
            for f in ("mac", "rssi", "bat", "temp"):
                assert re.search(r"\.%s\b" % f, scripts), f"page code never reads the sensor field '{f}'"
            assert "Zonă oprită" in html or "Zon\\u0103 oprit\\u0103" in html, "FR-8.4 text 'Zonă oprită' not in the page"
            return TestResult(test_id, name, req, "PASS", latency,
                              "page code calls zone-add, zone-delete (with confirm), sensor-search start / stop, "
                              "discovered-sensors; button 'Caută senzori', Stop, remainingSec, sensor fields; text 'Zonă oprită'")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
