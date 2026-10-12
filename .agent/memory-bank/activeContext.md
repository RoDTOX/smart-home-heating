# Active Context: Smart Home Heating System

## Current State (Updated 2026-10-12)
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Free Heap: **~120 KB** with native NimBLE passive scanner active.
  - Web Server & Web OTA: Active on port 80 (`http://192.168.1.20/` and `/update`).
  - Watchdog: Hardware TWDT configured to 60s for safe Web OTA headroom without false resets.
- **Aliniere Tabelară pe 2 Coloane, Categorii A-E, Curățare POST/INIT & Link TelinkMiFlasher (Task 23):**
  - **Tabel Aliniat pe 2 Coloane cu Litere A-E & Aerisire cu Newlines:** Categoriile din `STATUS` sunt prefixate alfabetic (`[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`), separate prin linii libere între secțiuni și între cardurile camerelor.
  - **POST 100% Hardware & Local:** Eliminat `Real-Time Clock` din tabelul POST (serviciu de rețea); POST raportează exclusiv cele 8 verificări locale definitive (Network, RAM, Actuators, Pump, Bluetooth, Boiler RF, Watchdog, Web API).
  - **Raportare Asincronă Unificată sub `[INIT]` cu Asterisc:** Sarcinile de fundal sunt declarate sub `[INIT] Background Tasks Dispatched:`, iar evenimentele ulterioare adoptă formatul `[INIT]   * Outdoor Weather: Synchronized...`, `[INIT]   * Real-Time Clock: Synchronized...`, `[INIT]   * Sensor Discovery: Acquired (X/Y)...` și `[INIT]   * Sensor Discovery: All X room sensors acquired in Ys!`.
  - **Link Configurare Senzori BLE Xiaomi:** Integrat hyperlink către instrumentul web oficial TelinkMiFlasher (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) în Web Console (`/console`), Setup Wizard (`/setup`), Dashboard (`/`) și manualul de comenzi (`HELP`) pentru reglarea puterii de transmisie RF (Tx Power).
  - **Reformatare Explicită:** `Sensor Status: Timeout` (radio) și `Loop Status: Sensor Timeout (Failsafe OFF)` (hidraulic).
  - **Mapare Culori Case-Insensitive `/gi` în Consola Web:** Toate stările de după `:` sunt colorate viu corespunzător, cu suport pentru etichetele `[A-E. ...]`. `Open-Meteo` este protejat de regex via negative lookahead.
  - **Paced Socket Streaming (`tools/ota_upload.py`):** Transfer cadențat cu pauze de 12ms/pachet (~110 KB/s), rezolvând definitiv blocajele TCP și permițând scrierea stabilă în flash.
  - **Watchdog Extins:** Timeout TWDT setat la 90s.
- **Hardware Telemetry, Per-Zone Telemetry Cards & Console Color Tuning (Task 22):**
  - **Telemetrie Hardware:** Adăugat `[CPU]` (Model ESP32-D0WD-V3, 2 Cores @ 240 MHz, Rev 3, Die Temp via `temperatureRead()`, Uptime), `[MEMORY]` (RAM liber/total, Watermark minim de la boot, Cel mai mare bloc contiguu), `[FLASH]` (NVS Active, Flash 4MB @ 40MHz, Firmware size și marjă liberă OTA).
  - **Carduri Zonale pe 4 Linii:** Redenumit în `[HEATING ZONES & ROOM TELEMETRY]`. Fiecare zonă afișează toate datele (valvă, temperatură, umiditate, status, senzor BLE, MAC, baterie, semnal cu min/max, pachete, vechime) structurat fără aglomerare orizontală.
  - **Eliminare Tabel `[SENSORS]`:** Tabelul separat a fost complet eliminat din `STATUS`.
  - **Simbolul Grad Celsius (`°C`):** Folosit simbolul `°C` nativ UTF-8 peste tot, cu `charset=utf-8` în HTTP headers și `esp_cli.py`.
  - **Consolă Web Stilizată:** Eticheta `[WEATHER]`, anotimpul (`Autumn`), temperaturile `°C` și etichetele hardware (`[CPU]`, `[MEMORY]`, `[FLASH]`) sunt evidențiate în culori vii.
