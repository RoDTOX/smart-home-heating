"""System tests of the page on LittleFS (FR-8.5, FR-8.6, FR-10.6), written from the requirement text only."""
from .base import *  # noqa: F401,F403
import gzip as _gzip
import os as _os

FLOOR = 24576
MAX_PAGE = 61440


class WebAssetTests:
    def _page(self):
        return requests.get(f"{self.base_url}/", timeout=self.timeout, headers={"Accept-Encoding": "identity"})

    def _upload(self, data, name="index.html.gz"):
        return requests.post(f"{self.base_url}/api/web/update", files={"f": (name, data)}, timeout=30)

    def _raw_page(self):
        r = requests.get(f"{self.base_url}/", timeout=self.timeout, stream=True, headers={"Accept-Encoding": "gzip"})
        return r.headers, r.raw.read(decode_content=False)

    def test_tc38_1_page_from_littlefs(self):
        """SWE.5-TC38.1: GET / answers the gzip page from LittleFS with X-Page-Source: littlefs (FR-8.5)"""
        test_id, name, req = "SWE.5-TC38.1", "Page From LittleFS", "FR-8.5"
        start = time.time()
        try:
            h, raw = self._raw_page()
            assert h.get("X-Page-Source") == "littlefs", f"X-Page-Source is {h.get('X-Page-Source')}"
            assert h.get("Content-Encoding") == "gzip", f"Content-Encoding is {h.get('Content-Encoding')}"
            assert raw[:2] == b"\x1f\x8b", "the body is not gzip"
            assert b"api/zones" in _gzip.decompress(raw), "the page does not read api/zones"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"{len(raw)} bytes gzip")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc38_2_refused_uploads_keep_page(self):
        """SWE.5-TC38.2: a file that is not gzip -> 400, over 61 440 B -> 413; the old page stays (FR-8.6)"""
        test_id, name, req = "SWE.5-TC38.2", "Refused Uploads Keep The Page", "FR-8.6"
        start = time.time()
        try:
            _, before = self._raw_page()
            r = self._upload(b"<html>not gzip, long enough to look like a page" * 5)
            assert r.status_code == 400, f"not gzip: HTTP {r.status_code}"
            big = _gzip.compress(_os.urandom(MAX_PAGE + 2000), 0)
            assert len(big) > MAX_PAGE
            r = self._upload(big)
            assert r.status_code == 413, f"too big: HTTP {r.status_code}"
            _, after = self._raw_page()
            assert after == before, "the page changed after refused uploads"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "400 and 413, page unchanged")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc38_3_upload_round_trip_and_floor(self):
        """SWE.5-TC38.3: uploading the served page again -> 200, the same bytes are served, free LittleFS stays >= 24 576 B (FR-8.6, FR-10.6)"""
        test_id, name, req = "SWE.5-TC38.3", "Upload Round Trip", "FR-8.6, FR-10.6"
        start = time.time()
        try:
            h0, before = self._raw_page()
            r = self._upload(before)
            assert r.status_code == 200, f"upload: HTTP {r.status_code} {r.text[:120]}"
            j = r.json()
            assert j.get("status") == "ok" and j.get("bytes") == len(before), f"answer {j}"
            h1, after = self._raw_page()
            assert after == before, "the served page differs from the uploaded one"
            assert h1.get("X-Page-Source") == "littlefs", "page not from littlefs after upload"
            m = requests.get(f"{self.base_url}/api/diag/mem", timeout=self.timeout).json()
            free = m["fsTotal"] - m["fsUsed"]
            assert free >= FLOOR, f"free LittleFS {free} B is under the floor {FLOOR} B"
            assert j.get("fsFree", 0) >= FLOOR, f"fsFree {j.get('fsFree')} under the floor"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"{len(before)} bytes, LittleFS free {free} B")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc38_4_stub_when_page_missing(self):
        """SWE.5-TC38.4: without the page file GET / answers the stub (<= 1024 B, X-Page-Source: stub, links /update); the upload restores the page (FR-8.5, FR-8.6)"""
        test_id, name, req = "SWE.5-TC38.4", "Stub Page When Missing", "FR-8.5, FR-8.6"
        start = time.time()
        _, saved = self._raw_page()
        try:
            r = requests.post(f"{self.base_url}/api/web/remove", timeout=self.timeout)
            assert r.status_code == 200, f"remove: HTTP {r.status_code}"
            p = self._page()
            assert p.status_code == 200, f"stub: HTTP {p.status_code}"
            assert p.headers.get("X-Page-Source") == "stub", f"X-Page-Source is {p.headers.get('X-Page-Source')}"
            assert len(p.content) <= 1024, f"stub is {len(p.content)} B"
            assert b"/update" in p.content, "stub does not offer /update"
            st = requests.get(f"{self.base_url}/api/boiler/status", timeout=self.timeout)
            assert st.status_code == 200, "the API stopped answering without the page"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"stub {len(p.content)} B")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
        finally:
            if saved[:2] == b"\x1f\x8b":
                try:
                    self._upload(saved)
                except Exception:
                    pass

    def test_tc38_5_memory_reports_littlefs(self):
        """SWE.5-TC38.5: /api/diag/mem reports fsTotal and fsUsed (LittleFS bytes) (FR-10.6)"""
        test_id, name, req = "SWE.5-TC38.5", "Memory Report Has LittleFS Figures", "FR-10.6"
        start = time.time()
        try:
            m = requests.get(f"{self.base_url}/api/diag/mem", timeout=self.timeout).json()
            assert isinstance(m.get("fsTotal"), int) and isinstance(m.get("fsUsed"), int), f"fields: {m.get('fsTotal')}, {m.get('fsUsed')}"
            assert 0 < m["fsUsed"] <= m["fsTotal"], f"fsUsed {m['fsUsed']} of fsTotal {m['fsTotal']}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, f"{m['fsUsed']} of {m['fsTotal']} B used")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
