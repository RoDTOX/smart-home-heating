# Active Context: Smart Home Heating System

- **Inițiere Migrare Home Server pe Lenovo ThinkPad L590 (În curs 19.10.2026):**
  - **Hardware:** Lenovo ThinkPad L590 (Intel Core i7 Gen 8, 32 GB RAM, 475 GB SSD NVMe). Baterie în stare bună (UPS integrat), ceas RTC sincronizat automat via NTP `systemd-timesyncd`.
  - **Arhitectură convenită:** Ubuntu Server 24.04 LTS (fără GUI, consum minim ~250MB RAM, instalare curată pe tot SSD-ul), `HandleLidSwitch=ignore` pentru funcționare cu capacul închis, prag încărcare baterie la 80% (`tlp`/sysfs) pentru protecție pe post de UPS.
  - **Servicii Docker Compose unificate:** Home Assistant Core (`smart_heating.yaml`), Mosquitto MQTT broker, TeslaMate + PostgreSQL 17 + Grafana (cu restaurare backup `pg_dump` de pe Galaxy-A6), Smart Aquarium, Nginx Reverse Proxy, Cloudflare Named Tunnel (`mysmarthome.cc` refolosind tokenul existent din `/etc/cloudflared/token`).
  - **Stare Curentă:** Imaginea ISO Ubuntu Server a fost descărcată. Urmează scrierea pe stick USB cu Rufus, setarea BIOS `Power On with AC Attach` și instalarea pe SSD.

- **Aliniere Riguroasă 1:1 Documentație & Testare ASPICE SWE.5 pe Hardware Real (Finalizat 18.10.2026):**
  - **44/44 Cerințe Acoperite (100.0%):** FR-1..FR-40 + NFR-1.1, NFR-1.5, NFR-2.2, NFR-3.2 documentate în detaliu în `docs/requirements_and_features.md` cu trasabilitate bidirecțională completă către codul sursă C++ și suita de testare.
  - **85/85 Teste Verificare Live (100.0% PASS):** Suita automată `tools/test_system.py` extinsă de la 68 la 85 de teste atomice, executate împotriva plăcii reale ESP32 (`192.168.1.20`). Zero cerințe netestate, zero contracte relaxate.
  - **Hardening Logică & Boundary Clamping:** Descoperit și corectat intervalul permis pentru pragul cădere geam deschis în C++: forțat strict `[0.8°C .. 2.0°C]` (default 1.2°C) în `ThermalZoneManager.cpp` și validare HTTP 400 în `main.cpp` pentru valori eronate/periculoase.
  - **Documentație Completă Actualizată:** `docs/requirements_and_features.md`, `docs/api_reference.md`, `docs/underfloor_heating_hydraulics_and_failsafes.md`, `docs/system_test_report.md`, `docs/plan.md`.

- **Suită Completă Detecție Geam Deschis, Prag Configurabil & Persistență Git Home Assistant (Finalizat 18.10.2026):**
  - **Folder Unificat `homeassistant/` Persistat în Git:**
    * Mutat și consolidat întreaga integrare direct în rădăcina proiectului în folderul `homeassistant/` (nu mai depinde de mașina virtuală WSL): `packages/smart_heating.yaml`, `dashboards/smart_heating.yaml`, `configuration.yaml`, `docker-compose.yml`, `README.md`.
    * Adăugat în `.gitignore` excluderile pentru fișierele de runtime generate local de container (baze de date `.db`, loguri, `.storage`).
  - **Sistem Inteligent Detecție Geam Deschis (Open Window Guard):**
    * **FSM & Siguranță Fizică:** La o cădere bruscă de temperatură (ex. $\ge 1.2^\circ\text{C}$ în $\le 5$ min), zona intră în `ZONE_WINDOW_PAUSE`, decuplează instant vana electrotermică (0V) și stinge cazanul dacă nu mai există cerere în alte camere.
    * **Ieșire Automată & Manuală:**
      1. Auto-timeout: 30 de minute expirate.
      2. Rebound termic: revenire automată dacă temperatura crește cu $+0.6^\circ\text{C}$ (geamul a fost închis și camera se reîncălzește).
      3. Override manual: utilizatorul poate anula instant pauza din butonul `[↺ Am Închis (Reia)]` atât din Web UI cât și din Home Assistant. La anulare, se resetează temperatura baseline la valoarea curentă pentru a preveni declanșarea falsă imediată.
    * **Prag de Sensibilitate Configurabil per Zonă (0.8°C – 2.0°C):**
      - Adăugat parametrul `windowDropThreshold` în `ThermalZoneConfig`, persistat în flash NVS (`<Preferences.h>`).
      - Endpoint dedicat `POST /api/zones/window/threshold?id=X&threshold=Y` și modal de configurare direct pe badge-ul din UI (`🪟 Prag: 1.2°C ✏️`).
    * **Telemetrie & Istoric:**
      - Înregistrează timestamp-ul și durata ultimei aerisiri (`lastDetectionEpoch`, `lastDetectionDurationSec`), expuse în `GET /api/zones`.
      - Când geamul este închis, se afișează subtil istoricul: `🪟 Aerisit: acum 15m (18m)`.
    * **Vizibilitate Completă Web UI & Home Assistant:**
      - Web UI: Banner animat proeminent pe cardul camerei cu cronometru rămas (`MM:SS`) și buton direct `[↺ Am Închis (Reia)]`. Schemă SVG actualizată cu minutele rămase.
      - Home Assistant: 3 senzori `binary_sensor` cu `device_class: window`, 3 senzori de countdown, 3 template buttons `button.reia_incalzire_geam_*` și carduri de alertă condiționale Lovelace (apar automat pe dashboard când un geam este deschis și dispar când se închide).
    * **Build & Verificare:** PlatformIO `pio run` SUCCESS (21.1s, RAM: 21.6%, Flash: 64.7%). Header web comprimat cu `build_web.py` (40.3 KB gzip).

