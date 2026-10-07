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

DEFAULT_IP = os.environ.get("ESP_HEATING_IP", "192.168.1.20")
DEFAULT_PORT_HTTP = 80
DEFAULT_PORT_TELNET = 23

def cmd_status(args):
    url_status = f"http://{args.ip}:{args.port}/api/boiler/status"
    url_zones = f"http://{args.ip}:{args.port}/api/zones"
    try:
        r_stat = requests.get(url_status, timeout=3)
        r_zon = requests.get(url_zones, timeout=3)
        
        if r_stat.status_code == 200 and r_zon.status_code == 200:
            sdata = r_stat.json()
            zdata = r_zon.json()
            
            print("==================================================")
            print("  ESP32 Smart Heating - Live Status Report")
            print("==================================================")
            print(f"Boiler State:       {'FIRE ACTIVE [ON]' if sdata.get('boiler') == 'ON' else 'IDLE [OFF]'}")
            print(f"Thermal Demand:     {'DEMAND ACTIVE' if sdata.get('boilerDemand') else 'NO DEMAND'}")
            print(f"Operation Mode:     {'AUTOMATIC' if sdata.get('autoMode') else 'MANUAL'}")
            print(f"MQTT Broker:        {'CONNECTED (Galaxy A6)' if sdata.get('mqttConnected') else 'DISCONNECTED'}")
            print(f"Free Heap RAM:      {sdata.get('freeHeap', 0) // 1024} KB")
            print(f"Wi-Fi RSSI:         {sdata.get('rssi')} dBm")
            print(f"System Uptime:      {sdata.get('uptimeSec')}s")
            print("--------------------------------------------------")
            print("Thermal Zones (Underfloor Loops):")
            for z in zdata.get("zones", []):
                cur_t = z.get('currentTemp')
                tgt_t = z.get('targetTemp', 0.0)
                temp_str = f"{cur_t:.1f}°C" if z.get('sensorValid') else "--.-°C"
                relay_str = "ON" if z.get('relayEngaged') else "OFF"
                print(f"  [Zone {z.get('id')}] {z.get('name'):<22}: {temp_str} / {tgt_t:.1f}°C | State: {z.get('state'):<16} | Relay: {relay_str}")
            print("--------------------------------------------------")
            print("MQTT Room Sensors (Xiaomi BLE):")
            for s in sdata.get("sensors", []):
                print(f"  - {s.get('name'):<15} ({s.get('mac')}): {s.get('temp', 0):.1f}°C, {s.get('hum', 0):.0f}%, Bat: {s.get('bat')}%, RSSI: {s.get('rssi')} dBm (seen {s.get('ageSec')}s ago)")
            print("==================================================")
        else:
            print(f"[-] HTTP Error: status={r_stat.status_code}, zones={r_zon.status_code}")
    except Exception as e:
        print(f"[-] Connection Error: {e}")

def cmd_boiler(args, action):
    url = f"http://{args.ip}:{args.port}/api/boiler/{action}"
    try:
        r = requests.post(url, timeout=3)
        print(f"[+] Boiler {action.upper()} response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_mode(args):
    url = f"http://{args.ip}:{args.port}/api/mode?mode={args.mode.lower()}"
    try:
        r = requests.post(url, timeout=3)
        print(f"[+] Mode switch response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_setpoint(args):
    url = f"http://{args.ip}:{args.port}/api/zones/setpoint?id={args.zone}&target={args.temp}"
    try:
        r = requests.post(url, timeout=3)
        print(f"[+] Setpoint Zone {args.zone} -> {args.temp}°C response: {r.status_code} {r.text}")
    except Exception as e:
        print(f"[-] Error: {e}")

def cmd_exec(args):
    """Executes any arbitrary command via the 2-way Network Console API."""
    url = f"http://{args.ip}:{args.port}/api/console/send?cmd={args.cmd}"
    try:
        r = requests.post(url, timeout=3)
        print(f"[+] Command '{args.cmd}' sent: {r.status_code} {r.text}")
        time.sleep(0.3)
        r_log = requests.get(f"http://{args.ip}:{args.port}/api/console/logs", timeout=3)
        lines = r_log.text.strip().split("\n")
        print("[+] Output tail:")
        for line in lines[-6:]:
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

def main():
    parser = argparse.ArgumentParser(description="ESP32 Smart Heating Fast CLI")
    parser.add_argument("--ip", default=DEFAULT_IP, help=f"ESP32 IP address (default: {DEFAULT_IP})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT_HTTP, help="HTTP Port (default: 80)")
    parser.add_argument("--telnet-port", type=int, default=DEFAULT_PORT_TELNET, help="Telnet Port (default: 23)")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("status", help="Get comprehensive system status report")
    sub.add_parser("on", help="Trigger boiler START via RF 433MHz")
    sub.add_parser("off", help="Trigger boiler STOP via RF 433MHz")

    p_mode = sub.add_parser("mode", help="Set operating mode")
    p_mode.add_argument("mode", choices=["auto", "manual"], help="auto or manual")

    p_set = sub.add_parser("set", help="Set zone target temperature")
    p_set.add_argument("zone", type=int, help="Zone index (0, 1, 2)")
    p_set.add_argument("temp", type=float, help="Target temperature in Celsius (e.g. 22.5)")

    p_exec = sub.add_parser("exec", help="Execute arbitrary command string via Network Console")
    p_exec.add_argument("cmd", help="Command string (e.g. STATUS, HELP, ON, OFF)")

    sub.add_parser("monitor", help="Stream live logs in real time via TCP:23")

    args = parser.parse_args()

    if args.command == "status":
        cmd_status(args)
    elif args.command == "on":
        cmd_boiler(args, "on")
    elif args.command == "off":
        cmd_boiler(args, "off")
    elif args.command == "mode":
        cmd_mode(args)
    elif args.command == "set":
        cmd_setpoint(args)
    elif args.command == "exec":
        cmd_exec(args)
    elif args.command == "monitor":
        cmd_monitor(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
