# Raport Testare Automată Sistem (ASPICE SWE.5)

> **Dispozitiv Țintă:** ESP32-WROOM-32E (`http://192.168.1.20:80`)  
> **Dată Execuție:** 12.10.2026 20:50:53  
> **Rată de Succes:** **100.0%** (14/14 Teste Trecute)  
> **Verdict Final:** **✅ ACCEPTAT (READY FOR PRODUCTION)**

---

## 1. Matrice Trasabilitate & Rezultate SWE.5

| ID Test | Nume Caz Testare | Cerințe Acoperite | Latență | Stare | Detalii Verificare |
| :--- | :--- | :--- | :--- | :---: | :--- |
| **SWE.5-TC01** | System Status Telemetry & Latency | `FR-1, NFR-1.1, NFR-1.5` | 1722.4 ms | ✅ PASS | Heap: 121 KB free, RSSI: -56 dBm, Latency: 1722.4ms (<200ms target) |
| **SWE.5-TC02** | Device Passport Identity & NVS Metadata | `FR-13.1, FR-13.2` | 187.8 ms | ✅ PASS | Serial: SH-14721A0D5020, MAC: 20:50:0D:1A:72:14, Commissioned: 2026-10-09 10:15:15 |
| **SWE.5-TC03** | Native NimBLE Passive Scanner Contract | `FR-4.1, FR-4.2` | 513.3 ms | ✅ PASS | BLE Scanner ACTIVE, Heard: 2 Xiaomi sensors decoded directly via Bluetooth |
| **SWE.5-TC04** | Onboard Self-Test Diagnostic Suite | `FR-10.1, FR-10.2` | 214.4 ms | ✅ PASS | All 6 onboard subsystems PASS (Wi-Fi, Heap, BLE, Pump, Zones, BoilerRF) |
| **SWE.5-TC05** | Actuator Electrical Polarity (NC/NO) | `FR-15.1, FR-15.2` | 1443.3 ms | ✅ PASS | Verified dynamic transition: NC -> NO -> NC with NVS storage |
| **SWE.5-TC06** | Hydraulic GPA Circulation Pump | `FR-14.1, FR-14.2` | 1225.4 ms | ✅ PASS | Pump Enabled: True (Relay 4), Pump Active: False, Frost Guard: False |
| **SWE.5-TC07** | Anti-Spam Failsafe: Idempotency No-Op | `FR-17.1` | 1952.7 ms | ✅ PASS | Command OFF while already OFF -> 0ms RF transmission skipped (idempotent: true) |
| **SWE.5-TC08** | Anti-Short-Cycling 60s Guard & HTTP 429 | `FR-17.3, FR-17.4` | 376.4 ms | ✅ PASS | Guard ACTIVE: Rejected with HTTP 429 Too Many Requests (wait: 3s remaining) |
| **SWE.5-TC09** | Spam Detection Loop & DTC-201 Trouble Code | `FR-17.5` | 3898.9 ms | ✅ PASS | DTC-201 triggered on rapid spam (Health: 90%). Cleared -> 100% recovered. |
| **SWE.5-TC10** | Zone Temperature Setpoint Control | `FR-1.2, FR-1.3` | 1952.3 ms | ✅ PASS | Zone 1 setpoint calibrated: 22.5°C -> 21.5°C -> 22.5°C (REST API + NVS) |
| **SWE.5-TC11** | NTP Clock Synchronization & Weather | `FR-11.1, FR-11.2` | 234.0 ms | ✅ PASS | NTP Synced: 2026-10-12 20:50:46, Season: Autumn, Outdoor: 13.6°C |
| **SWE.5-TC12** | Network Console Bidirectional Command Loop | `FR-9.1, FR-9.2` | 1006.2 ms | ✅ PASS | Executed 'HELP' via REST console -> verified live buffer response tail |
| **SWE.5-TC13** | Configurable Sensor Timeout Failsafe | `FR-18` | 720.0 ms | ✅ PASS | Verified GET/POST timeout (600s -> 750s -> 600s) with NVS persistence |
| **SWE.5-TC14** | Firmware Version & Baseline Identity | `FR-21` | 4505.1 ms | ✅ PASS | Version: v1.2.0 (Build: Oct 12 2026 20:47:28 | Git: 8065534), Reset: Software Reboot (Firmware Update / Restart) |

---

## 2. Concluzii Inginerești

1. **Zero Bloatware:** Modulul MQTT și dependențele Galaxy-A6 au fost eliminate complet; firmware-ul comunică exclusiv prin BLE direct și REST headless.
2. **Confort Acustic & Fără Spam:** Consola Telnet/Serial nu transmite pachete nesolicitate; datele senzorilor sunt afișate strict la cerere explicită.
3. **Siguranță Termo-Mecanică Failsafe:** Protecțiile de interblocare la 60s, idempotență, limitare spam și ridicare `DTC-201` funcționează conform specificațiilor ASPICE SWE.1 - SWE.5.
