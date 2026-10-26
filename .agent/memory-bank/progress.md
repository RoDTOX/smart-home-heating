# Progress: Smart Home Heating System

## Status
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, 2-way console, web OTA, diagnostic engine with DTCs, service mode, digital passport, NTP & weather sync, headless API-first architecture, hydronic safety controls, anti-spam burner interlocks, Bio-Schedule with presets engine & full factory/user customization, deferred action cooldown cancel, atomic PC system test suite, Home Assistant full integration, FSM race condition fix, Open Window Detection suite, new Configuration Panel with live relay testing, and Wi-Fi auto-recovery watchdog) are fully implemented, compiled, flashed over Web OTA, and verified live on ESP32 (`192.168.1.20`).
ASPICE SWE.5 test coverage: 49/49 Requirements (100.0%), 92/92 Tests Passed (100.0%), 0 uncovered requirements, 0 relaxed contracts.

## Completed Work (2026-10-21 - Remediere 6 Probleme Majore, Overhaul UI/UX FSM, Config Panel, Wi-Fi Watchdog & 92/92 Teste ASPICE SWE.5)
- **100% Acoperire Cerințe ASPICE SWE.5:**
  - 49/49 Cerințe verificate și documentate (FR-1..FR-45 + NFR-1.1, NFR-1.5, NFR-2.2, NFR-3.2).
  - 92/92 Teste Atomice Live pe Hardware Real (`192.168.1.20`): 100.0% PASS.
- **Remediere 6 Probleme Raportate de Utilizator:**
  - 1. Persistență Preset & Setări NVS la Reboot: Salvare automată ancore direct în cheia de preset NVS `${presetId}_cfg`.
  - 2. Orar Inteligent 100% Customizabil: `BioAnchorConfig.name[32]`, `daysMask` (L-D) per preset cu auto-switching determinist.
  - 3. Compactare UI: Eliminat selectorul de nivel redundant și butoanele legacy; switch unificat Simplu vs Inteligent.
  - 4. Grafice Sparkline Reale: Consumă direct `/api/history?range=24h`; rezolvat overflow slot 256 (>21:20) cu `uint16_t`.
  - 5. Reordonare Ergonomică Rânduri: `[Zonă]` -> `[Temp Citită]` -> `[Stepper Țintă]` -> `[Sparkline 24h]`.
  - 6. Decuplare Pompă GPA: Releul 4 eliberat ca spare contact (`pumpControlEnabled: false`).
- **Overhaul Brand & FSM:**
  - Eliminat 100% termenul "Tesla"; tab redenumit în `Schemă Hidraulică`.
  - Eliminat starea tranzitorie S0 BOOT/Warmup BLE; restructurat diagrama SVG și cardurile în Ciclul Circular Principal vs Gărzi Asincrone.
- **Panou de Configurare Sistem (`⚙️ Configurare`):**
  - Testare live relee cu whitelist hardware pini (32, 33, 25, 26).
  - Configurare timpi hidraulici (amorsare, delay centrală, bypass open loop, post-circulație pompă).
  - Configurare siguranță RF (anti-short-cycling, cooldown, heartbeat de siguranță).
  - Praguri și comutator gardă geam deschis.
- **Consolă Live & Wi-Fi Watchdog:**
  - Execuție comenzi prin API și reset memorie circulară hardware prin `CLEAR`.
  - Supervizor Wi-Fi non-blocking în `loop()` cu DTC-401 și radio hard reset la 90s.
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, 2-way console, web OTA, diagnostic engine with DTCs, service mode, digital passport, NTP & weather sync, headless API-first architecture, hydronic safety controls, anti-spam burner interlocks, Bio-Schedule with presets engine & full factory/user customization, deferred action cooldown cancel, atomic PC system test suite, Home Assistant full integration, FSM race condition fix, and Open Window Detection suite with per-zone configurable sensitivity and manual override) are fully implemented, compiled, flashed over Web OTA, and verified live on ESP32 (`192.168.1.20`).
ASPICE SWE.5 test coverage: 44/44 Requirements (100.0%), 85/85 Tests Passed (100.0%), 0 uncovered requirements, 0 relaxed contracts.

