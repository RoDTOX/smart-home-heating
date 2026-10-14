# Progress: Smart Home Heating System

## Status
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, 2-way console, web OTA, diagnostic engine with DTCs, service mode, digital passport, NTP & weather sync, headless API-first architecture, hydronic safety controls, anti-spam burner interlocks, Bio-Schedule with presets engine, and atomic PC system test suite with bidirectional traceability matrix) are fully implemented, compiled, uploaded, and verified live on ESP32 (`192.168.1.20`) with 100% ASPICE SWE.5 pass rate (63/63 PASS, 33/33 requirements covered, 0 uncovered).

## Completed Work (2026-10-14 - Sarcina 30 Schemă Tesla Service Mode & Sarcina 31 Diagramă Stări FSM)
- **Schemă Termodinamică & Hidraulică Interactivă (Sarcina 30):**
  - Adăugată vedere completă vectorială SVG în stilul Tesla Supermanifold:
    * Trasee animate pentru circuitul Tur (portocaliu cald) și Retur (albastru rece) cu flux de particule via `@keyframes teslaPipeFlow`.
    * Card cazan cu flacără dinamică (verde radiant la ardere, standby gri) și contor cooldown rămas.
    * Pompă GPA cu rotor animat rotativ (`.pump-spinning`), debit 1850 RPM și casetă de date pe fundal de contrast ridicat.
    * Supermanifold cu debitmetre, actuatoare electrotermice NC comutate 230V vs 0V și casete de cameră cu telemetrie live ($T_{cameră} \mid T_{target}$).
    * Casete telemetrie stil Tesla: $T_{tur}$ (48.5°C ardere / 24°C standby), $T_{retur}$ (38.0°C / 23.5°C), $\Delta T$ sorbție (10.5°C Optim / 0.5°C Echilibru) și stare pompă.
    * Panou de comandă service cu 5 acțiuni tactile: Purjare/Test Pompă GPA, Test RF Cazan, Test Valvă Copil, Test Valvă Living, Curățare DTC Diagnoză.
- **Diagramă Interactivă de Stări FSM & Fir Logic Live (Sarcina 31):**
  - Adăugată vedere cu 9 noduri de stare decizionale: `IDLE / STANDBY`, `CERERE TERMICĂ`, `AȘTEPTARE ACTUATOR`, `MANUAL HOLD`, `ÎNCĂLZIRE ACTIVĂ`, `POST-CIRCULAȚIE POMPĂ`, `GARDĂ COOLDOWN`, `PROTECȚIE ÎNGHEȚ`, `GEAM DESCHIS`.
  - Evidențiere dinamică a nodului activ (`.active`, bordură cyan electric, efect de glow) cu afișarea timerelor active în interiorul nodului (ex: `19s Lockout`) și indicator sintetic în colțul din dreapta.
- **Suită Automată de Testare E2E pe Browser Nativ (Task 32 - `tools/browser_live_test.py`):**
  - Rulat direct pe Google Chrome al utilizatorului via CDP WebSocket cu **100% succes (10/10 PASS)**:
    * `TC-UI-11` (Geometrie): Coloane paralele side-by-side ($X_1=56\text{px}$, $X_2=595\text{px}$).
    * `TC-UI-01`, `TC-UI-05`, `TC-UI-03`, `TC-UI-04`, `TC-UI-06`: Cele 3 moduri de control (simplu, preseturi, avansat) cu comutare ținte fără reload.
    * `TC-UI-09`: Anti-spam toast discret de 3s la clicuri repetate pe OFF.
    * `TC-UI-13`: Schemă Tesla vizibilă, circuite vectoriale și telemetrie tur/retur.
    * `TC-UI-15`: Butoane acțiuni service Tesla funcționale cu toast feedback instantaneu.
    * `TC-UI-14`: Diagramă FSM cu 9 noduri și evidențiere dinamică a nodului activ.
  - Salvate capturi de ecran complete în directorul de artefacte (`e2e_01_beginner_mode.png` .. `e2e_07_fsm_live_diagram.png`).