- **Integrare Telemetrie Istoric LittleFS pe ESP32, Model Termic Gaz, Grafic Canvas Live & Compatibilitate Home Assistant (Finalizat 17.10.2026):**
  - **Flash Telemetry History Manager pe ESP32 (`TelemetryHistoryManager.cpp/.h`):**
    * Structură binară ultra-compactă de 10 bytes/punct (3 zone scaled x10 int16, temp exterioară int16, flags biți flacără/pompă/valve/valid).
    * Două buffere circulare persistate pe LittleFS în partiția `spiffs` de 192KB: 24h @ 5 min (288 puncte = 2.8 KB) și 5 zile @ 30 min (240 puncte = 2.4 KB) — total doar ~5.2 KB.
    * Wear-leveling hardware peste 48 de sectoare flash: scriere pe oricare sector individual la fiecare ~4 ore (>15 ani durabilitate garantată).
    * Endpoint REST `/api/history?range=24h|5d` cu payload array-of-arrays compact (<4.5 KB) și fără fragmentare RAM.
    * Comenzi seriale / telnet adăugate: `HISTORY`, `HISTORY CAPTURE`, `HISTORY CLEAR`.
    * Compilare curată verificată cu `pio run` (SUCCESS, 119s, RAM: 21.5% [70 KB], Flash: 64.3% [1.26 MB]).
  - **Grafic Canvas Live de Înaltă Rezoluție (`web/index_v2.html`):**
    * Comutare instantanee `[24 Ore (5m) | 5 Zile (30m)]` cu fetch asincron din `/api/history` și cache local.
    * Randare curbe reale pentru cele 3 zone (Living portocaliu, Copil albastru, Matrimonial verde) și bare portocalii umbrite pentru ciclurile de ardere flacără cazan.
    * Fallback estetic pentru mod offline/demo.
  - **Model Termic Gaz & Card Telemetrie (Layer 4.5):**
    * Calibrat baseload apă caldă menajeră (ACM): `0.40 m³/zi` (~`12.0 m³/lună`), dedus strict din facturile de vară (Mai-Iulie 2026, 39 MC citit în 94 zile).
    * Calibrat debit centrală în condensație pe pardoseală la temperatură joasă: `0.60 m³/oră` de ardere flacără (~6 kW putere medie modulată).
    * Configurat preț reglementat plafonat gaz: `3.30 Lei/m³` (0.31 Lei/kWh).
    * Afișaj mecanic virtual cu role (Tumbler) pentru contorul fizic `Serie: 34119099055` și asistent transmitere index autocitire.
  - **Tab Calibrare & Autocitire în Admin Suite (PIN 1234):**
    * Sincronizare 1:1 index contor, parametri termici și reconciliere tabele facturi.
  - **Pachet YAML Complet Home Assistant (`integrations/homeassistant/`):**
    * `smart_heating.yaml` gata de producție cu `sensor.smart_heating_gas_meter_index` pentru Energy Dashboard, senzori zilnici, climate și switch-uri REST.
  - **Integrare Completă Home Assistant & Portare Sistem Totală (Finalizat 18.10.2026):**
    * Pachetul `packages/smart_heating.yaml` migrat la sintaxa modernă Home Assistant `template:` (suport complet 2024–2026):
      - Înlocuit blocurile deprecate `button: - platform: template` și `switch: - platform: template` cu blocul unificat `template: - switch: ... - button: ...`.
      - Rezolvat eroarea Home Assistant Repair (`platform_integration_no_support_button_template` și `switch_template`). 0 erori/repairs active.
      - 3 Termostate `climate` cu buclă bidirecțională automată fără latență (HA <-> ESP32).
      - Contor de gaz cu index fizic (m³), consum zilnic și cost RON pentru Energy Dashboard.
      - Comutator Mod Centrală `switch.smart_heating_auto_mode` (AUTO vs MANUAL).
      - Comutator `switch.smart_heating_schedule` și selector `input_select.smart_heating_preset` pentru Bio-Schedule.
      - Butoane comenzi forțate: Pornește Cazan, Oprește Cazan, Anulează Cooldown/Amânare, Rulează Self-Test, Șterge Erori DTC.
      - Telemetrie completă termometre Xiaomi BLE: Baterie %, Umiditate %, Semnal RSSI dBm per cameră.
      - Diagnostic hardware ESP32: Scor Sănătate %, RAM Liber (Heap KB), Uptime ore, Wi-Fi RSSI dBm, Serie și Firmware.
    * Creat Dashboard Dedicat Lovelace (`dashboards/smart_heating.yaml`) cu 3 vederi specializate:
      1. `Panou Principal`: Badges stare, card Centrală & Pompă, cele 3 termostate mari, card Bio-Schedule și grid comenzi rapide.
      2. `Gaz & Energie`: Carduri factură/consum și 2 grafice istorice (Temperaturi 24h + Cicluri ardere flacără și pompă).
      3. `Senzori & Diagnostic`: Tabel baterii și semnal BLE Xiaomi, diagnoză controller ESP32 și acțiuni de testare.
    * Verificate toate cele 46 de entități din dashboard: 100% asociate și înregistrate în Home Assistant Core. Zero avertismente "Entity not found".
    * Dashboard-ul este integrat direct în bara laterală Home Assistant (`Smart Heating`, icon `mdi:radiator`) pe mobil și desktop.

