#!/usr/bin/env python3
"""
ESP32 Smart Heating - Fast CLI & Automation Interface
Provides fast, direct command-line control and streaming logs for humans and AI agents.
"""

import sys
import os
import argparse
import socket
import time
import requests

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

DEFAULT_IP = os.environ.get("ESP_HEATING_IP", "192.168.1.20")
DEFAULT_PORT_HTTP = 80
DEFAULT_PORT_TELNET = 23
DEFAULT_TIMEOUT = 5.0

def get_link_quality(rssi):
    if rssi >= -72: return "EXCELLENT"
    elif rssi >= -82: return "GOOD"
    elif rssi >= -88: return "MARGINAL"
    else: return "POOR"

def cmd_status(args):
    url_status = f"http://{args.ip}:{args.port}/api/boiler/status"
    url_zones = f"http://{args.ip}:{args.port}/api/zones"
    try:
        r_stat = requests.get(url_status, timeout=DEFAULT_TIMEOUT)
        r_zon = requests.get(url_zones, timeout=DEFAULT_TIMEOUT)
        
        if r_stat.status_code == 200 and r_zon.status_code == 200:
            sdata = r_stat.json()
            zdata = r_zon.json()
            
            print("==================================================")
            print("  ESP32 Smart Heating - Live Status Report")
            print("==================================================")
            ver = sdata.get('firmwareVersion', '1.2.0')
            bld = sdata.get('buildDate', 'N/A')
            git_c = sdata.get('gitCommit', 'N/A')
            rst = sdata.get('resetReason', 'Normal Boot')
            print(f"Firmware Version:   v{ver} (Build: {bld} | Git: {git_c})")
            print(f"Reset Cause:        {rst}")
            print(f"Boiler State:       {'FIRE ACTIVE [ON]' if sdata.get('boiler') == 'ON' else 'IDLE [OFF]'}")
            print(f"Thermal Demand:     {'DEMAND ACTIVE' if sdata.get('boilerDemand') else 'NO DEMAND'}")
            print(f"Operation Mode:     {'AUTOMATIC' if sdata.get('autoMode') else 'MANUAL'}")
            print(f"Actuator Type:      {zdata.get('actuatorType', 'NC')} ({'Normally Closed' if zdata.get('actuatorType') == 'NC' else 'Normally Open'})")
            print(f"Circulation Pump:   {'RUNNING [ON]' if zdata.get('pumpActive') else 'IDLE [OFF]'} (Relay 4: {'ENABLED' if zdata.get('pumpEnabled') else 'DISABLED'})")
            print(f"Frost Guard:        {'ACTIVE (<12°C)' if zdata.get('frostGuardActive') else 'STANDBY'}")
            if sdata.get('scheduleEnabled'):
                print(f"Bio-Schedule:       ACTIVE ({sdata.get('activeAnchor')}) | Pre-Heat: {'ACTIVE' if sdata.get('preHeatActive') else 'STANDBY'}")
            tout = sdata.get('sensorTimeoutSec', 600)
            print(f"Sensor Timeout:     {tout}s ({tout // 60} min)")
            print(f"Free Heap RAM:      {sdata.get('freeHeap', 0) // 1024} KB")
            print(f"Wi-Fi RSSI:         {sdata.get('rssi')} dBm")
            print(f"System Uptime:      {sdata.get('uptimeSec')}s")
            print("--------------------------------------------------")
            print("Thermal Zones (Underfloor Loops):")
            for z in zdata.get("zones", []):
                cur_t = z.get('currentTemp')
                tgt_t = z.get('effectiveTarget', z.get('targetTemp', 0.0))
                temp_str = f"{cur_t:.1f}°C" if z.get('sensorValid') else "--.-°C"
                relay_str = "ON" if z.get('relayEngaged') else "OFF"
                fault_tag = " [SENSOR FAULT]" if z.get('sensorFault') else ""
                frost_tag = " [FROST GUARD]" if z.get('frostActive') else ""
                win_tag = f" [WINDOW PAUSE {z.get('windowPauseRemainingSec')}s]" if z.get('windowPauseActive') else ""
                print(f"  [Zone {z.get('id')}] {z.get('name'):<22}: {temp_str} / {tgt_t:.1f}°C | State: {z.get('state'):<20} | Relay: {relay_str}{fault_tag}{frost_tag}{win_tag}")
            print("--------------------------------------------------")
            print("Room Sensors (Xiaomi BLE Direct):")
            for s in sdata.get("sensors", []):
                rssi = s.get('rssi', -99)
                q = get_link_quality(rssi)
                print(f"  - {s.get('name'):<15} ({s.get('mac')}): {s.get('temp', 0):.1f}°C, {s.get('hum', 0):.0f}%, Bat: {s.get('bat')}%, RSSI: {rssi} dBm [{q}] (seen {s.get('ageSec')}s ago)")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: status={r_stat.status_code}, zones={r_zon.status_code}")
    except Exception as e:
        print(f"[-] Connection Error: {e}")