- **Validare Binar & ASPICE SWE.5:**
  - Firmware compilat (1176.5 KB, 60.9% flash, 112.5 KB spațiu liber sub limita OTA de 1.25 MB) și încărcat via OTA în 7.9s (`v1.2.0 Build: Oct 14 2026 16:55:37`).
  - Suita de 63 de teste SWE.5 (`tools/test_system.py`) continuă să ruleze cu **100.0% PASS**.

## Completed Work (2026-10-14 - Task 32 Native Browser E2E Automation, Layout Repair & Complexity Tuning)
- **Suită Automată de Testare E2E pe Browser Nativ (Task 32 - `tools/browser_live_test.py`):**
  - Executată cu succes direct pe Google Chrome prin Chrome DevTools Protocol (CDP) cu 100% rată de succes (**7/7 PASS**):
    * `TC-UI-11` (Geometrie): Coloana 1 ($X=56\text{px}, Y=199.6\text{px}$) și Coloana 2 ($X=595\text{px}, Y=199.6\text{px}$) sunt strict paralele, simetrice și aliniate side-by-side fără nicio cădere sub grid.
    * `TC-UI-01` (Nivel Simplu): Ascundere completă orar și instrumente de inginerie, afișare notificare discretă și control direct temperatură.
    * `TC-UI-05` (Nivel Preseturi): Ascundere butoane tehnice (`Reset fabrică`, `Salvare slot custom`) pentru un UX curat.
    * `TC-UI-03` & `TC-UI-04` (Scenarii Preseturi): Toate cele 5 chip-uri de preseturi comută dinamic temperaturile fără reîncărcare de pagină (`Home Office`, `Vacanță / Eco`, `Standard / Confort`).
    * `TC-UI-06` (Nivel Avansat): Deblocare completă editor tabelar ancore L-V / Weekend și acțiuni sloturi NVS.
    * `TC-UI-09` (Anti-Spam Toast Feedback): Clicurile repetate pe `OFF` când centrala este deja oprită afișează un toast discret de 3 secunde (`ℹ️ Centrala este deja oprită (Standby)`) fără declanșare de pachete HTTP sau bannere alarmiste.
  - Generat și salvat 5 artefacte vizuale de înaltă rezoluție în directorul brain (`e2e_01_beginner_mode.png` .. `e2e_05_anti_spam_toast.png`).
- **Corecție Structură HTML & Balanță Grid (`web/index.html`):**
  - Eliminat tagul duplicat `</div>` care rupea prematur containerul CSS Grid. Adăugat identificatori riguroși `#col-main-1` și `#col-main-2`.

## Completed Work (2026-10-14 - Boiler Failsafe, Deferred Pending Actions & PWA Kiosk)
- **Failsafe la Cold Boot (Power-Loss Safety Invariant):**
  - La pornirea din priză (`begin()`), se transmite forțat `sendBoilerOff(true)` burst pe 433.92 MHz și se stochează starea `OFF` în NVS. Acest comportament garantează stingerea fizică a arzătorului după orice cădere de tensiune sau resetare la cald, eliminând orice discrepanță între receptorul radio fizic și ecranul de control.
  - Adăugată persistență NVS cu protecție anti-uzură flash pe namespace-ul `"boiler"` și heartbeat periodic la 15 minute.
- **Coadă Execuție Amânată (Pending Execution & Anti-Short-Cycling Cooldown):**
  - În `handleApiOff` și `handleApiOn`, comenzile trimise în timpul perioadei de protecție a arzătorului (60s ardere minimă / 60s pauză minimă) sunt memorate în coada `PENDING_OFF` / `PENDING_ON` și returnează HTTP 429 cu `pending: true` și `waitSec`.
  - În `loop()`, metoda `boilerRadio.update()` execută automat comanda amânată imediat ce expiră timpul de gardă. Apăsarea comenzii inverse anulează comanda programată (`pendingCancelled: true`).
  - În interfața web: badge dedicat pulsatoriu (`.badge-pending`), banner informativ și numărătoare inversă la secundă (`⏳ Xs rămase`).