- **Remediere FSM Race Condition (Vane Blocate Deschis), Comportament Suprascriere Temporară (Temporary Hold) & Overrun Inteligent Cazan (Finalizat 18.10.2026):**
  - **Identificare Root-Cause Bug Vane Blocate Deschis:** La scăderea setpoint-ului, Pasul 1 (histerezis) forța prematur `state = ZONE_IDLE`. Pasul 2 (releele vanelor) verifica `if (state == ZONE_HEATING || state == ZONE_WARMING_UP)` pentru decuplare; condiția pica mereu, lăsând releul alimentat la 230V permanent (`relayEngaged: true`), chiar dacă cererea era `false` și starea era `Idle`.
  - **Remediere Arhitectură FSM (`src/ThermalZoneManager.cpp`):**
    * Pasul 1 setează exclusiv `demand = false`, decuplarea sigură a releelor și tranziția de stare fiind executate determinist în Pasul 2.
    * Adăugat **Hardware Safety Invariant**: dacă o vană are `relayEngaged == true` dar `demand == false` (și nu este keeper zone într-un overrun activ), releul este decuplat necondiționat.
    * **Instant Warmup Abort**: scăderea setpoint-ului în timpul dilatării cerii (120s) decuplează instant releul și anulează orice cerere de flacără/pompă.
    * **Overrun Inteligent Cazan**: post-circulația pompei de 180s și menținerea deschisă a vanei de disipare au loc **exclusiv dacă flacăra cazanului a ars efectiv** în ciclul respectiv; dacă flacăra nu s-a aprins (comanda a fost anulată sau a căzut cererea înainte de ardere), totul se oprește imediat (0s overrun).
  - **Suprascriere Temporară (Temporary Hold) Integrată cu Bio-Schedule:**
    * Modificarea setpoint-ului dintr-o cameră (din HA sau Web) activează regimul `isManualHold` în acea cameră fără a modifica sau strica presetul salvat.
    * La sosirea orei următoarei ancore circadiene (ex. Somn 20:00), hold-ul expiră automat și camera revine la valoarea din orar.
    * Adăugat endpoint `POST /api/zones/hold/clear?id=X` și buton contextual `[↺ Hold]` în interfața web pentru revenire manuală anticipată.
  - **Protecție Flash NVS & Anti-Chatter Relee:**
    * Salvarea setpoint-urilor în flash NVS este temporizată non-blocant cu debounce de **3 secunde** (`_nvsSavePendingMs`), protejând memoria cipului la glisarea rapidă a sliderelor de temperatură din telefon.
  - **Verificare & Build:** Compilare PlatformIO curată (`pio run` SUCCESS în 18.1s, RAM: 21.5%, Flash: 64.4%).

- **Refactorizare Densitate Extremă UI 720p, Redesenare Card Centrală & Rezolvare Comutare Preset Utilizator (Finalizat 16.10.2026):**
  - **Compactare Extremă Antet & Card Centrală pentru Ecrane 720p (`web/index.html`):**
    * Brand compact (`Smart Heating`) pe un singur rând, eliminat subtitlul redundant.
    * Scurtat denumirile tab-urilor (`Panou`, `Tesla Mode`, `FSM Logic`, `Consolă Live`) și ascuns textele ajutătoare de nivel și mod de vizualizare pentru a preveni wrap-ul pe ecrane de 720p / tablete.
    * Cardul centralei (`#card-boiler`) compactat pe o singură bandă orizontală (~40px) cu starea flăcării, pompa GPA și modul `[⚙️ AUTO | ✋ MANUAL]`.
    * Butoanele `Pornește Centrala` și `Oprește Centrala` sunt acum complet **ascunse** în mod `AUTOMATIC` și devin vizibile exclusiv pe modul `MANUAL`.
    * Cele 3 contoare de funcționare (`Azi`, `Ieri`, `Total`) și estimarea de gaz au fost mutate din cardul principal într-un modal dedicat (`#modal-gas-costs`) accesibil prin linkul discret `🔥 Consum & Cost Gaz` din footerul cardului de zone.
  - **Redesenare Ergonomică Rânduri Zone Termice:**
    * Butonul de alimentare `⏻` mutat ergonomic în stânga, între indicatorul BLE și numele camerei (`[🟢 Dot] [⏻] [Nume Cameră ✏️]`).
    * Eliminat chenarul verde intruziv "Repaus" când zona este inactivă. Badge-ul apare exclusiv când zona acționează (`♨️ Încălzire`, `⏳ Așteptare`, etc.).
    * Eliminat eticheta `țintă`; butoanele de reglaj `[-] 21.0° [+]` sunt grupate direct lângă temperatura curentă și graficul sparkline.
  - **Rezolvare Resetare Preset Activ pe "Standard / Confort":**
    * Identificat că testele `SWE.5-TC17.3` (`/api/schedule/reset`) și `SWE.5-TC18.4` (`apply?id=default`) din `tools/test_system.py` resetau presetul activ la `default`.
    * Adăugat în `tools/test_system.py` capturarea inițială `self.user_initial_preset` și restaurarea automată la finalul suitei.
    * Adăugat în `src/ThermalZoneManager.cpp` log explicit de tranziție: `[Schedule][PRESET] Preset switched: 'old' -> 'new'`.
    * Presetul utilizatorului `Bogdan` (`custom1`) este restaurat și confirmat activ.
  - **Format Widescreen 16:9 & Aliniere Laterală (`web/index.html`):**
    * Înlocuit `max-width: 1060px` cu un container expandabil până la `1560px` cu centrare automată (`margin: 0 auto; padding: 12px 16px 24px;`). Pe ecrane 16:9 conținutul acoperă armonios lățimea display-ului fără margini negre masive.
    * Decuplat breakpoint 2 coloane la 1140px; sub 1140px totul curge într-o singură coloană intuitivă (Bara de sus -> Centrală -> Zone Termice -> Bio-Schedule).
    * Eliminat complet textul static `NimBLE 90% Scan`.
  - **Consolă Wi-Fi Live Integrată & Proxy Local (`tools/dev_server.py`, `web/index.html`):**
    * `dev_server.py` pe portul 8080 servește ca reverse proxy transparent către ESP32 (`192.168.1.20`).
    * Adăugat tab dedicat `>_ Consolă Live` direct în dashboard-ul principal cu streaming în timp real și comenzi directe.
  - **Verificare & Calitate Cod:**
    * Compilare PlatformIO `pio run` (SUCCESS, Flash: 62.4%, RAM: 19.9%).
    * Suita automată ASPICE SWE.5 (`tools/test_system.py`) trecută cu **100% PASS (68/68, 33/33 cerințe)** cu păstrare garantată a presetului activ.
    * Verificare vizuală completă prin subagent browser pe `http://localhost:8080/index.html`.

