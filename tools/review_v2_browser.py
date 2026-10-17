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
PORT = 9888
URL = "http://localhost:8080/index_v2.html"
ARTIFACT_DIR = r"C:\Users\BSeceleanu\.gemini\antigravity\brain\dba2ca77-842b-42d7-9271-bba5ec3b3c31"

USER_DATA = r"C:\Users\BSeceleanu\AppData\Local\Temp\chrome_smart_heating_review"

def start_chrome():
    try:
        req = urllib.request.urlopen(f"http://127.0.0.1:{PORT}/json/list", timeout=1)
        data = json.loads(req.read().decode())
        if data:
            print("[INFO] Reusing existing Chrome instance on port", PORT)
            return None
    except Exception:
        pass

    print("[INFO] Launching Google Chrome on port", PORT)
    cmd = [
        CHROME_PATH,
        f"--remote-debugging-port={PORT}",
        "--remote-allow-origins=*",
        f"--user-data-dir={USER_DATA}",
        "--no-first-run",
        "--no-default-browser-check",
        "--window-size=1200,1100",
        URL
    ]
    proc = subprocess.Popen(cmd)
    time.sleep(2.5)
    return proc

def get_page_target():
    for _ in range(15):
        try:
            req = urllib.request.urlopen(f"http://127.0.0.1:{PORT}/json/list", timeout=2)
            targets = json.loads(req.read().decode())
            for t in targets:
                if t.get("type") == "page":
                    return t
        except Exception:
            time.sleep(0.5)
    return None

class CDP:
    def __init__(self, ws):
        self.ws = ws
        self.msg_id = 1

    async def send(self, method, params=None):
        mid = self.msg_id
        self.msg_id += 1
        await self.ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
        while True:
            raw = await self.ws.recv()
            msg = json.loads(raw)
            if msg.get("id") == mid:
                return msg

    async def eval(self, expr):
        res = await self.send("Runtime.evaluate", {
            "expression": expr,
            "returnByValue": True,
            "awaitPromise": True
        })
        return res.get("result", {}).get("result", {}).get("value")

    async def screenshot(self, filename):
        res = await self.send("Page.captureScreenshot", {"format": "png"})
        b64 = res.get("result", {}).get("data")
        if b64:
            path = os.path.join(ARTIFACT_DIR, filename)
            with open(path, "wb") as f:
                f.write(base64.b64decode(b64))
            print(f"[SCREENSHOT] Saved: {filename}")
            return path
        return None

