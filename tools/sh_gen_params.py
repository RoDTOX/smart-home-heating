#!/usr/bin/env python3
"""Single Source of Truth Parameter Generator (Step I-10, architecture P4).

Extracts parameter definitions, defaults, and ranges directly from src/ParamStore.cpp
and synchronizes:
1. docs/api_reference.md (under ## `/api/config/system`)
2. web/src/setup.js (PARAM_LIMITS object used for import verification & setup)

Usage:
  python tools/sh_gen_params.py           # Update docs and web/src/setup.js
  python tools/sh_gen_params.py --check   # Check if docs & web match ParamStore (exit 0 if OK, 1 if drift)
"""
import os
import re
import sys

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARAM_STORE_PATH = os.path.join(PROJECT_DIR, "src", "ParamStore.cpp")
API_REF_PATH = os.path.join(PROJECT_DIR, "docs", "api_reference.md")
SETUP_JS_PATH = os.path.join(PROJECT_DIR, "web", "src", "setup.js")

# Mapping of ParamStore parameter names to Web API JSON keys, units, and Romanian UI labels
API_NUM_PARAMS = [
    # (param_name, api_key, unit, label, decimals)
    ("interlockDelay", "actuatorDelaySec", "s", "Întârziere actuatori (secunde)", 0),
    ("overrun", "pumpOverrunSec", "s", "Overrun pompă (secunde)", 0),
    ("burnerMinRun", "burnerMinRunSec", "s", "Timp minim arzător ON (secunde)", 0),
    ("burnerMinPause", "burnerMinPauseSec", "s", "Pauză minimă arzător (secunde)", 0),
    ("hbInterval", "rfSafetyHeartbeatSec", "s", "Heartbeat RF (secunde)", 0),
    ("frostTemp", "freezeProtectionTemp", "°C", "Prag protecție îngheț (°C)", 0),
    ("sensorTimeout", "sensorTimeoutSec", "s", "Timeout senzor (secunde)", 0),
    ("hysteresis", "hysteresis", "°C", "Histerezis (°C)", 1),
    ("windowThreshold", "windowThreshold", "°C", "Sensibilitate geam deschis (°C)", 1),
]

API_FLAG_PARAMS = [
    # (param_name, api_key, range_desc)
    ("boilerLinkHeartbeat", "rfSafetyHeartbeatEnabled", "true / false"),
    ("openLoopBypass", "openLoopBypass", "true / false"),
    ("actuatorType", "actuatorNormallyClosed", "true = NC"),
    ("pumpOutput", "pumpControlEnabled", "true = relay 4"),
]


def parse_param_store(path=PARAM_STORE_PATH):
    """Parse P(name, key, def, lo, hi, scale, flags) from ParamStore.cpp."""
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # Pattern matches: P("interlockDelay", "intlk", 180, 120, 600, 1, P_AUTH)
    pattern = re.compile(
        r'P\(\s*"([^"]+)",\s*"([^"]+)",\s*([\d.]+f?),\s*([\d.]+f?),\s*([\d.]+f?),\s*(\d+),\s*(\w+)\s*\)'
    )
    params = {}
    for match in pattern.finditer(content):
        name, key, def_val, lo_val, hi_val, scale, flags = match.groups()
        scale = int(scale)
        params[name] = {
            "name": name,
            "key": key,
            "default_raw": def_val.rstrip("f"),
            "min_raw": lo_val.rstrip("f"),
            "max_raw": hi_val.rstrip("f"),
            "scale": scale,
            "flags": flags,
        }
    return params


def format_param_val(val_raw, decimals):
    f = float(val_raw)
    if decimals > 0:
        return f"{f:.{decimals}f}"
    return str(int(round(f)))


def generate_setup_limits_js(params):
    """Generate JavaScript PARAM_LIMITS object body."""
    lines = ["  const PARAM_LIMITS = {"]
    for param_name, api_key, _unit, label, decimals in API_NUM_PARAMS:
        if param_name not in params:
            continue
        p = params[param_name]
        min_v = format_param_val(p["min_raw"], decimals)
        max_v = format_param_val(p["max_raw"], decimals)
        lines.append(f'    {api_key}: {{min: {min_v}, max: {max_v}, label: "{label}"}},')
    # Remove trailing comma on last entry
    lines[-1] = lines[-1].rstrip(",")
    lines.append("  };")
    return "\n".join(lines)