- **Curățare Totală Consolă, Izolare Suită Teste ASPICE & Failsafe Fără Spam Boiler (Finalizat 15.10.2026):**
  - **Izolare Totală Suită Teste Hardware (`tools/test_system.py`):**
    - S-a eliminat spamul generat de testul `test_tc10_1` prin apelarea imediată a `/api/boiler/cancel` după cele 3 cereri de test pentru DTC-201, prevenind aprinderea amânată a flăcării reale (după 50s) care declanșa bucla de spam la oprire.
    - S-a eliminat poluarea consolei din `test_tc13_1` și `test_tc13_3`: testul `STATUS` șterge automat cele 100 de linii de tabel din inelul de loguri (`CLEAR`) imediat după aserțiune.
    - S-a corectat testul `test_tc19_2` (clamping temperaturi): folosește acum `/api/mode?mode=manual` și dezactivare temporară orar, izolând complet releele fizice și prevenind acționarea actuatorilor la 30°C.
    - S-au înlocuit bannerele lungi ASCII cu mesaje discrete de 1 rând (`[TEST-SUITE] >>> Automated Test Suite Started / Finished <<<`) și curățare completă la final.
  - **Remediere Comandă CLEAR / CLS pe Consolă:** În `NetworkConsole.cpp`, s-a corectat funcția `clear()` pentru a reseta `_totalBytes = 0`, `_head = 0`, `_count = 0` și `_logRing`. În `executeCommand()`, comanda `CLEAR` / `CLS` apelează direct `clear()` fără a mai re-inunda bufferul cu `[CMD] < END: CLEAR`. În `web/console.html`, comanda și butonul `CLEAR` curăță instantaneu DOM-ul (`term.innerHTML = ''`), resetează `lastSeq = 0` și șterg memoria circulară pe ESP32.
  - **Eliminare Spam Buclă Oprire Cazan (DTC-201 Fix):** În `src/ThermalZoneManager.cpp`, s-a remediat bug-ul în care `sendBoilerOff()` era apelat la fiecare secundă când zonele își atingeau setpoint-ul dar cazanul se afla încă în perioada de gardă (60s anti-short-cycling). Acum se verifică `boiler.canTransmit(false)`. Dacă garda este activă, se programează `PENDING_OFF` o singură dată în coada RF.
  - **Unificare Coloana 2 & Eliminare Card Duplicat:** Cardul superior `Zone Termice (Pardoseală)` și cel inferior `Telemetrie Senzori Xiaomi BLE` au fost consolidate într-un singur card compact `#card-zones-sensors`.
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS, suita de teste atomice `tools/test_system.py` cu 68/68 PASS (100.0%) în 24.51s, bufferul de consolă rămânând 100% curat cu un singur rând de finalizare.

- **Sincronizare Live Orar Circadian, Preseturi Calibrate din Fabrică & Tracking Runtime Cazan (Finalizat 15.10.2026):**
  - **Sincronizare Live Card Erou & Numărătoare Inversă:** Expus `activePreset`, `activeAnchor`, `nextAnchor` și `minsToNextAnchor` în `/api/status` la fiecare poll (3.5s). Frontend-ul actualizează instantaneu cardul erou circadian (`renderActiveAnchorHero`) și bannerul numărătorii inverse (`updateNextAnchorCountdown`) fără a mai necesita refresh manual de pagină (F5).
  - **Calibrare Preseturi din Fabrică (L-V & S-D):** Preset-urile `Standard / Confort` și `Home Office` au fost actualizate în codul sursă C++ cu orele și temperaturile exacte din capturile utilizatorului (ex. Somn 20:00 L-V, 21:00 S-D; Trezire 06:00 L-V, 07:30 S-D; Home Office 07:30 L-V, 12:00 S-D).
  - **Redesign Panou Preseturi & Editor Matriceal:** Sloturile utilizator libere (`Slot Liber (custom2..4) (Liber)`) sunt ascunse din UI. Adăugată bară dedicată de acțiuni: `➕ Adaugă Slot`, `💾 Salvează`, `↺ Anulează (Discard)`, `✏️ Redenumește`, `🗑️ Șterge`. Butonul matricei a fost retras într-un stil discret și compact; fereastra matriceală afișează coloane paralele `Oră L–V` și `Oră S–D` cu buton rapid de clonare `📋 Copiază L-V în S-D`.
  - **Jurnalizare Insight-uri & Clarificare Mesaje Cerere Termică:** Formatat log-ul de pornire cazan cu 2 zecimale exacte (`current 22.21 C <= start threshold 22.20 C`) pentru a elimina formulările confuze de tip `22.2 <= 22.2`. Adăugat log automat în consolă la răcire bruscă a camerei: `[Thermal][INSIGHT] Rapid cooling detected on Zone X: ... possible aeration`.
  - **Tracking Timp Funcționare Cazan & Estimare Factură Gaz:** Implementat contor non-blocant în `BoilerRadioTrigger` cu persistență NVS și wear-leveling (astăzi, ieri, total). Afișat în UI cu dialog `⚙️ Factură / Cost` pentru calibrare pe baza facturii reale de iarnă (default 600 lei / 180 ore flacără).
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS (Flash 1.22 MB, RAM 19.9%), suita de teste `tools/test_system.py` cu 68/68 PASS (100.0%) în 23.82s.