- **Sincronizare Preseturi & UX Ancore Dinamice:**
  - Sincronizate setpoint-urile fiecărei zone termice cu ancora orarului la aplicarea oricărui preset (`wfh`, `eco`, `default`).
  - Cardurile de zone afișează `effectiveTarget` din orar, iar butoanele `+`/`-` modifică ținta ancorei curente din Bio-Schedule.
  - Denumirile ancorelor se adaptează contextual la preset (`💼 HOME OFFICE (ZI)` la WFH, `🍃 VACANȚĂ / ECO (ZI)` la Eco).
- **Verificare ASPICE SWE.5:**
  - 100% PASS la toate cele 63 de teste atomice (33/33 cerințe acoperite) pe firmware flashuit live via OTA pe `192.168.1.20`.

## Completed Work (2026-10-13 - Task 26)
- **Smart Bio-Schedule cu 4 Ancore Naturale de Viață:**
  - Înlocuită paradigma clasică a orarelor de 168 celule cu un model adaptiv bazat pe 4 stări organice de confort: `Sleep` (20.0–21.0°C), `Wake` (22.0–22.5°C), `Away` (19.5–20.0°C), `Home` (21.5–23.0°C).
  - Profiluri separate configurabile pentru Luni–Vineri (Weekday) și Sâmbătă–Duminică (Weekend). Evaluare la fiecare 30 secunde conform ceasului de sistem NTP.
- **Detecție Fereastră Deschisă (Open Window Smart Pause Guard):**
  - Implementat istoric circular de eșantioane de temperatură (10 eșantioane la 30s).
  - La detectarea unei scăderi bruște de >1.2°C în <5 min, valva zonei este forțată automat în starea ÎNCHIS timp de 30 minute, prevenind risipa inutilă de gaz în timpul aerisirii. Anunțat în Network Console și în telemetria `STATUS`.
- **Pre-Heat Anticipativ cu Inerție Termică & Factor Meteo:**
  - Calcul dinamic al avansului de pornire (30–120 min) pe baza inerției șapei de beton și a temperaturii exterioare curente de la Open-Meteo ($k_{weather} = 1.0 + \max(0.0, (15.0 - T_{ext}) \times 0.04)$), asigurând temperatura optimă exact la ora sosirii sau trezirii.
- **Persistență NVS Anti-Uzură Flash:**
  - Structură de 136 octeți salvată în namespace-ul `"sched"`, protejată prin `memcmp()` pentru a scrie flash-ul strict la modificări reale.
- **REST API, CLI & Secțiune Dedicată în STATUS:**
  - Endpoint-uri `/api/schedule` (GET/POST) și `/api/schedule/enable` (POST); comenzi interactive în consolă (`SCHEDULE`, `SCHEDULE ON/OFF`, `SCHEDULE PREHEAT ON/OFF`, `SCHEDULE WINDOW ON/OFF`).
  - Secțiunea `[D. SMART BIO-SCHEDULE & PRE-HEAT]` adăugată în `STATUS`. Subcomanda `schedule` adăugată în `esp_cli.py`.
- **Eliminare Zgomot Meteo & Minificare Flash OTA:**
  - Eliminat spam-ul periodic din `netConsole` generat de actualizările meteo la 15 minute (redirecționate pe UART Serial).
  - Minificat CSS-ul paginilor web (`main.cpp`, `NetworkConsole.cpp`, `WebSetupPage.h`), aducând binarul la **1.24 MB** (1,305,280 bytes).
- **Validare Live pe Hardware (ASPICE SWE.5):**
  - Binar flashuit cu succes prin Web OTA (`192.168.1.20`), adăugat testul automat `SWE.5-TC15`, rezultând **15/15 PASS (100.0%)** în 15.65 secunde.

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
