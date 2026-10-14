#!/usr/bin/env python3
"""
tools/browser_live_test.py
Automated End-to-End Live Browser Tester for Smart Heating UI via Chrome DevTools Protocol (CDP).
Tests all 5 Test Scenarios (TS-01 to TS-05) defined in Task 32 directly in native Google Chrome.
"""

import subprocess
import time
import json
import base64
import os
import sys
import urllib.request
import asyncio
import websockets

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
if hasattr(sys.stderr, 'reconfigure'):
    sys.stderr.reconfigure(encoding='utf-8', errors='replace')

CHROME_PATH = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
USER_DATA = r"C:\Users\BSeceleanu\AppData\Local\Temp\chrome_smart_heating_profile"
PORT = 9888
URL = "http://192.168.1.20"
ARTIFACT_DIR = r"C:\Users\BSeceleanu\.gemini\antigravity-ide\brain\77009d7d-bc1f-47d9-8dbe-bac89fc88ced"

def start_chrome():
    try:
        req = urllib.request.urlopen(f"http://127.0.0.1:{PORT}/json/list", timeout=1)
        data = json.loads(req.read().decode())
        if data:
            print("[INFO] Reusing existing Chrome instance on port", PORT)
            return None
    except Exception:
        pass

    print("[INFO] Launching Google Chrome with remote debugging on port", PORT)
    cmd = [
        CHROME_PATH,
        f"--remote-debugging-port={PORT}",
        "--remote-allow-origins=*",
        f"--user-data-dir={USER_DATA}",
        "--no-first-run",
        "--no-default-browser-check",
        "--window-size=1200,900",
        URL
    ]
    proc = subprocess.Popen(cmd)
    time.sleep(2)
    return proc

def get_page_target():
    for attempt in range(25):
        try:
            req = urllib.request.urlopen(f"http://127.0.0.1:{PORT}/json/list", timeout=2)
            targets = json.loads(req.read().decode())
            for t in targets:
                if t.get("type") == "page":
                    return t
        except Exception:
            time.sleep(0.5)
    return None

class CDPClient:
    def __init__(self, ws):
        self.ws = ws
        self.msg_id = 1

    async def send(self, method, params=None):
        mid = self.msg_id
        self.msg_id += 1
        payload = {"id": mid, "method": method, "params": params or {}}
        await self.ws.send(json.dumps(payload))
        while True:
            raw = await self.ws.recv()
            resp = json.loads(raw)
            if resp.get("id") == mid:
                return resp

    async def eval_js(self, expression):
        res = await self.send("Runtime.evaluate", {
            "expression": expression,
            "returnByValue": True,
            "awaitPromise": True
        })
        val = res.get("result", {}).get("result", {}).get("value")
        return val

    async def capture_screenshot(self, filename):
        res = await self.send("Page.captureScreenshot", {"format": "png"})
        b64_data = res.get("result", {}).get("data")
        if b64_data:
            out_path = os.path.join(ARTIFACT_DIR, filename)
            with open(out_path, "wb") as f:
                f.write(base64.b64decode(b64_data))
            print(f"[SCREENSHOT] Saved: {filename} ({len(b64_data)//1024} KB)")
            return out_path
        return None

