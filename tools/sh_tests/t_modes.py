"""System tests of migration step 8 (ModeManager + Schedule): MANUAL timeout FR-52, season FR-12.2, global OFF FR-64, boost limits FR-1.2 / FR-39.

Written from the requirement text and requirements section 6 only. Status fields read from GET /api/status:
`effectiveMode` AUTO | MANUAL | OFF, `manualRemainingSec`, `seasonMode` AUTO | ON | OFF, `seasonState` HEATING | SUMMER.
Season mode is set with POST /api/season?mode=auto|on|off. A new previous-day mean is fed with the development hook
POST /api/dev/season-day?mean=X (or mean=none for a day without weather data; only in the dev-open build).
Every test leaves the board in AUTO with the season mode and state it had. Not testable on the board in reasonable time
(reported, no test): the end of MANUAL after manualTimeout (at least 1 h), the end of a boost after boostDuration (at
least 1 h), the 12 h safety net of the season, the last schedule level without valid time (FR-61, the board has NTP),
frost protection in global OFF (needs a reading below frostTemp, which would start the boiler).
"""
from .base import *  # noqa: F401,F403


class ModeTests:
    def _md_status(self):
        r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
        assert r.status_code == 200, f"/api/status: HTTP {r.status_code}"
        return r.json()

    def _md_season(self, mode):
        r = requests.post(f"{self.base_url}/api/season?mode={mode}", timeout=self.timeout)
        assert r.status_code == 200, f"/api/season?mode={mode}: HTTP {r.status_code}"

    def _md_day(self, mean):
        r = requests.post(f"{self.base_url}/api/dev/season-day?mean={mean}", timeout=self.timeout)
        assert r.status_code == 200, f"/api/dev/season-day?mean={mean}: HTTP {r.status_code}"

    def _md_force_state(self, state):
        """season mode ON -> HEATING, OFF -> SUMMER, then AUTO keeps the state."""
        self._md_season("on" if state == "HEATING" else "off")
        assert self._md_status()["seasonState"] == state, f"season mode forced, state is not {state}"
        self._md_season("auto")

    def _md_restore(self, saved):
        try:
            requests.post(f"{self.base_url}/api/mode?mode=auto", timeout=self.timeout)
            self._md_force_state(saved["seasonState"])
            self._md_season(saved["seasonMode"].lower())
        except Exception:
            pass

    def test_tc36_1_mode_and_season_parameters(self):
        """SWE.5-TC36.1: manualTimeout 3 h, boostDuration 3 h, heatLimit 15 C, seasonOnDays 2, seasonOffDays 5 with their ranges (FR-52, FR-39, FR-12.2)"""
        test_id, name, req = "SWE.5-TC36.1", "Mode And Season Parameters", "FR-12.2"
        start = time.time()
        try:
            c = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            want = {"manualTimeoutSec": 10800, "boostDurationSec": 10800, "heatLimit": 15, "seasonOnDays": 2, "seasonOffDays": 5}
            got = {k: c.get(k) for k in want}
            assert got == want, f"defaults {got}, expected {want}"
            bad = {"manualTimeoutSec": (3599, 21601), "boostDurationSec": (3599, 21601), "heatLimit": (9, 19),
                   "seasonOnDays": (0, 8), "seasonOffDays": (0, 11)}
            for key, vals in bad.items():
                for v in vals:
                    r = requests.post(f"{self.base_url}/api/config/system", data={key: v}, timeout=self.timeout)
                    assert r.status_code == 400, f"{key}={v}: HTTP {r.status_code}, expected 400"
            c2 = requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()
            assert {k: c2.get(k) for k in want} == want, "a rejected value changed a setting"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "5 defaults as in section 6; 10 out-of-range values -> 400, unchanged")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc36_2_manual_timeout_remaining(self):
        """SWE.5-TC36.2: MANUAL reports the remaining time of manualTimeout, AUTO reports none (FR-52)"""
        test_id, name, req = "SWE.5-TC36.2", "Manual Mode Remaining Time", "FR-52"
        start = time.time()
        saved = None
        try:
            saved = self._md_status()
            assert saved["effectiveMode"] in ("AUTO", "OFF"), f"board is in {saved['effectiveMode']} before the test"
            r = requests.post(f"{self.base_url}/api/mode?mode=manual", timeout=self.timeout)
            assert r.status_code == 200, f"mode manual: HTTP {r.status_code}"
            s = self._md_status()
            assert s["effectiveMode"] == "MANUAL", f"effectiveMode {s['effectiveMode']} in MANUAL"
            assert 10800 - 60 <= s["manualRemainingSec"] <= 10800, f"remaining {s['manualRemainingSec']} s, expected about 10800"
            time.sleep(5)
            s2 = self._md_status()
            assert s2["manualRemainingSec"] < s["manualRemainingSec"], "the remaining time does not run down"
            requests.post(f"{self.base_url}/api/mode?mode=auto", timeout=self.timeout)
            s3 = self._md_status()
            assert s3["manualRemainingSec"] == 0 and s3["effectiveMode"] != "MANUAL", f"after AUTO: {s3['effectiveMode']}, {s3['manualRemainingSec']} s"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"MANUAL: {s['manualRemainingSec']} s left, runs down; AUTO: 0")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            if saved:
                self._md_restore(saved)

    def test_tc36_3_global_off_no_demand(self):
        """SWE.5-TC36.3: season mode OFF is global OFF: effectiveMode OFF and no zone asks for heat (FR-64, FR-12.2)"""
        test_id, name, req = "SWE.5-TC36.3", "Global OFF Stops Demand", "FR-64"
        start = time.time()
        saved = None
        try:
            saved = self._md_status()
            self._md_season("off")
            time.sleep(3)
            s = self._md_status()
            assert s["effectiveMode"] == "OFF", f"effectiveMode {s['effectiveMode']}, expected OFF"
            assert s["seasonState"] == "SUMMER", f"seasonState {s['seasonState']}, expected SUMMER"
            zs = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"]
            asking = [z["id"] for z in zs if z["demand"] and not z["frostActive"]]
            assert not asking, f"global OFF but zones {asking} still ask for heat"
            self._md_season("on")
            time.sleep(2)
            s = self._md_status()
            assert s["seasonState"] == "HEATING" and s["effectiveMode"] == "AUTO", f"season ON: {s['seasonState']}, {s['effectiveMode']}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "OFF: effectiveMode OFF, no demand; ON: HEATING / AUTO")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            if saved:
                self._md_restore(saved)

    def test_tc36_4_season_automatic(self):
        """SWE.5-TC36.4: season AUTO: ON after seasonOnDays days below heatLimit, OFF after seasonOffDays days above heatLimit + 2 C; a day without data changes nothing (FR-12.2)"""
        test_id, name, req = "SWE.5-TC36.4", "Season Automatic", "FR-12.2"
        start = time.time()
        saved = None
        try:
            saved = self._md_status()
            st = lambda: self._md_status()["seasonState"]
            # SUMMER -> HEATING: 2 days in a row below 15 C
            self._md_force_state("SUMMER")
            self._md_day(10.0)
            assert st() == "SUMMER", "HEATING after 1 day below heatLimit (seasonOnDays 2)"
            self._md_day(15.0)               # not below heatLimit: the run of days starts again
            self._md_day(10.0)
            assert st() == "SUMMER", "days below heatLimit were not in a row"
            self._md_day("none")             # a day without data does not count and does not reset
            assert st() == "SUMMER", "a day without data changed the season"
            self._md_day(10.0)
            assert st() == "HEATING", "2 days in a row below heatLimit did not start the season"
            # HEATING -> SUMMER: 5 days in a row above 17 C
            for _ in range(4):
                self._md_day(17.5)
            assert st() == "HEATING", "SUMMER after 4 days above heatLimit + 2 C (seasonOffDays 5)"
            self._md_day(17.0)               # not above heatLimit + 2 C: the run starts again
            for _ in range(4):
                self._md_day(17.5)
            assert st() == "HEATING", "days above heatLimit + 2 C were not in a row"
            self._md_day("none")
            assert st() == "HEATING", "a day without data changed the season"
            self._md_day(17.5)
            assert st() == "SUMMER", "5 days in a row above heatLimit + 2 C did not end the season"
            # season mode ON / OFF ignore the means
            self._md_season("on")
            self._md_day(25.0)
            assert st() == "HEATING", "season mode ON followed the weather"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "ON after 2 days < 15, OFF after 5 days > 17; runs must be in a row; no data = no change; mode ON fixed")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            if saved:
                self._md_restore(saved)

    def test_tc36_5_boost_limits(self):
        """SWE.5-TC36.5: a boost is 0.5-2.0 C above the schedule level in 0.5 C steps, anything else is rejected with the target kept; hold/clear ends it (FR-1.2, FR-39)"""
        def body(cfg0):
            n = cfg0["zonesCount"] + 1
            assert self._t30_add().status_code == 200, "zone-add failed"
            zone = lambda: next(z for z in requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json()["zones"] if z["id"] == n)
            post = lambda t: requests.post(f"{self.base_url}/api/zones/setpoint?id={n}&target={t}", timeout=self.timeout)
            sched = zone()["scheduleTarget"]
            for off in (0.5, 1.0, 1.5, 2.0):
                r = post(sched + off)
                assert r.status_code == 200, f"boost +{off}: HTTP {r.status_code}"
                z = zone()
                assert z["isBoost"] is True and abs(z["effectiveTarget"] - (sched + off)) < 0.05, f"boost +{off}: isBoost {z['isBoost']}, target {z['effectiveTarget']}"
            kept = zone()["effectiveTarget"]
            for off in (2.5, 0.7, 3.0):
                r = post(sched + off)
                assert r.status_code == 400, f"boost +{off}: HTTP {r.status_code}, expected 400"
                assert abs(zone()["effectiveTarget"] - kept) < 0.05, f"rejected boost +{off} changed the target"
            requests.post(f"{self.base_url}/api/zones/hold/clear?id={n}", timeout=self.timeout)
            z = zone()
            assert z["isBoost"] is False and abs(z["effectiveTarget"] - sched) < 0.05, f"after hold/clear: isBoost {z['isBoost']}, target {z['effectiveTarget']}, schedule {sched}"
            r = post(sched - 1.0)
            assert r.status_code == 200 and zone()["isBoost"] is False, "a target below the schedule level must not be a boost"
            requests.post(f"{self.base_url}/api/zones/hold/clear?id={n}", timeout=self.timeout)
            return f"schedule {sched}: +0.5 / 1.0 / 1.5 / 2.0 boost; +2.5, +0.7, +3.0 -> 400 target kept; hold/clear ends it"
        return self._t30_run("SWE.5-TC36.5", "Boost Limits", "FR-1.2", body)