## Completed Work (2026-10-18 - Aliniere Riguroasă 1:1 Documentație & Suită Teste ASPICE SWE.5 44/44 Cerințe Live)
- **100% Acoperire Cerințe ASPICE SWE.5:**
  - 44/44 Cerințe verificate și documentate (FR-1..FR-40 + NFR-1.1, NFR-1.5, NFR-2.2, NFR-3.2).
  - 0 cerințe netestate, 0 decalaje (gaps) între cod și documentație.
- **Suită Automată Extinsă la 85 Teste Atomice (`tools/test_system.py`):**
  - Adăugat verificări automate pentru FR-33..FR-40: persistență LittleFS (/api/history), model termic gaz (baseload ACM 0.40 m³/zi, 0.60 m³/h flacără, 3.30 Lei/m³), validare sintaxă modernă Home Assistant Core (`template:` packages), abortare instantanee a perioadei de warmup la scăderea setpoint-ului, post-circulație pompă condiționată de aprinderea flăcării, anulare hold circadian la reglaj manual și suita completă de ferestre deschise.
  - Verificare live pe hardware real (`192.168.1.20`): **85/85 PASS (100.0%)** în 30.73 secunde.
- **Hardening Firmware & Corectare Limite Fereastră:**
  - Identificat și corectat în C++ intervalul pragului de detecție cădere: forțat `[0.8°C .. 2.0°C]` (default 1.2°C) în `ThermalZoneManager.cpp` și refuz cu HTTP 400 Bad Request în `main.cpp` pentru valori eronate.
- **Actualizare Completă Documentație de Sistem:**
  - `docs/requirements_and_features.md`: cerințe detaliate FR-34..FR-40 și matrice de trasabilitate 44/44 100% PASS.
  - `docs/api_reference.md`: documentate toate endpoint-urile noi (`/api/history`, `/api/boiler/cancel`, `/api/boiler/reset-runtime`, `/api/zones/hold/clear`, `/api/zones/window/clear`, `/api/zones/window/threshold`).
  - `docs/underfloor_heating_hydraulics_and_failsafes.md`: adăugate secțiunile 3.1 (Warmup Abort), 3.2 (Intelligent Overrun), 3.3 (Open Window Cutoff).
  - `docs/system_test_report.md`: generat raport formal ASPICE SWE.5.

## Completed Work (2026-10-18 - Suită Completă Detecție Geam Deschis, Salvare Folder Home Assistant în Git & Flashing OTA Live)
- **Persistență Git Folder `homeassistant/`:**
  - Migrat și structurat complet folderul `homeassistant/` în rădăcina proiectului Git (`packages/smart_heating.yaml`, `dashboards/smart_heating.yaml`, `configuration.yaml`, `docker-compose.yml`, `README.md`).
  - Containerul Home Assistant poate fi pornit în 1 comandă (`docker compose up -d`) pe orice mașină, independent de existența sau resetarea VM-ului WSL2.
- **Suită Completă Detecție Geam Deschis (Open Window Guard):**
  - FSM & Siguranță: la scădere de $\ge \text{prag}$ în $\le 5$ minute, zona trece în `ZONE_WINDOW_PAUSE`, taie instant releul la 0V și oprește cazanul dacă nu mai există cerere în casă.
  - Ieșiri din pauză: auto-timeout (30m), auto-rebound termic (+0.6°C), și override manual din Web UI sau Home Assistant (`[↺ Am Închis (Reia)]`) care resetează și baseline-ul la temperatura curentă.
  - Prag de sensibilitate configurabil per zonă (0.8°C – 2.0°C, implicit 1.2°C) salvat în NVS prin endpoint REST `POST /api/zones/window/threshold` și badge UI.
  - Istoric și telemetrie: înregistrează timestamp-ul și durata ultimei aerisiri (`lastDetectionEpoch`, `lastDetectionDurationSec`).
- **Vizibilitate Web UI & Home Assistant:**
  - Web UI: banner pulsant animat pe cameră când este aerisire activă cu cronometru rămas (`MM:SS`) și buton `[↺ Am Închis (Reia)]`; etichetă istoric discretă când este închis (`🪟 Aerisit: acum Xm`); schemă SVG actualizată.
  - Home Assistant: 3 `binary_sensor` cu `device_class: window`, 3 senzori de countdown, 3 comenzi REST și template buttons, plus carduri de alertă condiționale pe dashboard-ul principal.
