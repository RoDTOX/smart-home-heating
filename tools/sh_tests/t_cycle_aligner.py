"""System tests for Step 14 Part D (CycleAligner): FR-63.

Covers:
- SWE.5-TC49.1: minLoops parameter bounds (1..7 circuits, default 3) & zone circuits bounds (1..16, default 1) (FR-63, §6)
- SWE.5-TC49.2: CycleAligner status contract (/api/cycles/status) reporting cycle phase and alignment groups
- SWE.5-TC49.3: Zone circuits configuration contract via /api/zones and setup
- SWE.5-TC49.4: Cycle synchronization and minLoops grouping fallback / extension contract
"""
import time
import requests
from .base import SuiteBase, TestResult


class CycleAlignerTests:
    def test_tc49_1_min_loops_param_bounds(self):
        """SWE.5-TC49.1: minLoops [1..7 circuits] parameter bounds (FR-63, §6)"""
        test_id, name, req = "SWE.5-TC49.1", "Cycle Aligner Parameter Bounds", "FR-63"
        start = time.time()
        try:
            auth = self.api_auth()
            # minLoops: 1..7, default 3
            r = requests.post(f"{self.base_url}/api/config/system", data={"minLoops": "0"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"minLoops=0 must return 400, got {r.status_code}"
            r = requests.post(f"{self.base_url}/api/config/system", data={"minLoops": "8"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 400, f"minLoops=8 must return 400, got {r.status_code}"

            # Valid values: minLoops = 3
            r = requests.post(f"{self.base_url}/api/config/system", data={"minLoops": "3"}, auth=auth, timeout=self.timeout)
            assert r.status_code == 200, f"minLoops=3 must return 200, got {r.status_code}"

            # Verify in /api/config/system
            cfg = requests.get(f"{self.base_url}/api/config/system", auth=auth, timeout=self.timeout).json()
            assert cfg.get("minLoops") == 3, f"Expected minLoops 3, got {cfg.get('minLoops')}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "minLoops parameter bounds (1..7 circuits) correctly enforced")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc49_2_cycle_status_contract(self):
        """SWE.5-TC49.2: CycleAligner status contract (/api/cycles/status) (FR-63)"""
        test_id, name, req = "SWE.5-TC49.2", "Cycle Status Contract", "FR-63"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/cycles/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            assert "cycleLengthSec" in j, "Missing cycleLengthSec in /api/cycles/status"
            assert "cycleElapsedSec" in j, "Missing cycleElapsedSec in /api/cycles/status"
            assert "minLoops" in j, "Missing minLoops in /api/cycles/status"
            assert "activeCircuits" in j, "Missing activeCircuits in /api/cycles/status"
            assert "zones" in j and isinstance(j["zones"], list), "Missing zones list in /api/cycles/status"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "CycleAligner status contract verified (/api/cycles/status)")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc49_3_zone_circuits_contract(self):
        """SWE.5-TC49.3: Zone circuits exposed and persisted (FR-63, §6)"""
        test_id, name, req = "SWE.5-TC49.3", "Zone Circuits Contract", "FR-63"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/zones", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            zones = r.json().get("zones", [])
            assert len(zones) >= 1, "No zones available on board"
            z0 = zones[0]
            assert "circuits" in z0, "circuits field missing from zone record"
            assert 1 <= z0["circuits"] <= 16, f"Zone circuits out of bounds [1..16]: {z0['circuits']}"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Zone circuits field exposed and validated in range [1..16]")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc49_4_cycle_aligner_grouping_logic(self):
        """SWE.5-TC49.4: CycleAligner alignment and minLoops grouping (FR-63)"""
        test_id, name, req = "SWE.5-TC49.4", "Cycle Aligner Grouping Logic", "FR-63"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/cycles/status", timeout=self.timeout)
            assert r.status_code == 200, f"Expected HTTP 200, got {r.status_code}"
            j = r.json()
            # Verify structure of zone alignment items
            for z in j.get("zones", []):
                assert "index" in z, "Zone missing index"
                assert "circuits" in z, "Zone missing circuits"
                assert "aligned" in z, "Zone missing aligned flag"
                assert "extended" in z, "Zone missing extended flag"

            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "Cycle alignment and grouping flags verified in /api/cycles/status")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
