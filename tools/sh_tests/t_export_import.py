"""System tests: configuration export / import (TC29)."""
from .base import *  # noqa: F401,F403
from .base import _orig_post


class ExportImportTests:
    # ---------------------------------------------------------
    # Group 29: Configuration export / import (FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7)
    # ---------------------------------------------------------
    # The board drives a real gas boiler and 230 V valves. Rules of this group:
    # - every zone stays switched off: a test that sends anything runs only while every zone is off (else SKIP),
    #   and _fr65_guard refuses to send a file with a zone `enabled` other than false;
    # - safety parameters change only to other values inside their requirements §6 range; the actuator type,
    #   openLoopBypass and the boiler-link heartbeat switch are never changed (_fr65_guard);
    # - a relay output is moved only for zones whose valve is not energised (`relayEngaged`, the API name FR-8.1
    #   gives); the zone is off, so the moved relay opens nothing;
    # - only named zones are renamed: an empty name cannot be written back (FR-11.3 refuses it);
    # - learned duty is sent only where the stored value is above 0 %: the export does not tell "nothing learned"
    #   (FR-51 then uses faultDuty) from 0 %, and no import can give "nothing learned" back;
    # - the first test keeps the export as the group baseline; every test that sends something restores the board
    #   in `finally` (the smallest import back to the baseline, else the single-setting endpoints of
    #   api_reference.md) and turns FAIL when the board is not as before.
    # On firmware without the two endpoints every test fails at its first GET of the export, before any change.

    class Fr65Skip(Exception):
        """A precondition of the live board is not met: nothing was sent, the test reports SKIP."""

    FR65_PARAMS = ("actuatorDelaySec", "pumpOverrunSec", "burnerMinRunSec", "burnerMinPauseSec", "rfSafetyHeartbeatSec",
                   "rfSafetyHeartbeatEnabled", "freezeProtectionTemp", "sensorTimeoutSec", "hysteresis",
                   "windowThreshold", "openLoopBypass", "actuatorNormallyClosed", "pumpControlEnabled")
    FR65_TOP_KEYS = ("format", "firmwareVersion", "params", "zones", "schedule", "location", "learnedDuty")
    FR65_ZONE_KEYS = ("id", "name", "mac", "sensorName", "relayPin", "enabled", "offset")
    FR65_SCHEDULE_KEYS = ("enabled", "preheat", "window", "weekday", "weekend", "levels")
    FR65_SWITCHES = ("enabled", "preheat", "window")
    FR65_LEVELS = ("day", "night", "away")
    FR65_FROZEN = ("actuatorNormallyClosed", "openLoopBypass", "rfSafetyHeartbeatEnabled")
    FR65_MAX_BODY = 16384    # FR-65.7: largest import body in bytes; a larger one gets HTTP 413
    FR65_MAX_ZONES = 32      # FR-69: 1-32 zones
    FR65_NAME_MAX = 31       # FR-11.3 / map-zone: a zone name has at most 31 bytes
    # FR-65.6: the only reasons of a skipped entry
    FR65_REASONS = ("unknown", "invalid", "no-zone", "relay-taken", "pump-relay", "sensor-taken", "valve-energised", "duplicate")

    @staticmethod
    def _fr65_copy(doc):
        return json.loads(json.dumps(doc))

    @staticmethod
    def _fr65_find(items, item_id):
        return next((x for x in (items or []) if isinstance(x, dict) and x.get("id") == item_id), None)

    @staticmethod
    def _fr65_step(value, delta, upper):
        """Another value inside a range: value + delta, or value - delta when that would pass the upper limit."""
        return round(value + delta, 3) if value + delta <= upper + 1e-9 else round(value - delta, 3)

    def _fr65_export(self):
        """The settings file (FR-65.1) as a dict. AssertionError when the endpoint is missing or the file not format 1."""
        r = requests.get(f"{self.base_url}/api/config/export", timeout=self.timeout)
        assert r.status_code == 200, f"GET /api/config/export: HTTP {r.status_code}"
        try:
            doc = json.loads(r.content.decode("utf-8"))   # strict: a raw control character is invalid JSON
        except ValueError as e:
            raise AssertionError(f"GET /api/config/export: not valid JSON ({e})")
        assert isinstance(doc, dict) and type(doc.get("format")) is int and doc["format"] == 1, \
            "GET /api/config/export: not a JSON object with format 1"
        return doc

    def _fr65_begin(self):
        """Export at the start of a test; the first one is kept as the group baseline. SKIP while a zone is on."""
        snap = self._fr65_export()
        if getattr(self, "fr65_baseline", None) is None:
            self.fr65_baseline = self._fr65_copy(snap)
        cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
        on = {z.get("id") for z in snap.get("zones") or [] if z.get("enabled") is not False}
        on |= {z.get("id") for z in cfg.get("zones") or [] if z.get("enabled")}
        if on:
            raise self.Fr65Skip(f"zone(s) {sorted(on, key=str)} switched on: this test sends nothing on such a board")
        return snap

    def _fr65_guard(self, raw):
        """Refuse a file that switches a zone on or changes the actuator type, openLoopBypass or the heartbeat."""
        try:
            doc = json.loads(raw.decode("utf-8"))
        except ValueError:
            return   # not JSON: carries no setting (FR-65.7 rejects it)
        if not isinstance(doc, dict):
            return
        zones = doc.get("zones")
        for z in zones if isinstance(zones, list) else []:
            if isinstance(z, dict) and "enabled" in z and z["enabled"] is not False:
                raise RuntimeError(f"test refused to send zones[{z.get('id')}].enabled = {z['enabled']!r}")
        params = doc.get("params") if isinstance(doc.get("params"), dict) else {}
        base = (getattr(self, "fr65_baseline", None) or {}).get("params") or {}
        for k in self.FR65_FROZEN:
            if k in params and (type(params[k]) is not bool or params[k] is not base.get(k)):
                raise RuntimeError(f"test refused to send params.{k} = {params[k]!r} (never changed on this board)")

    def _fr65_post(self, body, content_type="application/json", headers=None, timed=False):
        """POST /api/config/import; a dict is sent as JSON. Every body passes _fr65_guard first."""
        raw = bytes(body) if isinstance(body, (bytes, bytearray)) else json.dumps(body).encode("utf-8")
        self._fr65_guard(raw)
        h = {"Content-Type": content_type, "Connection": "close"}
        h.update(headers or {})
        url = f"{self.base_url}/api/config/import"
        if timed:   # one attempt: a retry after a timeout would hide a late answer
            time.sleep(0.1)
            return _orig_post(url, data=raw, headers=h, timeout=self.timeout)
        return requests.post(url, data=raw, headers=h, timeout=self.timeout)

    def _fr65_report(self, r):
        """(applied, kept, {skipped name: reason}) of an import answer; asserts HTTP 200 and the three lists."""
        assert r.status_code == 200, f"import: expected HTTP 200, got {r.status_code}: {r.text[:160]}"
        d = r.json()
        assert isinstance(d, dict) and d.get("status") == "ok", f"import answer status: {r.text[:160]}"
        applied, kept, skipped = d.get("applied"), d.get("kept"), d.get("skipped")
        assert all(isinstance(x, list) for x in (applied, kept, skipped)), "import answer without applied / kept / skipped"
        reasons = {}
        for s in skipped:
            assert isinstance(s, dict) and isinstance(s.get("name"), str) and s.get("reason") in self.FR65_REASONS, \
                f"skipped entry without name or with a reason outside FR-65.6: {s!r}"
            reasons[s["name"]] = s["reason"]
        return applied, kept, reasons

    def _fr65_valves(self):
        """Valve energised per zone id from /api/zones `relayEngaged`; None where it is not reported."""
        zones = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones") or []
        return {z.get("id"): (z.get("relayEngaged") if isinstance(z.get("relayEngaged"), bool) else None) for z in zones}

    def _fr65_pick(self, snap, n, named=False, idle=False):
        """n switched-off zones; named: with a name (it can be written back); idle: valve not energised."""
        zs = [z for z in snap.get("zones") or [] if z.get("enabled") is False]
        if named:
            zs = [z for z in zs if isinstance(z.get("name"), str) and z["name"]]
        if idle:
            valves = self._fr65_valves()
            zs = [z for z in zs if valves.get(z.get("id")) is False]
        if len(zs) < n:
            raise self.Fr65Skip(f"needs {n} switched-off zone(s){' with a name' if named else ''}"
                                f"{' with the valve not energised' if idle else ''}; the board has {len(zs)}")
        return zs[:n]

    def _fr65_names(self, doc):
        """Every setting name of the import report (FR-65.6) for the zones, levels and duties of `doc`."""
        names = [f"params.{k}" for k in self.FR65_PARAMS]
        names += [f"zones[{z.get('id')}]" for z in doc.get("zones") or []]
        names += [f"schedule.{k}" for k in ("enabled", "preheat", "window", "weekday", "weekend")]
        names += [f"schedule.levels[{l.get('id')}].{lv}" for l in (doc.get("schedule") or {}).get("levels") or []
                  for lv in self.FR65_LEVELS]
        if doc.get("location"):   # a setting only while the controller has a location (api_reference.md)
            names.append("location")
        names += [f"learnedDuty[{d.get('id')}]" for d in doc.get("learnedDuty") or []]
        return names

    def _fr65_diff(self, a, b, path=""):
        """Paths where two settings files differ; firmwareVersion ignored, numbers compared to 0.001."""
        out = []
        if isinstance(a, dict) and isinstance(b, dict):
            for k in sorted(set(a) | set(b), key=str):
                if not path and k == "firmwareVersion":
                    continue
                p = f"{path}.{k}" if path else str(k)
                if k not in a or k not in b:
                    out.append(p)
                else:
                    out += self._fr65_diff(a[k], b[k], p)
        elif isinstance(a, list) and isinstance(b, list):
            if len(a) != len(b):
                out.append(f"{path} (length {len(a)} / {len(b)})")
            else:
                for i, (x, y) in enumerate(zip(a, b)):
                    out += self._fr65_diff(x, y, f"{path}[{x.get('id', i) if isinstance(x, dict) else i}]")
        elif isinstance(a, bool) or isinstance(b, bool):
            if a is not b:
                out.append(path or "(value)")
        elif isinstance(a, (int, float)) and isinstance(b, (int, float)):
            if abs(a - b) > 0.001:
                out.append(path or "(value)")
        elif a != b:
            out.append(path or "(value)")
        return out

    def _fr65_delta(self, target, cur):
        """The smallest settings file that turns `cur` into `target`: differing values only, never `enabled`."""
        f = {"format": 1}
        tp, cp = target.get("params") or {}, cur.get("params") or {}
        p = {k: v for k, v in tp.items() if self._fr65_diff(v, cp.get(k))}
        if p:
            f["params"] = p
        zones = []
        for z in target.get("zones") or []:
            c = self._fr65_find(cur.get("zones"), z.get("id")) or {}
            d = {k: v for k, v in z.items() if k not in ("id", "enabled") and self._fr65_diff(v, c.get(k))}
            if d:
                zones.append({"id": z.get("id"), **d})
        if zones:
            f["zones"] = zones
        ts, cs = target.get("schedule") or {}, cur.get("schedule") or {}
        s = {k: v for k, v in ts.items() if k != "levels" and self._fr65_diff(v, cs.get(k))}
        levels = []
        for l in ts.get("levels") or []:
            c = self._fr65_find(cs.get("levels"), l.get("id")) or {}
            d = {k: v for k, v in l.items() if k != "id" and self._fr65_diff(v, c.get(k))}
            if d:
                levels.append({"id": l.get("id"), **d})
        if levels:
            s["levels"] = levels
        if s:
            f["schedule"] = s
        if "location" in target and target["location"] != cur.get("location"):
            f["location"] = target["location"]
        duty = [d for d in target.get("learnedDuty") or []
                if self._fr65_diff(d.get("duty"), (self._fr65_find(cur.get("learnedDuty"), d.get("id")) or {}).get("duty"))]
        if duty:
            f["learnedDuty"] = duty
        return f

    def _fr65_restore_single(self, t, cur):
        """Fallback restore through the single-setting endpoints of api_reference.md, one value per request."""
        b, to = self.base_url, self.timeout

        def form(v):
            return ("true" if v else "false") if isinstance(v, bool) else str(v)

        tp, cp = t.get("params") or {}, cur.get("params") or {}
        pump_pin = requests.get(f"{b}/api/config/system", timeout=to).json().get("pumpRelayPin")
        if tp.get("pumpControlEnabled") is False and cp.get("pumpControlEnabled") is True:   # frees relay 4 first
            requests.post(f"{b}/api/config/system", data={"pumpControlEnabled": "false"}, timeout=to)
        for k, v in tp.items():
            if k != "pumpControlEnabled" and k not in self.FR65_FROZEN and self._fr65_diff(v, cp.get(k)):
                requests.post(f"{b}/api/config/system", data={k: form(v)}, timeout=to)
        want = {z.get("id"): z for z in t.get("zones") or []}
        for z in cur.get("zones") or []:
            w = want.get(z.get("id"))
            if not w:
                continue
            data = {arg: form(w[f]) for f, arg in (("name", "name"), ("mac", "mac"), ("sensorName", "sname"),
                                                   ("offset", "offset"))
                    if f in w and self._fr65_diff(w[f], z.get(f)) and not (f in ("name", "mac") and w[f] == "")}
            if data:
                requests.post(f"{b}/api/setup/map-zone", data={"zone": z["id"] - 1, **data}, timeout=to)
        for _ in range(4):   # relays: one zone may wait for another to leave; a swap goes through a free relay
            now = self._fr65_export()
            wrong = [(z["id"], want[z["id"]].get("relayPin")) for z in now.get("zones") or []
                     if z.get("id") in want and z.get("relayPin") != want[z["id"]].get("relayPin")]
            if not wrong:
                break
            moved = [requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "pin": pin},
                                   timeout=to).status_code == 200 for zid, pin in wrong]
            if not any(moved):
                pump_on = (now.get("params") or {}).get("pumpControlEnabled")
                used = {z.get("relayPin") for z in now.get("zones") or []}
                pins = {z.get("relayPin") for z in want.values()} | {pump_pin}
                free = [p for p in pins if p is not None and p not in used and not (p == pump_pin and pump_on)]
                if not free:
                    break
                requests.post(f"{b}/api/setup/map-zone", data={"zone": wrong[0][0] - 1, "pin": free[0]}, timeout=to)
        if tp.get("pumpControlEnabled") is True and cp.get("pumpControlEnabled") is False:   # after the zones left relay 4
            requests.post(f"{b}/api/config/system", data={"pumpControlEnabled": "true"}, timeout=to)
        ts, cs = t.get("schedule") or {}, cur.get("schedule") or {}
        sw = {k: ("1" if ts[k] else "0") for k in self.FR65_SWITCHES if k in ts and self._fr65_diff(ts[k], cs.get(k))}
        if sw:
            requests.post(f"{b}/api/schedule/enable", data=sw, timeout=to)
        for weekend, k in (("0", "weekday"), ("1", "weekend")):
            if k in ts and ts[k] != cs.get(k):
                requests.post(f"{b}/api/schedule/slots", data={"weekend": weekend, "slots": ts[k]}, timeout=to)
        for lv in ts.get("levels") or []:
            had = self._fr65_find(cs.get("levels"), lv.get("id")) or {}
            for k in self.FR65_LEVELS:
                if k in lv and self._fr65_diff(lv[k], had.get(k)):
                    requests.post(f"{b}/api/schedule/level", data={"zone": lv["id"], "level": k, "temp": form(lv[k])},
                                  timeout=to)
        if t.get("location") and t["location"] != cur.get("location"):
            requests.post(f"{b}/api/setup/location", data={"coords": t["location"]}, timeout=to)
        # learned duty has no single-setting endpoint: a difference left here is reported by _fr65_restore

    def _fr65_restore(self, start_snap, res):
        """Give the board the group baseline back (learned duty: as at the test start). Never raises."""
        if start_snap is None or getattr(self, "fr65_baseline", None) is None:
            return res
        target = self._fr65_copy(self.fr65_baseline)
        if "learnedDuty" in start_snap:
            target["learnedDuty"] = self._fr65_copy(start_snap["learnedDuty"])
        else:
            target.pop("learnedDuty", None)
        problem = None
        try:
            cur = self._fr65_export()
            if self._fr65_diff(cur, target):
                self._fr65_post(self._fr65_delta(target, cur))
                cur = self._fr65_export()
            if self._fr65_diff(cur, target):
                self._fr65_restore_single(target, cur)
                cur = self._fr65_export()
            left = self._fr65_diff(cur, target)
            if left:
                problem = f"BOARD NOT RESTORED, differs at {left[:6]}"
        except Exception as e:
            problem = f"BOARD RESTORE ERROR: {e}"
        if not problem:
            return res
        if res is None:
            return TestResult("SWE.5-TC29", "restore", "FR-65.3", "FAIL", 0.0, problem)
        return TestResult(res.test_id, res.name, res.requirement, "FAIL", res.latency_ms, f"{res.details}; {problem}")

    def _fr65_error(self, test_id, name, req, start, e):
        status = "SKIP" if isinstance(e, self.Fr65Skip) else "FAIL"
        return TestResult(test_id, name, req, status, (time.time() - start) * 1000, str(e) or type(e).__name__)

    def test_tc29_1_export_content(self):
        """SWE.5-TC29.1: Export Holds Format 1, the Firmware Version and Every Stored Setting (FR-65.1)"""
        test_id = "SWE.5-TC29.1"
        name = "Export Content"
        req = "FR-65.1"
        start = time.time()
        try:
            b, to = self.base_url, self.timeout
            exp = self._fr65_export()   # HTTP 200, one JSON object, format = 1
            if getattr(self, "fr65_baseline", None) is None:
                self.fr65_baseline = self._fr65_copy(exp)   # group baseline: every later test restores the board to it
            assert exp["format"] == 1, f"format {exp.get('format')!r}, expected 1"
            fw = requests.get(f"{b}/api/boiler/status", timeout=to).json().get("firmwareVersion")
            assert exp.get("firmwareVersion") == fw, f"firmwareVersion {exp.get('firmwareVersion')!r}, FR-21 status {fw!r}"
            # §6 parameters under their /api/config/system names, same values
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            params = exp.get("params")
            assert isinstance(params, dict), "params missing"
            for k in self.FR65_PARAMS:
                assert k in params, f"params.{k} missing"
                assert not self._fr65_diff(params[k], cfg.get(k)), \
                    f"params.{k} = {params[k]!r}, /api/config/system {cfg.get(k)!r}"
            # Per zone: name, MAC, relay, on / off as /api/config/system; offset as /api/zones; sensor name; no target (C-13)
            ez = {z.get("id"): z for z in exp.get("zones") or []}
            cz = {z.get("id"): z for z in cfg.get("zones") or []}
            assert ez and set(ez) == set(cz), \
                f"export zones {sorted(ez, key=str)}, /api/config/system zones {sorted(cz, key=str)}"
            live = {z.get("id"): z for z in requests.get(f"{b}/api/zones", timeout=to).json().get("zones") or []}
            for zid, z in ez.items():
                missing = [k for k in self.FR65_ZONE_KEYS if k not in z]
                assert not missing, f"zones[{zid}] without {missing}"
                for k in ("name", "mac", "relayPin", "enabled"):
                    assert not self._fr65_diff(z[k], cz[zid].get(k)), \
                        f"zones[{zid}].{k} = {z[k]!r}, /api/config/system {cz[zid].get(k)!r}"
                assert isinstance(z["sensorName"], str), f"zones[{zid}].sensorName {z['sensorName']!r} is not a text"
                assert zid in live, f"zone {zid} not in /api/zones"
                assert not self._fr65_diff(z["offset"], live[zid].get("tempOffset")), \
                    f"zones[{zid}].offset {z['offset']!r}, /api/zones tempOffset {live[zid].get('tempOffset')!r}"
                assert "target" not in z, f"zones[{zid}] holds the current target {z['target']!r}: the schedule sets it (FR-65.1)"
            # Schedule: slots and switch as /api/schedule, pre-heat and window detection on / off, levels per zone
            sch = requests.get(f"{b}/api/schedule", timeout=to).json()
            es = exp.get("schedule")
            assert isinstance(es, dict), "schedule missing"
            for k in ("weekday", "weekend", "enabled"):
                assert k in es and not self._fr65_diff(es[k], sch.get(k)), \
                    f"schedule.{k} = {es.get(k)!r}, /api/schedule {sch.get(k)!r}"
            for k in ("preheat", "window"):
                assert isinstance(es.get(k), bool), f"schedule.{k} = {es.get(k)!r}, expected true / false"
            levels = {l.get("id"): l for l in es.get("levels") or []}
            assert set(levels) == set(ez), f"schedule.levels for zones {sorted(levels, key=str)}, zones {sorted(ez, key=str)}"
            for i, sl in enumerate(sch.get("zones") or []):
                zid = sl.get("id", i + 1)
                for lv in self.FR65_LEVELS:
                    assert not self._fr65_diff((levels.get(zid) or {}).get(lv), sl.get(lv)), \
                        f"schedule.levels[{zid}].{lv} = {(levels.get(zid) or {}).get(lv)!r}, /api/schedule {sl.get(lv)!r}"
            # Location (FR-9.2): exported when set, absent while not set
            coords = requests.get(f"{b}/api/setup/location", timeout=to).json().get("coords")
            if coords:
                assert isinstance(exp.get("location"), str), f"location {coords!r} is set but not exported"
                got = [float(x) for x in exp["location"].split(",")]
                want = [float(x) for x in coords.split(",")]
                # 0.0001°: the two endpoints may print a different number of decimals
                assert len(got) == 2 and all(abs(g - w) < 0.0001 for g, w in zip(got, want)), \
                    f"location {exp['location']!r}, /api/setup/location {coords!r}"
            else:
                assert "location" not in exp, f"location exported while not set: {exp.get('location')!r}"
            # Learned base duty per zone, equal to learnedDuty of /api/zones, inside its bounds (FR-47: 0-60 %)
            duty = {d.get("id"): d for d in exp.get("learnedDuty") or []}
            assert set(duty) == set(ez), f"learnedDuty for zones {sorted(duty, key=str)}, zones {sorted(ez, key=str)}"
            for zid, d in duty.items():
                v = d.get("duty")
                assert isinstance(v, (int, float)) and not isinstance(v, bool) and 0.0 <= v <= 60.0, \
                    f"learnedDuty[{zid}] = {v!r} outside the learned base duty bounds 0-60 % (FR-47)"
                assert not self._fr65_diff(v, live[zid].get("learnedDuty")), \
                    f"learnedDuty[{zid}] = {v!r}, /api/zones learnedDuty {live[zid].get('learnedDuty')!r}"
            latency = (time.time() - start) * 1000
            details = (f"format 1, {exp['firmwareVersion']}, {len(params)} params = /api/config/system, {len(ez)} zones, "
                       f"schedule = /api/schedule, location {'= /api/setup/location' if coords else 'not set, absent'}")
            return TestResult(test_id, name, req, "PASS", latency, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc29_2_export_follows_settings(self):
        """SWE.5-TC29.2: Export Shows Settings Changed Through Their Own Endpoints (FR-65.1)"""
        test_id = "SWE.5-TC29.2"
        name = "Export Follows Stored Settings"
        req = "FR-65.1"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, b, to, st = z["id"], self.base_url, self.timeout, self._fr65_step
            new_name = "TC29 export" if z["name"] != "TC29 export" else "TC29 export 2"
            off = st(z["offset"], 0.5, 3.0)     # sensor offset -3.0...+3.0 °C (FR-49), upward: no frost effect
            r = requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "name": new_name,
                                                               "offset": off}, timeout=to)
            assert r.status_code == 200, f"map-zone: HTTP {r.status_code}"
            night = st((self._fr65_find(snap["schedule"]["levels"], zid) or {})["night"], 0.5, 30.0)   # levels 5-30 °C
            r = requests.post(f"{b}/api/schedule/level", data={"zone": zid, "level": "night", "temp": night}, timeout=to)
            assert r.status_code == 200, f"schedule/level: HTTP {r.status_code}"
            wkend = "07:00=day,23:00=night" if snap["schedule"]["weekend"] != "07:00=day,23:00=night" \
                else "07:30=day,23:00=night"
            r = requests.post(f"{b}/api/schedule/slots", data={"weekend": "1", "slots": wkend}, timeout=to)
            assert r.status_code == 200, f"schedule/slots: HTTP {r.status_code}"
            pre, win = not snap["schedule"]["preheat"], not snap["schedule"]["window"]
            r = requests.post(f"{b}/api/schedule/enable", data={"preheat": "1" if pre else "0",
                                                                "window": "1" if win else "0"}, timeout=to)
            assert r.status_code == 200, f"schedule/enable: HTTP {r.status_code}"
            hyst = st(snap["params"]["hysteresis"], 0.1, 1.0)   # hysteresis 0.1-1.0 °C (§6)
            r = requests.post(f"{b}/api/config/system", data={"hysteresis": str(hyst)}, timeout=to)
            assert r.status_code == 200, f"config/system: HTTP {r.status_code}"
            loc = None
            if snap.get("location"):
                lat, lon = (float(x) for x in snap["location"].split(","))
                loc = (st(lat, 0.01, 90.0), lon)   # 0.01° (about 1 km): same weather; latitude -90..90 (FR-9.2)
                r = requests.post(f"{b}/api/setup/location", data={"coords": f"{loc[0]:.4f},{loc[1]:.4f}"}, timeout=to)
                assert r.status_code == 200, f"setup/location: HTTP {r.status_code}"
            exp = self._fr65_export()
            ez = self._fr65_find(exp.get("zones"), zid) or {}
            assert ez.get("name") == new_name, f"zones[{zid}].name {ez.get('name')!r}, set {new_name!r}"
            assert not self._fr65_diff(ez.get("offset"), off), f"zones[{zid}].offset {ez.get('offset')!r}, set {off}"
            el = self._fr65_find(exp["schedule"].get("levels"), zid) or {}
            assert not self._fr65_diff(el.get("night"), night), f"schedule.levels[{zid}].night {el.get('night')!r}, set {night}"
            assert exp["schedule"].get("weekend") == wkend, f"schedule.weekend {exp['schedule'].get('weekend')!r}, set {wkend!r}"
            assert exp["schedule"].get("preheat") is pre and exp["schedule"].get("window") is win, \
                f"schedule.preheat / window {exp['schedule'].get('preheat')!r} / {exp['schedule'].get('window')!r}, set {pre} / {win}"
            assert not self._fr65_diff(exp["params"].get("hysteresis"), hyst), \
                f"params.hysteresis {exp['params'].get('hysteresis')!r}, set {hyst}"
            if loc:
                assert isinstance(exp.get("location"), str), "location missing after it was set"
                got = [float(x) for x in exp["location"].split(",")]
                assert len(got) == 2 and abs(got[0] - loc[0]) < 0.0001 and abs(got[1] - loc[1]) < 0.0001, \
                    f"location {exp['location']!r}, set {loc[0]:.4f},{loc[1]:.4f}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {zid} name / offset, night level, weekend slots, pre-heat, window, hysteresis"
                             f"{', location' if loc else ''} follow their endpoints; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_3_export_valid_json_any_text(self):
        """SWE.5-TC29.3: Export Stays Valid JSON With Special Characters in Stored Texts (FR-65.1)"""
        test_id = "SWE.5-TC29.3"
        name = "Export Valid JSON, Any Text"
        req = "FR-65.1"
        start = time.time()
        snap, res, z = None, None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, b, to = z["id"], self.base_url, self.timeout
            # A name FR-11.3 allows (1-31 bytes, no '"' or '\'): characters JSON, HTML and URL encoding treat
            # specially and 2-byte UTF-8 letters, filled up to the 31-byte maximum
            text = "<b>'&{[,:]}/%+ ăîșț"
            text += "x" * (self.FR65_NAME_MAX - len(text.encode("utf-8")))
            assert len(text.encode("utf-8")) == self.FR65_NAME_MAX, "test text is not at the name maximum"
            r = requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "name": text}, timeout=to)
            assert r.status_code == 200, f"map-zone refused a name FR-11.3 allows: HTTP {r.status_code}"
            # FR-11.3: a sensor name is 0-31 bytes, UTF-8, without '"', '\' or a control character - refused with
            # HTTP 400, the old value kept (PB-49)
            for hostile in ('q"b', "q\\b", "q\tb"):
                r_h = requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "sname": hostile}, timeout=to)
                assert r_h.status_code == 400, f"sensor name {hostile!r}: expected HTTP 400, got {r_h.status_code}"
            sname = "<s>'&{[,:]}/%+ ăîșț"
            sname += "y" * (self.FR65_NAME_MAX - len(sname.encode("utf-8")))
            r_s = requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "sname": sname}, timeout=to)
            assert r_s.status_code == 200, f"map-zone refused a sensor name FR-11.3 allows: HTTP {r_s.status_code}"
            raw = requests.get(f"{b}/api/config/export", timeout=to)
            assert raw.status_code == 200, f"GET /api/config/export: HTTP {raw.status_code}"
            try:
                doc = json.loads(raw.content.decode("utf-8"))   # strict UTF-8 and strict JSON
            except ValueError as e:
                raise AssertionError(f"export is not valid JSON with these texts stored: {e}")
            ez = self._fr65_find(doc.get("zones"), zid) or {}
            assert ez.get("name") == text, f"zones[{zid}].name {ez.get('name')!r}, stored {text!r}"
            assert ez.get("sensorName") == sname, f"zones[{zid}].sensorName {ez.get('sensorName')!r}, stored {sname!r}"
            sdet = "31-byte sensor name exported intact, names with quote / backslash / tab refused (HTTP 400)"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"31-byte name with <>'&{{[,:]}}/%+ and UTF-8 exported intact; {sdet}; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            if z is not None:
                try:   # write both texts back directly: the generic restore needs an export that parses
                    requests.post(f"{self.base_url}/api/setup/map-zone",
                                  data={"zone": z["id"] - 1, "name": z["name"], "sname": z.get("sensorName", "")},
                                  timeout=self.timeout)
                except Exception:
                    pass
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_4_export_every_name_character(self):
        """SWE.5-TC29.4: Export Valid JSON With Every Character a Zone Name Accepts (FR-65.1)"""
        test_id = "SWE.5-TC29.4"
        name = "Export Valid JSON, Every Character"
        req = "FR-65.1"
        start = time.time()
        snap, res, z = None, None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, b, to = z["id"], self.base_url, self.timeout
            # FR-11.3 refuses '"', '\' and control characters (and empty / over 31 bytes): every other printable
            # ASCII character is accepted, in names of at most 31 bytes, one name after the other.
            chars = "".join(chr(c) for c in range(0x20, 0x7F) if chr(c) not in '"\\')
            names = [chars[i:i + self.FR65_NAME_MAX] for i in range(0, len(chars), self.FR65_NAME_MAX)]
            assert "".join(names) == chars and all(len(n.encode("utf-8")) <= self.FR65_NAME_MAX for n in names), \
                "test names do not cover the characters"
            for text in names:
                r = requests.post(f"{b}/api/setup/map-zone", data={"zone": zid - 1, "name": text}, timeout=to)
                assert r.status_code == 200, f"map-zone refused name {text!r} that FR-11.3 allows: HTTP {r.status_code}"
                raw = requests.get(f"{b}/api/config/export", timeout=to)
                assert raw.status_code == 200, f"GET /api/config/export: HTTP {raw.status_code}"
                try:
                    doc = json.loads(raw.content.decode("utf-8"))
                except ValueError as e:
                    raise AssertionError(f"export not valid JSON with name {text!r}: {e}")
                got = (self._fr65_find(doc.get("zones"), zid) or {}).get("name")
                assert got == text, f"zones[{zid}].name {got!r}, stored {text!r}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(chars)} printable characters in {len(names)} names: export valid JSON, names intact; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_5_export_no_credential_no_state(self):
        """SWE.5-TC29.5: Export Holds No Credential, Counter or State (FR-65.2)"""
        test_id = "SWE.5-TC29.5"
        name = "Export Without Credential / State"
        req = "FR-65.2"
        start = time.time()
        try:
            exp = self._fr65_export()
            # Only the keys of file format 1 (api_reference.md): any other key could carry a credential (FR-59), a
            # burner counter, the mode, away, a boost, a DTC, an event, history or a sensor reading
            extra = sorted(set(exp) - set(self.FR65_TOP_KEYS))
            assert not extra, f"top-level keys outside format 1: {extra}"
            params = exp.get("params") or {}
            assert set(params) == set(self.FR65_PARAMS), \
                f"params keys outside / missing: {sorted(set(params) ^ set(self.FR65_PARAMS))}"
            for z in exp.get("zones") or []:
                assert set(z) == set(self.FR65_ZONE_KEYS), \
                    f"zones[{z.get('id')}] keys differ: {sorted(set(z) ^ set(self.FR65_ZONE_KEYS))}"
            sch = exp.get("schedule") or {}
            assert "away" not in sch, "schedule.away is exported: global away is state (FR-65.2)"
            assert set(sch) == set(self.FR65_SCHEDULE_KEYS), \
                f"schedule keys differ: {sorted(set(sch) ^ set(self.FR65_SCHEDULE_KEYS))}"
            for l in sch.get("levels") or []:
                assert set(l) == {"id", *self.FR65_LEVELS}, f"schedule.levels[{l.get('id')}] keys {sorted(l)}"
            for d in exp.get("learnedDuty") or []:
                assert set(d) == {"id", "duty"}, f"learnedDuty[{d.get('id')}] keys {sorted(d)}"
            # Every setting is one number, text or true / false: no nested record that could hold more
            records = [params, {k: v for k, v in sch.items() if k != "levels"}]
            records += list(exp.get("zones") or []) + list(sch.get("levels") or []) + list(exp.get("learnedDuty") or [])
            nested = [k for rec in records for k, v in rec.items() if isinstance(v, (dict, list))]
            assert not nested, f"nested values in the export: {nested}"
            assert isinstance(exp.get("location", ""), str), "location is not a text"
            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              "only format-1 keys: no credential, counter, mode, away, boost, DTC, event, history, reading")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc29_6_import_skips_credentials(self):
        """SWE.5-TC29.6: Credential in the File Skipped as Unknown, Nothing Changed (FR-65.2)"""
        test_id = "SWE.5-TC29.6"
        name = "Import Skips Credentials"
        req = "FR-65.2"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            # Credential key names are not specified (FR-59 not built): the usual ones. Each value has 7 characters,
            # below the 8-63 of the credentials row in §6, so a firmware that wrongly takes one cannot store it.
            creds = {"wifiPassword": "tc29-no", "otaPassword": "tc29-no", "apPassword": "tc29-no", "password": "tc29-no"}
            applied, kept, skipped = self._fr65_report(self._fr65_post({"format": 1, **creds, "params": dict(creds)}))
            for k in creds:
                for n in (k, f"params.{k}"):
                    assert skipped.get(n) == "unknown", f"{n}: {skipped.get(n)!r}, expected skipped 'unknown'"
                    assert n not in applied, f"{n} applied"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"settings changed: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{2 * len(creds)} credential names (top level and params) skipped 'unknown', nothing changed")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_7_import_params_range(self):
        """SWE.5-TC29.7: Imported Parameters Checked Against Their §6 Range, Both Sides (FR-65.3)"""
        test_id = "SWE.5-TC29.7"
        name = "Import Parameter Ranges"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            b, to = self.base_url, self.timeout
            # Accepted: one limit of each range, the safe end (longer guards, a more frequent heartbeat).
            # frostTemp is not moved: either end changes frost heating; its two refusals are below.
            ok = {"actuatorDelaySec": 600,     # interlockDelay 120-600 s
                  "pumpOverrunSec": 600,       # overrun 180-600 s
                  "burnerMinRunSec": 600,      # burnerMinRun 120-600 s
                  "burnerMinPauseSec": 600,    # burnerMinPause 120-600 s
                  "rfSafetyHeartbeatSec": 30,  # hbInterval 30-900 s
                  "sensorTimeoutSec": 3600,    # sensorTimeout 60-3600 s
                  "hysteresis": 0.1,           # hysteresis 0.1-1.0 °C
                  "windowThreshold": 2.0}      # windowThreshold 0.8-2.0 °C
            below = {"actuatorDelaySec": 119, "pumpOverrunSec": 179, "burnerMinRunSec": 119, "burnerMinPauseSec": 119,
                     "rfSafetyHeartbeatSec": 29, "freezeProtectionTemp": 7.9,   # frostTemp 8-16 °C
                     "sensorTimeoutSec": 59, "hysteresis": 0.05, "windowThreshold": 0.7}
            above = {"actuatorDelaySec": 601, "pumpOverrunSec": 601, "burnerMinRunSec": 601, "burnerMinPauseSec": 601,
                     "rfSafetyHeartbeatSec": 901, "freezeProtectionTemp": 16.1,
                     "sensorTimeoutSec": 3601, "hysteresis": 1.1, "windowThreshold": 2.1}
            applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "params": ok}))
            for k, v in ok.items():
                assert f"params.{k}" in applied and f"params.{k}" not in skipped, \
                    f"params.{k} = {v} inside §6 not applied: {skipped.get('params.' + k)!r}"
            for bad in (below, above):
                applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "params": bad}))
                for k, v in bad.items():
                    assert skipped.get(f"params.{k}") == "invalid", \
                        f"params.{k} = {v} outside §6: {skipped.get('params.' + k)!r}, expected skipped 'invalid'"
                    assert f"params.{k}" not in applied, f"params.{k} = {v} outside §6 applied"
            exp = self._fr65_export()
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            for k, v in ok.items():   # applied and stored: in the export and in the value control uses
                assert not self._fr65_diff(exp["params"].get(k), v), f"export params.{k} {exp['params'].get(k)!r}, imported {v}"
                assert not self._fr65_diff(cfg.get(k), v), f"/api/config/system {k} {cfg.get(k)!r}, imported {v}"
            assert not self._fr65_diff(exp["params"].get("freezeProtectionTemp"), snap["params"]["freezeProtectionTemp"]), \
                "frostTemp changed by refused values"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(ok)} limit values applied and stored, {len(below) + len(above)} values outside §6 "
                             "skipped 'invalid'; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_8_import_pump_not_on_zone_relay(self):
        """SWE.5-TC29.8: Imported Pump Output Refused While a Zone Uses Relay 4 (FR-65.3)"""
        test_id = "SWE.5-TC29.8"
        name = "Import Pump Check"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            b, to = self.base_url, self.timeout
            pump_pin = requests.get(f"{b}/api/config/system", timeout=to).json().get("pumpRelayPin")
            if snap["params"].get("pumpControlEnabled") or any(z.get("relayPin") == pump_pin for z in snap["zones"]):
                raise self.Fr65Skip("needs the pump output off and relay 4 free of zones")
            z = self._fr65_pick(snap, 1, idle=True)[0]
            r = requests.post(f"{b}/api/setup/map-zone", data={"zone": z["id"] - 1, "pin": pump_pin}, timeout=to)
            assert r.status_code == 200, f"relay 4 to switched-off zone {z['id']} (pump off): HTTP {r.status_code}"
            # Same check as POST /api/config/system: pumpOutput not on a relay a zone uses (FR-14.1, FR-11.3)
            applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "params": {"pumpControlEnabled": True}}))
            assert skipped.get("params.pumpControlEnabled") == "invalid", \
                f"params.pumpControlEnabled with zone {z['id']} on relay 4: {skipped.get('params.pumpControlEnabled')!r}, " \
                "expected skipped 'invalid'"
            assert "params.pumpControlEnabled" not in applied, "pump output applied on a zone relay"
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            assert cfg.get("pumpControlEnabled") is False, "pump output switched on while a zone uses relay 4"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {z['id']} on relay 4: imported pump output skipped 'invalid', stays off; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_9_import_zone_rules(self):
        """SWE.5-TC29.9: Imported Zone Checked by the Commissioning Rules, Limits Accepted (FR-65.3)"""
        test_id = "SWE.5-TC29.9"
        name = "Import Zone Rules"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, b, to = z["id"], self.base_url, self.timeout
            bad = [({"name": 'a"b'}, "name with a quote"),
                   ({"name": "a\\b"}, "name with a backslash"),
                   ({"name": ""}, "empty name"),
                   ({"name": "x" * (self.FR65_NAME_MAX + 1)}, "name of 32 bytes"),   # name at most 31 bytes
                   ({"name": "a\tb"}, "name with a control character"),
                   ({"name": "a\nb"}, "name with a line feed"),
                   ({"sensorName": "y" * (self.FR65_NAME_MAX + 1)}, "sensor name of 32 bytes"),   # 0-31 bytes
                   ({"sensorName": 's"b'}, "sensor name with a quote"),
                   ({"sensorName": "s\\b"}, "sensor name with a backslash"),
                   ({"sensorName": "s\tb"}, "sensor name with a control character"),
                   ({"relayPin": 99}, "relay output that is not a relay of the board"),   # same pin 99 as TC27.3
                   ({"mac": "nonsense"}, "malformed MAC"),
                   ({"offset": -3.1}, "offset below -3.0 °C"),     # sensor offset -3.0...+3.0 °C (FR-49)
                   ({"offset": 3.1}, "offset above +3.0 °C")]
            for fields, what in bad:
                applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "zones": [{"id": zid, **fields}]}))
                assert skipped.get(f"zones[{zid}]") == "invalid", \
                    f"{what}: zones[{zid}] {skipped.get(f'zones[{zid}]')!r}, expected skipped 'invalid'"
                assert f"zones[{zid}]" not in applied, f"{what}: zones[{zid}] applied"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"refused zones changed settings: {left}"
            # The limits are accepted: 31-byte name, offset +3.0 °C ...
            name31 = "TC29" + "x" * (self.FR65_NAME_MAX - 4)
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": zid, "name": name31, "offset": 3.0}]}))
            assert f"zones[{zid}]" in applied, f"zone at the upper limits not applied: {skipped.get(f'zones[{zid}]')!r}"
            ez = self._fr65_find(self._fr65_export().get("zones"), zid) or {}
            assert ez.get("name") == name31 \
                and not self._fr65_diff(ez.get("offset"), 3.0), f"zone {zid} after import: {ez}"
            # ... and offset -3.0 °C - the offset only where it cannot pull a room reading near
            # frostTemp (it would start frost heating on the real valves)
            live = self._fr65_find(requests.get(f"{b}/api/zones", timeout=to).json().get("zones"), zid) or {}
            t_now = live.get("currentTemp")
            frost = snap["params"]["freezeProtectionTemp"]
            low_ok = not isinstance(t_now, (int, float)) or t_now - 3.0 - 3.0 >= frost + 2.0
            low = {"offset": -3.0} if low_ok else {"offset": 0.0}
            applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "zones": [{"id": zid, **low}]}))
            assert f"zones[{zid}]" in applied, f"zone at the lower limits not applied: {skipped.get(f'zones[{zid}]')!r}"
            ez = self._fr65_find(self._fr65_export().get("zones"), zid) or {}
            if low_ok:
                assert not self._fr65_diff(ez.get("offset"), -3.0), f"zones[{zid}].offset {ez.get('offset')!r}, imported -3.0"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(bad)} bad zone values skipped 'invalid', nothing changed; 31-byte name, "
                             f"offset +3.0{' / -3.0' if low_ok else ' (-3.0 not sent: room reading)'} "
                             "applied; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_10_import_level_range(self):
        """SWE.5-TC29.10: Imported Level Temperatures Checked Against the Level Range (FR-65.3)"""
        test_id = "SWE.5-TC29.10"
        name = "Import Level Range"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            zid = self._fr65_pick(snap, 1)[0]["id"]
            before = self._fr65_find(snap["schedule"]["levels"], zid) or {}
            ok = {"day": 30.0, "away": 5.0}       # zone levels 5-30 °C (FR-29), both limits
            applied, kept, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "schedule": {"levels": [{"id": zid, **ok}]}}))
            for lv in ok:
                assert f"schedule.levels[{zid}].{lv}" in applied, \
                    f"level {lv} = {ok[lv]} not applied: {skipped.get(f'schedule.levels[{zid}].{lv}')!r}"
            assert f"schedule.levels[{zid}].night" in kept, "level missing from the file not reported kept"
            bad = {"day": 4.9, "night": 30.1}
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "schedule": {"levels": [{"id": zid, **bad}]}}))
            for lv, v in bad.items():
                n = f"schedule.levels[{zid}].{lv}"
                assert skipped.get(n) == "invalid" and n not in applied, f"{n} = {v}: {skipped.get(n)!r}, expected 'invalid'"
            got = self._fr65_find(self._fr65_export()["schedule"]["levels"], zid) or {}
            assert not self._fr65_diff(got.get("day"), 30.0) and not self._fr65_diff(got.get("away"), 5.0), \
                f"levels after import: {got}"
            assert not self._fr65_diff(got.get("night"), before.get("night")), f"night changed: {got.get('night')!r}"
            sch = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()
            sl = (sch.get("zones") or [])[zid - 1]
            assert not self._fr65_diff(sl.get("day"), 30.0) and not self._fr65_diff(sl.get("away"), 5.0), \
                f"/api/schedule zone {zid} after import: {sl}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {zid}: day 30.0 / away 5.0 applied, 4.9 and 30.1 skipped 'invalid'; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_11_import_slots(self):
        """SWE.5-TC29.11: Imported Slot Lists Checked Like the Schedule, Stored Sorted (FR-65.3)"""
        test_id = "SWE.5-TC29.11"
        name = "Import Slot Lists"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            # FR-31: up to 8 slots HH:MM=level, kept sorted
            unsorted8 = "22:30=night,06:30=day,08:30=away,17:00=day,12:00=day,13:00=away,20:00=night,05:00=night"
            sorted8 = "05:00=night,06:30=day,08:30=away,12:00=day,13:00=away,17:00=day,20:00=night,22:30=night"
            nine = sorted8 + ",23:30=night"
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "schedule": {"weekday": unsorted8, "weekend": nine}}))
            assert "schedule.weekday" in applied, f"8 valid slots not applied: {skipped.get('schedule.weekday')!r}"
            assert skipped.get("schedule.weekend") == "invalid", \
                f"9 slots: {skipped.get('schedule.weekend')!r}, expected skipped 'invalid'"
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "schedule": {"weekday": "25:00=day", "weekend": "06:30=party"}}))
            for n in ("schedule.weekday", "schedule.weekend"):
                assert skipped.get(n) == "invalid" and n not in applied, f"{n}: {skipped.get(n)!r}, expected 'invalid'"
            exp = self._fr65_export()
            assert exp["schedule"].get("weekday") == sorted8, f"weekday {exp['schedule'].get('weekday')!r}, expected sorted"
            assert exp["schedule"].get("weekend") == snap["schedule"]["weekend"], "weekend changed by refused lists"
            sch = requests.get(f"{self.base_url}/api/schedule", timeout=self.timeout).json()
            assert sch.get("weekday") == sorted8, f"/api/schedule weekday {sch.get('weekday')!r}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             "8 unsorted slots applied sorted; 9 slots, 25:00 and an unknown level skipped 'invalid'; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_12_import_location_refused(self):
        """SWE.5-TC29.12: Imported Location Outside Latitude / Longitude Range Skipped (FR-65.3)"""
        test_id = "SWE.5-TC29.12"
        name = "Import Location Refused"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            # FR-9.2: latitude -90..90, longitude -180..180
            for bad in ("91,26", "-91,26", "44,181", "44,-181", "abc"):
                applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "location": bad}))
                assert skipped.get("location") == "invalid" and "location" not in applied, \
                    f"location {bad!r}: {skipped.get('location')!r}, expected skipped 'invalid'"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"refused locations changed settings: {left}"
            # The limits are accepted (FR-9.2). Only where a location is set: it is written back in `finally`
            # (a location cannot be unset again). The weather of the board's place changes meanwhile; restored.
            limits = 0
            if snap.get("location"):
                lat, lon = (float(x) for x in snap["location"].split(","))
                for good, lat_g, lon_g in (("latitude 90", 90, lon), ("latitude -90", -90, lon),
                                           ("longitude 180", lat, 180), ("longitude -180", lat, -180)):
                    new = f"{lat_g:.4f},{lon_g:.4f}"
                    applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "location": new}))
                    assert "location" in applied, f"{good} ({new!r}) not applied: {skipped.get('location')!r}"
                    coords = requests.get(f"{self.base_url}/api/setup/location", timeout=self.timeout).json().get("coords", "")
                    got = [float(x) for x in coords.split(",")]
                    assert len(got) == 2 and abs(got[0] - lat_g) < 0.0001 and abs(got[1] - lon_g) < 0.0001, \
                        f"{good}: /api/setup/location {coords!r}, imported {new!r}"
                    limits += 1
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"5 bad locations skipped 'invalid', location {snap.get('location', 'not set')!r} kept; "
                             + (f"{limits} limit values 90 / -90 / 180 / -180 applied, restored" if limits
                                else "limits not sent: no location set to restore"))
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_13_import_location_applied(self):
        """SWE.5-TC29.13: Imported Valid Location Applied and Stored (FR-65.3)"""
        test_id = "SWE.5-TC29.13"
        name = "Import Location Applied"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            if not snap.get("location"):
                raise self.Fr65Skip("location not set: no endpoint removes one again, so none is set here")
            lat, lon = (float(x) for x in snap["location"].split(","))
            lat2 = self._fr65_step(lat, 0.01, 90.0)   # 0.01° (about 1 km): same weather; latitude -90..90 (FR-9.2)
            new = f"{lat2:.4f},{lon:.4f}"
            applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "location": new}))
            assert "location" in applied, f"location {new!r} not applied: {skipped.get('location')!r}"
            coords = requests.get(f"{self.base_url}/api/setup/location", timeout=self.timeout).json().get("coords", "")
            got = [float(x) for x in coords.split(",")]
            assert len(got) == 2 and abs(got[0] - lat2) < 0.0001 and abs(got[1] - lon) < 0.0001, \
                f"/api/setup/location {coords!r}, imported {new!r}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency, f"location {new!r} applied and stored; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_14_import_learned_duty_refused(self):
        """SWE.5-TC29.14: Imported Learned Duty Outside Its Bounds Skipped (FR-65.3)"""
        test_id = "SWE.5-TC29.14"
        name = "Import Learned Duty Refused"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            zid = self._fr65_pick(snap, 1)[0]["id"]
            diag = f"{self.base_url}/api/diag/status"
            # api_reference.md: every active dtcs[] entry has `code` = "DTC-nnn"
            has301 = lambda d: any(isinstance(e, dict) and e.get("code") == "DTC-301" for e in d or [])
            dtc_before = has301(requests.get(diag, timeout=self.timeout).json().get("dtcs"))
            for bad in (60.1, -0.1):   # learned base duty 0-60 % (FR-47)
                applied, _, skipped = self._fr65_report(self._fr65_post(
                    {"format": 1, "learnedDuty": [{"id": zid, "duty": bad}]}))
                n = f"learnedDuty[{zid}]"
                assert skipped.get(n) == "invalid" and n not in applied, \
                    f"{n} = {bad} %: {skipped.get(n)!r}, expected skipped 'invalid'"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"refused duties changed settings: {left}"
            # FR-65.3: a refused imported duty is not a learning reset - DTC-301 WARN_LEARNING_RESET not raised
            dtcs = requests.get(diag, timeout=self.timeout).json().get("dtcs")
            assert dtc_before or not has301(dtcs), f"DTC-301 raised by a refused import: {dtcs}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {zid}: 60.1 % and -0.1 % skipped 'invalid', kept, DTC-301 not raised"
                             f"{' (already active before)' if dtc_before else ''}")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_15_import_learned_duty_applied(self):
        """SWE.5-TC29.15: Imported Learned Duty Inside Its Bounds Applied and Stored (FR-65.3)"""
        test_id = "SWE.5-TC29.15"
        name = "Import Learned Duty Applied"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            # Only a zone whose stored duty is above 0 % is surely learned; 0 % may be "nothing learned" (FR-51 then
            # uses faultDuty), which no import can give back. 60 % is not sent: it would open a valve in FR-51.
            learned = [d for d in snap.get("learnedDuty") or [] if isinstance(d.get("duty"), (int, float))
                       and not isinstance(d.get("duty"), bool) and d["duty"] > 0]
            if not learned:
                raise self.Fr65Skip("no zone has a learned duty above 0 %: an import could not be undone")
            d = learned[0]
            zid = d["id"]
            values = (0.0, round(max(d["duty"] - 1.0, 0.5), 1))   # lower limit of 0-60 % (FR-47), one inside
            for v in values:
                applied, _, skipped = self._fr65_report(self._fr65_post(
                    {"format": 1, "learnedDuty": [{"id": zid, "duty": v}]}))
                assert f"learnedDuty[{zid}]" in applied, f"duty {v} % not applied: {skipped.get(f'learnedDuty[{zid}]')!r}"
                got = (self._fr65_find(self._fr65_export().get("learnedDuty"), zid) or {}).get("duty")
                assert not self._fr65_diff(got, v), f"learnedDuty[{zid}] {got!r} after importing {v}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {zid}: duty {values[0]} % and {values[1]} % applied and exported; {d['duty']} % restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_16_import_answer_time(self):
        """SWE.5-TC29.16: Import Writing Many Settings Answers Inside Its Time Bound (FR-65.3)"""
        test_id = "SWE.5-TC29.16"
        name = "Import Answer Time"
        req = "FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            p, s, st = snap["params"], snap["schedule"], self._fr65_step
            f = {"format": 1,
                 # other values inside the §6 ranges; frostTemp, hbInterval and the switches stay
                 "params": {"actuatorDelaySec": st(p["actuatorDelaySec"], 10, 600),     # interlockDelay 120-600 s
                            "pumpOverrunSec": st(p["pumpOverrunSec"], 10, 600),         # overrun 180-600 s
                            "burnerMinRunSec": st(p["burnerMinRunSec"], 10, 600),       # burnerMinRun 120-600 s
                            "burnerMinPauseSec": st(p["burnerMinPauseSec"], 10, 600),   # burnerMinPause 120-600 s
                            "sensorTimeoutSec": st(p["sensorTimeoutSec"], 60, 3600),    # sensorTimeout 60-3600 s
                            "hysteresis": st(p["hysteresis"], 0.1, 1.0),                # hysteresis 0.1-1.0 °C
                            "windowThreshold": st(p["windowThreshold"], 0.1, 2.0)},     # windowThreshold 0.8-2.0 °C
                 "zones": [],
                 "schedule": {k: not s[k] for k in self.FR65_SWITCHES}}
            for z in snap["zones"]:
                e = {"id": z["id"], "offset": st(z["offset"], 0.5, 3.0)}    # sensor offset ±3.0 °C, upward
                if z.get("name"):
                    e["name"] = f"TC29 t{z['id']}" if z["name"] != f"TC29 t{z['id']}" else f"TC29 u{z['id']}"
                f["zones"].append(e)
            f["schedule"]["weekday"] = "06:00=day,22:00=night" if s["weekday"] != "06:00=day,22:00=night" \
                else "06:15=day,22:00=night"
            f["schedule"]["weekend"] = "08:00=day,23:00=night" if s["weekend"] != "08:00=day,23:00=night" \
                else "08:15=day,23:00=night"
            f["schedule"]["levels"] = [{"id": l["id"], **{lv: st(l[lv], 0.5, 30.0) for lv in self.FR65_LEVELS}}
                                       for l in s["levels"]]   # levels 5-30 °C
            if snap.get("location"):
                lat, lon = (float(x) for x in snap["location"].split(","))
                f["location"] = f"{st(lat, 0.01, 90.0):.4f},{lon:.4f}"
            learned = [d for d in snap.get("learnedDuty") or [] if isinstance(d.get("duty"), (int, float))
                       and not isinstance(d.get("duty"), bool) and d["duty"] > 0]
            if learned:
                f["learnedDuty"] = [{"id": d["id"], "duty": round(max(d["duty"] - 1.0, 0.0), 1)} for d in learned]
            r1 = self._fr65_post(f, timed=True)
            t1 = r1.elapsed.total_seconds()
            applied, _, skipped = self._fr65_report(r1)
            assert not skipped, f"settings skipped: {skipped}"
            r2 = self._fr65_post(self._fr65_delta(snap, self._fr65_export()), timed=True)   # every value back
            t2 = r2.elapsed.total_seconds()
            self._fr65_report(r2)
            # FR-65.3: the answer comes within 2 s (elapsed = request sent to answer received, LAN included)
            assert t1 < 2.0 and t2 < 2.0, f"import answered after {t1:.2f} s / {t2:.2f} s, bound 2 s"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(applied)} settings changed in {t1:.2f} s, changed back in {t2:.2f} s (bound 2 s)")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_17_import_unknown_names(self):
        """SWE.5-TC29.17: Unknown Names Skipped as Unknown, the Rest Applied, HTTP 200 (FR-65.4)"""
        test_id = "SWE.5-TC29.17"
        name = "Import Unknown Names"
        req = "FR-65.4"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid = z["id"]
            token = "TC29 unknown" if z["name"] != "TC29 unknown" else "TC29 unknown 2"
            f = {"format": 1, "fooBar": 1, "params": {"fooBar": 1}, "schedule": {"fooBar": 1},
                 "zones": [{"id": zid, "name": token, "fooBar": 1}]}
            applied, _, skipped = self._fr65_report(self._fr65_post(f))   # asserts HTTP 200
            for n in ("fooBar", "params.fooBar", "schedule.fooBar", f"zones[{zid}].fooBar"):
                assert skipped.get(n) == "unknown", f"{n}: {skipped.get(n)!r}, expected skipped 'unknown'"
                assert n not in applied, f"{n} applied"
            # A zone's unknown field is skipped alone: the zone itself is applied
            assert f"zones[{zid}]" in applied, f"zones[{zid}] not applied: {skipped.get(f'zones[{zid}]')!r}"
            exp = self._fr65_export()
            left = self._fr65_diff(exp, snap)
            assert left == [f"zones[{zid}].name"], f"changed {left}, expected only zones[{zid}].name"
            assert (self._fr65_find(exp.get("zones"), zid) or {}).get("name") == token, "zone name not applied"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             "unknown top-level, params, schedule and zone names skipped 'unknown'; the zone applied; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_18_import_missing_kept(self):
        """SWE.5-TC29.18: Settings Missing From the File Keep Their Value and Are Reported Kept (FR-65.4)"""
        test_id = "SWE.5-TC29.18"
        name = "Import Missing Settings Kept"
        req = "FR-65.4"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid = z["id"]
            token = "TC29 partial" if z["name"] != "TC29 partial" else "TC29 partial 2"
            hyst = self._fr65_step(snap["params"]["hysteresis"], 0.1, 1.0)   # hysteresis 0.1-1.0 °C (§6)
            applied, kept, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "params": {"hysteresis": hyst}, "zones": [{"id": zid, "name": token}]}))
            assert sorted(applied) == sorted(["params.hysteresis", f"zones[{zid}]"]), f"applied {applied}"
            assert not skipped, f"skipped {skipped}"
            want = set(self._fr65_names(snap)) - {"params.hysteresis", f"zones[{zid}]"}
            want |= {f"zones[{zid}].{k}" for k in self.FR65_ZONE_KEYS if k not in ("id", "name")}
            assert set(kept) == want, f"kept: missing {sorted(want - set(kept))}, extra {sorted(set(kept) - want)}"
            left = sorted(self._fr65_diff(self._fr65_export(), snap))
            assert left == sorted(["params.hysteresis", f"zones[{zid}].name"]), \
                f"changed {left}, expected only params.hysteresis and zones[{zid}].name"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"2 settings applied, {len(want)} missing ones kept unchanged and reported; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_19_import_invalid_values_kept(self):
        """SWE.5-TC29.19: Failed or Wrongly Typed Values Skipped, Values Stay, HTTP 200 (FR-65.4)"""
        test_id = "SWE.5-TC29.19"
        name = "Import Invalid Values Kept"
        req = "FR-65.4"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            zid = self._fr65_pick(snap, 1, named=True)[0]["id"]
            f = {"format": 1,
                 "params": {"actuatorDelaySec": "190",   # a number as text: wrong JSON type
                            "pumpOverrunSec": 100},      # below overrun 180-600 s
                 "zones": [{"id": zid, "name": 123}],     # name not a text
                 "schedule": {"weekday": 5,               # slot list not a text
                              "levels": [{"id": zid, "day": "21"}]},
                 "location": 12,                          # not a "lat,lon" text
                 "learnedDuty": [{"id": zid, "duty": "5"}]}
            r = self._fr65_post(f)
            assert r.status_code == 200, f"file with failing values: expected HTTP 200, got {r.status_code}"
            applied, _, skipped = self._fr65_report(r)
            names = ("params.actuatorDelaySec", "params.pumpOverrunSec", f"zones[{zid}]", "schedule.weekday",
                     f"schedule.levels[{zid}].day", "location", f"learnedDuty[{zid}]")
            for n in names:
                assert skipped.get(n) == "invalid", f"{n}: {skipped.get(n)!r}, expected skipped 'invalid'"
                assert n not in applied, f"{n} applied"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"failing values changed settings: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"HTTP 200, {len(names)} wrong-type / out-of-range values skipped 'invalid', nothing changed")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_20_import_zone_as_a_whole(self):
        """SWE.5-TC29.20: One Bad Field Skips the Whole Zone; Unknown Zone Ids Skipped (FR-65.5)"""
        test_id = "SWE.5-TC29.20"
        name = "Import Zone As a Whole"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid = z["id"]
            token = "TC29 whole" if z["name"] != "TC29 whole" else "TC29 whole 2"
            # Valid name, offset outside -3.0...+3.0 °C (FR-49): neither is applied
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": zid, "name": token, "offset": 3.5}]}))
            assert skipped.get(f"zones[{zid}]") == "invalid" and f"zones[{zid}]" not in applied, \
                f"zones[{zid}] with a bad offset: {skipped.get(f'zones[{zid}]')!r}, expected skipped 'invalid'"
            # Ids the controller does not have: 0 (ids are 1-based), one past the highest exported id, and 33
            # (FR-69: at most 32 zones)
            past = max(x.get("id") for x in snap["zones"] if isinstance(x.get("id"), int)) + 1
            ids = sorted({0, past, self.FR65_MAX_ZONES + 1})
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": i, "name": token} for i in ids]}))
            for i in ids:
                assert skipped.get(f"zones[{i}]") == "no-zone" and f"zones[{i}]" not in applied, \
                    f"zones[{i}]: {skipped.get(f'zones[{i}]')!r}, expected skipped 'no-zone'"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"skipped zones changed settings: {left}"
            # Without the bad field the same zone is applied: the skip came from the offset alone
            applied, _, skipped = self._fr65_report(self._fr65_post({"format": 1, "zones": [{"id": zid, "name": token}]}))
            assert f"zones[{zid}]" in applied, f"zones[{zid}] with a valid name: {skipped.get(f'zones[{zid}]')!r}"
            assert (self._fr65_find(self._fr65_export().get("zones"), zid) or {}).get("name") == token, "name not applied"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"valid name + bad offset: whole zone skipped; zone ids {ids} skipped 'no-zone'; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_21_import_relay_of_other_zone(self):
        """SWE.5-TC29.21: Zone Given the Relay of a Zone Not in the File Skipped (FR-65.5)"""
        test_id = "SWE.5-TC29.21"
        name = "Import Relay Conflict"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            a, bz = self._fr65_pick(snap, 2, idle=True)
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": a["id"], "relayPin": bz["relayPin"]}]}))
            # A relay output serves at most one zone (FR-11.3)
            na = f"zones[{a['id']}]"
            assert skipped.get(na) == "relay-taken" and na not in applied, \
                f"zone {a['id']} on the relay of zone {bz['id']}: {skipped.get(na)!r}, expected skipped 'relay-taken'"
            cfg = {z.get("id"): z for z in requests.get(f"{self.base_url}/api/config/system",
                                                        timeout=self.timeout).json().get("zones") or []}
            assert cfg[a["id"]].get("relayPin") == a["relayPin"] and cfg[bz["id"]].get("relayPin") == bz["relayPin"], \
                f"relays now {cfg[a['id']].get('relayPin')} / {cfg[bz['id']].get('relayPin')}"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"settings changed: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {a['id']} -> relay {bz['relayPin']} of zone {bz['id']}: skipped 'relay-taken', "
                             "relays unchanged")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_22_import_relay_of_pump(self):
        """SWE.5-TC29.22: Zone on the Relay the Pump Gets From the Same File Skipped (FR-65.5)"""
        test_id = "SWE.5-TC29.22"
        name = "Import Pump Relay Conflict"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            b, to = self.base_url, self.timeout
            pump_pin = requests.get(f"{b}/api/config/system", timeout=to).json().get("pumpRelayPin")
            if any(z.get("relayPin") == pump_pin for z in snap["zones"]):
                raise self.Fr65Skip("needs relay 4 free of zones")
            z = self._fr65_pick(snap, 1, idle=True)[0]
            f = {"format": 1, "zones": [{"id": z["id"], "relayPin": pump_pin}]}
            if not snap["params"].get("pumpControlEnabled"):
                f["params"] = {"pumpControlEnabled": True}   # pumpOutput: none / one relay output (§6)
            applied, _, skipped = self._fr65_report(self._fr65_post(f))
            if "params" in f:
                assert "params.pumpControlEnabled" in applied, \
                    f"pump output (no zone on relay 4 yet) not applied: {skipped.get('params.pumpControlEnabled')!r}"
            # The pump as it results from params: relay 4 is not a zone relay (FR-14.1, FR-11.3)
            nz = f"zones[{z['id']}]"
            assert skipped.get(nz) == "pump-relay" and nz not in applied, \
                f"zone {z['id']} on the pump relay {pump_pin}: {skipped.get(nz)!r}, expected skipped 'pump-relay'"
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            zc = self._fr65_find(cfg.get("zones"), z["id"]) or {}
            assert zc.get("relayPin") == z["relayPin"], f"zone {z['id']} relay now {zc.get('relayPin')}"
            assert cfg.get("pumpControlEnabled") is True, "pump output not on after the import"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"pump on relay {pump_pin} from params; zone {z['id']} to the same relay skipped; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_23_import_conflict_check_repeats(self):
        """SWE.5-TC29.23: Conflict Check Repeats Until No Conflict Is Left (FR-65.5)"""
        test_id = "SWE.5-TC29.23"
        name = "Import Conflict Check Repeats"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            a, bz, c = self._fr65_pick(snap, 3, idle=True)
            if len({a["relayPin"], bz["relayPin"], c["relayPin"]}) != 3:
                raise self.Fr65Skip("needs three zones on three different relays")
            # Zone a -> relay of b, zone b -> relay of c, c not in the file. Pass 1: b and c share c's relay -> b
            # skipped; b stays on its relay. Pass 2: a and b share b's relay -> a skipped. Pass 3: no conflict.
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": a["id"], "relayPin": bz["relayPin"]},
                                        {"id": bz["id"], "relayPin": c["relayPin"]}]}))
            for zz in (a, bz):
                nz = f"zones[{zz['id']}]"
                assert skipped.get(nz) == "relay-taken" and nz not in applied, \
                    f"{nz}: {skipped.get(nz)!r}, expected skipped 'relay-taken' (applied {applied})"
            cfg = {z.get("id"): z for z in requests.get(f"{self.base_url}/api/config/system",
                                                        timeout=self.timeout).json().get("zones") or []}
            now = [cfg[zz["id"]].get("relayPin") for zz in (a, bz, c)]
            assert now == [a["relayPin"], bz["relayPin"], c["relayPin"]], f"relays now {now}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"chain {a['id']}->{bz['id']}->{c['id']}: both zones of the file skipped, relays unchanged")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_24_import_relay_swap(self):
        """SWE.5-TC29.24: Two Zones Swapping Relays in One File Both Applied (FR-65.5)"""
        test_id = "SWE.5-TC29.24"
        name = "Import Relay Swap"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            a, bz = self._fr65_pick(snap, 2, idle=True)
            if a["relayPin"] == bz["relayPin"]:
                raise self.Fr65Skip("needs two zones on different relays")
            # The check is on the configuration that results: after the swap each relay serves one zone
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": a["id"], "relayPin": bz["relayPin"]},
                                        {"id": bz["id"], "relayPin": a["relayPin"]}]}))
            for zz in (a, bz):
                nz = f"zones[{zz['id']}]"
                assert nz in applied, f"{nz} not applied: {skipped.get(nz)!r}"
            cfg = {z.get("id"): z for z in requests.get(f"{self.base_url}/api/config/system",
                                                        timeout=self.timeout).json().get("zones") or []}
            assert cfg[a["id"]].get("relayPin") == bz["relayPin"] and cfg[bz["id"]].get("relayPin") == a["relayPin"], \
                f"relays now {cfg[a['id']].get('relayPin')} / {cfg[bz['id']].get('relayPin')}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zones {a['id']} / {bz['id']} swapped relays {a['relayPin']} / {bz['relayPin']}; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_34_import_sensor_swap(self):
        """SWE.5-TC29.34: Two Zones Exchanging Their Sensors in One File Both Applied (FR-65.5)"""
        test_id = "SWE.5-TC29.34"
        name = "Import Sensor Swap"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            a, bz = self._fr65_pick(snap, 2, idle=True)
            # the real sensors are exchanged: a made-up MAC would make the scanner drop the real sensor's reading
            m1, m2 = str(a.get("mac") or "").upper(), str(bz.get("mac") or "").upper()
            if not m1 or not m2 or m1 == m2:
                raise self.Fr65Skip("needs two switched-off zones with different sensors")
            # A sensor MAC serves one zone; the check is on the configuration that results (FR-11.3)
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": a["id"], "mac": m2}, {"id": bz["id"], "mac": m1}]}))
            for zz in (a, bz):
                nz = f"zones[{zz['id']}]"
                assert nz in applied, f"{nz} not applied: {skipped.get(nz)!r}"
            cfg = {z.get("id"): z for z in requests.get(f"{self.base_url}/api/config/system",
                                                        timeout=self.timeout).json().get("zones") or []}
            got = (str(cfg[a["id"]].get("mac")).upper(), str(cfg[bz["id"]].get("mac")).upper())
            assert got == (m2, m1), f"sensors now {got}, expected {(m2, m1)}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zones {a['id']} / {bz['id']} exchanged their sensors in one file; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_35_import_sensor_of_other_zone(self):
        """SWE.5-TC29.35: Zone Given the Sensor of a Zone Not in the File Skipped With Reason sensor-taken (FR-65.5)"""
        test_id = "SWE.5-TC29.35"
        name = "Import Sensor Conflict"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            a, bz = self._fr65_pick(snap, 2, idle=True)
            m1, m2 = str(a.get("mac") or "").upper(), str(bz.get("mac") or "").upper()
            if not m1 or not m2 or m1 == m2:
                raise self.Fr65Skip("needs two switched-off zones with different sensors")
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": a["id"], "mac": m2}]}))
            na = f"zones[{a['id']}]"
            assert skipped.get(na) == "sensor-taken" and na not in applied,                 f"zone {a['id']} on the sensor of zone {bz['id']}: {skipped.get(na)!r}, expected skipped 'sensor-taken'"
            cfg = {z.get("id"): z for z in requests.get(f"{self.base_url}/api/config/system",
                                                        timeout=self.timeout).json().get("zones") or []}
            got = (str(cfg[a["id"]].get("mac")).upper(), str(cfg[bz["id"]].get("mac")).upper())
            assert got == (m1, m2), f"sensors now {got}, expected {(m1, m2)}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {a['id']} -> sensor of zone {bz['id']}: skipped 'sensor-taken', sensors unchanged")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_25_import_report_lists_each_setting_once(self):
        """SWE.5-TC29.25: Report Lists Every Setting Exactly Once as Applied, Kept or Skipped (FR-65.6)"""
        test_id = "SWE.5-TC29.25"
        name = "Import Report Complete"
        req = "FR-65.6"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            names = set(self._fr65_names(snap))
            info = {"format", "firmwareVersion"}   # file keys, not settings: not checked here

            def lists(r):
                assert r.status_code == 200, f"import: HTTP {r.status_code}"
                d = r.json()
                ap, kp, sk = d.get("applied") or [], d.get("kept") or [], d.get("skipped") or []
                sk_names = [x.get("name") for x in sk if isinstance(x, dict)]
                every = [n for n in ap + kp + sk_names if n not in info]
                dup = sorted({n for n in every if every.count(n) > 1})
                assert not dup, f"listed more than once: {dup}"
                why = {x.get("name"): x.get("reason") for x in sk if isinstance(x, dict)}
                return set(ap) - info, set(kp) - info, set(sk_names) - info, why

            # 1. A file without settings: every setting kept
            ap, kp, sk, _ = lists(self._fr65_post({"format": 1}))
            assert not ap and not sk, f"empty file: applied {sorted(ap)}, skipped {sorted(sk)}"
            assert kp == names, f"empty file, kept: missing {sorted(names - kp)}, extra {sorted(kp - names)}"
            # 2. One applied, one invalid, one unknown name: each in its list, the rest kept
            hyst = self._fr65_step(snap["params"]["hysteresis"], 0.1, 1.0)   # hysteresis 0.1-1.0 °C (§6)
            ap, kp, sk, why = lists(self._fr65_post({"format": 1, "fooBar": 1,
                                                "params": {"hysteresis": hyst, "pumpOverrunSec": 100}}))
            assert ap == {"params.hysteresis"}, f"applied {sorted(ap)}"
            assert sk == {"params.pumpOverrunSec", "fooBar"}, f"skipped {sorted(sk)}"
            assert why == {"params.pumpOverrunSec": "invalid", "fooBar": "unknown"}, f"reasons {why}"
            assert kp == names - {"params.hysteresis", "params.pumpOverrunSec"}, \
                f"kept: missing {sorted(names - ap - sk - kp)}, extra {sorted(kp - names)}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(names)} settings: all kept for an empty file; applied / invalid / unknown / kept "
                             "each once for a mixed one; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_26_import_applied_values_logged(self):
        """SWE.5-TC29.26: Every Applied Value Logged on the Console (FR-65.6)"""
        test_id = "SWE.5-TC29.26"
        name = "Import Applied Values Logged"
        req = "FR-65.6"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, b, to = z["id"], self.base_url, self.timeout
            st = self._fr65_step
            token = f"TC29log{int(time.time()) % 100000}"
            alt = int(st(snap["params"]["actuatorDelaySec"], 10, 600))   # interlockDelay 120-600 s
            wkend = "07:10=day,23:10=night" if snap["schedule"]["weekend"] != "07:10=day,23:10=night" \
                else "07:20=day,23:10=night"
            night = st((self._fr65_find(snap["schedule"]["levels"], zid) or {})["night"], 0.5, 30.0)   # levels 5-30 °C
            f = {"format": 1, "params": {"actuatorDelaySec": alt}, "zones": [{"id": zid, "name": token}],
                 "schedule": {"weekend": wkend, "levels": [{"id": zid, "night": night}]}}
            # entries: report name, alternatives of substrings that must all be on the same line as the name
            want = [("params.actuatorDelaySec", [[str(alt)]]), (f"zones[{zid}]", [[token]]),
                    ("schedule.weekend", [[wkend]]),
                    (f"schedule.levels[{zid}].night", [[f"{night}"], [f"{night:.1f}"], [f"{night:.2f}"]])]
            if snap.get("location"):
                lat, lon = (float(x) for x in snap["location"].split(","))
                lat2 = st(lat, 0.01, 90.0)   # 0.01° (about 1 km): same weather; latitude -90..90 (FR-9.2)
                f["location"] = f"{lat2:.4f},{lon:.4f}"
                want.append(("location", [[f"{lat2:.4f}", f"{lon:.4f}"]]))
            learned = [d for d in snap.get("learnedDuty") or [] if isinstance(d.get("duty"), (int, float))
                       and not isinstance(d.get("duty"), bool) and d["duty"] > 0]
            if learned:   # lowered by 1 % (0-60 %, FR-47); only where the stored duty is above 0 %
                dz, dv = learned[0]["id"], round(max(learned[0]["duty"] - 1.0, 0.5), 1)
                f["learnedDuty"] = [{"id": dz, "duty": dv}]
                want.append((f"learnedDuty[{dz}]", [[f"{dv}"], [f"{dv:.1f}"], [f"{dv:.2f}"]]))
            before = requests.get(f"{b}/api/console/logs?since=0", timeout=to).text.splitlines()
            applied, _, _ = self._fr65_report(self._fr65_post(f))
            for rep_name, _alts in want:
                assert rep_name in applied, f"{rep_name} not applied: {applied}"
            time.sleep(0.2)
            after = requests.get(f"{b}/api/console/logs?since=0", timeout=to).text.splitlines()
            old = {}
            for ln in before:
                old[ln] = old.get(ln, 0) + 1
            new = []
            for ln in after:   # lines that were not in the log before the import
                if old.get(ln, 0) > 0:
                    old[ln] -= 1
                else:
                    new.append(ln)
            # FR-65.6: one line per applied setting with its report name and new value, name and value on one line
            for rep_name, alts in want:
                hits = [ln for ln in new if rep_name in ln and any(all(s in ln for s in alt) for alt in alts)]
                assert len(hits) == 1, \
                    f"console lines with {rep_name} and its value {alts}: {len(hits)}, expected 1; new lines {new[-8:]!r}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"one console line each for {', '.join(n for n, _ in want)}; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_27_engineering_view_import_report(self):
        """SWE.5-TC29.27: Engineering View Offers Export / Import and Shows the Report (FR-65.6)"""
        test_id = "SWE.5-TC29.27"
        name = "Engineering View Import Report"
        req = "FR-65.6"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            # What the page does when used (clicks, rendering) cannot be observed through the API; the served page
            # code is read for the calls and the rendering of the answer fields (a code check, not a browser run).
            scripts = "\n".join(re.findall(r"<script(?![^>]*application/json)[^>]*>(.*?)</script>", html, re.S))
            assert scripts, "no script in the served page"
            call = lambda ep: re.search(r"""(?:fetch|api)\(\s*["'`][^"'`]*config/%s\b""" % ep, scripts)
            exp_call, imp_call = call("export"), call("import")
            assert exp_call, "page code has no fetch / api call of config/export"
            assert imp_call, "page code has no fetch / api call of config/import"
            # Relative URLs, as every request of the page (FR-8.1 proxy rule)
            absolute = re.findall(r"""(?:fetch|api)\(\s*["'`]/[^"'`]*config/(?:export|import)""", scripts)
            assert not absolute, f"root-absolute export / import URLs: {absolute}"
            # FR-8.1 / FR-65.7: the import is sent as a POST with Content-Type application/json, near the call
            near = scripts[imp_call.start():imp_call.start() + 600]
            assert re.search(r"POST", near) and "application/json" in near, \
                "the import call of the page does not send POST with Content-Type application/json"
            # FR-8.1: the export is saved as a .json file
            assert re.search(r"\.json\b", scripts), "page code never names a .json file for the export"
            # FR-65.6: the code reads and renders the three lists and the reason of each skipped entry
            for field in ("applied", "kept", "skipped", "reason"):
                assert re.search(r"""(?:\.\s*%s\b|\[\s*["']%s["']\s*\])""" % (field, field), scripts), \
                    f"page code never reads the answer field '{field}' of the import report"
            return TestResult(test_id, name, req, "PASS", latency,
                              "page code calls config/export and config/import (relative, POST, application/json), "
                              "saves .json, reads applied / kept / skipped / reason")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc29_38_pre_import_diff_preview_and_confirmation(self):
        """SWE.5-TC29.38: Served Page Offers Pre-Import Diff Preview and Confirmation Dialog (FR-8.1, FR-65.7)"""
        test_id = "SWE.5-TC29.38"
        name = "Pre-Import Diff Preview and Confirmation"
        req = "FR-8.1"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/", timeout=self.timeout)
            latency = (time.time() - start) * 1000
            assert r.status_code == 200, f"page: HTTP {r.status_code}"
            html = r.text
            assert "cfgPreview" in html, "Missing #cfgPreview container in served page"
            scripts = "\n".join(re.findall(r"<script(?![^>]*application/json)[^>]*>(.*?)</script>", html, re.S))
            assert scripts, "no script in served page"
            assert "analyzeImport" in scripts, "Missing analyzeImport logic in served page script"
            assert "btnConfirmImport" in scripts, "Missing confirmation button in import preview"
            assert "btnCancelImport" in scripts, "Missing cancel button in import preview"
            # Section 6 bounds check in PARAM_LIMITS:
            # rfSafetyHeartbeatSec must allow up to 900 s (hbInterval 30..900 s, FR-17.7, FR-55)
            hb_lim = re.search(r"rfSafetyHeartbeatSec\s*:\s*\{[^}]*max\s*:\s*(\d+)", scripts)
            assert hb_lim and int(hb_lim.group(1)) >= 900, f"rfSafetyHeartbeatSec max bound too low: {hb_lim.group(0) if hb_lim else 'none'}"
            # pumpOverrunSec must enforce min 180 s (overrun 180..600 s, FR-3)
            ovr_lim = re.search(r"pumpOverrunSec\s*:\s*\{[^}]*min\s*:\s*(\d+)", scripts)
            assert ovr_lim and int(ovr_lim.group(1)) == 180, f"pumpOverrunSec min bound wrong: {ovr_lim.group(0) if ovr_lim else 'none'}"
            # burnerMinRunSec must enforce min 120 s (burnerMinRun 120..600 s, FR-4)
            brun_lim = re.search(r"burnerMinRunSec\s*:\s*\{[^}]*min\s*:\s*(\d+)", scripts)
            assert brun_lim and int(brun_lim.group(1)) == 120, f"burnerMinRunSec min bound wrong: {brun_lim.group(0) if brun_lim else 'none'}"
            return TestResult(test_id, name, req, "PASS", latency,
                              "served page has #cfgPreview, analyzeImport, confirmation dialog and §6 parameter bounds")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc29_28_import_rejects_not_format_1(self):
        """SWE.5-TC29.28: Not a JSON Object or Format Missing / Not 1 Gives HTTP 400, Nothing Changed (FR-65.7)"""
        test_id = "SWE.5-TC29.28"
        name = "Import Rejects Bad File (400)"
        req = "FR-65.7"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            hyst = self._fr65_step(snap["params"]["hysteresis"], 0.1, 1.0)   # a valid change, hysteresis 0.1-1.0 °C
            change = {"params": {"hysteresis": hyst}}
            bodies = [(b"not json", "text"), (b"[1, 2]", "array"), (b'"settings"', "string"), (b"42", "number"),
                      (json.dumps({"format": 1, **change}).encode("utf-8")[:-3], "cut JSON"),
                      (json.dumps(change).encode("utf-8"), "format missing"),
                      (json.dumps({"format": 2, **change}).encode("utf-8"), "format 2"),
                      (json.dumps({"format": 0, **change}).encode("utf-8"), "format 0"),
                      (json.dumps({"format": None, **change}).encode("utf-8"), "format null"),
                      (json.dumps({"format": "1", **change}).encode("utf-8"), "format as text"),   # not the number 1
                      (json.dumps({"format": True, **change}).encode("utf-8"), "format true")]
            for raw, what in bodies:
                r = self._fr65_post(raw)
                assert r.status_code == 400, f"{what}: expected HTTP 400, got {r.status_code}"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"rejected files changed settings: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency, f"{len(bodies)} bad files: HTTP 400, nothing changed")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_29_import_size_bound(self):
        """SWE.5-TC29.29: Body Over the Size Bound Gives HTTP 413, Body at the Bound Accepted (FR-65.7)"""
        test_id = "SWE.5-TC29.29"
        name = "Import Size Bound (413)"
        req = "FR-65.7"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            hyst = self._fr65_step(snap["params"]["hysteresis"], 0.1, 1.0)   # a valid change, hysteresis 0.1-1.0 °C
            body = json.dumps({"format": 1, "params": {"hysteresis": hyst}}).encode("utf-8")
            over = body + b" " * (self.FR65_MAX_BODY + 1 - len(body))   # trailing spaces keep it valid JSON
            at = body + b" " * (self.FR65_MAX_BODY - len(body))
            assert len(over) == self.FR65_MAX_BODY + 1 and len(at) == self.FR65_MAX_BODY, "test bodies wrong size"
            r = self._fr65_post(over)
            assert r.status_code == 413, f"{len(over)} bytes: expected HTTP 413, got {r.status_code}"
            # Size is checked before the content: an oversized body that is not JSON also gets 413, not 400
            junk = b"x" * (self.FR65_MAX_BODY + 1)
            r = self._fr65_post(junk)
            assert r.status_code == 413, f"{len(junk)} bytes, not JSON: expected HTTP 413 (before 400), got {r.status_code}"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"rejected body changed settings: {left}"
            applied, _, skipped = self._fr65_report(self._fr65_post(at))   # asserts HTTP 200
            assert "params.hysteresis" in applied, f"{len(at)}-byte body not applied: {skipped}"
            assert not self._fr65_diff(self._fr65_export()["params"].get("hysteresis"), hyst), "hysteresis not applied"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(over)} bytes: HTTP 413, nothing changed; {len(at)} bytes accepted; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_30_import_content_type(self):
        """SWE.5-TC29.30: Content-Type Other Than application/json Gives HTTP 415, Nothing Changed (FR-65.7)"""
        test_id = "SWE.5-TC29.30"
        name = "Import Content-Type (415)"
        req = "FR-65.7"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            hyst = self._fr65_step(snap["params"]["hysteresis"], 0.1, 1.0)   # a valid change, hysteresis 0.1-1.0 °C
            body = json.dumps({"format": 1, "params": {"hysteresis": hyst}}).encode("utf-8")
            # The two types a page from another origin can send without asking first
            for ct in ("text/plain", "application/x-www-form-urlencoded"):
                r = self._fr65_post(body, content_type=ct)
                assert r.status_code == 415, f"Content-Type {ct}: expected HTTP 415, got {r.status_code}"
            # The type is checked before size and content: oversized, not JSON, wrong type -> 415
            r = self._fr65_post(b"x" * (self.FR65_MAX_BODY + 1), content_type="text/plain")
            assert r.status_code == 415, f"wrong type and oversized: expected HTTP 415 (before 413), got {r.status_code}"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"rejected bodies changed settings: {left}"
            # A charset parameter is accepted: the same body is applied, so the 415 came from the type alone
            applied, _, skipped = self._fr65_report(self._fr65_post(body, content_type="application/json; charset=utf-8"))
            assert "params.hysteresis" in applied, f"same body as application/json; charset=utf-8 not applied: {skipped}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             "text/plain and form type: HTTP 415 (also before 413), nothing changed; "
                             "application/json; charset=utf-8 applied; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_31_no_cross_origin(self):
        """SWE.5-TC29.31: Export and Import Send No Access-Control-Allow-Origin Header (FR-65.7)"""
        test_id = "SWE.5-TC29.31"
        name = "Export / Import Not Cross-Origin"
        req = "FR-65.7"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            b, to = self.base_url, self.timeout
            cors = ("Access-Control-Allow-Origin", "Access-Control-Allow-Headers")
            checked = 0
            for origin in ("http://example.com", "null", f"http://{self.ip}"):   # whatever the Origin (FR-65.7)
                other = {"Origin": origin}
                answers = [("GET export", requests.get(f"{b}/api/config/export", headers=other, timeout=to)),
                           ("POST import", self._fr65_post({"format": 1}, headers=other))]   # no settings: no change
                for path, method in (("/api/config/export", "GET"), ("/api/config/import", "POST")):   # pre-flight
                    time.sleep(0.1)
                    answers.append((f"OPTIONS {path}", requests.options(
                        f"{b}{path}", timeout=to, headers={**other, "Access-Control-Request-Method": method,
                                                           "Access-Control-Request-Headers": "content-type",
                                                           "Connection": "close"})))
                for what, r in answers:
                    for h in cors:
                        assert h not in r.headers, f"{what} (Origin {origin}) answers {h}: {r.headers.get(h)!r}"
                    checked += 1
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{checked} answers (GET, POST, OPTIONS x 3 origins) without {' / '.join(cors)}")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_32_import_duplicate_ids(self):
        """SWE.5-TC29.32: The Same Id Twice Skips Every Entry With It as Duplicate (FR-65.5)"""
        test_id = "SWE.5-TC29.32"
        name = "Import Duplicate Ids"
        req = "FR-65.5"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            z = self._fr65_pick(snap, 1, named=True)[0]
            zid, st = z["id"], self._fr65_step
            day = (self._fr65_find(snap["schedule"]["levels"], zid) or {})["day"]
            f = {"format": 1,
                 "zones": [{"id": zid, "name": "TC29 dup a"}, {"id": zid, "name": "TC29 dup b"}],
                 # both values valid, inside the level range 5-30 °C (FR-29)
                 "schedule": {"levels": [{"id": zid, "day": st(day, 0.5, 30.0)}, {"id": zid, "day": st(day, 1.0, 30.0)}]}}
            want = [f"zones[{zid}]", f"schedule.levels[{zid}].day"]
            # learnedDuty only for a zone surely learned (> 0 %), both entries with the stored value: nothing can change
            learned = self._fr65_find(snap.get("learnedDuty"), zid) or {}
            if isinstance(learned.get("duty"), (int, float)) and not isinstance(learned.get("duty"), bool) \
                    and learned["duty"] > 0:
                f["learnedDuty"] = [{"id": zid, "duty": learned["duty"]}, {"id": zid, "duty": learned["duty"]}]
                want.append(f"learnedDuty[{zid}]")
            applied, _, skipped = self._fr65_report(self._fr65_post(f))
            for n in want:
                assert skipped.get(n) == "duplicate" and n not in applied, \
                    f"{n} twice in the file: {skipped.get(n)!r}, expected skipped 'duplicate'"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"duplicate entries changed settings: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{', '.join(want)} twice: skipped 'duplicate', nothing changed"
                             f"{'' if len(want) == 3 else ' (learnedDuty not sent: no learned duty above 0 %)'}")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_36_target_not_in_file(self):
        """SWE.5-TC29.36: The Current Zone Target Is Not in the Export and a Target in a File Is Skipped as Unknown (FR-65.1, FR-65.4)"""
        test_id = "SWE.5-TC29.36"
        name = "Target Not in File"
        req = "FR-65.1, FR-65.4"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            for z in self._fr65_export().get("zones") or []:
                assert "target" not in z, f"zones[{z.get('id')}] holds target {z.get('target')!r}"
            zid = self._fr65_pick(snap, 1)[0]["id"]
            applied, _, skipped = self._fr65_report(self._fr65_post(
                {"format": 1, "zones": [{"id": zid, "target": 25.0}]}))
            n = f"zones[{zid}].target"
            assert skipped.get(n) == "unknown" and n not in applied, f"{n}: {skipped.get(n)!r}, expected skipped 'unknown'"
            left = self._fr65_diff(self._fr65_export(), snap)
            assert not left, f"settings changed: {left}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             "no zone has a target in the export; a target in a file is skipped 'unknown', nothing changed")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_37_levels_restored_from_export(self):
        """SWE.5-TC29.37: Day, Night and Away Setpoints of a Zone Are Restored by Importing an Earlier Export (FR-65.1, FR-65.3)"""
        test_id = "SWE.5-TC29.37"
        name = "Setpoints Restored From Export"
        req = "FR-65.1, FR-65.3"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            zid, b, to, st = self._fr65_pick(snap, 1)[0]["id"], self.base_url, self.timeout, self._fr65_step
            old = self._fr65_find(snap["schedule"]["levels"], zid) or {}
            mine = {lv: st(old[lv], 0.5, 30.0) for lv in ("day", "night", "away")}    # levels 5-30 °C (FR-29)
            for lv, v in mine.items():
                r = requests.post(f"{b}/api/schedule/level", data={"zone": zid, "level": lv, "temp": v}, timeout=to)
                assert r.status_code == 200, f"schedule/level {lv}: HTTP {r.status_code}"
            earlier = self._fr65_export()                  # the file the owner keeps
            for lv in mine:                                # a test or a reflash overwrites the setpoints
                requests.post(f"{b}/api/schedule/level", data={"zone": zid, "level": lv, "temp": old[lv]}, timeout=to)
            now = self._fr65_find(self._fr65_export()["schedule"]["levels"], zid) or {}
            assert all(not self._fr65_diff(now[lv], old[lv]) for lv in mine), f"levels were not overwritten: {now}"
            applied, _, skipped = self._fr65_report(self._fr65_post(earlier))
            for lv in mine:
                n = f"schedule.levels[{zid}].{lv}"
                assert n in applied, f"{n} not applied: {skipped.get(n)!r}"
            got = self._fr65_find(self._fr65_export()["schedule"]["levels"], zid) or {}
            for lv, v in mine.items():
                assert not self._fr65_diff(got.get(lv), v), f"zone {zid} {lv} {got.get(lv)!r} after import, expected {v}"
            sch = (requests.get(f"{b}/api/schedule", timeout=to).json().get("zones") or [])[zid - 1]
            for lv, v in mine.items():
                assert not self._fr65_diff(sch.get(lv), v), f"/api/schedule zone {zid} {lv} {sch.get(lv)!r}, expected {v}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"zone {zid} day / night / away {mine} restored from an earlier export; restored")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res

    def test_tc29_33_round_trip(self):
        """SWE.5-TC29.33: Unchanged Export Imported With Nothing Skipped, Next Export Identical (FR-65.8)"""
        test_id = "SWE.5-TC29.33"
        name = "Export / Import Round Trip"
        req = "FR-65.8"
        start = time.time()
        snap, res = None, None
        try:
            snap = self._fr65_begin()
            # A learned duty of exactly 0 % may stand for "nothing learned" (FR-51 then heats with faultDuty); sent
            # back it could become a learned 0 % that no import undoes - not risked on the live board (report)
            zero = [d.get("id") for d in snap.get("learnedDuty") or [] if d.get("duty") == 0]
            if zero:
                raise self.Fr65Skip(f"learnedDuty 0 % for zone(s) {zero}: may be 'nothing learned', not sent back")
            r0 = requests.get(f"{self.base_url}/api/config/export", timeout=self.timeout)
            assert r0.status_code == 200, f"GET /api/config/export: HTTP {r0.status_code}"
            applied, kept, skipped = self._fr65_report(self._fr65_post(r0.content))   # the body unchanged
            assert not skipped, f"unchanged export: skipped {skipped}"
            r1 = requests.get(f"{self.base_url}/api/config/export", timeout=self.timeout)
            assert r1.status_code == 200, f"second export: HTTP {r1.status_code}"
            assert r1.content == r0.content, \
                f"export after the import differs: {self._fr65_diff(json.loads(r1.content), json.loads(r0.content))[:6]}"
            latency = (time.time() - start) * 1000
            res = TestResult(test_id, name, req, "PASS", latency,
                             f"{len(r0.content)}-byte export imported: {len(applied)} applied, {len(kept)} kept, "
                             "none skipped; next export byte-identical")
        except Exception as e:
            res = self._fr65_error(test_id, name, req, start, e)
        finally:
            res = self._fr65_restore(snap, res)
        return res