- **Igienizare Memorie Heap C++ & Decuplare Dev Server Frontend (Finalizat 15.10.2026):**
  - **Zero Fragmentare Heap pe ESP32:** Implementat pre-alocare exactă de buffer `json.reserve(...)` / `out.reserve(...)` în toate serializatoarele JSON și rapoartele din C++ (`main.cpp`, `ThermalZoneManager.cpp`, `DiagnosticEngine.cpp`, `TimeAndWeatherService.cpp`, `DevicePassport.cpp`). Eliminat zecile de micro-realocări heap per ciclu de interogare HTTP, asigurând stabilitate pe termen lung a Free Heap-ului (>115 KB).
  - **Curățare Frontend & Optimizare CSS:** Eliminat blocul duplicat `@media (max-width: 520px)` din `web/index.html`. Compilare gzip automată `build_web.py` validată (119 KB -> 29.1 KB).
  - **Decuplare Dev Server Local (`tools/dev_server.py`):** Server local pe portul 8080 (`http://localhost:8080/index.html`) conectat prin CORS la ESP32 (`192.168.1.20`) pentru iterații UI ultra-rapide cu zero uzură flash.
  - **Micro-Grafice Vectoriale Sparkline de Trend & Aliniere Tabel Senzori:** Upgrade complet în `web/index.html` și `include/Config.h`. Chei canonice de istoric unificate (`getCanonicalTempKey`) garantând curbe 100% identice între cardul camerei și linia senzorului (fără linii dințate/zbârcite). Tabelul de senzori BLE a fost re-aliniat strict pe coloane (`.sensor-temp-col`, `.sensor-hum-col`), eliminând badge-urile text late în favoarea unei buline luminoase circulare (`.sensor-signal-dot`) cu tooltip de semnal dBm. Senzorul `A4:C1:38:94:0F:44` a fost actualizat din `Bathroom` în `Dormitor Matrimonial` cu funcție de redenumire directă din GUI (✏️). Verificat prin capturi CDP (`e2e_11_zones_aligned.png`, `e2e_12_sensors_aligned.png`).
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS, suita de testare `tools/test_system.py` cu 68/68 PASS (100.0%) în 24.54s, acoperire 33/33 cerințe.

- **Clarificare Terminologie Zone, Corectare Indexare ID-uri & Decuplare Telemetrie Senzori (Finalizat 14.10.2026):**
  - **Decuplare Scanare Senzori:** `ThermalZoneManager::update()` rezolvă acum pachetele de telemetrie BLE înaintea verificării `enabled`. Temperatura fizică a camerei este monitorizată permanent, eliminând falsul `TIMEOUT (Failsafe OFF)` din consolă și afișajul `--.-°C` când un circuit este oprit manual.
  - **Fix Bug Indexare UI (Off-by-One):** `renderZones` transmite acum ID-ul explicit `z.id` (1, 2, 3) către `toggleZone` și `adjTarget`, eliminând decalajul cauzat de indexul 0-based care comuta camera greșită (Zona 3 oprea Zona 2 / Camera Copilului).
  - **Redesign Terminologie Carduri:** Eliminat jargonul confuz `Releu GPIOxx: INACTIV (Închisă) Dezactivează`. Noul design prezintă starea termostatului (`🟢 Temp OK (Repaus)` / `⚪ Zonă Oprită`), starea fizică a șapei (`Robinet șapă: ÎNCHIS (Repaus / Temp atinsă)` vs `DESCHIS (Circulă apă caldă)`), și butoane clare de acțiune `[⛔ Oprește camera]` / `[▶️ Pornește camera]`.
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Firmware Version: **v1.2.0 (Build: Oct 14 2026 23:06:40)**.
  - Flash Binary Size: **1,210,741 bytes (1.21 MB, 61.6% flash)** — sub limita maximă a partiției OTA (1.25 MB).
  - Free Heap: **~120 KB** stabil.
  - Web Server & Web OTA: Active on port 80.
  - ASPICE SWE.5 Host Verification: **68/68 PASS (100.0%)** pe suita completă de teste atomice ASPICE SWE.5 cu Matrice de Trasabilitate Bidirecțională (33/33 cerințe acoperite 100.0%, 0 defecte).
  - Browser E2E Live Test Suite (Task 32): **10/10 PASS (100.0%)** pe browserul Google Chrome nativ via CDP.
  - Wall Kiosk Display: Tableta **Samsung Galaxy Tab A9 8.7" LTE** configurată cu aplicația web PWA.
- **Personalizare Completă Preseturi & Control Răzgândire Cazan (Finalizat 14.10.2026):**
  - **Personalizare Preseturi:** Deblocat salvarea și editarea preseturilor din fabrică (`default`, `eco`, `wfh`) direct în NVS fără a mai arunca eroare. Adăugat suport pentru 4 sloturi libere de utilizator (`custom1` .. `custom4`), cu `resetPreset(presetId)` pe backend și buton contextual `[↺ Reset Fabrică]` în UI.
  - **Protecție Slot `custom1` (`Bogdan`):** Suita de teste a fost mutată pe slotul de test dedicat `custom4` cu auto-reset, protejând 100% presetul utilizatorului salvat pe `custom1` (`Bogdan`).
  - **Răzgândire Cooldown & Anulare Comandă Amânată:** Rezolvat bug-ul din `web/index.html` unde `triggerBoiler` bloca comanda dacă starea era deja `ON`. Utilizatorul poate apăsa din nou pe „Pornește Centrala” pentru a anula instantaneu oprirea programată sau poate folosi noul buton `[✕ Anulează]` din bannerul `#boiler-pending-banner` (apelând `/api/boiler/cancel`).