- **Flashing OTA & Verificare Live pe Hardware (ESP32 `192.168.1.20`):**
  - Compilare PlatformIO curată: `pio run` SUCCESS (RAM: 21.6%, Flash: 64.7%).
  - Flashed via Web OTA (`tools/ota_upload.py`) în 7.92s.
  - Verificat live prin API: releul de pe Camera Copilului a fost decuplat (`relayEngaged: false`), toate câmpurile noi de telemetrie sunt active și testate.

## Completed Work (2026-10-16 - Refactorizare Densitate Înaltă UI, Format Widescreen 16:9, De-Cramping Zone Termice & Consolă Wi-Fi Live pe Localhost)
- **Format Widescreen 16:9 & Aliniere Laterală (`web/index.html`):**
  - Înlocuit constrângerea îngustă `max-width: 1060px` cu un container expandabil până la `1560px` cu centrare automată (`margin: 0 auto; padding: 12px 16px 24px;`). Pe monitoare 16:9 conținutul acoperă armonios spațiul lateral fără margini negre masive.
  - Breakpoint decuplat la 1140px: pe ecrane sub 1140px, elementele sunt aranjate într-o singură coloană verticală intuitivă ca pe smartphone prin `display: contents;` (Bara sus -> Centrală -> Zone Termice & Senzori -> Bio-Schedule).
- **De-Cramping Zone Termice & Senzori BLE:**
  - Eliminat complet textul static `NimBLE 90% Scan`.
  - Cu o lățime de coloană de ~800px pe widescreen și dispunere verticală pe ecrane mai mici, liniile de zone (nume, status vană, buton pornire/oprire, temperatură live, grafic sparkline și butoane stepper) respiră aerisit, fără nicio suprapunere de text.
- **Compactare Spațiu Vertical (High-Density UI):**
  - Bara superioară unificată (`.top-nav-card`): Comasate cele 3 carduri separate (Antet, Nivel de Control, Mod Vizualizare) într-un singur card subțire, economisind ~120px de spațiu vertical inutil.
  - Card Centrală Gaz compactat: Titlul, starea flăcării și butoanele de mod (`AUTOMATIC` / `MANUAL`) sunt aliniate compact pe un singur rând; butoane de acțiune slim (36px).
  - Card Smart Bio-Schedule compactat: Comutatoarele (`Orar Inteligent`, `Pre-Heat`, `Geam Deschis`) sunt grupate pe un singur rând; cardul erou al ancorei active a fost redus de la 110px la ~40px înălțime.
- **Activare Consolă Bidirecțională Live pe Localhost & Tab Integrat (`tools/dev_server.py`, `web/index.html`, `web/console.html`):**
  - Serverul `dev_server.py` pe portul 8080 acționează ca reverse proxy transparent pentru toate cererile `/api/*` către ESP32 (`192.168.1.20`), adăugând rutare pentru `/console` și `/index.html` cu suport CORS complet.
  - Adăugat al 4-lea mod de vizualizare `>_ Consolă Live` (`#card-console-view`) direct în interfața principală: streaming live de loguri cu colorizare sintactică, butoane rapide (`STATUS`, `ON`, `OFF`, `AUTO`, `MANUAL`, `DIAG`, `HELP`, `CLEAR`) și câmp de comenzi pentru trimitere directă prin `POST /api/console/cmd`.
  - Rezolvare URL relativ universal pe localhost (`base = '/'`).
- **Validare ASPICE SWE.5 & Calitate:**
  - `tools/build_web.py` a compilat `index.html` la 38.5 KB (25.0% gzip).
  - Compilare PlatformIO curată (RAM 19.9%, Flash 62.4%, 100% SUCCESS).
  - Suita de teste automată `tools/test_system.py` cu 68/68 PASS (100.0%) în 23.88s pe toate cele 33 de cerințe.
  - Verificare vizuală completă prin subagent browser pe `http://localhost:8080/index.html` cu capturi de ecran salvate.

