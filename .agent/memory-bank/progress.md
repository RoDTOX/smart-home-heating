# Progress: Smart Home Heating System

## Status
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, 2-way console, web OTA, diagnostic engine with DTCs, service mode, digital passport, NTP & weather sync, headless API-first architecture, hydronic safety controls, anti-spam burner interlocks, and automated PC system test suite) are fully implemented, compiled, uploaded, and verified live on ESP32 (`192.168.1.20`) with 100% ASPICE SWE.5 pass rate.

## Completed Work (2026-10-12 - Task 25)
- **Eliminare Word Wrap pe Mobil & Scroll Orizontal Fluid (`NetworkConsole`):**
  - Înlocuită formatarea fragilă cu `white-space: pre-wrap; word-break: break-all;` prin formatare rigidă monospace (`white-space: pre; word-break: normal;`) cu suport nativ de accelerare tactilă pentru scroll orizontal și vertical (`overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`).
  - Tabelele `STATUS`, bannerele POST și mesajele `[INIT]` rămân perfect aliniate pe telefon fără a fărâmița textul.
- **Comutator Interactiv Word Wrap & Scalare Font (`A-` / `A+`):**
  - Adăugat checkbox în footer: `<input type="checkbox" id="wrapToggle"> Wrap` cu memorare în `localStorage` (`termWrap`).
  - Adăugate butoane de ajustare fină a fontului (`A-` și `A+`), cu font implicit redus automat la `0.70rem` pe ecrane <640px și salvare în `localStorage` (`termFontSize`).
- **Watchdog Feed la Încărcare OTA (`main.cpp`):**
  - Adăugat callback `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` direct înainte de `httpUpdater.setup()`, prevenind declanșarea TWDT la scrierea firmware-ului în flash.
- **Validare Live pe Hardware:**
  - Firmware compilat, flashuit prin OTA pe ESP32 (`192.168.1.20`), testat Web Console și rulată suita completă de 14 teste ASPICE SWE.5 cu **14/14 PASS (100.0%)**.

## Completed Work (2026-10-12 - Task 24)
- **Remediere Scurgere Memorie Heap (`DTC-402`) & NimBLE `setMaxResults(0)`:**
  - Izolată și rezolvată scurgerea lentă de memorie din scannerul BLE pasiv cauzată de stocarea implicită (`m_maxResults = 0xFF`) a tuturor pachetelor de la dispozitive cu adrese MAC rotative (MAC randomization).
  - Setat `setMaxResults(0)` și `clearResults()` la inițializare conform contractului NimBLE (eliberare instantanee cu `erase()`), garantând zero acumulare pe heap și un consum de memorie 100% stabil și plat în timp.
  - Implementat auto-clear cu histerezis în `DiagnosticEngine` pentru `DTC-402` (clear la >75 KB) și `DTC-401` (clear la >-78 dBm).
  - Flashuit live via Web OTA: Free Heap stabil la **125 KB**, `DTC-402` stins, Scorul de Sănătate **100% [EXCELLENT]**, suita ASPICE SWE.5: **14/14 PASS (100.0%)**.

## Completed Work (2026-10-12 - Task 23)
- **Aliniere Tabelară Milimetrică pe 2 Coloane (`STATUS`):**
  - Înlocuit formatul de linii orizontale lungi cu pipe-uri printr-un tabel structurat pe 2 coloane paralele perfect aliniate. Coloana stângă conține parametrii hidraulici și termici (`Target Temperature`, `Current Temperature`, `Room Humidity`, `Loop Status`, `Actuator Valve`, `Hydraulic Relay`), iar coloana dreaptă cuprinde telemetria radio (`Sensor Name`, `Sensor Status`, `Sensor Battery`, `RF Signal Link`, `Telemetry Data`).
  - Reformatat `Status: Sensor Timeout` în `Sensor Status: Timeout` (radio) și `Loop Status: Sensor Timeout (Failsafe OFF)` (hidraulic).
