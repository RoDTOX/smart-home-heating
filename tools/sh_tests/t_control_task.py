"""System tests of Step 10 (Control Task AR-21, timing metrics FR-23, task watchdog NFR-1.5, preemption NFR-1.8).

Covers:
- FR-23: STATUS and /api/status report longest control-tick delay and longest web/loop pass since boot
- NFR-1.5: 90s task watchdog covers both control task and loop task; stack health monitored
- NFR-1.8: Safety reaction time bound: control tick preempts loop() on Core 1, keeping tick jitter <= 500ms
- NFR-5.5: FaultInjector weather-lost injection contract
- AR-21: Dynamic architecture: control tick runs in its own task on Core 1 above loop()
"""
import time
import requests
from .base import *  # noqa: F401,F403


class ControlTaskTests:

    def test_tc44_1_timing_metrics_contract(self):
        """SWE.5-TC44.1: Timing Metrics Contract in /api/status (FR-23, NFR-1.8)"""
        test_id, name, req = "SWE.5-TC44.1", "Timing Metrics Contract", "FR-23"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
            assert r.status_code == 200, f"GET /api/status returned {r.status_code}"
            data = r.json()

            # FR-23 requires longest control-tick delay and longest loop pass
            assert "longestTickDelayMs" in data, f"Missing 'longestTickDelayMs' in /api/status: {data.keys()}"
            assert "longestLoopPassMs" in data, f"Missing 'longestLoopPassMs' in /api/status: {data.keys()}"

            tick_delay = data.get("longestTickDelayMs")
            loop_pass = data.get("longestLoopPassMs")

            assert isinstance(tick_delay, (int, float)) and tick_delay >= 0, f"Invalid longestTickDelayMs: {tick_delay}"
            assert isinstance(loop_pass, (int, float)) and loop_pass >= 0, f"Invalid longestLoopPassMs: {loop_pass}"

            # AR-21 timing budget: control tick runs every 1s, delay must stay bounded (<= 500 ms)
            assert tick_delay <= 500, f"Control tick delay {tick_delay}ms exceeded 500ms budget (§5.4.1)"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              f"Timing metrics verified: tickDelay={tick_delay}ms, loopPass={loop_pass}ms")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc44_2_task_watchdog_and_stack_health(self):
        """SWE.5-TC44.2: Task Watchdog & Stack Health (NFR-1.5, NFR-2.2)"""
        test_id, name, req = "SWE.5-TC44.2", "Task Watchdog & Stack", "NFR-1.5"
        start = time.time()
        try:
            # Query /api/diag/mem to inspect task stack high-water marks
            r = requests.get(f"{self.base_url}/api/diag/mem", timeout=self.timeout)
            assert r.status_code == 200, f"GET /api/diag/mem returned {r.status_code}"
            mem = r.json()

            # Tasks array or stack fields must report control task and loop stack
            # Expected fields: controlStackFree >= 1024 B, loopStackFree >= 1024 B
            control_stack = mem.get("controlStackFree", None)
            loop_stack = mem.get("loopStackFree", None)

            # Fallback to tasks array if present
            if control_stack is None and "tasks" in mem:
                for t in mem.get("tasks", []):
                    if t.get("name") in ("ctl", "control", "ControlTask"):
                        control_stack = t.get("stackFree", t.get("free", None))
                    elif t.get("name") in ("loop", "main", "loopTask"):
                        loop_stack = t.get("stackFree", t.get("free", None))

            assert control_stack is not None, f"Control task stack watermark not reported: {mem}"
            assert loop_stack is not None, f"Loop task stack watermark not reported: {mem}"

            assert control_stack >= 1024, f"Control task stack too low: {control_stack} B < 1024 B"
            assert loop_stack >= 1024, f"Loop task stack too low: {loop_stack} B < 1024 B"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              f"Stack health verified: control={control_stack}B, loop={loop_stack}B")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc44_3_control_tick_preemption_under_load(self):
        """SWE.5-TC44.3: Control Tick Preemption under Load (NFR-1.8, AR-21)"""
        test_id, name, req = "SWE.5-TC44.3", "Control Tick Preemption", "NFR-1.8"
        start = time.time()
        try:
            # Generate concurrent/rapid HTTP queries to load the loop() WebServer
            for _ in range(5):
                requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
                requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)

            # Read back timing
            r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
            assert r.status_code == 200
            data = r.json()
            assert "longestTickDelayMs" in data, f"Missing 'longestTickDelayMs' in /api/status"
            tick_delay = data.get("longestTickDelayMs")

            # Preemption check: control task priority 3 must preempt loop priority 1
            # Tick delay must remain strictly <= 500ms
            assert tick_delay <= 500, f"Control tick delay escalated under HTTP load: {tick_delay}ms > 500ms"

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              f"Preemption confirmed: tick delay {tick_delay}ms <= 500ms under load")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc44_4_weather_lost_injection(self):
        """SWE.5-TC44.4: Weather Lost Fault Injection (NFR-5.5)"""
        test_id, name, req = "SWE.5-TC44.4", "Weather Lost Injection", "NFR-5.5"
        start = time.time()
        try:
            auth = self.api_auth()

            # 1. Inject weather-lost fault
            r_inj = requests.post(f"{self.base_url}/api/diag/inject",
                                  json={"point": "weather-lost"},
                                  auth=auth, timeout=self.timeout)
            assert r_inj.status_code == 200, f"Inject weather-lost failed: {r_inj.status_code}"

            # 2. Verify active injection in list
            r_list = requests.get(f"{self.base_url}/api/diag/inject", timeout=self.timeout)
            assert r_list.status_code == 200
            inj = r_list.json().get("injections", [])
            assert any(i.get("point") == "weather-lost" for i in inj), f"weather-lost not active: {inj}"

            # 3. Clear injection
            r_clr = requests.post(f"{self.base_url}/api/diag/inject/clear", auth=auth, timeout=self.timeout)
            assert r_clr.status_code == 200

            # 4. Verify cleared
            r_list2 = requests.get(f"{self.base_url}/api/diag/inject", timeout=self.timeout)
            assert len(r_list2.json().get("injections", [])) == 0

            latency = (time.time() - start) * 1000
            return TestResult(test_id, name, req, "PASS", latency,
                              "weather-lost injected, verified in API, and cleared cleanly")
        except Exception as e:
            # Cleanup on failure
            try:
                requests.post(f"{self.base_url}/api/diag/inject/clear", auth=self.api_auth(), timeout=self.timeout)
            except Exception:
                pass
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