- **Acronym Elimination, Unified Mapping & RTC Resilience (Task 21):**
  - **Eliminare Acronime Tehnice:** Înlocuit complet `GPA` (`Secondary Pump on Relay 4`), `TWDT` (`Hardware Watchdog Armed: 60s timeout`), `NC Polarity` (`Normally Closed [NC], User-Configured in NVS`), `NimBLE Active` (`Bluetooth Receiver active, listening`).
  - **Timp Real & Reziliență Offline Fără Wi-Fi:** Adăugat `Boot Time: YYYY-MM-DD HH:MM:SS` în banner; anunț explicit la sincronizarea ceasului NTP; documentat comportamentul timer-ului RTC hardware intern pe ESP32 (menține ceasul chiar dacă Wi-Fi este indisponibil 2 zile; controlul termic este 100% autonom local).
  - **Integrare Servicii în POST:** `Real-Time Clock`, `Outdoor Weather` și `Sensor Boot Warmup` monitorizate direct în tabelul Power-On Self-Test.
  - **Unificare Tabel Zone-Senzori (`STATUS`):** Integrată maparea completă a senzorului (Nume, MAC, Baterie %, Semnal dBm + Badge) direct în fiecare rând din tabelul `[ZONES] Heating Loops & Sensor Mapping`, eliminând confuzia dintre denumirea senzorului și a camerei.
  - **Evidențiere Vizuală Web Console:** Stilizare automată cuvinte-cheie (`OFF`, `ON`, `IDLE`, `DEMAND`, `AUTOMATIC`, `MANUAL`, `WARMUP`, `PASS`, `FAIL`) și link badges.
- **Boot Sensor Warmup Grace Period & Tabular Status (Task 20):**
  - **Grace Period la Boot (Max 5 min / 300s):** Eliminată alarma falsă instantanee de timeout la pornirea la rece; starea dedicată `ZONE_WARMUP` așteaptă balizele BLE (30–60s) ale termometrelor asociate.
  - **Ieșire Anticipată la Sincronizare Completă:** Când toți senzorii asociați au fost detectați (ex. 3/3 în 30–60s), warmup-ul se finalizează imediat (`[SCANNING COMPLETE - ALL ACQUIRED]`) și buclele trec în modul operațional fără întârziere.
  - **Decuplare Izolată & Ridicare DTC-101:** Dacă un senzor nu raportează în 5 minute, se ridică codul `DTC-101`, se izolează doar zona respectivă, iar sistemul pornește încălzirea pe camerele funcționale.
  - **Tabel Aliniat Bucle & Senzori (`STATUS` & `BLE`):** Coloane fixe cu aliniere milimetrică pentru `ID`, `Zone Name`, `Current Temp`, `Target Temp`, `Status`, `Actuator Valve (Relay)` și registrul termometrelor cu semnal Min/Cur/Max dBm, pachete și vârstă.
  - **Etichetă Clară Protecții (`[PROTECTIONS]`):** `Sensor Offline Failsafe: 10 min (all zones) | Pump Anti-Seize: Weekly (Ran: ... | Next in ...)`.
- **Console Ergonomics & Clean Boot (Task 18 & 19):**
  - Pristine boot: zero verbose module initialization lines before banner; title and POST self-test table are the absolute first items emitted to virtual console.
  - Internal boot logs (`Thermal`, `Time`, `Passport`, `BLE`, `DIAG`, TWDT) redirected strictly to physical UART `Serial`.
  - Real NTP timestamps `[HH:MM:SS]` automatically prepended to every log line on Serial, Telnet TCP:23, and Web Console.
  - Category badge styling and HTML error page filtering (prevents Nginx 502 Bad Gateway pollution in `/console`).
  - Link Quality badges (`[EXCELLENT]`, `[GOOD]`, `[MARGINAL]`, `[POOR]`) integrated directly into `STATUS` and `BLE` commands.
  - Command deduplication: removed redundant double-prints for `ON`/`OFF`, `AUTO`/`MANUAL`, `SET`.
  - Categorized help command manual (`HELP` / `?`) divided into 4 functional groups.
  - Added `CLEAR` / `CLS` terminal reset command.
- **Open-Meteo Weather Resilience (Task 18 & 19):**
  - Socket timeout increased to 5000ms with initial 5s post-boot settling delay and silent background retry, completely eliminating transient HTTP -11 read-timeout warnings on cold boot.
- **Hydronic Calendar Anti-Seize & Configurable Sensor Timeout (Task 18):**
  - Anti-seize pump exercise uses UNIX epoch (`time(nullptr)`) in NVS, eliminating 20s false pump runs on reboot.
  - Sensor timeout failsafe (`DTC-101`) dynamically configurable via CLI and REST (`sensorTimeoutSec`, default 600s / 10 min).
  - Link Quality badges (`EXCELLENT`, `GOOD`, `MARGINAL`, `POOR`) in Web Setup and CLI for easy on-site installation verification.