## Completed Work (2026-10-16 - Fix Frontend SyntaxError, Protecție Anti-Underflow Timp Funcționare Cazan & Comandă Reset Runtime)
- **Rezolvare Frontend Local (`web/index.html`):**
  - Eliminat fragmentul duplicat neterminat `renderZones()` (48 de linii) lăsat deasupra funcției principale, care bloca executarea scripturilor JS prin `SyntaxError: Unexpected end of input` la linia 4105. Validat cu `node --check` (0 erori) și reîncărcare în browser (toate cardurile și datele live se afișează corect).
- **Protecție Anti-Underflow Matematică & Comandă Reset Timp Funcționare Cazan:**
  - Identificat și eliminat bug-ul de sub-depășire (underflow) pe tipul `uint32_t` la calculul `(millis() - _flameStartTimeMs)` când apelurile forțate de oprire se suprapuneau peste interblocări, ce producea valoarea reziduală de $4.295.027\text{ secunde} \approx 1193\text{ ore}$ salvată în NVS.
  - Adăugat verificări stricte `if (now >= _flameStartTimeMs)` și limită de siguranță `burnSec < 86400UL` în `update()`, `sendBoilerOff()` și `getCurrentFlameDurationSec()`.
  - Implementat metoda `boilerRadio.resetRuntime()` pentru resetarea tuturor contoarelor de flacără din NVS la 0.
  - Expus comanda de Network Console `BOILER RESET RUNTIME` (și `RESET RUNTIME`).
  - Expus endpoint-ul REST `POST /api/boiler/runtime/reset`.
- **Verificare & Validare Live pe Hardware (ESP32 `192.168.1.20`):**
  - Compilare PlatformIO curată (RAM 19.9%, Flash 62.2%, 100% SUCCESS).
  - Flash OTA pe ESP32 în 11.6s.
  - Resetat contorul din NVS prin API la `totalSec: 0`, `todaySec: 0`.
  - Suita automată ASPICE SWE.5 (`tools/test_system.py`) a trecut cu **100% PASS (68/68, 33/33 cerințe)**.

## Completed Work (2026-10-15 - Fix Spam Buclă Oprire Cazan, Unificare Carduri Zone/Senzori, Notificări Teste & Fix Consolă CLEAR)
- **Notificare Start & Finalizare Suită de Teste pe Consola Virtuală ESP32:**
  - În `tools/test_system.py`, s-a adăugat `notify_esp_console()` care transmite automat bannere de informare pe `/api/console/send`: `[TEST-SUITE] ⚠️ ASPICE SWE.5 AUTOMATED TEST SUITE STARTED` la start și `[TEST-SUITE] 🏁 ASPICE SWE.5 AUTOMATED TEST SUITE FINALIZAT (68/68 PASS)` la final, asigurând operatorul că modificările de parametri din loguri sunt emise de testele automate și că starea a fost restaurată la normal.
  - În `web/console.html`, eticheta `[TEST-SUITE]` beneficiază de stil dedicat `.t-tst` (badge galben/chihlimbar luminos).
- **Remediere Comandă CLEAR / CLS pe Consola Virtuală:**
  - În `src/NetworkConsole.cpp`, metoda `clear()` resetează acum complet `_totalBytes = 0`, `_head = 0`, `_count = 0` și golește ring buffer-ul cu `memset`. Comanda `CLEAR` / `CLS` este interceptată în `executeCommand()` pentru a nu mai re-inunda bufferul cu delimitatori de sfârșit.
  - În `web/console.html`, tastarea `CLEAR` sau acționarea butonului `CLEAR` curăță instantaneu DOM-ul (`term.innerHTML = ''`), resetează `lastSeq = 0` și instruiește serverul să șteargă bufferul circular.
- **Eliminare Spam Buclă Oprire Cazan (Fix DTC-201):**
  - În `ThermalZoneManager::update()`, s-a adăugat verificarea `boiler.canTransmit(false)`. Dacă garda anti-short-cycling (60s) este activă, se programează `PENDING_OFF` o singură dată în coada RF cu log-ul `[Thermal] BOILER STOP queued (Anti-short-cycling guard active)...`. În secundele următoare nu se mai apelează `sendBoilerOff()`, prevenind declanșarea erorii `DTC-201` și eliminând complet inundarea consolei. La expirarea gărzii, `BoilerRadioTrigger::update()` execută automat și silențios pachetul RF de oprire.