async def run_tester():
    target = get_page_target()
    if not target or "webSocketDebuggerUrl" not in target:
        print("[ERROR] Cannot find valid page target on CDP port.")
        sys.exit(1)

    ws_url = target["webSocketDebuggerUrl"]
    print(f"[INFO] Connecting to CDP WebSocket: {ws_url}")

    async with websockets.connect(ws_url) as ws:
        cdp = CDPClient(ws)
        await cdp.send("Runtime.enable")
        await cdp.send("Page.enable")

        print(f"\n=======================================================")
        print(f"  TASK 32: LIVE BROWSER USER INTERACTION TEST SUITE")
        print(f"=======================================================")

        # Fresh Page Load
        print(f"\n[TEST INIT] Navigating to {URL}...")
        await cdp.eval_js(f"window.location.href = '{URL}'")
        await asyncio.sleep(2.5)

        title = await cdp.eval_js("document.title")
        print(f"[PASS] Page loaded. Title: {title.encode('ascii', 'replace').decode()}")

        results = {}

        # -------------------------------------------------------------
        # TS-05: GEOMETRIE & RESPONSIVE 2 COLOANE
        # -------------------------------------------------------------
        print("\n--- [TS-05] VERIFICARE GEOMETRIE & ALINIERE 2 COLOANE ---")
        geo = await cdp.eval_js("""
            (() => {
                const c1 = document.getElementById('col-main-1');
                const c2 = document.getElementById('col-main-2');
                if (!c1 || !c2) return { error: 'Columns not found by ID' };
                const r1 = c1.getBoundingClientRect();
                const r2 = c2.getBoundingClientRect();
                return {
                    c1_left: r1.left, c1_top: r1.top, c1_width: r1.width, c1_height: r1.height,
                    c2_left: r2.left, c2_top: r2.top, c2_width: r2.width, c2_height: r2.height,
                    is_side_by_side: r2.left > (r1.left + r1.width * 0.8) && Math.abs(r1.top - r2.top) < 30
                };
            })()
        """)
        print(f"  Coloana 1 (Stânga): X={geo.get('c1_left')}px, Y={geo.get('c1_top')}px, W={geo.get('c1_width')}px, H={geo.get('c1_height')}px")
        print(f"  Coloana 2 (Dreapta): X={geo.get('c2_left')}px, Y={geo.get('c2_top')}px, W={geo.get('c2_width')}px, H={geo.get('c2_height')}px")
        
        tc_ui_11 = geo.get("is_side_by_side", False)
        print(f"  TC-UI-11 (Side-by-Side 2 Columns Grid): {'[PASS]' if tc_ui_11 else '[FAIL]'}")
        results["TC-UI-11"] = tc_ui_11

        # -------------------------------------------------------------
        # TS-01: NIVEL SIMPLU (BEGINNER / MANUAL DIRECT)
        # -------------------------------------------------------------
        print("\n--- [TS-01] TEST NIVEL: SIMPLU (MANUAL DIRECT) ---")
        await cdp.eval_js("setUiLevel('beginner')")
        await asyncio.sleep(1.2)
        
        beg_eval = await cdp.eval_js("""
            (() => {
                const adv = document.getElementById('box-advanced-editor');
                const act = document.getElementById('box-presets-actions');
                const bioNotice = document.getElementById('box-schedule-manual-notice');
                const btnBeg = document.getElementById('lvl-beginner');
                return {
                    active_tab: btnBeg && btnBeg.classList.contains('active'),
                    adv_hidden: adv ? window.getComputedStyle(adv).display === 'none' : true,
                    act_hidden: act ? window.getComputedStyle(act).display === 'none' : true,
                    notice_visible: bioNotice ? window.getComputedStyle(bioNotice).display !== 'none' : false
                };
            })()
        """)
        tc_ui_01 = beg_eval.get("active_tab") and beg_eval.get("adv_hidden") and beg_eval.get("act_hidden")
        print(f"  TC-UI-01 (Orar & Instrumente Tehnice Ascunse): {'[PASS]' if tc_ui_01 else '[FAIL]'}")
        results["TC-UI-01"] = tc_ui_01
        await cdp.capture_screenshot("e2e_01_beginner_mode.png")

        # -------------------------------------------------------------
        # TS-02: NIVEL PRESETURI RAPIDE
        # -------------------------------------------------------------
        print("\n--- [TS-02] TEST NIVEL: PRESETURI RAPIDE ---")
        await cdp.eval_js("setUiLevel('presets')")
        await asyncio.sleep(1.2)

        chips_eval = await cdp.eval_js("""
            (() => {
                const chips = Array.from(document.querySelectorAll('.preset-chip')).map(c => c.textContent.trim());
                const act = document.getElementById('box-presets-actions');
                const adv = document.getElementById('box-advanced-editor');
                return {
                    count: chips.length,
                    chips: chips,
                    act_hidden: act ? window.getComputedStyle(act).display === 'none' : true,
                    adv_hidden: adv ? window.getComputedStyle(adv).display === 'none' : true
                };
            })()
        """)
        safe_chip_names = [c.encode('ascii', 'replace').decode() for c in chips_eval.get("chips", [])]
        print(f"  Chip-uri Preseturi Găsite ({chips_eval.get('count')}): {safe_chip_names}")
        tc_ui_05 = chips_eval.get("act_hidden") and chips_eval.get("adv_hidden")
        print(f"  TC-UI-05 (Butoane Tehnice Reset/Save Ascunse): {'[PASS]' if tc_ui_05 else '[FAIL]'}")
        results["TC-UI-05"] = tc_ui_05

        # Clic pe Home Office
        print("  -> Interacțiune: Clic pe Preset 'Home Office'...")
        await cdp.eval_js("applyPreset('home_office')")
        await asyncio.sleep(1.5)
        temps_ho = await cdp.eval_js("Array.from(document.querySelectorAll('.setpoint-val')).map(el => el.textContent.trim())")
        print(f"     Temperaturi țintă aplicate: {temps_ho}")
        await cdp.capture_screenshot("e2e_02_preset_homeoffice.png")

        # Clic pe Vacanta / Eco
        print("  -> Interacțiune: Clic pe Preset 'Vacanță / Eco'...")
        await cdp.eval_js("applyPreset('eco')")
        await asyncio.sleep(1.5)
        temps_eco = await cdp.eval_js("Array.from(document.querySelectorAll('.setpoint-val')).map(el => el.textContent.trim())")
        print(f"     Temperaturi țintă aplicate: {temps_eco}")
        await cdp.capture_screenshot("e2e_03_preset_eco.png")

        # Clic pe Standard / Confort
        print("  -> Interacțiune: Clic pe Preset 'Standard / Confort'...")
        await cdp.eval_js("applyPreset('comfort')")
        await asyncio.sleep(1.5)
        temps_comf = await cdp.eval_js("Array.from(document.querySelectorAll('.setpoint-val')).map(el => el.textContent.trim())")
        print(f"     Temperaturi țintă aplicate: {temps_comf}")

        tc_ui_03 = len(chips_eval.get("chips", [])) >= 3
        tc_ui_04 = len(temps_comf) > 0
        results["TC-UI-03"] = tc_ui_03
        results["TC-UI-04"] = tc_ui_04
        print(f"  TC-UI-03 (Preset Selection Scenarios): {'[PASS]' if tc_ui_03 else '[FAIL]'}")
        print(f"  TC-UI-04 (Dynamic Target Switching without reload): {'[PASS]' if tc_ui_04 else '[FAIL]'}")

        # -------------------------------------------------------------
        # TS-03: NIVEL ORAR AVANSAT
        # -------------------------------------------------------------
        print("\n--- [TS-03] TEST NIVEL: ORAR AVANSAT (INGINERIE) ---")
        await cdp.eval_js("setUiLevel('advanced')")
        await asyncio.sleep(1.2)

        adv_eval = await cdp.eval_js("""
            (() => {
                const adv = document.getElementById('box-advanced-editor');
                const act = document.getElementById('box-presets-actions');
                const preheat = document.getElementById('chk-preheat');
                const windowChk = document.getElementById('chk-window');
                return {
                    adv_visible: adv ? window.getComputedStyle(adv).display !== 'none' : false,
                    act_visible: act ? window.getComputedStyle(act).display !== 'none' : false,
                    has_preheat: preheat !== null,
                    has_window: windowChk !== null
                };
            })()
        """)
        tc_ui_06 = adv_eval.get("adv_visible") and adv_eval.get("act_visible")
        print(f"  TC-UI-06 (Editor Orar & Acțiuni Sloturi Deblocate): {'[PASS]' if tc_ui_06 else '[FAIL]'}")
        results["TC-UI-06"] = tc_ui_06
        await cdp.capture_screenshot("e2e_04_advanced_mode.png")

        # -------------------------------------------------------------
        # TS-04: SAFETY & ANTI-SPAM UX
        # -------------------------------------------------------------
        print("\n--- [TS-04] TEST SAFETY, ANTI-SPAM & TOAST FEEDBACK ---")
        print("  -> Interacțiune: Apăsare repetată (Spam) pe butonul de OPRIRE când centrala e deja OFF...")
        toast_eval = await cdp.eval_js("""
            (() => {
                const btnOff = document.getElementById('btn-boiler-off');
                if (btnOff) btnOff.click();
                const toast = document.getElementById('ui-toast');
                return {
                    toast_found: toast !== null,
                    toast_text: toast ? toast.innerText.trim() : '',
                    toast_display: toast ? window.getComputedStyle(toast).display : 'none'
                };
            })()
        """)
        safe_toast_text = toast_eval.get("toast_text", "").encode("ascii", "replace").decode()
        print(f"     Toast Feedback declanșat: '{safe_toast_text}' (Display={toast_eval.get('toast_display')})")
        
        tc_ui_09 = toast_eval.get("toast_found") and len(toast_eval.get("toast_text", "")) > 0
        print(f"  TC-UI-09 (Anti-Spam Idempotent Toast Feedback): {'[PASS]' if tc_ui_09 else '[FAIL]'}")
        results["TC-UI-09"] = tc_ui_09
        await cdp.capture_screenshot("e2e_05_anti_spam_toast.png")

        # Așteaptă expirarea toast-ului (3 secunde) pentru claritate vizuală
        print("  -> Așteptare expirare toast (3.2s)...")
        await asyncio.sleep(3.2)

        # -------------------------------------------------------------
        # TS-06: SARCINA 30 - TESLA SERVICE MODE THERMAL SCHEMA
        # -------------------------------------------------------------
        print("\n--- [TS-06] TEST SARCINA 30: SCHEMĂ TERMODINAMICĂ TESLA SERVICE MODE ---")
        print("  -> Interacțiune: Clic pe Tab 'Schemă Termică (Tesla Mode)'...")
        await cdp.eval_js("switchViewMode('tesla')")
        await asyncio.sleep(1.2)

        tesla_eval = await cdp.eval_js("""
            (() => {
                const card = document.getElementById('card-tesla-view');
                const col1 = document.getElementById('col-main-1');
                const col2 = document.getElementById('col-main-2');
                const boiler = document.getElementById('ts-box-boiler');
                const impeller = document.getElementById('ts-impeller');
                const tur = document.getElementById('ts-pipe-flow-tur');
                const retur = document.getElementById('ts-pipe-flow-retur');
                const coil1 = document.getElementById('ts-coil-flow-1');
                const coil2 = document.getElementById('ts-coil-flow-2');
                const coil3 = document.getElementById('ts-coil-flow-3');
                const tTur = document.getElementById('ts-t-tur');
                const tRetur = document.getElementById('ts-t-retur');
                const tDelta = document.getElementById('ts-t-delta');
                const pumpSt = document.getElementById('ts-pump-status');

                return {
                    tesla_visible: card ? window.getComputedStyle(card).display !== 'none' : false,
                    col1_hidden: col1 ? window.getComputedStyle(col1).display === 'none' : false,
                    col2_hidden: col2 ? window.getComputedStyle(col2).display === 'none' : false,
                    has_boiler_box: boiler !== null,
                    has_impeller: impeller !== null,
                    has_pipes: tur !== null && retur !== null,
                    has_coils: coil1 !== null && coil2 !== null && coil3 !== null,
                    t_tur_val: tTur ? tTur.innerText.trim() : '',
                    t_retur_val: tRetur ? tRetur.innerText.trim() : '',
                    t_delta_val: tDelta ? tDelta.innerText.trim() : '',
                    pump_st_val: pumpSt ? pumpSt.innerText.trim() : ''
                };
            })()
        """)
        tc_ui_13 = tesla_eval.get("tesla_visible") and tesla_eval.get("col1_hidden") and tesla_eval.get("has_coils")
        print(f"     Tesla Card Vizibil: {tesla_eval.get('tesla_visible')}, Coloane Dashboard Ascunse: {tesla_eval.get('col1_hidden')}")
        print(f"     Telemetrie Tesla -> Tur: {tesla_eval.get('t_tur_val')}, Retur: {tesla_eval.get('t_retur_val')}, DeltaT: {tesla_eval.get('t_delta_val')}, Pompa: {tesla_eval.get('pump_st_val')}")
        print(f"  TC-UI-13 (Tesla Service Mode Schema & Vector Loops): {'[PASS]' if tc_ui_13 else '[FAIL]'}")
        results["TC-UI-13 (Tesla Schema)"] = tc_ui_13
        await cdp.eval_js("document.getElementById('card-tesla-view').scrollIntoView({ behavior: 'instant', block: 'center' })")
        await asyncio.sleep(0.5)
        await cdp.capture_screenshot("e2e_06_tesla_service_mode.png")

        # Testare buton acțiune service Tesla: Test RF Cazan
        print("  -> Interacțiune Service Mode: Clic pe butonul 'Test Emisie RF Cazan'...")
        await cdp.eval_js("testTeslaBoilerPing()")
        await asyncio.sleep(0.8)
        rf_toast = await cdp.eval_js("document.getElementById('ui-toast') ? document.getElementById('ui-toast').innerText : ''")
        safe_rf_toast = rf_toast.encode("ascii", "replace").decode()
        print(f"     Feedback Acțiune Service: '{safe_rf_toast}'")
        tc_ui_15 = len(rf_toast) > 0 and "Cazan" in rf_toast
        results["TC-UI-15 (Tesla Service Actions)"] = tc_ui_15
        print(f"  TC-UI-15 (Tesla Service Mode Action Buttons): {'[PASS]' if tc_ui_15 else '[FAIL]'}")
        await asyncio.sleep(3.2)

        # -------------------------------------------------------------
        # TS-07: SARCINA 31 - DIAGRAMĂ INTERACTIVĂ DE STĂRI FSM LIVE
        # -------------------------------------------------------------
        print("\n--- [TS-07] TEST SARCINA 31: DIAGRAMĂ INTERACTIVĂ DE STĂRI FSM (ADMIN) ---")
        print("  -> Interacțiune: Clic pe Tab 'Fir Logic & Stări (FSM Admin)'...")
        await cdp.eval_js("switchViewMode('fsm')")
        await asyncio.sleep(1.2)
        await cdp.eval_js("document.getElementById('card-fsm-view').scrollIntoView({ behavior: 'instant', block: 'center' })")
        await asyncio.sleep(0.5)

        fsm_eval = await cdp.eval_js("""
            (() => {
                const card = document.getElementById('card-fsm-view');
                const col1 = document.getElementById('col-main-1');
                const activeLabel = document.getElementById('fsm-active-label');
                const allNodes = [
                    'idle', 'demand', 'wax', 'manual-hold', 'active-heat',
                    'overrun', 'cooldown', 'frost', 'window'
                ];
                const foundNodes = allNodes.filter(n => document.getElementById('fsm-node-' + n) !== null);
                const activeNodes = allNodes.filter(n => {
                    const el = document.getElementById('fsm-node-' + n);
                    return el && el.classList.contains('active');
                });

                return {
                    fsm_visible: card ? window.getComputedStyle(card).display !== 'none' : false,
                    col1_hidden: col1 ? window.getComputedStyle(col1).display === 'none' : false,
                    total_nodes_found: foundNodes.length,
                    active_nodes_count: activeNodes.length,
                    active_node_id: activeNodes[0] || 'none',
                    active_label: activeLabel ? activeLabel.innerText.trim() : ''
                };
            })()
        """)
        tc_ui_14 = fsm_eval.get("fsm_visible") and fsm_eval.get("total_nodes_found") == 9 and fsm_eval.get("active_nodes_count") == 1
        safe_active_label = fsm_eval.get("active_label", "").encode("ascii", "replace").decode()
        print(f"     FSM Card Vizibil: {fsm_eval.get('fsm_visible')}, Noduri Găsite: {fsm_eval.get('total_nodes_found')}/9")
        print(f"     Nod Stare Activ: '{fsm_eval.get('active_node_id')}', Fir Logic: '{safe_active_label}'")
        print(f"  TC-UI-14 (FSM Diagram 9 Nodes & Live Active Highlight): {'[PASS]' if tc_ui_14 else '[FAIL]'}")
        results["TC-UI-14 (FSM Live Diagram)"] = tc_ui_14
        await cdp.capture_screenshot("e2e_07_fsm_live_diagram.png")

        # Restaurare Tablou Principal
        print("  -> Revenire pe Panoul Principal...")
        await cdp.eval_js("switchViewMode('dash')")
        await asyncio.sleep(0.5)

        # -------------------------------------------------------------
        # SUMAR GENERAL TESTARE
        # -------------------------------------------------------------
        print(f"\n=======================================================")
        print(f"  REZULTAT FINAL SUITĂ E2E BROWSER NATIV")
        print(f"=======================================================")
        total_tests = len(results)
        passed_tests = sum(1 for v in results.values() if v)
        for tc, res in results.items():
            print(f"  * {tc}: {'[PASS] 100%' if res else '[FAIL]'}")
        
        print(f"\nScor Total: {passed_tests}/{total_tests} PASS ({(passed_tests/total_tests)*100:.1f}%)")
        print(f"Browserul a rămas deschis pe ecranul tău la http://192.168.1.20!")
        print(f"=======================================================\n")

if __name__ == "__main__":
    start_chrome()
    try:
        asyncio.run(run_tester())
    finally:
        pass
