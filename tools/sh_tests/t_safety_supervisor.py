"""System tests of migration step 4 (SafetySupervisor): interlock FR-2, overrun FR-3, burner guard FR-4.

Written from the requirement text and requirements section 6 only. MANUAL mode drives the valves
(POST /api/relay/test) and the boiler (POST /api/boiler/on / off); every test leaves the board in AUTO,
the boiler OFF and the valves closed. Frost protection FR-16.1 needs a reading below frostTemp, which the
board cannot be given yet (no fault injector before migration step 10): see PB-116.
"""
from .base import *  # noqa: F401,F403


class SafetySupervisorTests:
    def _ss_zones(self):
        return requests.get(f"{self.base_url}/api/zones", timeout=self.timeout).json().get("zones", [])

    def _ss_cfg(self):
        return requests.get(f"{self.base_url}/api/config/system", timeout=self.timeout).json()

    def _ss_boiler(self):
        return requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout).json()

    def _ss_pins(self):
        return [z["relayPin"] for z in self._ss_cfg()["zones"]]

    def _ss_energised(self):
        return [z.get("relayEngaged") for z in self._ss_zones()]

    def _ss_valve(self, pin, on):
        r = requests.post(f"{self.base_url}/api/relay/test?pin={pin}&state={1 if on else 0}", timeout=self.timeout)
        assert r.status_code == 200, f"relay/test pin {pin} state {int(on)}: HTTP {r.status_code}"

    def _ss_mode(self, mode):
        r = requests.post(f"{self.base_url}/api/mode?mode={mode}", timeout=self.timeout)
        assert r.status_code == 200, f"mode {mode}: HTTP {r.status_code}"

    def _ss_cleanup(self, saved_cfg=None):
        """Board back to: MANUAL, valves requested closed, boiler OFF (forced), then AUTO; settings restored."""
        b, to = self.base_url, self.timeout
        try:
            requests.post(f"{b}/api/mode?mode=manual", timeout=to)
            requests.post(f"{b}/api/boiler/cancel", timeout=to)
            requests.post(f"{b}/api/boiler/off?force=true", timeout=to)
            for pin in self._ss_pins():
                requests.post(f"{b}/api/relay/test?pin={pin}&state=0", timeout=to)
            if saved_cfg:
                requests.post(f"{b}/api/config/system", data=saved_cfg, timeout=to)
        except Exception:
            pass
        finally:
            try:
                requests.post(f"{b}/api/mode?mode=auto", timeout=to)
            except Exception:
                pass

    def _ss_baseline_off(self):
        """Boiler OFF without resetting the burner guard when it already is OFF."""
        b, to = self.base_url, self.timeout
        requests.post(f"{b}/api/boiler/cancel", timeout=to)
        if self._ss_boiler().get("boiler") != "OFF":
            requests.post(f"{b}/api/boiler/off?force=true", timeout=to)

    def test_tc33_1_boiler_on_refused_without_valve(self):
        """SWE.5-TC33.1: Boiler ON is refused (HTTP 409) while no zone valve is energised (FR-2)"""
        test_id, name, req = "SWE.5-TC33.1", "Boiler ON Refused Without Open Valve", "FR-2"
        start = time.time()
        try:
            self._ss_mode("manual")
            self._ss_baseline_off()
            for pin in self._ss_pins():
                self._ss_valve(pin, False)
            time.sleep(3)
            assert not any(self._ss_energised()), f"a valve is still energised: {self._ss_energised()}"
            r = requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
            assert r.status_code == 409, f"MANUAL, no valve energised: expected HTTP 409, got {r.status_code} {r.text[:120]}"
            st = self._ss_boiler()
            assert st.get("boiler") == "OFF", f"boiler is {st.get('boiler')} after the refused ON"
            assert st.get("pendingAction") == "NONE", f"a refused ON was queued: {st.get('pendingAction')}"
            # PB-1: also in AUTO the same request is refused (when no zone has demand of its own right now)
            self._ss_mode("auto")
            time.sleep(2)
            if not any(self._ss_energised()):
                r = requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
                assert r.status_code == 409, f"AUTO, no valve energised: expected HTTP 409, got {r.status_code}"
                assert self._ss_boiler().get("boiler") == "OFF", "boiler ON in AUTO without an open valve"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "ON without an energised valve -> 409, boiler OFF, nothing queued")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            self._ss_cleanup()

    def test_tc33_2_boiler_on_refused_inside_interlock_delay(self):
        """SWE.5-TC33.2: Boiler ON is refused (HTTP 409) while the valve has been energised for less than interlockDelay (FR-2)"""
        test_id, name, req = "SWE.5-TC33.2", "Boiler ON Refused Inside interlockDelay", "FR-2"
        start = time.time()
        try:
            cfg = self._ss_cfg()
            assert cfg["actuatorDelaySec"] >= 120, f"interlockDelay {cfg['actuatorDelaySec']} s below the §6 floor"
            pins = self._ss_pins()
            assert pins, "no zone on the board"
            self._ss_mode("manual")
            self._ss_baseline_off()
            for pin in pins:
                self._ss_valve(pin, False)
            self._ss_valve(pins[0], True)
            time.sleep(5)
            r = requests.post(f"{self.base_url}/api/boiler/on", timeout=self.timeout)
            assert r.status_code == 409, f"valve energised 5 s of {cfg['actuatorDelaySec']} s: expected HTTP 409, got {r.status_code}"
            st = self._ss_boiler()
            assert st.get("boiler") == "OFF" and st.get("pendingAction") == "NONE", f"boiler {st.get('boiler')}, pending {st.get('pendingAction')}"
            # the boiler never ran: FR-3.2 / FR-37 - the valve closes at the next control tick
            self._ss_valve(pins[0], False)
            time.sleep(3)
            assert not any(self._ss_energised()), f"valve still energised after the request without flame: {self._ss_energised()}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "ON 5 s after the valve opened -> 409; no flame, so the valve closed at the next tick")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            self._ss_cleanup()

    def test_tc33_3_full_burn_interlock_overrun_burner_guard(self):
        """SWE.5-TC33.3: A full burn - ON after interlockDelay, last valve held while ON and for overrun after OFF, OFF after burnerMinRun (FR-2, FR-3, FR-4)"""
        test_id, name, req = "SWE.5-TC33.3", "Full Burn: Interlock, Overrun, Burner Guard", "FR-2, FR-3, FR-4"
        start = time.time()
        b, to = self.base_url, self.timeout
        saved = None
        try:
            cfg = self._ss_cfg()
            saved = {k: cfg[k] for k in ("actuatorDelaySec", "pumpOverrunSec", "burnerMinRunSec")}
            # requirements §6 floors: interlockDelay 120 s, overrun 180 s, burnerMinRun 120 s
            r = requests.post(f"{b}/api/config/system", data={"actuatorDelaySec": "120", "pumpOverrunSec": "180",
                                                              "burnerMinRunSec": "120"}, timeout=to)
            assert r.status_code == 200, f"config: HTTP {r.status_code}"
            pins = self._ss_pins()
            assert pins, "no zone on the board"
            self._ss_mode("manual")
            self._ss_baseline_off()
            for pin in pins:
                self._ss_valve(pin, False)
            self._ss_valve(pins[0], True)
            t_valve = time.time()
            time.sleep(122 - 0)  # interlockDelay 120 s + margin
            # ON: accepted now, or queued behind the burner pause (FR-4, FR-32)
            r = requests.post(f"{b}/api/boiler/on", timeout=to)
            assert r.status_code in (200, 429), f"ON after interlockDelay: HTTP {r.status_code}"
            deadline = time.time() + 240
            while time.time() < deadline and self._ss_boiler().get("boiler") != "ON":
                time.sleep(3)
            assert self._ss_boiler().get("boiler") == "ON", "boiler did not start after interlockDelay and the burner pause"
            assert time.time() - t_valve >= 120, "boiler started before interlockDelay elapsed"
            t_on = time.time()
            # FR-3: the last valve is asked closed while the boiler is ON -> it stays energised
            self._ss_valve(pins[0], False)
            time.sleep(4)
            assert self._ss_energised()[0] is True, "last valve closed while the boiler was ON (FR-3)"
            # FR-4: OFF right after ON is deferred, not dropped and not executed
            r = requests.post(f"{b}/api/boiler/off", timeout=to)
            assert r.status_code == 429, f"OFF {int(time.time() - t_on)} s after ON: expected HTTP 429, got {r.status_code}"
            assert r.json().get("waitSec", 0) > 0, "no waitSec in the deferred OFF"
            assert self._ss_boiler().get("boiler") == "ON", "boiler OFF inside burnerMinRun"
            deadline = time.time() + 200
            while time.time() < deadline and self._ss_boiler().get("boiler") != "OFF":
                assert self._ss_energised()[0] is True, "valve closed while the boiler was ON or PENDING_OFF (FR-3)"
                time.sleep(3)
            assert self._ss_boiler().get("boiler") == "OFF", "deferred OFF was not executed"
            t_off = time.time()
            assert t_off - t_on >= 118, f"burner ran {int(t_off - t_on)} s, below burnerMinRun 120 s"
            # FR-3: overrun - valve energised for 180 s after the OFF, closed soon after
            time.sleep(max(0, 150 - (time.time() - t_off)))
            assert self._ss_energised()[0] is True, f"valve closed {int(time.time() - t_off)} s after OFF, overrun is 180 s (FR-3)"
            time.sleep(max(0, 195 - (time.time() - t_off)))
            assert self._ss_energised()[0] is False, f"valve still energised {int(time.time() - t_off)} s after OFF (overrun 180 s)"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"ON after {int(t_on - t_valve)} s of interlock; ran {int(t_off - t_on)} s; valve held to overrun end")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            self._ss_cleanup(saved)