- **Unificare Coloana 2 & Eliminare Card Duplicat:**
  - Eliminat cardul redundant de sus `Zone Termice (Pardoseală)`. Integrat totul într-un singur card compact `#card-zones-sensors`.
  - Temperatură reală evidențiată cu font bold și grafic sparkline vectorial.
  - Setpoint subordonat (slave) cu steppere slim compacte și etichetă discretă `țintă`.
  - Mini-badge de status hidraulic cu tooltip informativ complet la hover.
  - Buton discret de oprire/pornire cameră (`⏻`) cu tooltip dedicat.
- **Editor Matriceal Circadian Integrat Direct (Fără Popup Modal, 100% Separat L-V vs S-D):**
  - Tabel matricial 4 ancore &times; 3 zone integrat direct în `#box-advanced-editor`.
  - Tab-urile `[L–V (Săptămână)]` și `[S–D (Weekend)]` permit setarea de temperaturi complet independente (ex. Acasă Z3 la 22.5°C pe L–V vs 23.0°C pe S–D), cu buton util `📋 Copiază L-V → S-D` doar la cerere.
  - Auto-salvare non-blocantă cu debounce de 700ms direct în memoria Flash NVS a controllerului.
- **Validare ASPICE SWE.5 & Calitate:**
  - Compilare PlatformIO curată (RAM 19.9%, Flash 62.2%, 100% SUCCESS).
  - Suita de teste automată `tools/test_system.py` cu 68/68 PASS (100.0%) în 24.38s pe toate cele 33 de cerințe.

## Completed Work (2026-10-15 - Sincronizare Live Orar Circadian, Preseturi Calibrate din Fabrică & Tracking Runtime Cazan)
- **Sincronizare Automată în Timp Real Orar Circadian (Zero Refresh Manual):**
  - Cardul erou circadian (`renderActiveAnchorHero`) și bannerul numărătorii inverse (`updateNextAnchorCountdown`) se actualizează acum instantaneu la fiecare 3.5s din `/api/status` fără a mai necesita reîncărcarea paginii cu F5.
- **Calibrare Preseturi din Fabrică (L-V & S-D):**
  - Valorile implicite din `ThermalZoneManager.cpp` au fost aliniate 100% cu orele și temperaturile reale din cele 4 capturi ale utilizatorului (`Standard / Confort` și `Home Office`).
- **Redesign Panou Preseturi & Editor Matriceal:**
  - Ascunse sloturile libere nepopulate. Adăugată bară dedicată de acțiuni: `➕ Adaugă Slot`, `💾 Salvează`, `↺ Anulează (Discard)`, `✏️ Redenumește`, `🗑️ Șterge`.
  - Butonul de matrice a fost reproiectat într-un stil compact și discret, iar modalul matriceal conține coloane paralele `Oră L–V` și `Oră S–D` cu buton de clonare rapidă `📋 Copiază L-V în S-D`.
- **Jurnalizare & Insight-uri Termice în Consolă:**
  - Formatat log-ul de pornire cazan cu 2 zecimale exacte (`current 22.21 C <= start threshold 22.20 C`) eliminând confuzia de rotunjire `22.2 <= 22.2`.
  - Adăugat log automat în consolă la detecția unei răciri bruște: `[Thermal][INSIGHT] Rapid cooling detected on Zone X: ... possible aeration`.
- **Tracking Timp Funcționare Cazan (Runtime) & Estimare Factură Gaz:**
  - Contor hardware de flacără în `BoilerRadioTrigger` salvat în NVS cu wear-leveling (astăzi, ieri, total). Card dedicat în UI cu estimare consum gaz bazată pe factura reală de iarnă (dialog `⚙️ Factură / Cost`).
- **Validare ASPICE SWE.5 & Calitate:**
  - Compilare PlatformIO curată (RAM 19.9%, Flash 62.1%, 100% SUCCESS).
  - Suita de teste automată `tools/test_system.py` cu 68/68 PASS (100.0%) în 23.82s pe toate cerințele.