- **Eliminare Linii SYNCING din POST:**
  - POST-ul la boot include exclusiv verificările definitive (10/10 PASS). Serviciul NTP se sincronizează în bucla de boot pentru a raporta `Real-Time Clock: PASS (YYYY-MM-DD HH:MM:SS via NTP)`.
  - Adăugat blocul asincron `[INIT] Background Tasks Dispatched:` imediat sub POST pentru procesele de lungă durată (Outdoor Weather, Sensor Discovery).
- **Logare Progresivă în Timp Real:** La achiziția fiecărui senzor se raportează progresul live: `[Warmup] Sensor acquired (1/3)...`, `(2/3)...`, eliminând apariția bruscă la final.
- **Colorare Exhaustivă Case-Insensitive `/gi` în Web Console:**
  - Mapate toate stările de după `:` (`Idle`, `Timeout`, `Online`, `Offline`, `Closed`, `Open`, `Warmup`, `Demand`, `Automatic`, etc.) cu stiluri CSS dedicate. Folosit negative lookahead pe `OPEN` pentru a exclude `Open-Meteo`.
- **Optimizare Paced Socket OTA & TWDT 90s:**
  - Upload-ul de firmware este cadențat la ~110 KB/s cu pauze de 12ms/pachet, eliminând definitiv supraîncărcarea bufferelor TCP `pbuf` din ESP32. Timp de upload stabil de ~13.5s.
  - Teste automate ASPICE SWE.5: **14/14 PASS (100.0%)**.

## Completed Work (2026-10-12 - Task 22)
- **Telemetrie Hardware Avansată:** Adăugat `[CPU]` (ESP32-D0WD-V3, 2 Cores @ 240 MHz, Rev 3, Die Temp via `temperatureRead()`, Uptime), `[MEMORY]` (RAM liber/total, Lowest Watermark, Max Allocable Block), `[FLASH]` (NVS Active, Flash 4MB @ 40MHz, Firmware size, OTA Margin).
- **Redesign Complet Zone în STATUS:** Tabelul separat `[SENSORS]` a fost scos definitiv. Creată secțiunea `[HEATING ZONES & ROOM TELEMETRY]` cu carduri structurate pe 4 linii per cameră, integrând toate detaliile (temperatură target/curentă în `°C`, umiditate, valvă, releu, nume senzor, MAC, baterie, semnal min/max, pachete, vechime).
- **Simbolul Grad Celsius (`°C`) & UTF-8 Header:** Setat `text/plain; charset=utf-8` și forțat UTF-8 în CLI pentru redare clară fără caractere distorsionate.
- **Colorare & Contrast Web Console:** Stilizat `[WEATHER]`, anotimpuri, temperaturi și tag-uri hardware în `/console`.
- **Verificare SWE.5 100%:** Rulată suita automată completă cu 14/14 PASS.

## Completed Work (2026-10-12 - Task 21)
- **Eliminare Acronime Tehnice (Zero Bloat):** Înlocuit complet `GPA` (`Secondary Pump on Relay 4`), `TWDT` (`Hardware Watchdog Armed: 60s timeout`), `NC Polarity` (`Normally Closed [NC], User-Configured in NVS`), `NimBLE Active` (`Bluetooth Receiver active, listening`).
- **Timp Real & Reziliență Offline Fără Wi-Fi:**
  - Adăugat `Boot Time` în bannerul de pornire și mesaj dedicat de confirmare a sincronizării ceasului (`[Time] Real-time clock synchronized`).
  - Documentat și testat comportamentul RTC intern ESP32: menține ora chiar dacă Wi-Fi este oprit timp de 2 zile.
  - Logica de reglaj termic, comanda actuatoarelor, pompa și protecția la îngheț sunt 100% autonome local, fără dependență de rețea.