- **Remediere Audit Chrome DevTools & Lighthouse (FSM Admin, Tesla Mode & Panou Principal) (Finalizat 14.10.2026):**
  - **FSM Admin Overlap Fix:** Decalat nodul `S3` la `y=24..60` și `S4` la `y=76..164` în SVG (16px separare curată, 0 coliziune). În CSS: `.fsm-grid` cu `gap: 1.5rem` (1.25rem mobil), `.fsm-node` cu `overflow: hidden; z-index: 1;` și `z-index: 10;` pe hover/active.
  - **Accesibilitate & Contrast WCAG AA/AAA:** Ridicat textele muted de la `#6a707a` / `#475161` la `#cbd5e1` și `#f1f5f9` (contrast > 9:1 pe `#0a0f1d`), opacitate carduri inactive crescută la 0.65.
  - **Performanță TBT (Total Blocking Time < 40ms):** Decuplat randarea schemelor complexe (`updateTeslaSchema` și `updateFsmDiagram` execută exclusiv pe tab-ul vizibil, nu la fiecare tick de fundal); boot sequence încapsulat în `requestAnimationFrame`.
  - **Toleranță Rețea & Polling Adaptiv:** Adăugat pill de sincronizare `#pill-net` (`🟢 Live` / `🟡 1.4s` / `🔴 Reconnect...`), înlocuit intervalul fix de 3.5s cu polling adaptiv (`scheduleNextPoll`) relaxat la 5.0s pe latențe mari (>1.2s) și backoff la 6.0s pe erori.
  - **Ergonomie Mobilă & Touch Targets:** Butoanele `.btn-install`, `.mode-btn`, `.btn-tesla`, `.view-tab-btn`, `.lvl-btn` ridicate la `min-height: 42px–44px; padding: 10px 14px–16px;`.
  - **Punct 1 (Contrast Cromatic):** `#64748b` înlocuit cu `#94a3b8` (>5.8:1 contrast pe `#0f172a`/`#0a0f1d`), `#0284c7` pe butoane active înlocuit cu `#0369a1` (5.93:1 contrast pe text alb, 100% WCAG AA).
  - **Punct 2 (Form Controls Labels):** Atribute `aria-label` și `title` adăugate pe switch-urile toggle `#sw-schedule-en`, `#sw-preheat-en`, `#sw-window-en`, inputurile de timp (`#mtime-*`, `#time-*`), selectoarele din modale și pe butoanele stepper de temperatură.
  - **Punct 3 (Semantic Landmark):** Containerul `.container` transformat în `<main class="container" role="main">`.
  - **Punct 4 (Ergonomie Mobilă & Touch Targets):** Adăugată regula `@media (max-width: 520px)` cu flex-wrap pe `.header-pills` și `min-height: 38px` pe `.btn-small` și `.tab-btn`.
  - **Punct 5 (Toleranță Latență Proxy):** Timeout-ul `AbortController` din `refresh()` mărit la 4000ms, cu curățare sigură în `finally` și blocare ciclu concurent `if (isRefreshing) return;`.
- **Directivă Arhitecturală Advisor (Argon Maximum) — Remediere UI Kiosk, Stateflow FSM & Schemă Termică (Finalizat 14.10.2026):**
  - **Problema 1: Panou Principal, Editare Ancore Circadiene & Naming Zona 3:**
    * Corectat bug-ul de pe `POST /api/schedule/anchor`: tratare robustă case-insensitive (`WAKE` / `wake`), parsare atât URL params cât și body JSON, conversie 12h AM/PM în 24h `HH:MM`. Modificarea ancorei active sincronizează instant setpoint-urile de zonă și forțează re-evaluarea orarului.
    * Naming corectat: Zona 3 (GPIO25) este garantată ca **"Dormitor Matrimonial"** (nu "Baie mare"), cu migrare forțată în NVS din C++.
    * Re-arhitecturare Kiosk: Eliminat dezastrul vizual (grila cu 4 carduri dense) din coloana stângă a panoului principal; mutat editorul de ancore într-o **Matrice Tabelară Compactă (4 Ancore × 3 Zone)** deschisă printr-un modal dedicat (`modal-schedule-matrix`), cu auto-save debounced (800ms) și butoane per rând cu feedback vizual verde (`✓ Salvat în NVS`).
    * Adăugat banner dinamic cu numărătoare inversă (`next-anchor-banner`) pentru următoarea ancoră circadiană.
  - **Problema 2: Firul Logic & Stările (FSM Admin) — Stateflow Ierarhic pe 2 Planuri în SVG:**
    * Înlocuit grid-ul plat 1–9 cu o diagramă orientată SVG pe 2 planuri cu săgeți vectoriale (`<path marker-end="url(#fsm-arrow)">`):
      - **Planul A: Master Plant & Hydronic FSM** (Cazan Gaz + Pompă GPA): tranziții orientate $S0 \rightarrow S1 \rightarrow S2 \rightarrow (S3 \mid S4) \rightarrow S5 \rightarrow S6 \rightarrow S1$ cu substările 4.1 (Min Burn Guard 60s), 4.2 (Steady Burn), 4.3 (Pre-Heat) și stările speciale S7 (Anti-Seize), S8 (Manual Hold).
      - **Planul B: Zone Parallel FSM**: 3 benzi orizontale paralele pentru Z1 Living (GPIO32), Z2 Copil (GPIO33), Z3 Matrimonial (GPIO25), plus casetă de interblocări locale (Geam deschis, Protecție îngheț, DTC-101 Failsafe).
    * Păstrate toate cele 9 ID-uri de noduri pentru compatibilitate 100% cu ASPICE SWE.5 și suita CDP (`fsm-node-idle`, `fsm-node-demand`, `fsm-node-wax`, `fsm-node-manual-hold`, `fsm-node-active-heat`, `fsm-node-overrun`, `fsm-node-cooldown`, `fsm-node-frost`, `fsm-node-window`).
  - **Problema 3: Dinamizarea Schemei Termice (Tesla Mode):**
    * Topologie reală cu **7 circuite de șapă** (Z1: Buclele 1, 2, 3; Z2: Buclele 6, 7; Z3: Buclele 4, 5) + ramură de **Bypass Hidraulic Permanent (Portprosop Băi)** legată la senzorul BLE `Bathroom` (22.6°C).
    * Progres dinamic dilatare ceară actuatoare (`0% -> 100% [openSec/120s]`) pe baza parametrului `openSec` din `/api/zones`, cu bare de progres dedicate și pornirea animației de curgere la >30% deschidere (`openSec >= 36`).
    * Model termodinamic dinamic Tur / Retur inerțial (`[Estimat Inerțial]`) fără temperaturi fixe hardcodate.
  - **Verificare & Calitate:** PlatformIO build curat (Flash 1.20 MB, 61.4%), 63/63 teste PASS (100.0%) pe `tools/test_system.py`, 10/10 teste PASS (100.0%) pe `tools/browser_live_test.py` în Chrome nativ.