## Completed Work (2026-10-15 - Igienizare Memorie Heap C++, Curățare Frontend & Decuplare Dev Server)
- **Igienizare Memorie Heap C++ (Zero Fragmentare pe ESP32):**
  - Implementat pre-alocare de buffer `json.reserve(...)` / `out.reserve(...)` în toate serializatoarele mari REST API din `main.cpp`, `ThermalZoneManager.cpp`, `DiagnosticEngine.cpp`, `TimeAndWeatherService.cpp`, `DevicePassport.cpp`.
  - Eliminat zecile de micro-realocări heap consecutive la fiecare interogare la 3.5s, menținând Free Heap stabil la >115 KB fără fragmentare RAM pe termen lung.
- **Curățare Frontend & Optimizare CSS:**
  - Șters blocul duplicat `@media (max-width: 520px)` din `web/index.html`.
  - Validat compresia automată `tools/build_web.py` (119 KB -> 29.1 KB).
- **Server Local de Dezvoltare Decuplat (`tools/dev_server.py`):**
  - Configurat server local Python pe portul 8080 (`http://localhost:8080/index.html`) conectat prin CORS la ESP32 (`192.168.1.20`) pentru iterații instantanee ale interfeței fără flash pe placă.
- **Validare ASPICE SWE.5 & Calitate:**
  - Compilare PlatformIO curată (RAM 19.9%, Flash 61.6%, 100% SUCCESS).
  - Suita de teste automată `tools/test_system.py` trecută cu 68/68 PASS (100.0%) în 24.54s pe toate cele 33 de cerințe.

## Completed Work (2026-10-14 - Personalizare Preseturi Fabrică & Sloturi Utilizator, Anulare Cooldown Cazan)
- **Personalizare Preseturi Predefinite & Extindere Sloturi:**
  - Deblocată posibilitatea de a salva modificări peste preseturile din fabrică (`default`, `eco`, `wfh`) direct în NVS fără erori. La aplicare, se verifică prioritar configurația salvată în NVS, revenind transparent la valorile din fabrică dacă nu există personalizări.
  - Adăugată metoda `resetPreset(presetId)` și ruta `POST /api/schedule/preset/reset?id=<id>` pentru revenirea oricărui preset la valorile din fabrică.
  - Numărul de sloturi libere de utilizator extins la 4 (`custom1` .. `custom4`).
  - Suita de teste ASPICE SWE.5 redirecționată pe slotul dedicat `custom4` cu auto-reset, protejând 100% presetul `custom1` (`Bogdan`).
- **Anulare Cooldown / Comandă Amânată Cazan (Re-armare):**
  - Eliminat bug-ul din `web/index.html` unde starea `currentBoilerState === 'ON'` bloca clicurile utilizatorului pe „Pornește Centrala” dacă era programată o oprire amânată (`boilerPendingAction === 'OFF'`).
  - Adăugat buton dedicat `[✕ Anulează]` în bannerul `#boiler-pending-banner` apelând `/api/boiler/cancel`.
  - Cazanul poate fi re-armat instantaneu la răzgândire, iar starea revine la ardere activă fără oprire forțată.
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
  - Implementat [`tools/test_system.py`](file:///d:/_git/smart-home-heating/tools/test_system.py) și integrat în CLI (`python tools/esp_cli.py test`).
  - 12 cazuri de testare end-to-end acoperind toate cerințele din PRD (`FR-1`, `FR-4`, `FR-10`, `FR-11`, `FR-13`, `FR-14`, `FR-15`, `FR-17`, `NFR-1.1`).
  - Validat live pe hardware cu rată de succes de **100.0% (12/12 PASS)**.
  - Generat raport oficial de testare în [`docs/system_test_report.md`](file:///d:/_git/smart-home-heating/docs/system_test_report.md).
- **Headless API-First Architecture & UI Latency Elimination:**
  - Decoupled backend REST endpoints from front-end volatility.
  - Eliminated UI lag: converted all buttons to 0ms optimistic DOM updates and asynchronous background POSTs (removed `location.reload()`).
  - Smart location selection without GPS coordinate requirements (Geocoding search + 41 Romanian county capitals + IP/GPS auto-detection).
  - Zone index normalization (supports 1-based and 0-based indices).
  - Comprehensive CLI utility [`tools/esp_cli.py`](file:///d:/_git/smart-home-heating/tools/esp_cli.py).
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
