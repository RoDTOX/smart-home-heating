"""System tests of migration step 10 (Weather in the network worker): FR-9 validity Ok / Held / Lost (architecture 8.1).

Written from the requirement text and architecture section 8.1 only. The board has Internet; the failure of a poll is
simulated with the development hook POST /api/dev/weather (only in the dev-open build): `failed=1&ageSec=N` makes the
last successful poll N seconds old and the poll after it failed, `refresh=1` starts a real poll at once.
The state is read from GET /api/setup/time: `weatherState` ok | held | lost.
"""
from .base import *  # noqa: F401,F403


class WeatherTests:
    def _wx(self):
        r = requests.get(f"{self.base_url}/api/setup/time", timeout=self.timeout)
        assert r.status_code == 200, f"/api/setup/time: HTTP {r.status_code}"
        return r.json()

    def _wx_hook(self, **kv):
        r = requests.post(f"{self.base_url}/api/dev/weather", data=kv, timeout=self.timeout)
        assert r.status_code == 200, f"/api/dev/weather {kv}: HTTP {r.status_code}"

    def _wx_wait_ok(self, secs=40):
        end = time.time() + secs
        while time.time() < end:
            if self._wx().get("weatherState") == "ok":
                return True
            time.sleep(2)
        return False

    def test_tc35_1_weather_values_and_previous_day_mean(self):
        """SWE.5-TC35.1: outdoor temperature, state ok, age and the previous-day mean are reported (FR-9, FR-12.2)"""
        test_id, name, req = "SWE.5-TC35.1", "Weather Values And Previous Day Mean", "FR-9"
        start = time.time()
        try:
            self._wx_hook(refresh=1)
            assert self._wx_wait_ok(), f"no successful poll within 40 s: {self._wx()}"
            w = self._wx()
            assert w["weatherValid"] is True and -40 <= w["outdoorTemp"] <= 50, f"outdoorTemp {w['outdoorTemp']}"
            assert w["weatherAgeSec"] < 900, f"age {w['weatherAgeSec']} s after a successful poll"
            assert w["prevDayMeanValid"] is True and -40 <= w["prevDayMean"] <= 40, \
                f"previous-day mean {w.get('prevDayMean')} valid {w.get('prevDayMeanValid')}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"state ok, {w['outdoorTemp']} C, age {w['weatherAgeSec']} s, previous-day mean {w['prevDayMean']} C")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc35_2_held_then_lost_after_6_h(self):
        """SWE.5-TC35.2: after a failed poll the last value is held for 6 h, then it is lost (FR-9 fallback)"""
        test_id, name, req = "SWE.5-TC35.2", "Weather Held Then Lost", "FR-9"
        start = time.time()
        try:
            self._wx_hook(refresh=1)
            assert self._wx_wait_ok(), "no successful poll before the test"
            self._wx_hook(failed=1, ageSec=3600)
            time.sleep(1)
            w = self._wx()
            assert w["weatherState"] == "held", f"poll failed, value 1 h old: state {w['weatherState']}, expected held"
            assert w["weatherValid"] is True, "a held value (6 h or younger) must stay usable"
            self._wx_hook(failed=1, ageSec=6 * 3600 + 60)
            time.sleep(1)
            w = self._wx()
            assert w["weatherState"] == "lost", f"value older than 6 h: state {w['weatherState']}, expected lost"
            assert w["weatherValid"] is False, "a value older than 6 h must not be used"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "1 h old -> held (usable); 6 h 1 min -> lost")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            try:
                self._wx_hook(refresh=1)
                self._wx_wait_ok()
            except Exception:
                pass

    def test_tc35_3_poll_does_not_block_the_page(self):
        """SWE.5-TC35.3: a weather poll runs in the network worker: no answer of the page takes longer than 700 ms (architecture 5.1, FR-23)"""
        test_id, name, req = "SWE.5-TC35.3", "Poll Does Not Block The Page", "FR-23"
        start = time.time()
        try:
            slowest = 0.0
            for _ in range(3):
                self._wx_hook(refresh=1)
                t_end = time.time() + 6
                while time.time() < t_end:
                    t0 = time.time()
                    r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
                    dt = (time.time() - t0) * 1000
                    assert r.status_code == 200, f"/api/status: HTTP {r.status_code}"
                    slowest = max(slowest, dt)
                    time.sleep(0.15)
            assert slowest < 700, f"the slowest answer during polls took {slowest:.0f} ms"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"3 polls, slowest page answer {slowest:.0f} ms")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
