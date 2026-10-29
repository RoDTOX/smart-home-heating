#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Telemetry Harvester & Thermal Inertia Research Analyzer
Pulls 24h telemetry and state events from ESP32, stores into CSV,
and calculates screed dead time (transport delay) and heating rates per zone.
"""

import os
import sys
import json
import urllib.request
from datetime import datetime

ESP_HOST = os.environ.get("ESP_HOST", "192.168.1.20")
DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
CSV_PATH = os.path.join(DATA_DIR, "telemetry_research.csv")

def fetch_json(endpoint):
    url = f"http://{ESP_HOST}{endpoint}"
    req = urllib.request.Request(url, headers={"User-Agent": "ThermalResearcher/1.0"})
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))

def fmt_val(v):
    if v is None:
        return ""
    return f"{v:.1f}"

def harvest():
    os.makedirs(DATA_DIR, exist_ok=True)
    
    print(f"[*] Fetching telemetry from {ESP_HOST}...")
    try:
        zones_data = fetch_json("/api/zones")
        hist_data = fetch_json("/api/history?range=24h")
    except Exception as e:
        print(f"[!] Connection failed: {e}")
        return

    points = hist_data.get("points", [])
    step_sec = hist_data.get("stepSec", 300)
    cur_slot = hist_data.get("currentSlot", 0)
    
    print(f"[+] Received {len(points)} slots (step {step_sec}s, curSlot: {cur_slot})")
    
    header_needed = not os.path.exists(CSV_PATH)
    new_rows = []
    now_str = datetime.now().isoformat()
    
    for p in points:
        slot, z1, z2, z3, out, flags = p
        boiler = int((flags & 1) != 0)
        pump = int((flags & 2) != 0)
        z1_v = int((flags & 4) != 0)
        z2_v = int((flags & 8) != 0)
        z3_v = int((flags & 16) != 0)
        valid = int((flags & 128) != 0)
        
        # Calculate relative age in minutes from current slot
        age_slots = (cur_slot - slot + 288) % 288
        age_min = age_slots * (step_sec // 60)
        
        row = f"{now_str},{slot},{age_min},{fmt_val(z1)},{fmt_val(z2)},{fmt_val(z3)},{fmt_val(out)},{boiler},{pump},{z1_v},{z2_v},{z3_v},{valid}\n"
        new_rows.append(row)

    with open(CSV_PATH, "w" if header_needed else "a", encoding="utf-8") as f:
        if header_needed:
            f.write("fetch_timestamp,slot,age_minutes,z1_temp,z2_temp,z3_temp,outdoor_temp,boiler_on,pump_on,z1_valve,z2_valve,z3_valve,valid\n")
        f.writelines(new_rows)

    print(f"[+] Saved {len(new_rows)} snapshots to {os.path.relpath(CSV_PATH)}")
    
    # Research Analysis: Identify Heating Cycles
    analyze_heating_cycles(points, step_sec, cur_slot)

def analyze_heating_cycles(points, step_sec, cur_slot):
    print("\n" + "=" * 60)
    print("🔬 THERMAL RESEARCH ANALYSIS: UNDERFLOOR SCREED INERTIA")
    print("=" * 60)
    
    # Sort points chronologically from oldest (age 287) to newest (age 0)
    chrono = []
    for p in points:
        slot, z1, z2, z3, out, flags = p
        if z1 is None or z2 is None or z3 is None:
            continue
        age_slots = (cur_slot - slot + 288) % 288
        chrono.append((age_slots, slot, z1, z2, z3, out, flags))
    chrono.sort(key=lambda x: -x[0]) # oldest first

    if not chrono:
        print("[!] No valid temperature readings in history buffer.")
        return

    # Find continuous boiler burn runs
    runs = []
    current_run = []
    
    for item in chrono:
        age_slots, slot, z1, z2, z3, out, flags = item
        boiler = (flags & 1) != 0
        if boiler:
            current_run.append(item)
        else:
            if len(current_run) >= 2: # At least 10 minutes
                runs.append(current_run)
            current_run = []
    if len(current_run) >= 2:
        runs.append(current_run)

    print(f"[i] Identified {len(runs)} significant active boiler cycles in the last 24h:\n")
    
    for idx, r in enumerate(runs, 1):
        start_age = r[0][0] * (step_sec // 60)
        end_age = r[-1][0] * (step_sec // 60)
        duration_min = len(r) * (step_sec // 60)
        
        z1_start, z1_end = r[0][2], r[-1][2]
        z2_start, z2_end = r[0][3], r[-1][3]
        z3_start, z3_end = r[0][4], r[-1][4]
        out_vals = [x[5] for x in r if x[5] is not None]
        out_avg = (sum(out_vals) / len(out_vals)) if out_vals else 0.0
        
        dz1 = z1_end - z1_start
        dz2 = z2_end - z2_start
        dz3 = z3_end - z3_start
        
        rate1 = (dz1 / (duration_min / 60)) if duration_min > 0 else 0
        rate2 = (dz2 / (duration_min / 60)) if duration_min > 0 else 0
        rate3 = (dz3 / (duration_min / 60)) if duration_min > 0 else 0
        
        print(f"Cycle #{idx}: Started ~{start_age}m ago, Ended ~{end_age}m ago | Duration: {duration_min} min active flame")
        print(f"  Outdoor Temp Avg: {out_avg:.1f}°C")
        print(f"  Zone 1 (Living):      {z1_start:.1f}°C -> {z1_end:.1f}°C (Delta: {dz1:+.1f}°C | Rate: {rate1:+.2f}°C/h)")
        print(f"  Zone 2 (Copil):       {z2_start:.1f}°C -> {z2_end:.1f}°C (Delta: {dz2:+.1f}°C | Rate: {rate2:+.2f}°C/h)")
        print(f"  Zone 3 (Matrimonial): {z3_start:.1f}°C -> {z3_end:.1f}°C (Delta: {dz3:+.1f}°C | Rate: {rate3:+.2f}°C/h)")
        print("-" * 60)

if __name__ == "__main__":
    harvest()
