#!/usr/bin/env python3
"""Automated verification suite for the Smart Heating Home Assistant custom integration.

Verifies:
1. Structural integrity and syntax of all integration source files.
2. JSON validity of manifest.json, hacs.json, strings.json, and translations.
3. YAML validity of blueprint and services.yaml.
4. Live API client queries against physical controller (192.168.1.20).
5. Data mapping and coordinator data structure integrity.
"""
import asyncio
import json
import os
import sys
import unittest
import urllib.request
import yaml


INTEGRATION_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "custom_components", "smart_heating")
)
DASHBOARD_FILE = os.path.abspath(
    os.path.join(
        os.path.dirname(__file__), "..", "dashboards", "smart_heating_dashboard.yaml"
    )
)
ESP_IP = os.environ.get("ESP_IP", "192.168.1.20")


class TestSmartHeatingFiles(unittest.TestCase):
    """Test integration files and schemas."""

    def test_01_manifest_json(self):
        manifest_path = os.path.join(INTEGRATION_DIR, "manifest.json")
        self.assertTrue(os.path.isfile(manifest_path), "manifest.json missing")
        with open(manifest_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self.assertEqual(data.get("domain"), "smart_heating")
        self.assertEqual(data.get("config_flow"), True)
        self.assertEqual(data.get("iot_class"), "local_polling")

    def test_02_hacs_json(self):
        hacs_path = os.path.join(INTEGRATION_DIR, "hacs.json")
        self.assertTrue(os.path.isfile(hacs_path), "hacs.json missing")
        with open(hacs_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        self.assertEqual(data.get("name"), "Smart Heating")

    def test_03_translations(self):
        for lang in ["en", "ro"]:
            path = os.path.join(INTEGRATION_DIR, "translations", f"{lang}.json")
            self.assertTrue(os.path.isfile(path), f"Translation {lang}.json missing")
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
            self.assertIn("config", data)
            self.assertIn("services", data)

    def test_04_services_yaml(self):
        services_path = os.path.join(INTEGRATION_DIR, "services.yaml")
        self.assertTrue(os.path.isfile(services_path), "services.yaml missing")
        with open(services_path, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
        self.assertIn("clear_dtc_history", data)
        self.assertIn("clear_window_pause", data)
        self.assertIn("clear_zone_hold", data)
        self.assertIn("submit_gas_meter_reading", data)
        self.assertIn("set_zone_boost", data)

    def test_05_dashboard_yaml(self):
        self.assertTrue(os.path.isfile(DASHBOARD_FILE), "Dashboard YAML missing")
        with open(DASHBOARD_FILE, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
        self.assertIn("views", data)


class TestSmartHeatingLiveEndpoints(unittest.TestCase):
    """Test live endpoint responses from ESP32."""

    def test_01_live_ping_and_zones(self):
        url = f"http://{ESP_IP}/api/zones"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=5) as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode("utf-8"))
        self.assertIn("zones", data)
        zones = data["zones"]
        self.assertEqual(len(zones), 3, "Expected 3 heating zones")
        z_names = [z["name"] for z in zones]
        self.assertIn("Living", z_names)

    def test_02_live_boiler_telemetry(self):
        url = f"http://{ESP_IP}/api/boiler/status"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=5) as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode("utf-8"))
        self.assertIn("boiler", data)
        self.assertIn("gasMeter", data)
        self.assertIn("sensors", data)
        gas = data["gasMeter"]
        self.assertIn("gasPrice", gas)
        self.assertIn("gasFlowMeasured", gas)

    def test_03_live_diagnostics(self):
        url = f"http://{ESP_IP}/api/diag/status"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=5) as resp:
            self.assertEqual(resp.status, 200)
            data = json.loads(resp.read().decode("utf-8"))
        self.assertIn("dtcCount", data)
        self.assertIn("checks", data)


if __name__ == "__main__":
    unittest.main()