- **Clean Architecture & 100% English Codebase (Zero Bloatware):**
  - Zero `#ifdef ESP8266` legacy branches. Native ESP32 libraries used throughout.
  - Eliminated legacy MQTT / Galaxy-A6 code completely (`MqttTelemetryClient` deleted, `PubSubClient` unlinked, `-D MQTT_MAX_PACKET_SIZE` removed).
  - Production architecture relies 100% on native passive BLE scanning via `NimBLE-Arduino` directly on ESP32 (BTHome v2 & PVVX/ATC).
  - Zero unrequested console spam: Telnet TCP:23 and Serial output remain completely quiet during loop; sensor readings are displayed strictly upon explicit request (`STATUS`, `BLE`, `SENSORS` or REST).
- **Headless API-First Transition & Automated Testing:**
  - Fast, deterministic REST/JSON endpoints (<150 ms response time).
  - Dedicated CLI tool [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).
  - Automated System Verification Test Suite ([`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py) / `python tools/esp_cli.py test`): 14 end-to-end integration test cases verifying ASPICE SWE.5 with 100.0% pass rate.
  - Live execution report: [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).
- **Relay Mapping & Actuator Control:**
  - Zone 1 (Living & Kitchen): Relay 1 on `GPIO32` (active HIGH).
  - Zone 2 (Kids Room): Relay 2 on `GPIO33` (active HIGH).
  - Zone 3 (Master Bedroom): Relay 3 on `GPIO25` (active HIGH).
  - Relay 4 (Secondary Circulation Pump GPA): on `GPIO26`.
  - Status LED: `GPIO23` (active HIGH).
  - RF Transmitter DATA: `GPIO19` (OOK 433.92 MHz).
- **Onboard Diagnostics & Service Mode:**
  - `DiagnosticEngine` tracking standardized DTCs (`DTC-101` through `DTC-402`).
  - 100ms onboard self-test suite (`/api/diag/selftest`) reporting percentage health score.
  - Service Mode toggle (`/api/diag/service`) for telephone remote troubleshooting.
- **Digital Device Passport & Provisioning:**
  - Unique hardware serial generated from MAC (`SH-14721A0D5020`).
  - Commissioning timestamp, client name, smart location, installer notes saved persistently to ESP32 NVS.
  - Sincronizare NTP timp (Europe/Bucharest) și vreme prin Open-Meteo REST API (`TimeAndWeatherService`).
- **Hydronic Safety Hardening & Circulation Pump Control:**
  - Automated secondary circulation pump controller on Relay 4 (`GPIO26`) with interlock safety and 180s pump overrun (with keeper zone).
  - Configurable actuator polarity: NC (Normally Closed) vs NO (Normally Open) via NVS (`act_nc`).
  - Emergency Frost Protection Guard (<12°C) forcing heating demand even in manual/off mode.
  - Sensor timeout failsafe (`DTC-101`) shutting down zones without signal for >30min, with instant automatic recovery upon reception.
  - Automated 20s anti-seize exercise cycle for summer maintenance against rotor calcification.
  - ESP-IDF Hardware Task Watchdog Timer (TWDT 30s) monitoring `loop()`.
- **Anti-Spam Failsafe, Rate-Limiting & DTC-201 (ASPICE V-Model SWE.1 - SWE.5):**
  - Fully implemented and hardware-verified anti-short-cycling guard (`FR-17`, `NFR-1.6`) on boiler RF trigger:
    * `FR-17.1`: Idempotency filter answering in 0 ms (`idempotent: true`), suppressing redundant RF bursts.
    * `FR-17.2`: RF transmitter cooldown (min 3s).
    * `FR-17.3`: Anti-short-cycling burner guard enforcing min 60s run time after ON and min 60s pause time after OFF.
    * `FR-17.4`: Standard HTTP 429 Too Many Requests response with remaining `waitSec` countdown.
    * `FR-17.5`: Excessive spam mitigation raising `DTC-201` when >=3 rapid commands sent within 10s.
    * `FR-17.6`: Pre-validation with zero CPU locking and protected TWDT.
    * Emergency override via `--force` flag on CLI and REST API.

## Next Immediate Steps
1. Wait for physical actuators delivery and wire up to distributor manifold valves.
2. Cloud fleet connectivity prototype: Outbound TLS reverse tunnel (MQTT over TLS / WebSocket).
3. Await explicit user instruction for git commit/push (`commit si push`).