- **Integrare Servicii în Power-On Self-Test (POST):** Ceasul real (`Real-Time Clock`), serviciul meteo (`Outdoor Weather`) și perioada de grație a senzorilor (`Sensor Boot Warmup`) integrate direct în tabelul POST.
- **Unificare Tabel Zone-Senzori (`STATUS`):** Integrată maparea completă a senzorului (Nume, MAC, Baterie %, Semnal dBm + Badge) direct în tabelul `[ZONES] Heating Loops & Sensor Mapping (%u loops)`, eliminând confuzia dintre denumirea senzorului și a camerei.
- **Evidențiere Vizuală Web Console:** Cuvinte-cheie de stare (`OFF`, `ON`, `IDLE`, `DEMAND`, `AUTOMATIC`, `MANUAL`, `WARMUP`, `PASS`, `FAIL`) și link badges colorate viu și cu bold în `/console`.
- **Suită ASPICE SWE.5 100%:** Rulate toate cele 14 teste automate (`test_system.py`) pe hardware cu 14/14 PASS (100%).

## Completed Work (2026-10-12 - Task 20)
- **Perioadă de Warmup Senzori la Boot (Max 5 min / 300s):**
  - Implementată starea `ZONE_WARMUP` pentru a elimina alertele false premature de deconectare a senzorilor.
  - Ieșire anticipată din warmup în momentul în care toți senzorii asociați au fost detectați (ex. 3/3 în 30–60s).
  - Ridicare automată cod `DTC-101` la expirarea celor 5 minute dacă un senzor rămâne nedetectat, izolând doar zona afectată.
- **Formatare Tabelară Aliniată:** Tabele cu lățimi fixe pentru buclele de încălzire și registrul termometrelor.

## Completed Work (2026-10-12 - Task 18 & 19)
- **Consolă & Ergonomie Terminal (`NetworkConsole`):**
  - Marcaje de timp NTP `[HH:MM:SS]` la fiecare linie în Serial, Telnet și Web Console.
  - Colorare categorii loguri în Web Terminal (`/console`).
  - Prevenire poluare terminal cu pagini de eroare Nginx 502/504 Bad Gateway.
- **Reziliență Serviciu Meteo (`TimeAndWeatherService`):**
  - Socket HTTP timeout mărit la 3000ms și backoff de 60s la erori HTTP -11.
- **Programare Calendaristică Anti-Seize pe UNIX Epoch (`ThermalZoneManager`):**
  - Migrare de la `millis()` la `time(nullptr)` în NVS, eliminând pornirea falsă a pompei timp de 20s la reboot.
- **Timeout Senzor Failsafe Configurabil (`DTC-101`):**
  - Configurabil persistent în NVS (`_sensorTimeoutSec`, default 600s / 10 min) via CLI `timeout` și `/api/setup/sensor-timeout`.
- **Indicatori Calitate Semnal BLE:**
  - Evaluare RSSI (`EXCELLENT`, `GOOD`, `MARGINAL`, `POOR`) în Setup Wizard Web și CLI.
- **Hardware Watchdog TWDT 60s & Verificare SWE.5 100%:**
  - Calibrare TWDT la 60s pentru stabilitate Web OTA (1.28 MB). Suită automată pe hardware: **12/12 PASS (100.0%)**.

## Completed Work (2026-10-09 - Task 17)
- **Eliminare Bloatware (Galaxy-A6 & MQTT):**
  - Eliminat complet modulul legacy MQTT (`MqttTelemetryClient.cpp`, `MqttTelemetryClient.h`).
  - Dezactivat `-D MQTT_MAX_PACKET_SIZE=512` și eliminat dependența de `PubSubClient`.
  - Firmware-ul funcționează 100% autonom pe radio Bluetooth BLE nativ (`NimBLE-Arduino`).
- **Confort Acustic & Curățare Consolă:**
  - Eliminat orice spam automat de pachete BLE din consolă.
  - Datele senzorilor sunt afișate strict la cerere explicită (`STATUS`, `BLE`, `SENSORS` sau interogare REST).