- **Sincronizare Aprindere Cazan, 3 Niveluri de Control UI & Anti-Spam (Rezolvat 14.10.2026):**
  - **Failsafe Power-Loss Invariant:** La pornirea din priză (`begin()`), se emite imediat `sendBoilerOff(true)` burst pe 433.92 MHz și se stochează `OFF` în NVS. Acest lucru garantează oprirea fizică a flăcării după orice cădere de tensiune sau reboot și sincronizează receptorul bistabil cu ecranul.
  - **Coadă Execuție Amânată (Pending Actions):** La comutarea stării în perioada de anti-short-cycling guard (60s ardere / 60s pauză), serverul înregistrează `PENDING_OFF` sau `PENDING_ON` și returnează HTTP 429 cu `pending: true`, `target`, `waitSec`.
  - **Execuție Automată la Expirare:** În bucla `loop()`, `boilerRadio.update()` verifică disponibilitatea transmisiei și execută comanda fără intervenția utilizatorului. Apăsarea comenzii inverse anulează imediat acțiunea amânată (`pendingCancelled: true`).
  - **UI Kiosk Reacțional:** Badge pulsatoriu portocaliu (`.badge-pending`), banner informativ dedicat și contor de cooldown la secundă (`⏳ Xs rămase`).
- **Sincronizare Preseturi & UX Ancore Dinamice (Rezolvat 14.10.2026):**
  - **Sincronizare Setpoint-uri Zone cu Orarul:**
    * La aplicarea oricărui preset (`wfh`, `eco`, `default`) sau resetare, `_configs[i].targetTemp` din fiecare zonă este sincronizat automat cu țintele ancorei active din noul preset.
    * Cardurile din dreapta (`Zone Termice Pardoseală`) afișează acum `effectiveTarget` (temperatura reală comandată de Bio-Schedule), eliminând discrepanța vizuală între orar și zone.
    * Butoanele `+`/`-` din cardurile de zone propagă ajustarea instantaneu în ancora activă curentă din Bio-Schedule (`setAnchorTarget`), făcând trecerea automată pe profilul `custom` și determinând termostatul să reacționeze fără latență.
  - **Afișaj Ancore Contextuale (Home Office / WFH):**
    * Când presetul activ este `wfh`, intervalul de zi (ancora tehnică `away`, 08:30–17:00) nu mai afișează derutant `🚶 PLECAT (AWAY) - Casa este goală`, ci `💼 HOME OFFICE (ZI)` cu descrierea `Lucru de acasă & confort termic optim de birou`.
    * Pentru presetul `eco`, ancora afișează `🍃 VACANȚĂ / ECO (ZI)` cu descrierea `Temperatură economică redusă (economie gaz)`.
    * În editorul de orar din stânga jos, cardul ancorei de zi își adaptează eticheta dinamic (`💼 Home Office` / `🍃 Zi Eco` / `🚶 Plecat`).
- **Interfață Grafică PWA Kiosk & Bio-Schedule Dashboard (web/index.html):**
  - **PWA Nativ cu Suport WebAPK:** Manifest complet cu `display: standalone`, `theme_color: #080c14`, pictograme fizice rasterizate PNG (`icon-192.png`, `icon-512.png` cu masturi maskable/any) și vectoriale SVG. Service Worker non-blocant (`/sw.js`) cu precaching și cache passthrough pentru API-uri.
  - **Buton Instalare & Asistență Kiosk:** Buton tactil `📲 Instalează App` integrat cu `beforeinstallprompt` și dialog modal detaliat pentru adăugarea ca scurtătură/aplicație nativă pe tablete Android/Chrome via `chrome://flags` secure origins.
  - **Layout Kiosk Responsive pe 2 Coloane:**
    * Col 1: Cazan Gaz (indicator erou flacără ON/OFF, comutator AUTOMATIC / MANUAL, comenzi), Smart Bio-Schedule (comutatoare master, Pre-Heat anticipativ, Geam deschis), Card Ancoră Circadiană activă (`🌙 Sleep`, `🌅 Wake`, `💼 Home Office`, `🏡 Home`), Selector rapid Preset-uri chips (`standard`, `eco`, `wfh`, `custom1`, `custom2`), Modal salvare preset NVS și Editor ancore L-V vs Weekend.
    * Col 2: Card Zone Termice Pardoseală (Dormitor, Copil, Baie mare cu steppere `+`/`-`, ținte bio, stare valve NC), Card Telemetrie Senzori Xiaomi BLE cu badge-uri dBm (`EXCELENT`, `BUN`, `MARGINAL`, `SLAB`), Card Diagnoză Sistem (Heap RAM, Uptime, Wi-Fi RSSI, Flash OTA liber) și linkuri de navigare rapidă către Telnet Console și Setup Passport.