def generate_api_ref_table(params):
    """Generate Markdown table for api_reference.md under /api/config/system."""
    lines = [
        "| Field | §6 parameter | Range | POST |",
        "|---|---|---|---|",
    ]

    def get_range_str(param_name, decimals=0):
        if param_name in params:
            lo = format_param_val(params[param_name]["min_raw"], decimals)
            hi = format_param_val(params[param_name]["max_raw"], decimals)
            return f"{lo}–{hi}"
        return ""

    lines.append(f"| `actuatorDelaySec` | `interlockDelay` | {get_range_str('interlockDelay', 0)} s | yes |")
    lines.append(f"| `pumpOverrunSec` | `overrun` | {get_range_str('overrun', 0)} s | yes |")
    lines.append(f"| `burnerMinRunSec`, `burnerMinPauseSec` | `burnerMinRun`, `burnerMinPause` | {get_range_str('burnerMinRun', 0)} s | yes |")
    lines.append(f"| `rfSafetyHeartbeatSec` | `hbInterval` | {get_range_str('hbInterval', 0)} s | yes |")
    lines.append("| `rfSafetyHeartbeatEnabled` | `boilerLinkHeartbeat` | true / false | yes |")
    lines.append(f"| `freezeProtectionTemp` | `frostTemp` | {get_range_str('frostTemp', 0)} °C | yes |")
    lines.append(f"| `sensorTimeoutSec` | `sensorTimeout` | {get_range_str('sensorTimeout', 0)} s | yes |")
    lines.append(f"| `hysteresis` | `hysteresis` | {get_range_str('hysteresis', 1)} °C | yes |")
    lines.append(f"| `windowThreshold` | `windowThreshold` | {get_range_str('windowThreshold', 1)} °C | yes |")

    lines.append("| `openLoopBypass` | `openLoopBypass` | true / false | yes |")
    lines.append("| `actuatorNormallyClosed` | `actuatorType` | true = NC | yes |")
    lines.append("| `pumpControlEnabled` | `pumpOutput` | true = relay 4 | yes |")

    # Read-only fields
    lines.append("| `rfMinCooldownSec` | FR-17.2 (fixed 3 s) | — | read only |")
    lines.append("| `pumpRelayPin` | relay 4 (GPIO26) | — | read only |")
    lines.append("| `zonesCount` | number of zones (§6), 0-32 | — | read only (use `/api/setup/zone-add`, `/api/setup/zone-delete`) |")
    lines.append("| `zones[]` | `id`, `name`, `mac`, `relayPin`, `enabled`: one entry per existing zone | — | read only (use `/api/setup/map-zone`) |")

    return "\n".join(lines)


def sync_setup_js(new_limits_js, check_only=False):
    """Update or verify PARAM_LIMITS in web/src/setup.js."""
    with open(SETUP_JS_PATH, "r", encoding="utf-8") as f:
        content = f.read()

    eol = "\r\n" if "\r\n" in content else "\n"
    norm_content = content.replace("\r\n", "\n")

    pattern = re.compile(r"  const PARAM_LIMITS = \{.*?\n  \};", re.DOTALL)
    match = pattern.search(norm_content)
    if not match:
        print(f"ERROR: Could not find PARAM_LIMITS block in {SETUP_JS_PATH}")
        return False

    current_block = match.group(0)
    if current_block == new_limits_js:
        return True

    if check_only:
        print(f"DRIFT: PARAM_LIMITS in {SETUP_JS_PATH} does not match ParamStore.cpp")
        return False

    updated_norm = norm_content[:match.start()] + new_limits_js + norm_content[match.end():]
    updated = updated_norm.replace("\n", eol)
    with open(SETUP_JS_PATH, "w", encoding="utf-8", newline="") as f:
        f.write(updated)
    print(f"Updated {SETUP_JS_PATH} from ParamStore.cpp")
    return True


def sync_api_reference(new_table_md, check_only=False):
    """Update or verify parameter table in docs/api_reference.md."""
    with open(API_REF_PATH, "r", encoding="utf-8") as f:
        content = f.read()

    eol = "\r\n" if "\r\n" in content else "\n"
    norm_content = content.replace("\r\n", "\n")

    # Look for table under /api/config/system
    pattern = re.compile(
        r"(\| Field \| §6 parameter \| Range \| POST \|\n\|---\|---\|---\|---\|\n(?:\| [^\n]+\n)+)"
    )
    match = pattern.search(norm_content)
    if not match:
        print(f"ERROR: Could not find parameter table in {API_REF_PATH}")
        return False

    current_table = match.group(1).rstrip("\n")
    if current_table == new_table_md:
        return True

    if check_only:
        print(f"DRIFT: Parameter table in {API_REF_PATH} does not match ParamStore.cpp")
        return False

    updated_norm = norm_content[:match.start(1)] + new_table_md + norm_content[match.end(1):]
    updated = updated_norm.replace("\n", eol)
    with open(API_REF_PATH, "w", encoding="utf-8", newline="") as f:
        f.write(updated)
    print(f"Updated {API_REF_PATH} from ParamStore.cpp")
    return True


def main():
    check_only = "--check" in sys.argv
    params = parse_param_store()
    if not params:
        print("ERROR: No parameters parsed from ParamStore.cpp")
        sys.exit(1)

    new_limits_js = generate_setup_limits_js(params)
    new_table_md = generate_api_ref_table(params)

    ok_js = sync_setup_js(new_limits_js, check_only=check_only)
    ok_md = sync_api_reference(new_table_md, check_only=check_only)

    if check_only:
        if ok_js and ok_md:
            print("[PASS] ParamStore is synchronized with docs/api_reference.md and web/src/setup.js")
            sys.exit(0)
        else:
            print("[FAIL] Drift detected between ParamStore.cpp and generated targets")
            sys.exit(1)
    else:
        print("[SUCCESS] All parameter metadata synchronized from ParamStore.cpp")


if __name__ == "__main__":
    main()