- **Suită Automată de Testare Sistem ASPICE SWE.5:**
  - Implementat [`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py) și integrat în CLI (`python tools/esp_cli.py test`).
  - 12 cazuri de testare end-to-end acoperind toate cerințele din PRD (`FR-1`, `FR-4`, `FR-10`, `FR-11`, `FR-13`, `FR-14`, `FR-15`, `FR-17`, `NFR-1.1`).
  - Validat live pe hardware cu rată de succes de **100.0% (12/12 PASS)**.
  - Generat raport oficial de testare în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).
- **Headless API-First Architecture & UI Latency Elimination:**
  - Decoupled backend REST endpoints from front-end volatility.
  - Eliminated UI lag: converted all buttons to 0ms optimistic DOM updates and asynchronous background POSTs (removed `location.reload()`).
  - Smart location selection without GPS coordinate requirements (Geocoding search + 41 Romanian county capitals + IP/GPS auto-detection).
  - Zone index normalization (supports 1-based and 0-based indices).
  - Comprehensive CLI utility [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).
- **Failsafes de Siguranță, Hardening Firmware & Suport Hidraulic (Task 15):**
  - Control pompă secundară GPA via Releu 4 (`GPIO26`) cu interlock și 180s overrun.
  - Actuatoare NC / NO configurabile persistent în NVS.
  - Protecție forțată la îngheț (<12°C).
  - Izolare avarie senzor (`DTC-101`) >30min.
  - Watchdog hardware Task Watchdog Timer (TWDT 30s).
- **Failsafe Anti-Spam & Interblocare de Siguranță Arzător Cazan (Task 16):**
  - Filtru idempotență (FR-17.1), Cooldown RF (FR-17.2), Gardă anti-short-cycling 60s (FR-17.3), răspuns HTTP 429 cu waitSec (FR-17.4), detecție abuz spam DTC-201 (FR-17.5), suprascriere `--force`.

## Next Milestones
- [x] Hardware wiring & Pinout confirmation for RF transmitter daughterboard.
- [x] ESP8266 C++ driver for boiler RF transmission (`BoilerRadioTrigger`).
- [x] Live boiler RF firing test (ON / OFF) via web UI or Serial (Confirmat de utilizator).
- [x] Multi-zone thermal hysteresis & actuator interlock engine (`ThermalZoneManager`).
- [x] Full architectural migration to native **ESP32** (ESP32-Relay X4 V1.1, DOIT ESP32-32E N4) on `192.168.1.20`.
- [x] Codebase English localization (UI, CLI, REST APIs, logs, comments).
- [x] Wireless Web OTA update verified live via Wi-Fi (`/update`).
- [x] 2-Way Virtual Serial Network Console via Wi-Fi (Telnet TCP :23 & Web `/console`).
- [x] Native ESP32 BLE Scanner for Xiaomi sensors via NimBLE.
- [x] Setup Wizard, NVS persistence, Device Passport, NTP & Weather service.
- [x] Diagnostic Engine (DTCs, Self-Test, Service Mode).
- [x] Headless API-First refactoring, UI latency elimination, and `esp_cli.py`.
- [x] Failsafes de Siguranță, Hardening Firmware & Suport Hidraulic Universal pentru Pardoseală.
- [x] Failsafe Anti-Spam & Interblocare de Siguranță Arzător Cazan (ASPICE SWE.1 - SWE.4).
- [x] Eliminare Bloatware MQTT & Dependențe Galaxy-A6 (Arhitectură 100% BLE nativă).
- [x] Suită Automată de Testare pe PC & Verificare ASPICE SWE.5 (100% Pass Rate).
- [ ] Cloud Fleet Outbound Tunnel prototype (Outbound TLS WebSocket/MQTTS).
- [ ] Field installation on physical manifold actuators when received.