def cmd_ble(args):
    url = f"http://{args.ip}:{args.port}/api/ble/sensors"
    try:
        r = requests.get(url, timeout=DEFAULT_TIMEOUT)
        if r.status_code == 200:
            data = r.json()
            print("==================================================")
            print(f"  ESP32 BLE Sensors Registry (Scanning: {'ACTIVE' if data.get('scanning') else 'INACTIVE'})")
            print("==================================================")
            sensors = data.get("sensors", [])
            if not sensors:
                print("  No BLE thermometers decoded yet (listening for broadcasts)...")
            for i, s in enumerate(sensors, 1):
                name = s.get('name', 'Unknown')
                mac = s.get('mac', '')
                temp = s.get('temp', 0.0)
                hum = s.get('hum', 0.0)
                bat = s.get('bat', 0)
                rssi = s.get('rssi', 0)
                q = get_link_quality(rssi)
                pkts = s.get('packets', 0)
                src = s.get('source', '')
                age = s.get('ageSec', 0)
                print(f"  [{i}] {name:<16} ({mac}) | {temp:.2f}°C | {hum:.1f}% | Bat: {bat}% | RSSI: {rssi} dBm [{q}] | Pkts: {pkts} | {src} ({age}s ago)")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_diag(args):
    url = f"http://{args.ip}:{args.port}/api/diag/status"
    try:
        r = requests.get(url, timeout=DEFAULT_TIMEOUT)
        if r.status_code == 200:
            d = r.json()
            score = d.get('healthScore', 100)
            dtcs = d.get('dtcs', [])
            print("==================================================")
            print(f"  ESP32 Health & Diagnostics (Score: {score}%)")
            print("==================================================")
            print(f"Active DTC Codes:   {len(dtcs)}")
            if not dtcs:
                print("  [OK] All subsystems nominal. Zero active trouble codes.")
            for item in dtcs:
                print(f"  * [{item.get('code')}] {item.get('severity')}: {item.get('message')} (x{item.get('count')}, {item.get('ageSec')}s ago)")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_selftest(args):
    url = f"http://{args.ip}:{args.port}/api/diag/selftest"
    try:
        r = requests.get(url, timeout=5)
        print(r.text)
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_passport(args):
    url = f"http://{args.ip}:{args.port}/api/setup/passport"
    try:
        r = requests.get(url, timeout=DEFAULT_TIMEOUT)
        if r.status_code == 200:
            d = r.json()
            print("==================================================")
            print("  Device Installation Passport (NVS)")
            print("==================================================")
            print(f"Serial Number:  {d.get('serialNumber')}")
            print(f"MAC Address:    {d.get('macAddress')}")
            print(f"Commissioned:   {'YES' if d.get('isCommissioned') else 'NO (Factory Default)'}")
            print(f"Customer Name:  {d.get('customerName') or 'Unassigned'}")
            print(f"Address:        {d.get('customerAddress') or 'Unassigned'}")
            print(f"GPS Coords:     {d.get('locationCoords') or 'Unassigned'}")
            print(f"Installed Date: {d.get('commissionDate') or 'N/A'} ({d.get('ageDays', 0)} days active)")
            print(f"Notes:          {d.get('installerNotes') or 'None'}")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: {r.status_code}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_time(args):
    url = f"http://{args.ip}:{args.port}/api/setup/time"
    try:
        r = requests.get(url, timeout=DEFAULT_TIMEOUT)
        if r.status_code == 200:
            d = r.json()
            print("==================================================")
            print("  System Time & Outdoor Weather (Wi-Fi NTP & REST)")
            print("==================================================")
            print(f"Local Time:     {d.get('time')} ({d.get('dayOfWeek')})")
            print(f"Date:           {d.get('date')} (Year: {d.get('year')})")
            print(f"NTP Synced:     {'YES' if d.get('timeSynced') else 'NO'}")
            print(f"Season:         {d.get('season')}")
            print(f"Heating Mode:   {'ACTIVE (Winter)' if d.get('isHeatingSeason') else 'OFF-SEASON (Summer)'}")
            ot = d.get('outdoorTemp')
            oh = d.get('outdoorHumidity')
            if d.get('weatherValid') and ot is not None:
                print(f"Outdoor Temp:   {ot:.1f} C (Humidity: {oh:.1f}%) [Open-Meteo API]")
            else:
                print("Outdoor Temp:   Pending")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: {r.status_code}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_boiler(args, action):
    url = f"http://{args.ip}:{args.port}/api/boiler/{action}"
    if getattr(args, 'force', False):
        url += "?force=true"
    try:
        r = requests.post(url, timeout=DEFAULT_TIMEOUT)
        if r.status_code == 200:
            d = r.json()
            if d.get("idempotent"):
                print(f"[i] Centrala este deja in starea ceruta ({action.upper()}). Transmisia RF a fost omisa (idempotent no-op).")
            else:
                print(f"[+] Comanda executata cu succes: Boiler {action.upper()} transmis via RF 433MHz.")
        elif r.status_code == 429:
            try:
                d = r.json()
                wait = d.get('waitSec', 0)
                err = d.get('error', 'Rate limited')
                cur = d.get('boiler', 'UNKNOWN')
                print(f"[!] Comanda respinsa de siguranta (HTTP 429): {err}")
                print(f"    Stare curenta cazan: {cur} | Timp minim ramas pana la deblocare: {wait} secunde.")
                if action == "off":
                    print(f"    (Sugestie: Foloseste optiunea '--force' pentru oprire imediata).")
            except Exception:
                print(f"[!] Respins de siguranta (HTTP 429): {r.text}")
        else:
            print(f"[-] HTTP Error {r.status_code}: {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_mode(args):
    url = f"http://{args.ip}:{args.port}/api/mode?mode={args.mode.lower()}"
    try:
        r = requests.post(url, timeout=DEFAULT_TIMEOUT)
        print(f"[+] Mode switch response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_setpoint(args):
    url = f"http://{args.ip}:{args.port}/api/zones/setpoint?id={args.zone}&target={args.temp}"
    try:
        r = requests.post(url, timeout=DEFAULT_TIMEOUT)
        print(f"[+] Setpoint Zone {args.zone} -> {args.temp}°C response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_pump(args):
    """View or configure circulation pump (Relay 4)."""
    if args.action:
        act = args.action.lower()
        if act in ("enable", "on", "1"):
            url = f"http://{args.ip}:{args.port}/api/setup/pump?enabled=true"
        elif act in ("disable", "off", "0"):
            url = f"http://{args.ip}:{args.port}/api/setup/pump?enabled=false"
        else:
            print(f"[-] Unknown pump action: {args.action}")
            return
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Pump control response: {r.status_code} {r.text}")
        except Exception as e:
            print(f"[-] Error: {e}")
    else:
        try:
            r = requests.get(f"http://{args.ip}:{args.port}/api/setup/pump", timeout=DEFAULT_TIMEOUT)
            d = r.json()
            print("==================================================")
            print("  Circulation Pump Configuration (Relay 4)")
            print("==================================================")
            print(f"Pump Control:   {'ENABLED' if d.get('pumpEnabled') else 'DISABLED'}")
            print(f"Pump State:     {'RUNNING [ON]' if d.get('pumpActive') else 'IDLE [OFF]'}")
            print("==================================================")
        except Exception as e:
            print(f"[-] Error: {e}")

def cmd_actuator(args):
    """View or configure actuator electrical polarity (NC vs NO)."""
    if args.type:
        t = args.type.lower()
        url = f"http://{args.ip}:{args.port}/api/setup/actuator-type?type={t}"
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Actuator type response: {r.status_code} {r.text}")
        except Exception as e:
            print(f"[-] Error: {e}")
    else:
        try:
            r = requests.get(f"http://{args.ip}:{args.port}/api/setup/actuator-type", timeout=DEFAULT_TIMEOUT)
            d = r.json()
            at = d.get('actuatorType', 'NC')
            desc = 'Normally Closed (Energize 230V to Open)' if at == 'NC' else 'Normally Open (De-energize 230V to Open)'
            print("==================================================")
            print("  Actuator Type & Electrical Polarity")
            print("==================================================")
            print(f"Current Type:   {at} - {desc}")
            print("==================================================")
        except Exception as e:
            print(f"[-] Error: {e}")

def cmd_exercise(args):
    """Trigger 20s anti-seize cycle for circulation pump and underfloor manifold valves."""
    url = f"http://{args.ip}:{args.port}/api/diag/exercise"
    try:
        r = requests.post(url, timeout=5)
        print(f"[+] Anti-seize exercise response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_timeout(args):
    """View or configure sensor failsafe timeout delay (seconds)."""
    if args.sec is not None:
        url = f"http://{args.ip}:{args.port}/api/setup/sensor-timeout?timeoutSec={args.sec}"
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Sensor timeout response: {r.status_code} {r.text}")
        except Exception as e:
            print(f"[-] Error: {e}")
    else:
        try:
            r = requests.get(f"http://{args.ip}:{args.port}/api/setup/sensor-timeout", timeout=DEFAULT_TIMEOUT)
            d = r.json()
            tout = d.get('sensorTimeoutSec', 600)
            print("==================================================")
            print("  Sensor Ingestion Failsafe Timeout")
            print("==================================================")
            print(f"Current Timeout: {tout} seconds ({tout // 60} minutes)")
            print("==================================================")
        except Exception as e:
            print(f"[-] Error: {e}")

def cmd_schedule(args):
    """View or configure Smart Bio-Schedule & natural anchors."""
    action = args.action
    if isinstance(action, list):
        action = " ".join(action) if len(action) > 0 else ""
    elif action is None:
        action = ""
    action_lower = action.lower().strip()

    if action_lower in ("on", "off"):
        val = "true" if action_lower == "on" else "false"
        url = f"http://{args.ip}:{args.port}/api/schedule/enable?enabled={val}"
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Bio-Schedule toggle ({action_lower}): HTTP {r.status_code}")
        except Exception as e:
            print(f"[-] Error: {e}")
    elif action_lower in ("preheat-on", "preheat-off"):
        val = "true" if action_lower == "preheat-on" else "false"
        url = f"http://{args.ip}:{args.port}/api/schedule/enable?preheat={val}"
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Predictive Pre-Heat toggle ({action_lower}): HTTP {r.status_code}")
        except Exception as e:
            print(f"[-] Error: {e}")
    elif action_lower in ("window-on", "window-off"):
        val = "true" if action_lower == "window-on" else "false"
        url = f"http://{args.ip}:{args.port}/api/schedule/enable?window={val}"
        try:
            r = requests.post(url, timeout=DEFAULT_TIMEOUT)
            print(f"[+] Open Window Detection toggle ({action_lower}): HTTP {r.status_code}")
        except Exception as e:
            print(f"[-] Error: {e}")
    elif action_lower == "reset":
        try:
            r = requests.post(f"http://{args.ip}:{args.port}/api/schedule/reset", timeout=DEFAULT_TIMEOUT)
            print(f"[+] Bio-Schedule reset to factory DEFAULT preset: HTTP {r.status_code}")
        except Exception as e:
            print(f"[-] Error: {e}")
    elif action_lower.startswith("preset"):
        parts = action_lower.split()
        subcmd = parts[1] if len(parts) > 1 else "list"
        if subcmd == "list":
            try:
                r = requests.get(f"http://{args.ip}:{args.port}/api/schedule/presets", timeout=DEFAULT_TIMEOUT)
                d = r.json()
                print("==================================================")
                print("  Bio-Schedule Presets Engine")
                print("==================================================")
                print(f"Active Preset: {d.get('activePreset', 'N/A').upper()}")
                print("Available Presets:")
                for p in d.get("presets", []):
                    saved_tag = " (Saved)" if p.get("saved") else ""
                    print(f"  - {p['id']:<10} : {p['name']} [{p['type'].upper()}]{saved_tag}")
                print("==================================================")
            except Exception as e:
                print(f"[-] Error: {e}")
        elif subcmd == "apply" and len(parts) > 2:
            pid = parts[2]
            try:
                r = requests.post(f"http://{args.ip}:{args.port}/api/schedule/preset/apply?id={pid}", timeout=DEFAULT_TIMEOUT)
                if r.status_code == 200:
                    print(f"[+] Applied Bio-Schedule preset '{pid}' successfully.")
                else:
                    print(f"[-] Failed to apply preset '{pid}': HTTP {r.status_code} {r.text}")
            except Exception as e:
                print(f"[-] Error: {e}")
        elif subcmd == "save" and len(parts) > 2:
            pid = parts[2]
            orig_parts = action.split()
            pname = " ".join(orig_parts[3:]) if len(orig_parts) > 3 else pid
            try:
                r = requests.post(f"http://{args.ip}:{args.port}/api/schedule/preset/save?id={pid}&name={pname}", timeout=DEFAULT_TIMEOUT)
                if r.status_code == 200:
                    print(f"[+] Saved active schedule to preset '{pid}' ({pname}) successfully.")
                else:
                    print(f"[-] Failed to save preset: HTTP {r.status_code} {r.text}")
            except Exception as e:
                print(f"[-] Error: {e}")
        else:
            print("[-] Usage: schedule preset <list | apply <id> | save <id> [name]>")
    elif action_lower.startswith("set"):
        parts = action_lower.split()
        if len(parts) >= 4:
            # schedule set <weekday|weekend> <anchor> <HH:MM> [z1] [z2] [z3]
            prof, anc, tm = parts[1], parts[2], parts[3]
            url = f"http://{args.ip}:{args.port}/api/schedule/anchor?profile={prof}&anchor={anc}&time={tm}"
            if len(parts) > 4: url += f"&z1={parts[4]}"
            if len(parts) > 5: url += f"&z2={parts[5]}"
            if len(parts) > 6: url += f"&z3={parts[6]}"
            try:
                r = requests.post(url, timeout=DEFAULT_TIMEOUT)
                if r.status_code == 200:
                    print(f"[+] Updated anchor {anc.upper()} ({prof}) at {tm}: HTTP 200 OK")
                else:
                    print(f"[-] Failed to update anchor: HTTP {r.status_code} {r.text}")
            except Exception as e:
                print(f"[-] Error: {e}")
        else:
            print("[-] Usage: schedule set <weekday|weekend> <anchor> <HH:MM> [z1] [z2] [z3]")
    else:
        try:
            r = requests.get(f"http://{args.ip}:{args.port}/api/schedule", timeout=DEFAULT_TIMEOUT)
            d = r.json()
            print("==================================================")
            print("  Smart Bio-Schedule & Natural Anchors")
            print("==================================================")
            print(f"Schedule Engine:     {'ENABLED' if d.get('enabled') else 'DISABLED'}")
            print(f"Active Preset:       {d.get('activePreset', 'default').upper()}")
            print(f"Active Anchor:       {d.get('activeAnchor', 'NONE')}")
            print(f"Next Anchor:         {d.get('nextAnchor', 'NONE')} in {d.get('minsToNextAnchor', 0)} min")
            print(f"Predictive Pre-Heat: {'ACTIVE (Pre-Warming)' if d.get('preHeatActive') else ('ENABLED' if d.get('preHeatEnabled') else 'DISABLED')}")
            print(f"Open Window Guard:   {'ARMED (>1.2°C drop in 5m)' if d.get('windowDetectionEnabled') else 'DISABLED'}")
            print("--------------------------------------------------")
            print("Weekday Profile (Mon-Fri):")
            wd = d.get("weekday", {})
            if isinstance(wd, dict):
                for name, anc in wd.items():
                    t_str = " | ".join(f"Z{idx+1}: {t:.1f}°C" for idx, t in enumerate(anc.get("targets", [])))
                    print(f"  - {name.upper():<6} @ {anc.get('hour', 0):02d}:{anc.get('minute', 0):02d} -> {t_str}")
            elif isinstance(wd, list):
                for anc in wd:
                    t_str = " | ".join(f"Z{idx+1}: {t:.1f}°C" for idx, t in enumerate(anc.get("targets", [])))
                    print(f"  - {anc.get('name', 'ANCHOR'):<6} @ {anc.get('hour', 0):02d}:{anc.get('minute', 0):02d} -> {t_str}")
            print("Weekend Profile (Sat-Sun):")
            we = d.get("weekend", {})
            if isinstance(we, dict):
                for name, anc in we.items():
                    t_str = " | ".join(f"Z{idx+1}: {t:.1f}°C" for idx, t in enumerate(anc.get("targets", [])))
                    print(f"  - {name.upper():<6} @ {anc.get('hour', 0):02d}:{anc.get('minute', 0):02d} -> {t_str}")
            elif isinstance(we, list):
                for anc in we:
                    t_str = " | ".join(f"Z{idx+1}: {t:.1f}°C" for idx, t in enumerate(anc.get("targets", [])))
                    print(f"  - {anc.get('name', 'ANCHOR'):<6} @ {anc.get('hour', 0):02d}:{anc.get('minute', 0):02d} -> {t_str}")
            print("==================================================")
        except Exception as e:
            print(f"[-] Error: {e}")

def cmd_exec(args):
    """Executes any arbitrary command via the 2-way Network Console API."""
    url = f"http://{args.ip}:{args.port}/api/console/send?cmd={args.cmd}"
    try:
        r = requests.post(url, timeout=DEFAULT_TIMEOUT)
        print(f"[+] Command '{args.cmd}' sent: {r.status_code} {r.text}")
        time.sleep(0.3)
        r_log = requests.get(f"http://{args.ip}:{args.port}/api/console/logs", timeout=DEFAULT_TIMEOUT)
        r_log.encoding = 'utf-8'
        lines = r_log.text.strip().split("\n")
        print("[+] Output:")
        cmd_marker = f"[CMD] > {args.cmd}".upper()
        start_idx = -1
        for idx in range(len(lines) - 1, -1, -1):
            if cmd_marker in lines[idx].upper():
                start_idx = idx
                break
        if start_idx != -1:
            for line in lines[start_idx:]:
                print(f"    {line}")
        else:
            for line in lines[-20:]:
                print(f"    {line}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_monitor(args):
    """Connects to Telnet TCP:23 and streams logs live like 'ssh' or 'tail -f'."""
    print(f"[*] Connecting to live terminal stream at {args.ip}:{args.telnet_port} (Ctrl+C to exit)...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((args.ip, args.telnet_port))
        s.setblocking(True)
        print("[+] Connected! Live stream active:\n")
        while True:
            data = s.recv(1024)
            if not data:
                break
            sys.stdout.write(data.decode('utf-8', errors='ignore'))
            sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n[*] Monitoring session ended.")
    except Exception as e:
        print(f"\n[-] Socket error: {e}")

def cmd_interactive_shell(args):
    """Interactive REPL shell for instant command execution, live testing, and telemetry."""
    print("==================================================")
    print("  ESP32 Smart Heating - Interactive Command Shell")
    print(f"  Target: http://{args.ip}:{args.port} | Telnet: {args.ip}:{args.telnet_port}")
    print("  Type 'help' for available commands, 'exit' to quit.")
    print("==================================================")
    
    # Run initial status
    cmd_status(args)
    
    while True:
        try:
            line = input("\nsmart-heating> ").strip()
            if not line:
                continue
            parts = line.split()
            cmd = parts[0].lower()
            
            if cmd in ("exit", "quit", "q"):
                print("Exiting interactive shell.")
                break
            elif cmd in ("help", "?"):
                print("\nInteractive Shell Commands:")
                print("  status                 - Live telemetry status report")
                print("  sensors / ble          - List discovered BLE room thermometers")
                print("  on                     - Turn gas boiler ON via RF 433MHz")
                print("  off [--force]          - Turn gas boiler OFF via RF 433MHz")
                print("  mode <auto|manual>     - Set controller operating mode")
                print("  set <zone_id> <temp>   - Set zone target temperature (e.g. set 1 22.5)")
                print("  diag                   - Diagnostic trouble codes & health score")
                print("  selftest               - Run diagnostic self-test suite")
                print("  clear                  - Clear active diagnostic trouble codes (DTCs)")
                print("  schedule [action]      - Bio-Schedule (actions: on, off, preheat-on, window-on)")
                print("  passport               - Device identity, hardware serial, MAC")
                print("  time                   - System time, NTP & outdoor weather")
                print("  pump [on|off]          - GPA circulation pump relay toggle")
                print("  actuator [nc|no]       - Thermal actuator electrical polarity")
                print("  exercise               - 20s anti-seize exercise cycle (pump & valves)")
                print("  test                   - Run automated ASPICE verification suite")
                print("  monitor                - Stream live logs over Telnet TCP:23")
                print("  exec <cmd>             - Execute raw Virtual Terminal command (e.g. exec HELP)")
                print("  exit / quit            - Exit shell")
            elif cmd == "status":
                cmd_status(args)
            elif cmd in ("ble", "sensors"):
                cmd_ble(args)
            elif cmd == "diag":
                args.action = parts[1].lower() if len(parts) > 1 else None
                cmd_diag(args)
            elif cmd == "selftest":
                cmd_selftest(args)
            elif cmd == "clear":
                args.action = "clear"
                cmd_diag(args)
            elif cmd in ("on", "off"):
                args.force = cmd == "off" and "--force" in parts
                cmd_boiler(args, cmd)
            elif cmd == "mode" and len(parts) > 1:
                args.mode = parts[1].lower()
                cmd_mode(args)
            elif cmd == "set" and len(parts) >= 3:
                try:
                    args.zone = int(parts[1])
                    args.temp = float(parts[2])
                    cmd_setpoint(args)
                except ValueError:
                    print("[-] Usage: set <zone_id> <temperature> (e.g. set 1 22.5)")
            elif cmd == "schedule":
                args.action = parts[1:] if len(parts) > 1 else None
                cmd_schedule(args)
            elif cmd == "passport":
                cmd_passport(args)
            elif cmd == "time":
                cmd_time(args)
            elif cmd == "pump":
                args.action = parts[1].lower() if len(parts) > 1 else None
                cmd_pump(args)
            elif cmd == "actuator":
                args.type = parts[1].lower() if len(parts) > 1 else None
                cmd_actuator(args)
            elif cmd == "exercise":
                cmd_exercise(args)
            elif cmd == "test":
                from test_system import SystemTestSuite
                suite = SystemTestSuite(ip=args.ip, port=args.port)
                suite.run_all()
            elif cmd == "monitor":
                print("[*] Streaming live Telnet logs (Ctrl+C to return to shell)...")
                try:
                    cmd_monitor(args)
                except KeyboardInterrupt:
                    print("\n[*] Stopped log stream.")
            elif cmd == "exec":
                if len(parts) > 1:
                    args.cmd = " ".join(parts[1:])
                    cmd_exec(args)
                else:
                    print("[-] Usage: exec <command> (e.g. exec HELP)")
            else:
                # Direct passthrough for ESP32 console commands (e.g. HELP, STATUS, AUTO, MANUAL, SET 1 22)
                args.cmd = line
                cmd_exec(args)
        except KeyboardInterrupt:
            print("\nExiting interactive shell.")
            break
        except Exception as e:
            print(f"[-] Error: {e}")

def main():
    parser = argparse.ArgumentParser(description="ESP32 Smart Heating Fast CLI & Interactive Shell")
    parser.add_argument("--ip", default=DEFAULT_IP, help=f"ESP32 IP address (default: {DEFAULT_IP})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT_HTTP, help="HTTP Port (default: 80)")
    parser.add_argument("--telnet-port", type=int, default=DEFAULT_PORT_TELNET, help="Telnet Port (default: 23)")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("shell", help="Start interactive REPL console (default when run without arguments)")
    sub.add_parser("status", help="Get comprehensive system status report")
    sub.add_parser("ble", help="List BLE room thermometers registry")
    sub.add_parser("sensors", help="Alias for ble command")
    sub.add_parser("diag", help="Show system diagnostics and active DTC codes")
    sub.add_parser("selftest", help="Run comprehensive onboard self-test suite")
    sub.add_parser("passport", help="View device commissioning passport and metadata")
    sub.add_parser("time", help="View NTP calendar, season and outdoor weather")
    p_on = sub.add_parser("on", help="Trigger boiler START via RF 433MHz")
    p_off = sub.add_parser("off", help="Trigger boiler STOP via RF 433MHz")
    p_off.add_argument("--force", action="store_true", help="Force immediate STOP bypassing minimum run time")

    p_pump = sub.add_parser("pump", help="View or configure circulation pump (Relay 4)")
    p_pump.add_argument("action", nargs="?", choices=["enable", "disable", "on", "off"], help="enable or disable pump control")

    p_act = sub.add_parser("actuator", help="View or configure actuator polarity (NC vs NO)")
    p_act.add_argument("type", nargs="?", choices=["nc", "no"], help="nc or no")

    sub.add_parser("exercise", help="Trigger 20s anti-seize exercise cycle (pump & valves)")

    p_tout = sub.add_parser("timeout", help="View or configure sensor failsafe timeout (seconds)")
    p_tout.add_argument("sec", nargs="?", type=int, default=None, help="Timeout in seconds (e.g. 600)")

    p_mode = sub.add_parser("mode", help="Set operating mode")
    p_mode.add_argument("mode", choices=["auto", "manual"], help="auto or manual")

    p_set = sub.add_parser("set", help="Set zone target temperature")
    p_set.add_argument("zone", type=int, help="Zone index (0, 1, 2)")
    p_set.add_argument("temp", type=float, help="Target temperature in Celsius (e.g. 22.5)")

    p_exec = sub.add_parser("exec", help="Execute arbitrary command string via Network Console")
    p_exec.add_argument("cmd", help="Command string (e.g. STATUS, HELP, ON, OFF)")

    p_sched = sub.add_parser("schedule", help="View or configure Smart Bio-Schedule")
    p_sched.add_argument("action", nargs="*", help="Action to perform (on, off, reset, preset list/apply/save, set)")

    p_test = sub.add_parser("test", help="Run automated ASPICE SWE.5 verification test suite against ESP32")
    p_test.add_argument("--report", default="docs/system_test_report.md", help="Path to write markdown test report")

    sub.add_parser("monitor", help="Stream live logs in real time via TCP:23")

    args = parser.parse_args()

    if not args.command or args.command == "shell":
        cmd_interactive_shell(args)
    elif args.command == "status":
        cmd_status(args)
    elif args.command in ("ble", "sensors"):
        cmd_ble(args)
    elif args.command == "diag":
        cmd_diag(args)
    elif args.command == "selftest":
        cmd_selftest(args)
    elif args.command == "passport":
        cmd_passport(args)
    elif args.command == "time":
        cmd_time(args)
    elif args.command == "pump":
        cmd_pump(args)
    elif args.command == "actuator":
        cmd_actuator(args)
    elif args.command == "exercise":
        cmd_exercise(args)
    elif args.command == "timeout":
        cmd_timeout(args)
    elif args.command == "on":
        cmd_boiler(args, "on")
    elif args.command == "off":
        cmd_boiler(args, "off")
    elif args.command == "mode":
        cmd_mode(args)
    elif args.command == "set":
        cmd_setpoint(args)
    elif args.command == "schedule":
        cmd_schedule(args)
    elif args.command == "exec":
        cmd_exec(args)
    elif args.command == "test":
        from test_system import SystemTestSuite
        suite = SystemTestSuite(ip=args.ip, port=args.port)
        ok = suite.run_all()
        if args.report:
            suite.generate_markdown_report(args.report)
        sys.exit(0 if ok else 1)
    elif args.command == "monitor":
        cmd_monitor(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
