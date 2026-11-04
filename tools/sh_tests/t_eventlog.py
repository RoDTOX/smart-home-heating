"""System tests of PB-121 (event log of safety and I/O, FR-62.1, C-37).

Written from the requirement text FR-62.1 and requirements section 6. Every test leaves the board in AUTO,
the boiler OFF and the valves closed. Frost and sensor-loss entries need a fault injector (PB-116) and are
not tested here.
"""
import re
from datetime import datetime

from .base import *  # noqa: F401,F403


class EventLogTests:
    ET_TYPES = ["out", "rf", "refused", "pending", "interlock", "overrun", "frost", "sensor", "mode",
                "boot", "warmup", "time-sync", "dtc", "dropped"]
    ET_REASONS = ["none", "boot", "demand", "no-demand", "manual", "frost", "sensor-loss", "overrun-end",
                  "exercise", "config", "safety", "user", "queue", "heartbeat", "guard-run", "guard-pause",
                  "guard-cooldown", "interlock", "demand-lost", "pump-guard", "wifi"]

    def _el_get(self, n=200):
        r = requests.get(f"{self.base_url}/api/diag/log?n={n}", timeout=self.timeout)
        assert r.status_code == 200, f"/api/diag/log HTTP {r.status_code}"
        return r.json()

    @staticmethod
    def _el_epoch(e):
        """Epoch seconds of an entry with a valid time (`yymmddhhmmss`, local time, C-38)."""
        return datetime.strptime(e["time"], "%y%m%d%H%M%S").timestamp()

    def _el_new(self, before, n=400):
        """Entries written after `before` (a list taken earlier, newest first), oldest first."""
        now = self._el_get(n)["events"]
        keep = len(now)
        if before:
            # the entries of `before` are the tail of `now`: cut at the first entry equal to before[0]
            for i in range(len(now)):
                if now[i:i + min(3, len(before))] == before[:min(3, len(before))]:
                    keep = i
                    break
        return list(reversed(now[:keep]))

    def _el_pins(self):
        cfg = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
        return [(z["relayPin"], z.get("id", i + 1)) for i, z in enumerate(cfg["zones"])]

    def test_tc40_1_log_format(self):
        """SWE.5-TC40.1: Event log answer has the fields of FR-62.1, newest first, n limits the count (FR-62.1)"""
        test_id, name, req = "SWE.5-TC40.1", "Event Log Format", "FR-62.1"
        start = time.time()
        try:
            data = self._el_get(5)
            for key in ("count", "stored", "queued", "events"):
                assert key in data, f"no '{key}' in {list(data)}"
            ev = data["events"]
            assert len(ev) <= 5 and data["count"] == len(ev), f"n=5 gave count {data['count']} / {len(ev)} events"
            assert data["stored"] >= 0 and data["queued"] >= 0
            big = self._el_get(9999)
            assert len(big["events"]) <= 4096, f"{len(big['events'])} entries, the maximum of n is 4096"
            for e in big["events"][:60]:
                for key in ("time", "timeValid", "type", "subject", "zone", "reason", "old", "new",
                            "boiler", "overrun", "pending"):
                    assert key in e, f"entry without '{key}': {e}"
                assert re.fullmatch(r"\d{12}" if e["timeValid"] else r"\d+", e["time"]), f"time {e['time']} (C-38)"
                assert e["type"] in self.ET_TYPES, f"unknown type {e['type']}"
                assert e["reason"] in self.ET_REASONS, f"unknown reason {e['reason']}"
                assert e["pending"] in ("none", "off", "on"), f"pending {e['pending']}"
                assert isinstance(e["boiler"], bool) and isinstance(e["overrun"], bool)
                if e["type"] == "dtc":
                    assert str(e.get("code", "")).startswith("DTC-"), f"dtc entry without code: {e}"
            valid = [e["time"] for e in big["events"] if e["timeValid"]]
            assert valid == sorted(valid, reverse=True), "entries with a valid time are not newest first"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"{len(big['events'])} entries, stored {big['stored']}, queued {big['queued']}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc40_2_relay_changes_logged(self):
        """SWE.5-TC40.2: Every relay level change is an 'out' entry with pin, zone, old and new level, reason, time (FR-62.1)"""
        test_id, name, req = "SWE.5-TC40.2", "Relay Level Changes Logged", "FR-62.1"
        start = time.time()
        b, to = self.base_url, self.timeout
        try:
            pins = self._el_pins()
            assert pins, "no zone on the board"
            pin, zone = pins[0]
            requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            time.sleep(1.5)
            before = self._el_get(400)["events"]
            t0 = time.time()
            requests.post(f"{b}/api/relay/test?pin={pin}&state=1", timeout=to)
            time.sleep(1.5)
            requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            time.sleep(1.5)
            new = [e for e in self._el_new(before) if e["type"] == "out" and e["subject"] == pin]
            assert len(new) == 2, f"{len(new)} 'out' entries for GPIO{pin} after one ON and one OFF, expected 2: {new}"
            on, off = new
            assert (on["old"], on["new"]) == (0, 1), f"ON entry old/new {on['old']}/{on['new']}, expected 0/1"
            assert (off["old"], off["new"]) == (1, 0), f"OFF entry old/new {off['old']}/{off['new']}, expected 1/0"
            assert on["zone"] == zone, f"zone {on['zone']}, expected {zone}"
            assert on["reason"] == "manual", f"reason '{on['reason']}', expected 'manual'"
            if on["timeValid"]:
                assert t0 - 5 <= self._el_epoch(on) <= time.time() + 5, f"time {on['time']} is not now"
            # an unchanged level writes no entry
            before2 = self._el_get(400)["events"]
            requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            time.sleep(1.5)
            again = [e for e in self._el_new(before2) if e["type"] == "out" and e["subject"] == pin]
            assert not again, f"an unchanged level was logged: {again}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"GPIO{pin}: 0->1 and 1->0, zone {zone}, reason manual, no entry for an unchanged level")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            requests.post(f"{b}/api/mode?mode=auto", timeout=to)

    def test_tc40_3_mode_change_logged(self):
        """SWE.5-TC40.3: A mode change is a 'mode' entry, old and new = 1 AUTO / 0 MANUAL (FR-62.1)"""
        test_id, name, req = "SWE.5-TC40.3", "Mode Change Logged", "FR-62.1"
        start = time.time()
        b, to = self.base_url, self.timeout
        try:
            requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            time.sleep(1.5)
            before = self._el_get(400)["events"]
            requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            time.sleep(1.5)
            requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            time.sleep(1.5)
            modes = [e for e in self._el_new(before) if e["type"] == "mode"]
            assert len(modes) == 2, f"{len(modes)} 'mode' entries, expected 2: {modes}"
            assert (modes[0]["old"], modes[0]["new"]) == (1, 0), f"AUTO->MANUAL logged as {modes[0]['old']}/{modes[0]['new']}"
            assert (modes[1]["old"], modes[1]["new"]) == (0, 1), f"MANUAL->AUTO logged as {modes[1]['old']}/{modes[1]['new']}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "AUTO->MANUAL->AUTO as 1/0 and 0/1")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            requests.post(f"{b}/api/mode?mode=auto", timeout=to)

    def test_tc40_4_refusals_dtc_and_batching(self):
        """SWE.5-TC40.4: Refused ON is a 'refused' entry (interlock), repeats within 10 s are one, DTC raise and clear are 'dtc' entries, queued entries reach the file within 30 s (FR-62.1, FR-2, FR-10.2)"""
        test_id, name, req = "SWE.5-TC40.4", "Refusals, DTC and Batching", "FR-62.1, FR-2, FR-10.2"
        start = time.time()
        b, to = self.base_url, self.timeout
        try:
            requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            requests.post(f"{b}/api/boiler/cancel", timeout=to)
            requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
            for pin, _ in self._el_pins():
                requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            time.sleep(3)
            requests.post(f"{b}/api/diag/clear", timeout=to)
            time.sleep(1.5)
            before = self._el_get(400)["events"]
            for _ in range(5):  # no valve is open: refused (FR-2), and 5 rejected requests raise DTC-201
                requests.post(f"{b}/api/boiler/on", timeout=to)
                time.sleep(0.08)
            requests.post(f"{b}/api/boiler/cancel", timeout=to)
            requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
            time.sleep(1.5)
            requests.post(f"{b}/api/diag/clear", timeout=to)
            time.sleep(1.5)
            new = self._el_new(before)
            ref = [e for e in new if e["type"] == "refused"]
            assert ref, "no 'refused' entry for a boiler ON without an open valve"
            assert len([e for e in ref if e["reason"] == "interlock"]) == 1, \
                f"5 identical refusals within 10 s must be one entry: {ref}"
            assert ref[0]["reason"] == "interlock" and ref[0]["new"] == 1, f"refusal entry: {ref[0]}"
            dtc = [e for e in new if e["type"] == "dtc" and e.get("code") == "DTC-201"]
            assert [e["new"] for e in dtc][:1] == [1] and 0 in [e["new"] for e in dtc], f"DTC-201 raise / clear entries: {dtc}"
            # batching: within 30 s (+ margin) the queue is written to the file
            time.sleep(34)
            d = self._el_get(5)
            assert d["queued"] == 0, f"{d['queued']} entries still queued 34 s after the last event"
            assert d["stored"] >= len(new), f"stored {d['stored']} is below the {len(new)} new entries"
            assert d["stored"] <= 4096, f"stored {d['stored']} exceeds 2 x 2048"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"refused once (interlock), DTC-201 raised and cleared, queued 0, stored {d['stored']}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            try:
                requests.post(f"{b}/api/boiler/cancel", timeout=to)
                requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
                requests.post(f"{b}/api/mode?mode=auto", timeout=to)
                requests.post(f"{b}/api/diag/clear", timeout=to)
            except Exception:
                pass

    def test_tc40_5_full_burn_logged(self):
        """SWE.5-TC40.5: A full burn shows valve open, interlock start and end, boiler ON, guard refusal, overrun hold and end, valve close, with times and the boiler-side state (FR-62.1, FR-2, FR-3, FR-4)"""
        test_id, name, req = "SWE.5-TC40.5", "Full Burn Logged", "FR-62.1, FR-2, FR-3, FR-4"
        start = time.time()
        b, to = self.base_url, self.timeout
        saved = None
        try:
            cfg = requests.get(f"{b}/api/config/system", timeout=to).json()
            saved = {k: cfg[k] for k in ("actuatorDelaySec", "pumpOverrunSec", "burnerMinRunSec")}
            r = requests.post(f"{b}/api/config/system", data={"actuatorDelaySec": "120", "pumpOverrunSec": "180",
                                                              "burnerMinRunSec": "120"}, timeout=to)
            assert r.status_code == 200, f"config: HTTP {r.status_code}"
            pin, zone = self._el_pins()[0]
            requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            requests.post(f"{b}/api/boiler/cancel", timeout=to)
            if requests.get(f"{b}/api/boiler/status", timeout=to).json().get("boiler") != "OFF":
                requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
            requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            time.sleep(3)
            before = self._el_get(400)["events"]
            requests.post(f"{b}/api/relay/test?pin={pin}&state=1", timeout=to)
            t_valve = time.time()
            time.sleep(122)
            r = requests.post(f"{b}/api/boiler/on", timeout=to)
            assert r.status_code in (200, 429), f"ON: HTTP {r.status_code}"
            deadline = time.time() + 240
            while time.time() < deadline and requests.get(f"{b}/api/boiler/status", timeout=to).json().get("boiler") != "ON":
                time.sleep(3)
            assert requests.get(f"{b}/api/boiler/status", timeout=to).json().get("boiler") == "ON", "boiler did not start"
            requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)  # last valve asked closed: held (FR-3)
            time.sleep(4)
            requests.post(f"{b}/api/boiler/off", timeout=to)  # inside burnerMinRun: refused / queued (FR-4)
            deadline = time.time() + 200
            while time.time() < deadline and requests.get(f"{b}/api/boiler/status", timeout=to).json().get("boiler") != "OFF":
                time.sleep(3)
            assert requests.get(f"{b}/api/boiler/status", timeout=to).json().get("boiler") == "OFF", "boiler did not stop"
            time.sleep(198)  # overrun 180 s
            time.sleep(3)
            new = self._el_new(before, 600)
            assert new, "no entries for a whole burn"

            def find(pred, label):
                hits = [e for e in new if pred(e)]
                assert hits, f"no entry for: {label}"
                return hits[0]
            v_open = find(lambda e: e["type"] == "out" and e["subject"] == pin and e["new"] == 1, "valve relay energised")
            assert v_open["zone"] == zone and v_open["reason"] == "manual", f"valve open entry: {v_open}"
            i_start = find(lambda e: e["type"] == "interlock" and e["zone"] == zone and e["new"] == 1, "interlock start")
            assert i_start["old"] == 120, f"interlock start carries the delay {i_start['old']}, expected 120"
            i_end = find(lambda e: e["type"] == "interlock" and e["zone"] == zone and e["new"] == 2, "interlock end")
            assert 118 <= i_end["old"] <= 130, f"interlock end after {i_end['old']} s, expected about 120"
            rf_on = find(lambda e: e["type"] == "rf" and e["new"] == 1 and e["reason"] != "heartbeat", "boiler ON frame")
            assert rf_on["old"] == 0, f"ON frame old {rf_on['old']}"
            find(lambda e: e["type"] == "overrun" and e["new"] == 3, "valve close held (overrun hold)")
            rf_off = find(lambda e: e["type"] == "rf" and e["new"] == 0 and e["reason"] != "heartbeat", "boiler OFF frame")
            o_start = find(lambda e: e["type"] == "overrun" and e["new"] == 1, "overrun start")
            assert o_start["old"] == 180, f"overrun start carries {o_start['old']} s, expected 180"
            o_end = find(lambda e: e["type"] == "overrun" and e["new"] == 2, "overrun end")
            v_close = find(lambda e: e["type"] == "out" and e["subject"] == pin and e["new"] == 0, "valve relay de-energised")
            assert v_close["reason"] == "overrun-end", f"valve close reason '{v_close['reason']}', expected 'overrun-end'"
            assert v_close["boiler"] is False, "valve close entry says the boiler was ON"
            ref = [e for e in new if e["type"] == "refused" or (e["type"] == "pending" and e["new"] == 1)]
            assert ref, "OFF inside burnerMinRun left no 'refused' or 'pending' entry (FR-4)"
            # order and time: valve, interlock end, ON, hold, OFF, overrun, close
            seq = [new.index(x) for x in (v_open, i_end, rf_on, rf_off, o_end, v_close)]
            assert seq == sorted(seq), f"entries out of order: {seq}"
            if v_open["timeValid"] and v_close["timeValid"]:
                assert self._el_epoch(v_close) - self._el_epoch(v_open) >= 120 + 120 + 180, "valve cycle shorter than interlock + run + overrun"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"{len(new)} entries; interlock {i_end['old']} s, overrun {o_start['old']} s, valve closed with reason overrun-end")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            try:
                requests.post(f"{b}/api/mode?mode=manual", timeout=to)
                requests.post(f"{b}/api/boiler/cancel", timeout=to)
                requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
                for pin_, _ in self._el_pins():
                    requests.post(f"{b}/api/relay/test?pin={pin_}&state=0", timeout=to)
                if saved:
                    requests.post(f"{b}/api/config/system", data=saved, timeout=to)
            except Exception:
                pass
            try:
                requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            except Exception:
                pass

    def test_tc40_6_boot_and_time_sync_logged(self):
        """SWE.5-TC40.6: The log holds the last boot (boot entry, relays driven closed, boiler OFF) and the time-sync entry with the uptime (FR-62.1, NFR-1.4)"""
        test_id, name, req = "SWE.5-TC40.6", "Boot and Time Sync Logged", "FR-62.1, NFR-1.4"
        start = time.time()
        try:
            data = self._el_get(4096)
            ev = data["events"]
            boots = [i for i, e in enumerate(ev) if e["type"] == "boot"]
            if not boots:
                if data["stored"] >= 2048:
                    return TestResult(test_id, name, req, "SKIP", (time.time() - start) * 1000,
                                      "the last boot was rotated out of the log (2048 or more entries since)")
                raise AssertionError("no 'boot' entry in the log")
            i0 = boots[0]
            after_boot = list(reversed(ev[:i0]))
            outs = [e for e in after_boot[:40] if e["type"] == "out" and e["reason"] == "boot"]
            pins = {p for p, _ in self._el_pins()}
            assert pins and pins <= {e["subject"] for e in outs}, \
                f"relays {sorted(pins)} not all driven closed at boot: {[e['subject'] for e in outs]}"
            assert all(e["new"] == 0 for e in outs if e["subject"] in pins), "a relay was not driven to level 0 at boot"
            rf = [e for e in after_boot[:40] if e["type"] == "rf"]
            assert rf and rf[0]["new"] == 0 and rf[0]["reason"] == "boot", f"no boiler OFF frame at boot: {rf[:1]}"
            sync = [e for e in after_boot if e["type"] == "time-sync"]
            assert sync, "no 'time-sync' entry after the boot"
            assert sync[0]["timeValid"], "the time-sync entry has no valid time"
            up = (sync[0]["old"] << 16) | sync[0]["new"]
            assert 0 < up < 3600, f"time-sync uptime {up} s is not plausible"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"boot, {len(outs)} relay entries, boiler OFF, clock valid after {up} s")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