- **Smart Bio-Schedule, Fereastră Deschisă & Pre-Heat Anticipativ (Task 26 - Finalizat & Extins cu Presets):**
  - **4 Ancore Naturale de Viață Customizabile:** Modificate ancorele circadiene: `Sleep`, `Wake`, `Away`, `Home` pentru a fi complet customizabile atât la nivel de oră (`HH:MM`), cât și temperatură per zonă (`z1`, `z2`, `z3`).
  - **Catalog Preset-uri & Salvare NVS (Cerere Utilizator):**
    * Preset-uri din fabrică (read-only): `default` (Standard/Confort), `eco` (Vacanță/18.0°C), `wfh` (Work From Home / 22.5°C).
    * Sloturi salvabile în NVS: `custom1`, `custom2` (salvare orar curent cu etichetă customizabilă, stocare în namespace `"presets"`).
    * Endpoint-uri REST dedicate: `GET /api/schedule/presets`, `POST /api/schedule/preset/apply`, `POST /api/schedule/preset/save`, `POST /api/schedule/reset`, `POST /api/schedule/anchor`.
    * Comenzi consolă adăugate: `SCHEDULE RESET`, `SCHEDULE PRESET <id>`.
    * CLI extins (`esp_cli.py schedule ...`) cu timeout robust de 5.0s.
    * Suită de teste SWE.5 extinsă la 17 teste (adăugate TC16 și TC17) cu 100% PASS.
    * **Decuplare UI:** Modificările vizuale pe `web/index.html` și `web/setup.html` au fost amânate deliberat pentru următorul prompt, conform instrucțiunilor utilizatorului.
  - **Open Window Smart Pause Guard:** Istoric circular de pantă termică. Scădere >1.2°C în <5 min forțează valva în PAUZĂ timp de 30 min pentru a elimina risipa de gaz la aerisire.
  - **Predictive Pre-Heat:** Calcul dinamic al inerției șapei cu factor meteo $k_{weather}$ de la Open-Meteo, anticipând pornirea căldurii cu 30–120 minute înainte de trezire sau sosire acasă.
  - **NVS Anti-Wear Flash:** Structură binară stocată în namespace-ul `"sched"` și `"presets"` cu comparație `memcmp()` înainte de scriere.
  - **Consolă Curată & Zero Spam Meteo:** Eliminat logurile repetate de vreme la 15 min; actualizările periodice sunt redirecționate pe Serial UART.
  - **Non-blocking Socket Timeout:** Setat timeout de 1500ms pe conectarea HTTP Open-Meteo pentru a preveni orice stall în `loop()`.
- **Eliminare Word Wrap pe Mobil, Scalare Font (`A-`/`A+`) & Watchdog Feed la OTA (Task 25):**
  - **Identificare Root-Cause:** Clasa CSS `.term-container` impunea `white-space: pre-wrap; word-break: break-all;`. Pe ecrane de telefon (<450px), liniile tabelelor de diagnostic și `STATUS` (de 70-100 caractere) erau rupte arbitrar la mijlocul cuvintelor pe 2-3 rânduri, făcând citirea impracticabilă.
  - **Formatare Monospace Rigidă (No-Wrap):** Setat `white-space: pre; word-break: normal;` în `.term-container` cu `overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`. Tabelele își mențin alinierea perfectă pe coloane, permițând scroll orizontal fluid cu gesturi tactile pe telefon.
  - **Comutator Interactiv de Word Wrap (Opțional, Memorat în LocalStorage):** Adăugat checkbox în footer: `<input type="checkbox" id="wrapToggle"> Wrap`. Utilizatorul poate activa manual ruperea rândurilor (`.wrap-enabled`) dacă dorește, starea fiind salvată persistent în `localStorage` (`termWrap`).
  - **Scalare Dinamică a Fontului Terminalului (`A-` / `A+`):** Butoane tactile `A-` și `A+` în footer pentru ajustare fină (0.52rem - 1.20rem). Pe ecrane mobile (<640px), fontul implicit coboară automat la `0.70rem` pentru o densitate crescută a datelor. Dimensiunea aleasă este salvată persistent în `localStorage` (`termFontSize`).
  - **Design Responsive Mobile (`@media (max-width: 640px)`):** Compactate marginile, padding-ul, butoanele rapide (`qbtn`) și bara de input pentru a maximiza suprafața utilă pe smartphone.
  - **Watchdog Feed la Încărcare OTA (`main.cpp`):** Atașat `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` înainte de `httpUpdater.setup()`, resetând periodic Task Watchdog Timer-ul pe fiecare pachet HTTP de firmware scris în flash și eliminând complet riscul de watchdog reboot la actualizări Web OTA.
- **Remediere Scurgere Memorie Heap (`DTC-402`), NimBLE `setMaxResults(0)` & Auto-Clear Histerezis (Task 24):**
  - **Identificare Root-Cause:** La 4h de uptime, heap-ul liber scăzuse de la 127 KB la 51 KB declanșând `DTC-402`. `NimBLEScan` acumula pe heap (`m_maxResults = 0xFF`) toate pachetele BLE de la telefoane/ceasuri cu adrese MAC rotative (MAC randomization) fără a le elibera, scanarea fiind continuă.
  - **Rezolvare NimBLE:** Apelat `_pBLEScan->setMaxResults(0)` și `_pBLEScan->clearResults()`. NimBLE execută callback-ul `onResult` și eliberează instantaneu dispozitivele (`pScan->erase()`), stabilizând memoria ca fiind 100% constantă în timp (zero leak).
  - **Auto-Clear Histerezis:** `DTC-402` se stinge automat când heap-ul depășește 75 KB; `DTC-401` se stinge când RSSI Wi-Fi depășește -78 dBm. Scorul de sănătate a revenit la 100% [EXCELLENT], Free Heap stabil la ~125 KB.
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
3. Production firmware v1.2.0 is live and fully synchronized on `origin/main` (commit `66fbbae`).

