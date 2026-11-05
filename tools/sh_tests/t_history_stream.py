"""System tests of migration step 11 (History streamed from LittleFS): FR-34, NFR-2.2.

Written from the requirement text and architecture AR-15 only: the history answer is streamed (HTTP chunked, no
Content-Length) and holds the same data as before; the free heap does not shrink by a request.
"""
from .base import *  # noqa: F401,F403


class HistoryStreamTests:
    def test_tc37_1_history_is_streamed(self):
        """SWE.5-TC37.1: GET /api/history?range=24h|5d is streamed (chunked) valid JSON with the 24 h / 5 day points (FR-34, AR-15)"""
        test_id, name, req = "SWE.5-TC37.1", "History Streamed", "FR-34"
        start = time.time()
        try:
            for rng, pts in (("24h", 288), ("5d", 240)):
                r = requests.get(f"{self.base_url}/api/history?range={rng}", timeout=15)
                assert r.status_code == 200, f"range {rng}: HTTP {r.status_code}"
                assert r.headers.get("Transfer-Encoding", "").lower() == "chunked", \
                    f"range {rng}: not streamed (headers {dict(r.headers)})"
                j = r.json()
                assert isinstance(j, dict) and j, f"range {rng}: empty or not an object"
                lists = [v for v in j.values() if isinstance(v, list)]
                assert lists and max(len(v) for v in lists) <= pts + 1, f"range {rng}: no list or more than {pts} points"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "24h and 5d: chunked, valid JSON")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc37_2_history_request_keeps_heap(self):
        """SWE.5-TC37.2: five history requests in a row leave the free heap within 1.5 KB of its start (NFR-2.2, AR-15)"""
        test_id, name, req = "SWE.5-TC37.2", "History Request Keeps Heap", "NFR-2.2"
        start = time.time()
        try:
            heap = lambda: requests.get(f"{self.base_url}/api/status", timeout=self.timeout).json()["freeHeap"]
            h0 = heap()
            for _ in range(5):
                requests.get(f"{self.base_url}/api/history?range=5d", timeout=15)
            time.sleep(1.5)  # allow lwIP TCP buffers to drain
            h1 = heap()
            assert h1 >= h0 - 1500, f"free heap {h0} -> {h1} bytes after 5 history requests"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"free heap {h0} -> {h1} bytes")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