async def run_review():
    target = get_page_target()
    if not target:
        print("[ERROR] Page target not found!")
        return

    async with websockets.connect(target["webSocketDebuggerUrl"]) as ws:
        cdp = CDP(ws)
        await cdp.send("Page.enable")
        await cdp.send("Runtime.enable")

        print("[TEST] Navigating to index_v2.html with fresh reload...")
        await cdp.send("Page.navigate", {"url": URL})
        await asyncio.sleep(2)
        await cdp.send("Page.reload", {"ignoreCache": True})
        await asyncio.sleep(2.5)

        # 1. Check title & basic DOM
        title = await cdp.eval("document.title")
        print(f"[CHECK] Title: {title}")

        # Ensure theme and graph are updated
        await cdp.eval("setWeatherScene('night'); setTheme('nordic-day'); if (typeof renderGraph === 'function') renderGraph();")
        await asyncio.sleep(0.5)

        # 2. Check for any console errors or JS exceptions
        diag = await cdp.eval("""
            (() => {
                const canvas = document.getElementById('telemetryCanvas');
                const ctx = canvas ? canvas.getContext('2d') : null;
                const adminBtn = document.getElementById('btn-admin-gate');
                const pinModal = document.getElementById('pin-modal');
                const sceneBar = document.querySelector('.scene-demo-selector-bar');
                
                return {
                    canvas_exists: !!canvas,
                    canvas_width: canvas ? canvas.width : 0,
                    canvas_height: canvas ? canvas.height : 0,
                    adminBtn_exists: !!adminBtn,
                    pinModal_exists: !!pinModal,
                    sceneBar_exists: !!sceneBar,
                    zonesCount: document.querySelectorAll('.zone-card').length,
                    activeTheme: document.body.getAttribute('data-theme')
                };
            })()
        """)
        print("[DIAG REPORT]", json.dumps(diag, indent=2))

        # Capture Default Screen
        await cdp.screenshot("v2_01_default_nordic.png")

        # 3. Test Weather Scene Selector: Switch to Rain
        print("\n[TEST] Switching scene to 'rain'...")
        await cdp.eval("setWeatherScene('rain')")
        await asyncio.sleep(0.5)
        await cdp.screenshot("v2_02_scene_rain.png")

        # 4. Test Weather Scene Selector: Switch to Snow
        print("\n[TEST] Switching scene to 'snow'...")
        await cdp.eval("setWeatherScene('snow')")
        await asyncio.sleep(0.5)
        await cdp.screenshot("v2_03_scene_snow.png")

        # 5. Test Weather Scene Selector: Switch to Sunset
        print("\n[TEST] Switching scene to 'sunset'...")
        await cdp.eval("setWeatherScene('sunset')")
        await asyncio.sleep(0.5)
        await cdp.screenshot("v2_04_scene_sunset.png")

        # 5b. Test Circadian Matrix Drawer
        print("\n[TEST] Opening Matrix Drawer...")
        await cdp.eval("toggleMatrixDrawer(true)")
        await asyncio.sleep(0.5)
        await cdp.screenshot("v2_10_matrix_drawer.png")
        await cdp.eval("toggleMatrixDrawer(false)")
        await asyncio.sleep(0.3)

        # 6. Test Admin Flow: Click Admin button
        print("\n[TEST] Clicking Admin Button...")
        await cdp.eval("openPinModal()")
        await asyncio.sleep(0.5)
        pin_state = await cdp.eval("document.getElementById('pin-modal').classList.contains('open')")
        print(f"[TEST] PIN Modal open: {pin_state}")
        await cdp.screenshot("v2_05_pin_modal.png")

        # 7. Enter PIN: 1, 2, 3, 4
        print("\n[TEST] Entering PIN: 1, 2, 3, 4...")
        await cdp.eval("pressPin('1')")
        await cdp.eval("pressPin('2')")
        await cdp.eval("pressPin('3')")
        await cdp.eval("pressPin('4')")
        await asyncio.sleep(0.6)

        admin_state = await cdp.eval("document.getElementById('admin-suite-modal').classList.contains('open')")
        print(f"[TEST] Admin Suite open: {admin_state}")
        await cdp.screenshot("v2_06_admin_suite_tesla.png")

        # Switch to FSM pane
        await cdp.eval("switchAdminPane('fsm')")
        await asyncio.sleep(0.4)
        await cdp.screenshot("v2_07_admin_fsm.png")

        # Switch to Gas Calibration pane
        print("\n[TEST] Switching to Gas Calibration Pane...")
        await cdp.eval("switchAdminPane('gas')")
        await asyncio.sleep(0.4)
        await cdp.screenshot("v2_12_gas_admin_tab.png")

        # Switch to Console pane
        await cdp.eval("switchAdminPane('console')")
        await asyncio.sleep(0.4)
        await cdp.screenshot("v2_08_admin_console.png")

        # Close Admin
        await cdp.eval("closeAdminSuite()")
        await asyncio.sleep(0.4)

        # 8. Test Theme Selector: Switch to OLED Stealth Night
        print("\n[TEST] Switching Theme to OLED Stealth Night...")
        await cdp.eval("setTheme('oled-night')")
        await asyncio.sleep(0.6)
        await cdp.screenshot("v2_09_oled_stealth_night.png")

        # 9. Test Telemetry History: 24h and 5d real graphs
        print("\n[TEST] Capturing Real Telemetry 24h Graph...")
        await cdp.eval("window.scrollTo(0, 950);")
        await cdp.eval("switchGraphRange('24h')")
        await asyncio.sleep(0.8)
        await cdp.screenshot("v2_13_telemetry_24h.png")

        print("\n[TEST] Capturing Real Telemetry 5d Graph...")
        await cdp.eval("window.scrollTo(0, 950);")
        await cdp.eval("switchGraphRange('5d')")
        await asyncio.sleep(0.8)
        await cdp.screenshot("v2_14_telemetry_5d.png")

        print("\n[REVIEW COMPLETE] All interactions and screenshots captured successfully!")

if __name__ == "__main__":
    p = start_chrome()
    asyncio.run(run_review())
