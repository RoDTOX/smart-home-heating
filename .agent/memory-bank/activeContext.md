# Active Context: Smart Home Heating System

- **Release v1.11.0: Lovelace Neo-Bento UI/UX, Decupling Aerisire Geam 2R2C, Consolă Anti-Spam (Finalizat 29.10.2026):**
  - **1. Firmware v1.10.29.1 Live pe ESP32 (192.168.1.20):** Flashuit OTA cu succes; memorie RAM liberă ~93 KB (stabilă cu NimBLE și Wi-Fi), flash 63.3% (1,244,721 bytes) cu 65.9 KB marjă liberă sub slotul OTA de 1.25 MB.
  - **2. Decuplare Aerisire Geam & Calibrare 2R2C:** Buffer circular extins la 4 eșantioane (15 min) fără clamps rigide (suportă praguri 0.2°C .. 3.0°C). Calibrarea nocturnă ($k_{\text{loss}}$) și pornirea predictivă sunt strict izolate la aerisire pentru a preveni decalibrarea modelului clădirii.
  - **3. Consolă Anti-Spam & Buffer RAM 8 KB:** Filtru inteligent de deduplicare mesaje identice consecutive (`[Last message repeated N times]`), reducând spam-ul de burst-uri RF 433 MHz cu peste 90% și oferind istoric de 3–5 zile fără uzură flash.
  - **4. Lovelace Neo-Bento Dashboard (`web/index.html`):** Arhitectură offline 100% (zero dependențe externe CDN), stil Simple Thermostat + Neo-Bento 2026 (steppere circulare tactile `(-) 24.5° (+)`, culori pastel lime/lilac, sheet modal detaliat cameră, tab-uri configurare 1–12 zone și debug). Servit direct din LittleFS (8.4 KB gzip).
  - **5. Validare 100% ASPICE SWE.5 & Teste Automate:**
    * `test_system.py`: **95/95 PASS (100.0%)** cu acoperire 49/49 cerințe.
    * `test_window_detection.py`: **7/7 PASS (100.0%)**.
    * `test_thermodynamics.py`: **7/7 PASS (100.0%)**.
    * `test_thermodynamics_adversarial.py`: **8/8 PASS (100.0%)**.

- **ASPICE 4.0 Capability Level 1 (CL1) Restructuring & Baseline BL_2026_v1.10.28.1 (Finalizat 28.10.2026):**
  - **1. Firmware v1.10.28.1 Live pe ESP32 (192.168.1.20):** Recompilat pe espressif32@6.5.0 cu binar compact de 1.23 MB (1,295,040 bytes), flashuit cu succes OTA cu 15.68 KB headroom sub slotul fizic de 1.25 MB. Toate cele 3 zone functionale pe noul motor termodinamic.
  - **2. Motor Termodinamic Predictiv 2R2C:** Implementat ThermodynamicEngine.h/.cpp integrat in ThermalZoneManager. Calibrare pasiva nocturna (k_loss), test activ step-response in mod Plecat, pornire predictiva cu eroare de sosire de doar 0.042 C (< 0.3 C cerinta).
  - **3. Detectie Rapida Geam Deschis pe Baza Derivatelor:** Buffer circular 12 esantioane (5 min), calcul derivate centrale dT/dt si d2T/dt2, declansare la 2.5x Rate_natural (<= 10 min la 15 C, <= 5 min la -5 C, 0 alarme false pe 6h). Auto-recuperare pe revenire derivata.
  - **4. Wall-Tablet UX pe LittleFS:** web/index.html servit direct din LittleFS (53 KB gzip), layout dedicat 1280x800 fara scroll vertical, modal tactil configurare sapa/pardoseala, badge-uri animate geam deschis cu dismiss dintr-o atingere.
  - **5. Specificatie Hardware Turnkey 8-Zone & BOM:** PCBA dedicata ESP32-S3 cu 8 SSR-uri zero-cross silentioase 0dB, OpenTherm Master izolat galvanic (EN 50559), antena externa SMA bulkhead (+32 dB boost). Cost unitar .80 USD (~349 RON) la MOQ 1000 (<  USD cerinta).
  - **6. Verificare & Teste Automate:** 7/7 Nominal Tests PASS (72.8ms) + 8/8 Adversarial Tests PASS (270ms) + 95/95 HIL Qualification Tests PASS (21.4ms) + compilare curata PlatformIO 0 erori.
  - **7. Restructurare Documentatie ASPICE 4.0:** Directoare canonice docs/sys/ (SYS.2-SYS.5), docs/swe/ (SWE.1-SWE.5), docs/sup/ (SUP.1, SUP.8), docs/biz/ (BIZ.1) si router PROJECT.md.
  - **8. Audit Independent de Calitate:** Raport emis de Lead Assessor cu verdict formal APPROVED (UNCONDITIONAL).

- **Wall-Tablet UX, LittleFS Direct Web Serving, Telemetry Events & Versioning v1.MM.DD.rev (Finalizat 27.10.2026):**
  - **1. Ergonomie Wall-Tablet UX & Orar 24h:** Carduri cameră cu cadran tactil circular (`room-dial`), afișare mărită a temperaturii citite (2.4rem), ținte tactile 48px, linie explicativă „de ce” (`room-reason`) și acțiuni rapide (Plec de acasă, Boost, Vacanță, Revin la orar).
  - **2. Timeline Interactiv 24h & Marker Real-Time:** Drag handles cu pas de 15 min; indicatorul de timp real `.tl-now` a fost îmbogățit cu insignă superioară `[ACUM] ▼`, eliminând confuzia cu un defect grafic la ora 10:50.
  - **3. Compactare Schemă Hidraulică (Bare Verticale Actuatoare):** Înlocuit barele orizontale separate de ceară cu bare verticale de progres integrate direct în actuatoare (`ts-act-fill`), reducând înălțimea SVG-ului de la 510px la 455px și economisind spațiu pe tableta de perete.
  - **4. Rezolvare Bug Plec de acasă / Am revenit:** Fallback determinist pe preset-ul activ al zilei (`wfh` sau `default`) în `ThermalZoneManager::endAway()` și `setAway()`, eliminând blocarea pe `eco`; `resumeSchedule()` anulează acum și starea globală Away. Verificat live pe placă: Plec de acasă -> Eco, Am revenit -> restaurat WFH.
  - **5. Noua Schemă de Versionare:** Trecut la formatul `v1.<lună>.<zi>.<rev_zi>` (ex: `v1.10.27.4`) prin `tools/version_helper.py`, sincronizat pe firmware (`include/git_version.h`) și pagini web (`window.UI_VERSION`). Placa rulează live `v1.10.27.4`.
  - **6. LittleFS Web Serving & Instant UI Uploader:** Pagini web servite direct din LittleFS cu fallback pe PROGMEM; utilitar autonom `tools/upload_ui.py` pentru actualizarea interfeței în <1s fără recompilare firmware, fără flash OTA și fără restart.
  - **7. Optimizare Flash Firmware:** Redus firmware-ul sub limita slotului OTA (1.28 MB vs 1.31 MB slot fizic) prin eliminare CSS mort (-498 linii), compresie PWA și flag `-D CORE_DEBUG_LEVEL=0`.
  - **8. Telemetrie pe Evenimente:** Structură `StateEvent` (8 bytes) cu buffer circular de 2.048 intrări în LittleFS (`/events.bin`) și endpoint `GET /api/history/events?limit=N` pentru urmărirea exactă la secundă a pornirilor/opririlor cazanului și pompei.
  - **9. Diagnoză Hardware & Consolă:** Expus în `/api/boiler/status` și panou `minFreeHeap`, `cpuFreqMHz`, `fsTotalBytes`, `fsUsedBytes`; adăugat comanda de consolă `SYS` / `MEM`.

- **Overhaul Aplicație & Remediere 6 Defecte Majore, Panou Configurare & ASPICE SWE.5 (Finalizat 21.10.2026):**
  - **1. PersistenÈ›Äƒ Preset & SetÄƒri NVS la Reboot:** ModificÄƒrile ancorelor se persistÄƒ automat Ã®n slotul presetului curent (`${presetId}_cfg`), garantÃ¢nd restaurarea exactÄƒ dupÄƒ panÄƒ de curent.
  - **2. Orar Inteligent 100% Customizabil & Nume Flexibile:** Buffer ancore lÄƒrgit la `char name[32]` (fÄƒrÄƒ trunchieri); adÄƒugat `daysMask` (L-D) per preset cu auto-switching determinist Ã®n funcÈ›ie de ziua sÄƒptÄƒmÃ¢nii NTP; sincronizare automatÄƒ temperaturÄƒ È›intÄƒ la prima conectare NTP.
  - **3. Compactare UI & Eliminare Moduri Redundante:** Eliminat selectorul de nivel ("NIVEL: Simplu / Preseturi / Avansat"), butoanele "App" È™i "Prototip V2"; comutatorul "Orar Inteligent" a devenit selectorul unificat Simplu vs Inteligent; buton dedicat "âš™ï¸ ConfigureazÄƒ orar preset" pentru matricea inline.
  - **4. Grafice Sparkline Reale din RAM/LittleFS:** Eliminat `localStorage` din browser; sparklines consumÄƒ direct `/api/history?range=24h`; remediat bug-ul de overflow slot 256 (>21:20) prin lÄƒrgirea la `uint16_t` Ã®n `TelemetryHistoryManager`.
  - **5. Reordonare ErgonomicÄƒ RÃ¢nduri Camere:** Flux optimizat: `[ZonÄƒ]` -> `[TemperaturÄƒ CititÄƒ]` -> `[Reglaj TemperaturÄƒ ÈšintÄƒ Stepper]` -> `[Sparkline 24h]`.
  - **6. Decuplare PompÄƒ GPA & Releu Liber:** Dat fiind cÄƒ distribuitorul nu are pompÄƒ secundarÄƒ GPA, Releul 4 (`GPIO26`) a fost decuplat È™i setat ca contact liber (`pumpControlEnabled: false`).
  - **CurÄƒÈ›are Brand:** Eliminat complet "Tesla" din tot codul HTML/CSS/JS/endpoints; tab redenumit Ã®n `SchemÄƒ HidraulicÄƒ`.
  - **Overhaul FSM:** Eliminat S0 (BOOT/Warmup BLE), eliminat Planul B redundant, redimensionat SVG (`0 0 940 235`), separat Ã®n Ciclul Circular Principal vs GÄƒrzi Asincrone de SiguranÈ›Äƒ.
  - **Panou Nou Configurare (`âš™ï¸ Configurare`):** Testare live relee cu whitelist GPIO (32, 33, 25, 26), timpi amorsare actuatori (default 3m), delay aprindere centralÄƒ (default 3m), comutator bypass open loop (aprindere instant fÄƒrÄƒ delay), post-circulaÈ›ie pompÄƒ (default 3m), anti-short-cycling (default 180s), RF cooldown (3s), RF safety heartbeat (15m + toggle), senzori tur/retur, comutator È™i prag gardÄƒ geam deschis.
  - **ConsolÄƒ Live & Memory Clear:** Comenzile interactive funcÈ›ionale prin `/api/console/send?cmd=`; comanda `CLEAR` È™terge memoria circularÄƒ hardware din ESP32.
  - **Wi-Fi Watchdog:** Supervizor non-blocking Ã®n `loop()`: la pierderea linkului ridicÄƒ `DTC-401`, reÃ®ncearcÄƒ la 10s È™i hard-reseteazÄƒ radio Wi-Fi dupÄƒ 90s.
  - **PlatformIO & Flash Safety:** Versiune fixatÄƒ la `espressif32@6.5.0` (binar 1.22 MB, Ã®ncadrat perfect Ã®n partiÈ›ia OTA de 1.25 MB).
  - **Validare Teste Automate:** AdÄƒugat Grupul 27 (`SWE.5-TC27.1` .. `SWE.5-TC27.5`) pentru `FR-42` .. `FR-45`; total **95/95 PASS (100.0%)**, **49/49 CerinÈ›e Acoperite (100.0%)** pe hardware real (`192.168.1.20`).

- **ThinkPad L590 Home Server 24/7 OPERAÈšIONAL & CONSOLIDAT (Finalizat 19.10.2026):**
  - **Hardware & OS:** Lenovo ThinkPad L590 (Intel Core i7-8565U, 32 GB RAM, 475 GB SSD NVMe). Baterie Ã®n stare excepÈ›ionalÄƒ (98% sÄƒnÄƒtate, 58 cicluri), configuratÄƒ Ã®n mod UPS hardware cu praguri TLP 75%â€“80%. FuncÈ›ioneazÄƒ 24/7 cu capacul Ã®nchis (`HandleLidSwitch=ignore`).
  - **ReÈ›ea & Conectivitate:** 
    * Wi-Fi dedicat asociat pe IP static **`192.168.1.11`** (blocajul BIOS `WirelessAutoDisconnection` rezolvat direct din Linux via `think-lmi`).
    * **Tailscale Mesh VPN:** ÃŽnrolat pe IP-ul privat **`100.100.72.10`** cu Tailscale SSH activat, conectat direct cu telefonul `oppo-find-x8-pro` È™i tableta `tab-a9` pentru acces securizat de oriunde (inclusiv de la plajÄƒ).
  - **Stack Docker Modular Unificat (/opt/smarthome/docker-compose.yml - 8 Containere UP 21.10.2026):**
    * **`mosquitto` (Eclipse Mosquitto 2.x):** Broker MQTT activ pe `1883:1883` cu persistenÈ›Äƒ pe disc.
    * **`teslamate_db` (PostgreSQL 17):** BazÄƒ de date pregÄƒtitÄƒ pentru telemetria vehiculului.
      - Notă: Baza de date goală, importul este amânat pentru dimineață când se preia dump-ul din 22.10.2026 ora 03:00.
    * **`teslamate` (Latest):** Serviciul activ pe portul `4000:4000`.
    * **`teslamate_grafana` (Latest):** Dashboard-uri active pe portul `3000:3000`.
    * **homeassistant (Home Assistant Stable):** Activ pe portul 8123:8123 (network_mode: host), sincronizat bidirecțional cu controllerul ESP32 (192.168.1.20) prin pachetul smart_heating.yaml.
    * **dozzle (Amir20 Dozzle Latest):** Monitor vizual containere & loguri live în browser pe portul 8888:8080.
    * **`nginx_gateway` (Nginx Alpine):** Reverse proxy gateway pe portul `8088:8088` cu Basic Auth (`seceleanu:bogdan`) È™i portalul web modern Dark Mode **Smart Home Nexus**.
    * **`cloudflare_tunnel` (Cloudflared QUIC):** Conectat la 4 noduri edge Cloudflare cu tokenul permanent, rutÃ¢nd domeniul `https://ha.mysmarthome.cc/` direct Ã®n gateway-ul ThinkPad-ului.
    * **`aquarium_proxy`:** Rutare funcÈ›ionalÄƒ cÄƒtre Acvariu (`192.168.1.32:80`) pe `https://ha.mysmarthome.cc/aquarium/`.
  - **Home Assistant:** Decuplat ca orchestrator opÈ›ional conform consensului /grill-me din 21.10.2026.
  - **Front-Door & Back-Door (100% OPERAÈšIONAL 21.10.2026):**
    * https://mysmarthome.cc -> Public Showcase (200 OK, fÄƒrÄƒ parolÄƒ, Dark Mode, Hardware, Montaj, Simulator Live).
    * https://ha.mysmarthome.cc -> Private Hub (401/200 OK cu Basic Auth, TeslaMate, Grafana, ESP32 API, Acvariu).
    * Configurat modular Nginx Virtual Hosts Ã®n /opt/smarthome/nginx/conf.d/default.conf.
  - **ArhitecturÄƒ Multi-Tenant IoT SaaS (`mysmarthome.cc` - Adoptat 19.10.2026):**
    * DocumentaÈ›ie de referinÈ›Äƒ: [`docs/architecture_multitenant_iot_saas.md`](file:///d:/_git/smart-home-heating/docs/architecture_multitenant_iot_saas.md) È™i ADR Ã®n `decisions.md`.
    * **Topologie Cloudflare:** `mysmarthome.cc` (prezentare comercialÄƒ), `app.mysmarthome.cc` (portal multi-tenant cu autentificare È™i filtrare date per utilizator/locuinÈ›Äƒ), `mqtt.mysmarthome.cc` (ingestie dispozitive ESP32 din teren fÄƒrÄƒ deschidere de porturi).
    * **Aliniere Sesiune ParalelÄƒ ("SmartHeating UI/UX/ESP32"):**
      - **UI/UX:** Decuplare completÄƒ de IP-ul local fix (`192.168.1.20`) - utilizare URL-uri relative `/api/...` sau configurabile. Suport dinamic pentru 1..N zone per locuinÈ›Äƒ (scalabil pentru clienÈ›i diferiÈ›i).
      - **ESP32 C++ Firmware:** MenÈ›inere buclÄƒ de siguranÈ›Äƒ offline-first (hardware & relee 100% autonome); pregÄƒtire conexiune MQTT outbound cu `device_id` È™i token de autentificare securizat (`DevicePassport`) pentru telemetrie cloud È™i update-uri OTA la distanÈ›Äƒ.

- **Aliniere RiguroasÄƒ 1:1 DocumentaÈ›ie & Testare ASPICE SWE.5 pe Hardware Real (Finalizat 18.10.2026):**
  - **44/44 CerinÈ›e Acoperite (100.0%):** FR-1..FR-40 + NFR-1.1, NFR-1.5, NFR-2.2, NFR-3.2 documentate Ã®n detaliu Ã®n `docs/requirements_and_features.md` cu trasabilitate bidirecÈ›ionalÄƒ completÄƒ cÄƒtre codul sursÄƒ C++ È™i suita de testare.
  - **85/85 Teste Verificare Live (100.0% PASS):** Suita automatÄƒ `tools/test_system.py` extinsÄƒ de la 68 la 85 de teste atomice, executate Ã®mpotriva plÄƒcii reale ESP32 (`192.168.1.20`). Zero cerinÈ›e netestate, zero contracte relaxate.
  - **Hardening LogicÄƒ & Boundary Clamping:** Descoperit È™i corectat intervalul permis pentru pragul cÄƒdere geam deschis Ã®n C++: forÈ›at strict `[0.8Â°C .. 2.0Â°C]` (default 1.2Â°C) Ã®n `ThermalZoneManager.cpp` È™i validare HTTP 400 Ã®n `main.cpp` pentru valori eronate/periculoase.
  - **DocumentaÈ›ie CompletÄƒ ActualizatÄƒ:** `docs/requirements_and_features.md`, `docs/api_reference.md`, `docs/underfloor_heating_hydraulics_and_failsafes.md`, `docs/system_test_report.md`, `docs/plan.md`.

- **SuitÄƒ CompletÄƒ DetecÈ›ie Geam Deschis, Prag Configurabil & PersistenÈ›Äƒ Git Home Assistant (Finalizat 18.10.2026):**
  - **Folder Unificat `homeassistant/` Persistat Ã®n Git:**
    * Mutat È™i consolidat Ã®ntreaga integrare direct Ã®n rÄƒdÄƒcina proiectului Ã®n folderul `homeassistant/` (nu mai depinde de maÈ™ina virtualÄƒ WSL): `packages/smart_heating.yaml`, `dashboards/smart_heating.yaml`, `configuration.yaml`, `docker-compose.yml`, `README.md`.
    * AdÄƒugat Ã®n `.gitignore` excluderile pentru fiÈ™ierele de runtime generate local de container (baze de date `.db`, loguri, `.storage`).
  - **Sistem Inteligent DetecÈ›ie Geam Deschis (Open Window Guard):**
    * **FSM & SiguranÈ›Äƒ FizicÄƒ:** La o cÄƒdere bruscÄƒ de temperaturÄƒ (ex. $\ge 1.2^\circ\text{C}$ Ã®n $\le 5$ min), zona intrÄƒ Ã®n `ZONE_WINDOW_PAUSE`, decupleazÄƒ instant vana electrotermicÄƒ (0V) È™i stinge cazanul dacÄƒ nu mai existÄƒ cerere Ã®n alte camere.
    * **IeÈ™ire AutomatÄƒ & ManualÄƒ:**
      1. Auto-timeout: 30 de minute expirate.
      2. Rebound termic: revenire automatÄƒ dacÄƒ temperatura creÈ™te cu $+0.6^\circ\text{C}$ (geamul a fost Ã®nchis È™i camera se reÃ®ncÄƒlzeÈ™te).
      3. Override manual: utilizatorul poate anula instant pauza din butonul `[â†º Am ÃŽnchis (Reia)]` atÃ¢t din Web UI cÃ¢t È™i din Home Assistant. La anulare, se reseteazÄƒ temperatura baseline la valoarea curentÄƒ pentru a preveni declanÈ™area falsÄƒ imediatÄƒ.
    * **Prag de Sensibilitate Configurabil per ZonÄƒ (0.8Â°C â€“ 2.0Â°C):**
      - AdÄƒugat parametrul `windowDropThreshold` Ã®n `ThermalZoneConfig`, persistat Ã®n flash NVS (`<Preferences.h>`).
      - Endpoint dedicat `POST /api/zones/window/threshold?id=X&threshold=Y` È™i modal de configurare direct pe badge-ul din UI (`ðŸªŸ Prag: 1.2Â°C âœï¸`).
    * **Telemetrie & Istoric:**
      - ÃŽnregistreazÄƒ timestamp-ul È™i durata ultimei aerisiri (`lastDetectionEpoch`, `lastDetectionDurationSec`), expuse Ã®n `GET /api/zones`.
      - CÃ¢nd geamul este Ã®nchis, se afiÈ™eazÄƒ subtil istoricul: `ðŸªŸ Aerisit: acum 15m (18m)`.
    * **Vizibilitate CompletÄƒ Web UI & Home Assistant:**
      - Web UI: Banner animat proeminent pe cardul camerei cu cronometru rÄƒmas (`MM:SS`) È™i buton direct `[â†º Am ÃŽnchis (Reia)]`. SchemÄƒ SVG actualizatÄƒ cu minutele rÄƒmase.
      - Home Assistant: 3 senzori `binary_sensor` cu `device_class: window`, 3 senzori de countdown, 3 template buttons `button.reia_incalzire_geam_*` È™i carduri de alertÄƒ condiÈ›ionale Lovelace (apar automat pe dashboard cÃ¢nd un geam este deschis È™i dispar cÃ¢nd se Ã®nchide).
    * **Build & Verificare:** PlatformIO `pio run` SUCCESS (21.1s, RAM: 21.6%, Flash: 64.7%). Header web comprimat cu `build_web.py` (40.3 KB gzip).

- **Integrare Telemetrie Istoric LittleFS pe ESP32, Model Termic Gaz, Grafic Canvas Live & Compatibilitate Home Assistant (Finalizat 17.10.2026):**
  - **Flash Telemetry History Manager pe ESP32 (`TelemetryHistoryManager.cpp/.h`):**
    * StructurÄƒ binarÄƒ ultra-compactÄƒ de 10 bytes/punct (3 zone scaled x10 int16, temp exterioarÄƒ int16, flags biÈ›i flacÄƒrÄƒ/pompÄƒ/valve/valid).
    * DouÄƒ buffere circulare persistate pe LittleFS Ã®n partiÈ›ia `spiffs` de 192KB: 24h @ 5 min (288 puncte = 2.8 KB) È™i 5 zile @ 30 min (240 puncte = 2.4 KB) â€” total doar ~5.2 KB.
    * Wear-leveling hardware peste 48 de sectoare flash: scriere pe oricare sector individual la fiecare ~4 ore (>15 ani durabilitate garantatÄƒ).
    * Endpoint REST `/api/history?range=24h|5d` cu payload array-of-arrays compact (<4.5 KB) È™i fÄƒrÄƒ fragmentare RAM.
    * Comenzi seriale / telnet adÄƒugate: `HISTORY`, `HISTORY CAPTURE`, `HISTORY CLEAR`.
    * Compilare curatÄƒ verificatÄƒ cu `pio run` (SUCCESS, 119s, RAM: 21.5% [70 KB], Flash: 64.3% [1.26 MB]).
  - **Grafic Canvas Live de ÃŽnaltÄƒ RezoluÈ›ie (`web/index_v2.html`):**
    * Comutare instantanee `[24 Ore (5m) | 5 Zile (30m)]` cu fetch asincron din `/api/history` È™i cache local.
    * Randare curbe reale pentru cele 3 zone (Living portocaliu, Copil albastru, Matrimonial verde) È™i bare portocalii umbrite pentru ciclurile de ardere flacÄƒrÄƒ cazan.
    * Fallback estetic pentru mod offline/demo.
  - **Model Termic Gaz & Card Telemetrie (Layer 4.5):**
    * Calibrat baseload apÄƒ caldÄƒ menajerÄƒ (ACM): `0.40 mÂ³/zi` (~`12.0 mÂ³/lunÄƒ`), dedus strict din facturile de varÄƒ (Mai-Iulie 2026, 39 MC citit Ã®n 94 zile).
    * Calibrat debit centralÄƒ Ã®n condensaÈ›ie pe pardosealÄƒ la temperaturÄƒ joasÄƒ: `0.60 mÂ³/orÄƒ` de ardere flacÄƒrÄƒ (~6 kW putere medie modulatÄƒ).
    * Configurat preÈ› reglementat plafonat gaz: `3.30 Lei/mÂ³` (0.31 Lei/kWh).
    * AfiÈ™aj mecanic virtual cu role (Tumbler) pentru contorul fizic `Serie: 34119099055` È™i asistent transmitere index autocitire.
  - **Tab Calibrare & Autocitire Ã®n Admin Suite (PIN 1234):**
    * Sincronizare 1:1 index contor, parametri termici È™i reconciliere tabele facturi.
  - **Pachet YAML Complet Home Assistant (`integrations/homeassistant/`):**
    * `smart_heating.yaml` gata de producÈ›ie cu `sensor.smart_heating_gas_meter_index` pentru Energy Dashboard, senzori zilnici, climate È™i switch-uri REST.
  - **Integrare CompletÄƒ Home Assistant & Portare Sistem TotalÄƒ (Finalizat 18.10.2026):**
    * Pachetul `packages/smart_heating.yaml` migrat la sintaxa modernÄƒ Home Assistant `template:` (suport complet 2024â€“2026):
      - ÃŽnlocuit blocurile deprecate `button: - platform: template` È™i `switch: - platform: template` cu blocul unificat `template: - switch: ... - button: ...`.
      - Rezolvat eroarea Home Assistant Repair (`platform_integration_no_support_button_template` È™i `switch_template`). 0 erori/repairs active.
      - 3 Termostate `climate` cu buclÄƒ bidirecÈ›ionalÄƒ automatÄƒ fÄƒrÄƒ latenÈ›Äƒ (HA <-> ESP32).
      - Contor de gaz cu index fizic (mÂ³), consum zilnic È™i cost RON pentru Energy Dashboard.
      - Comutator Mod CentralÄƒ `switch.smart_heating_auto_mode` (AUTO vs MANUAL).
      - Comutator `switch.smart_heating_schedule` È™i selector `input_select.smart_heating_preset` pentru Bio-Schedule.
      - Butoane comenzi forÈ›ate: PorneÈ™te Cazan, OpreÈ™te Cazan, AnuleazÄƒ Cooldown/AmÃ¢nare, RuleazÄƒ Self-Test, È˜terge Erori DTC.
      - Telemetrie completÄƒ termometre Xiaomi BLE: Baterie %, Umiditate %, Semnal RSSI dBm per camerÄƒ.
      - Diagnostic hardware ESP32: Scor SÄƒnÄƒtate %, RAM Liber (Heap KB), Uptime ore, Wi-Fi RSSI dBm, Serie È™i Firmware.
    * Creat Dashboard Dedicat Lovelace (`dashboards/smart_heating.yaml`) cu 3 vederi specializate:
      1. `Panou Principal`: Badges stare, card CentralÄƒ & PompÄƒ, cele 3 termostate mari, card Bio-Schedule È™i grid comenzi rapide.
      2. `Gaz & Energie`: Carduri facturÄƒ/consum È™i 2 grafice istorice (Temperaturi 24h + Cicluri ardere flacÄƒrÄƒ È™i pompÄƒ).
      3. `Senzori & Diagnostic`: Tabel baterii È™i semnal BLE Xiaomi, diagnozÄƒ controller ESP32 È™i acÈ›iuni de testare.
    * Verificate toate cele 46 de entitÄƒÈ›i din dashboard: 100% asociate È™i Ã®nregistrate Ã®n Home Assistant Core. Zero avertismente "Entity not found".
    * Dashboard-ul este integrat direct Ã®n bara lateralÄƒ Home Assistant (`Smart Heating`, icon `mdi:radiator`) pe mobil È™i desktop.

- **Remediere FSM Race Condition (Vane Blocate Deschis), Comportament Suprascriere TemporarÄƒ (Temporary Hold) & Overrun Inteligent Cazan (Finalizat 18.10.2026):**
  - **Identificare Root-Cause Bug Vane Blocate Deschis:** La scÄƒderea setpoint-ului, Pasul 1 (histerezis) forÈ›a prematur `state = ZONE_IDLE`. Pasul 2 (releele vanelor) verifica `if (state == ZONE_HEATING || state == ZONE_WARMING_UP)` pentru decuplare; condiÈ›ia pica mereu, lÄƒsÃ¢nd releul alimentat la 230V permanent (`relayEngaged: true`), chiar dacÄƒ cererea era `false` È™i starea era `Idle`.
  - **Remediere ArhitecturÄƒ FSM (`src/ThermalZoneManager.cpp`):**
    * Pasul 1 seteazÄƒ exclusiv `demand = false`, decuplarea sigurÄƒ a releelor È™i tranziÈ›ia de stare fiind executate determinist Ã®n Pasul 2.
    * AdÄƒugat **Hardware Safety Invariant**: dacÄƒ o vanÄƒ are `relayEngaged == true` dar `demand == false` (È™i nu este keeper zone Ã®ntr-un overrun activ), releul este decuplat necondiÈ›ionat.
    * **Instant Warmup Abort**: scÄƒderea setpoint-ului Ã®n timpul dilatÄƒrii cerii (120s) decupleazÄƒ instant releul È™i anuleazÄƒ orice cerere de flacÄƒrÄƒ/pompÄƒ.
    * **Overrun Inteligent Cazan**: post-circulaÈ›ia pompei de 180s È™i menÈ›inerea deschisÄƒ a vanei de disipare au loc **exclusiv dacÄƒ flacÄƒra cazanului a ars efectiv** Ã®n ciclul respectiv; dacÄƒ flacÄƒra nu s-a aprins (comanda a fost anulatÄƒ sau a cÄƒzut cererea Ã®nainte de ardere), totul se opreÈ™te imediat (0s overrun).
  - **Suprascriere TemporarÄƒ (Temporary Hold) IntegratÄƒ cu Bio-Schedule:**
    * Modificarea setpoint-ului dintr-o camerÄƒ (din HA sau Web) activeazÄƒ regimul `isManualHold` Ã®n acea camerÄƒ fÄƒrÄƒ a modifica sau strica presetul salvat.
    * La sosirea orei urmÄƒtoarei ancore circadiene (ex. Somn 20:00), hold-ul expirÄƒ automat È™i camera revine la valoarea din orar.
    * AdÄƒugat endpoint `POST /api/zones/hold/clear?id=X` È™i buton contextual `[â†º Hold]` Ã®n interfaÈ›a web pentru revenire manualÄƒ anticipatÄƒ.
  - **ProtecÈ›ie Flash NVS & Anti-Chatter Relee:**
    * Salvarea setpoint-urilor Ã®n flash NVS este temporizatÄƒ non-blocant cu debounce de **3 secunde** (`_nvsSavePendingMs`), protejÃ¢nd memoria cipului la glisarea rapidÄƒ a sliderelor de temperaturÄƒ din telefon.
  - **Verificare & Build:** Compilare PlatformIO curatÄƒ (`pio run` SUCCESS Ã®n 18.1s, RAM: 21.5%, Flash: 64.4%).

- **Refactorizare Densitate ExtremÄƒ UI 720p, Redesenare Card CentralÄƒ & Rezolvare Comutare Preset Utilizator (Finalizat 16.10.2026):**
  - **Compactare ExtremÄƒ Antet & Card CentralÄƒ pentru Ecrane 720p (`web/index.html`):**
    * Brand compact (`Smart Heating`) pe un singur rÃ¢nd, eliminat subtitlul redundant.
    * Scurtat denumirile tab-urilor (`Panou`, `Tesla Mode`, `FSM Logic`, `ConsolÄƒ Live`) È™i ascuns textele ajutÄƒtoare de nivel È™i mod de vizualizare pentru a preveni wrap-ul pe ecrane de 720p / tablete.
    * Cardul centralei (`#card-boiler`) compactat pe o singurÄƒ bandÄƒ orizontalÄƒ (~40px) cu starea flÄƒcÄƒrii, pompa GPA È™i modul `[âš™ï¸ AUTO | âœ‹ MANUAL]`.
    * Butoanele `PorneÈ™te Centrala` È™i `OpreÈ™te Centrala` sunt acum complet **ascunse** Ã®n mod `AUTOMATIC` È™i devin vizibile exclusiv pe modul `MANUAL`.
    * Cele 3 contoare de funcÈ›ionare (`Azi`, `Ieri`, `Total`) È™i estimarea de gaz au fost mutate din cardul principal Ã®ntr-un modal dedicat (`#modal-gas-costs`) accesibil prin linkul discret `ðŸ”¥ Consum & Cost Gaz` din footerul cardului de zone.
  - **Redesenare ErgonomicÄƒ RÃ¢nduri Zone Termice:**
    * Butonul de alimentare `â»` mutat ergonomic Ã®n stÃ¢nga, Ã®ntre indicatorul BLE È™i numele camerei (`[ðŸŸ¢ Dot] [â»] [Nume CamerÄƒ âœï¸]`).
    * Eliminat chenarul verde intruziv "Repaus" cÃ¢nd zona este inactivÄƒ. Badge-ul apare exclusiv cÃ¢nd zona acÈ›ioneazÄƒ (`â™¨ï¸ ÃŽncÄƒlzire`, `â³ AÈ™teptare`, etc.).
    * Eliminat eticheta `È›intÄƒ`; butoanele de reglaj `[-] 21.0Â° [+]` sunt grupate direct lÃ¢ngÄƒ temperatura curentÄƒ È™i graficul sparkline.
  - **Rezolvare Resetare Preset Activ pe "Standard / Confort":**
    * Identificat cÄƒ testele `SWE.5-TC17.3` (`/api/schedule/reset`) È™i `SWE.5-TC18.4` (`apply?id=default`) din `tools/test_system.py` resetau presetul activ la `default`.
    * AdÄƒugat Ã®n `tools/test_system.py` capturarea iniÈ›ialÄƒ `self.user_initial_preset` È™i restaurarea automatÄƒ la finalul suitei.
    * AdÄƒugat Ã®n `src/ThermalZoneManager.cpp` log explicit de tranziÈ›ie: `[Schedule][PRESET] Preset switched: 'old' -> 'new'`.
    * Presetul utilizatorului `Bogdan` (`custom1`) este restaurat È™i confirmat activ.
  - **Format Widescreen 16:9 & Aliniere LateralÄƒ (`web/index.html`):**
    * ÃŽnlocuit `max-width: 1060px` cu un container expandabil pÃ¢nÄƒ la `1560px` cu centrare automatÄƒ (`margin: 0 auto; padding: 12px 16px 24px;`). Pe ecrane 16:9 conÈ›inutul acoperÄƒ armonios lÄƒÈ›imea display-ului fÄƒrÄƒ margini negre masive.
    * Decuplat breakpoint 2 coloane la 1140px; sub 1140px totul curge Ã®ntr-o singurÄƒ coloanÄƒ intuitivÄƒ (Bara de sus -> CentralÄƒ -> Zone Termice -> Bio-Schedule).
    * Eliminat complet textul static `NimBLE 90% Scan`.
  - **ConsolÄƒ Wi-Fi Live IntegratÄƒ & Proxy Local (`tools/dev_server.py`, `web/index.html`):**
    * `dev_server.py` pe portul 8080 serveÈ™te ca reverse proxy transparent cÄƒtre ESP32 (`192.168.1.20`).
    * AdÄƒugat tab dedicat `>_ ConsolÄƒ Live` direct Ã®n dashboard-ul principal cu streaming Ã®n timp real È™i comenzi directe.
  - **Verificare & Calitate Cod:**
    * Compilare PlatformIO `pio run` (SUCCESS, Flash: 62.4%, RAM: 19.9%).
    * Suita automatÄƒ ASPICE SWE.5 (`tools/test_system.py`) trecutÄƒ cu **100% PASS (68/68, 33/33 cerinÈ›e)** cu pÄƒstrare garantatÄƒ a presetului activ.
    * Verificare vizualÄƒ completÄƒ prin subagent browser pe `http://localhost:8080/index.html`.

- **CurÄƒÈ›are TotalÄƒ ConsolÄƒ, Izolare SuitÄƒ Teste ASPICE & Failsafe FÄƒrÄƒ Spam Boiler (Finalizat 15.10.2026):**
  - **Izolare TotalÄƒ SuitÄƒ Teste Hardware (`tools/test_system.py`):**
    - S-a eliminat spamul generat de testul `test_tc10_1` prin apelarea imediatÄƒ a `/api/boiler/cancel` dupÄƒ cele 3 cereri de test pentru DTC-201, prevenind aprinderea amÃ¢natÄƒ a flÄƒcÄƒrii reale (dupÄƒ 50s) care declanÈ™a bucla de spam la oprire.
    - S-a eliminat poluarea consolei din `test_tc13_1` È™i `test_tc13_3`: testul `STATUS` È™terge automat cele 100 de linii de tabel din inelul de loguri (`CLEAR`) imediat dupÄƒ aserÈ›iune.
    - S-a corectat testul `test_tc19_2` (clamping temperaturi): foloseÈ™te acum `/api/mode?mode=manual` È™i dezactivare temporarÄƒ orar, izolÃ¢nd complet releele fizice È™i prevenind acÈ›ionarea actuatorilor la 30Â°C.
    - S-au Ã®nlocuit bannerele lungi ASCII cu mesaje discrete de 1 rÃ¢nd (`[TEST-SUITE] >>> Automated Test Suite Started / Finished <<<`) È™i curÄƒÈ›are completÄƒ la final.
  - **Remediere ComandÄƒ CLEAR / CLS pe ConsolÄƒ:** ÃŽn `NetworkConsole.cpp`, s-a corectat funcÈ›ia `clear()` pentru a reseta `_totalBytes = 0`, `_head = 0`, `_count = 0` È™i `_logRing`. ÃŽn `executeCommand()`, comanda `CLEAR` / `CLS` apeleazÄƒ direct `clear()` fÄƒrÄƒ a mai re-inunda bufferul cu `[CMD] < END: CLEAR`. ÃŽn `web/console.html`, comanda È™i butonul `CLEAR` curÄƒÈ›Äƒ instantaneu DOM-ul (`term.innerHTML = ''`), reseteazÄƒ `lastSeq = 0` È™i È™terg memoria circularÄƒ pe ESP32.
  - **Eliminare Spam BuclÄƒ Oprire Cazan (DTC-201 Fix):** ÃŽn `src/ThermalZoneManager.cpp`, s-a remediat bug-ul Ã®n care `sendBoilerOff()` era apelat la fiecare secundÄƒ cÃ¢nd zonele Ã®È™i atingeau setpoint-ul dar cazanul se afla Ã®ncÄƒ Ã®n perioada de gardÄƒ (60s anti-short-cycling). Acum se verificÄƒ `boiler.canTransmit(false)`. DacÄƒ garda este activÄƒ, se programeazÄƒ `PENDING_OFF` o singurÄƒ datÄƒ Ã®n coada RF.
  - **Unificare Coloana 2 & Eliminare Card Duplicat:** Cardul superior `Zone Termice (PardosealÄƒ)` È™i cel inferior `Telemetrie Senzori Xiaomi BLE` au fost consolidate Ã®ntr-un singur card compact `#card-zones-sensors`.
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS, suita de teste atomice `tools/test_system.py` cu 68/68 PASS (100.0%) Ã®n 24.51s, bufferul de consolÄƒ rÄƒmÃ¢nÃ¢nd 100% curat cu un singur rÃ¢nd de finalizare.

- **Sincronizare Live Orar Circadian, Preseturi Calibrate din FabricÄƒ & Tracking Runtime Cazan (Finalizat 15.10.2026):**
  - **Sincronizare Live Card Erou & NumÄƒrÄƒtoare InversÄƒ:** Expus `activePreset`, `activeAnchor`, `nextAnchor` È™i `minsToNextAnchor` Ã®n `/api/status` la fiecare poll (3.5s). Frontend-ul actualizeazÄƒ instantaneu cardul erou circadian (`renderActiveAnchorHero`) È™i bannerul numÄƒrÄƒtorii inverse (`updateNextAnchorCountdown`) fÄƒrÄƒ a mai necesita refresh manual de paginÄƒ (F5).
  - **Calibrare Preseturi din FabricÄƒ (L-V & S-D):** Preset-urile `Standard / Confort` È™i `Home Office` au fost actualizate Ã®n codul sursÄƒ C++ cu orele È™i temperaturile exacte din capturile utilizatorului (ex. Somn 20:00 L-V, 21:00 S-D; Trezire 06:00 L-V, 07:30 S-D; Home Office 07:30 L-V, 12:00 S-D).
  - **Redesign Panou Preseturi & Editor Matriceal:** Sloturile utilizator libere (`Slot Liber (custom2..4) (Liber)`) sunt ascunse din UI. AdÄƒugatÄƒ barÄƒ dedicatÄƒ de acÈ›iuni: `âž• AdaugÄƒ Slot`, `ðŸ’¾ SalveazÄƒ`, `â†º AnuleazÄƒ (Discard)`, `âœï¸ RedenumeÈ™te`, `ðŸ—‘ï¸ È˜terge`. Butonul matricei a fost retras Ã®ntr-un stil discret È™i compact; fereastra matricealÄƒ afiÈ™eazÄƒ coloane paralele `OrÄƒ Lâ€“V` È™i `OrÄƒ Sâ€“D` cu buton rapid de clonare `ðŸ“‹ CopiazÄƒ L-V Ã®n S-D`.
  - **Jurnalizare Insight-uri & Clarificare Mesaje Cerere TermicÄƒ:** Formatat log-ul de pornire cazan cu 2 zecimale exacte (`current 22.21 C <= start threshold 22.20 C`) pentru a elimina formulÄƒrile confuze de tip `22.2 <= 22.2`. AdÄƒugat log automat Ã®n consolÄƒ la rÄƒcire bruscÄƒ a camerei: `[Thermal][INSIGHT] Rapid cooling detected on Zone X: ... possible aeration`.
  - **Tracking Timp FuncÈ›ionare Cazan & Estimare FacturÄƒ Gaz:** Implementat contor non-blocant Ã®n `BoilerRadioTrigger` cu persistenÈ›Äƒ NVS È™i wear-leveling (astÄƒzi, ieri, total). AfiÈ™at Ã®n UI cu dialog `âš™ï¸ FacturÄƒ / Cost` pentru calibrare pe baza facturii reale de iarnÄƒ (default 600 lei / 180 ore flacÄƒrÄƒ).
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS (Flash 1.22 MB, RAM 19.9%), suita de teste `tools/test_system.py` cu 68/68 PASS (100.0%) Ã®n 23.82s.

- **Igienizare Memorie Heap C++ & Decuplare Dev Server Frontend (Finalizat 15.10.2026):**
  - **Zero Fragmentare Heap pe ESP32:** Implementat pre-alocare exactÄƒ de buffer `json.reserve(...)` / `out.reserve(...)` Ã®n toate serializatoarele JSON È™i rapoartele din C++ (`main.cpp`, `ThermalZoneManager.cpp`, `DiagnosticEngine.cpp`, `TimeAndWeatherService.cpp`, `DevicePassport.cpp`). Eliminat zecile de micro-realocÄƒri heap per ciclu de interogare HTTP, asigurÃ¢nd stabilitate pe termen lung a Free Heap-ului (>115 KB).
  - **CurÄƒÈ›are Frontend & Optimizare CSS:** Eliminat blocul duplicat `@media (max-width: 520px)` din `web/index.html`. Compilare gzip automatÄƒ `build_web.py` validatÄƒ (119 KB -> 29.1 KB).
  - **Decuplare Dev Server Local (`tools/dev_server.py`):** Server local pe portul 8080 (`http://localhost:8080/index.html`) conectat prin CORS la ESP32 (`192.168.1.20`) pentru iteraÈ›ii UI ultra-rapide cu zero uzurÄƒ flash.
  - **Micro-Grafice Vectoriale Sparkline de Trend & Aliniere Tabel Senzori:** Upgrade complet Ã®n `web/index.html` È™i `include/Config.h`. Chei canonice de istoric unificate (`getCanonicalTempKey`) garantÃ¢nd curbe 100% identice Ã®ntre cardul camerei È™i linia senzorului (fÄƒrÄƒ linii dinÈ›ate/zbÃ¢rcite). Tabelul de senzori BLE a fost re-aliniat strict pe coloane (`.sensor-temp-col`, `.sensor-hum-col`), eliminÃ¢nd badge-urile text late Ã®n favoarea unei buline luminoase circulare (`.sensor-signal-dot`) cu tooltip de semnal dBm. Senzorul `A4:C1:38:94:0F:44` a fost actualizat din `Bathroom` Ã®n `Dormitor Matrimonial` cu funcÈ›ie de redenumire directÄƒ din GUI (âœï¸). Verificat prin capturi CDP (`e2e_11_zones_aligned.png`, `e2e_12_sensors_aligned.png`).
  - **ASPICE SWE.5 & Build:** PlatformIO build 100% SUCCESS, suita de testare `tools/test_system.py` cu 68/68 PASS (100.0%) Ã®n 24.54s, acoperire 33/33 cerinÈ›e.

- **Clarificare Terminologie Zone, Corectare Indexare ID-uri & Decuplare Telemetrie Senzori (Finalizat 14.10.2026):**
  - **Decuplare Scanare Senzori:** `ThermalZoneManager::update()` rezolvÄƒ acum pachetele de telemetrie BLE Ã®naintea verificÄƒrii `enabled`. Temperatura fizicÄƒ a camerei este monitorizatÄƒ permanent, eliminÃ¢nd falsul `TIMEOUT (Failsafe OFF)` din consolÄƒ È™i afiÈ™ajul `--.-Â°C` cÃ¢nd un circuit este oprit manual.
  - **Fix Bug Indexare UI (Off-by-One):** `renderZones` transmite acum ID-ul explicit `z.id` (1, 2, 3) cÄƒtre `toggleZone` È™i `adjTarget`, eliminÃ¢nd decalajul cauzat de indexul 0-based care comuta camera greÈ™itÄƒ (Zona 3 oprea Zona 2 / Camera Copilului).
  - **Redesign Terminologie Carduri:** Eliminat jargonul confuz `Releu GPIOxx: INACTIV (ÃŽnchisÄƒ) DezactiveazÄƒ`. Noul design prezintÄƒ starea termostatului (`ðŸŸ¢ Temp OK (Repaus)` / `âšª ZonÄƒ OpritÄƒ`), starea fizicÄƒ a È™apei (`Robinet È™apÄƒ: ÃŽNCHIS (Repaus / Temp atinsÄƒ)` vs `DESCHIS (CirculÄƒ apÄƒ caldÄƒ)`), È™i butoane clare de acÈ›iune `[â›” OpreÈ™te camera]` / `[â–¶ï¸ PorneÈ™te camera]`.
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Firmware Version: **v1.2.0 (Build: Oct 14 2026 23:06:40)**.
  - Flash Binary Size: **1,210,741 bytes (1.21 MB, 61.6% flash)** â€” sub limita maximÄƒ a partiÈ›iei OTA (1.25 MB).
  - Free Heap: **~120 KB** stabil.
  - Web Server & Web OTA: Active on port 80.
  - ASPICE SWE.5 Host Verification: **68/68 PASS (100.0%)** pe suita completÄƒ de teste atomice ASPICE SWE.5 cu Matrice de Trasabilitate BidirecÈ›ionalÄƒ (33/33 cerinÈ›e acoperite 100.0%, 0 defecte).
  - Browser E2E Live Test Suite (Task 32): **10/10 PASS (100.0%)** pe browserul Google Chrome nativ via CDP.
  - Wall Kiosk Display: Tableta **Samsung Galaxy Tab A9 8.7" LTE** configuratÄƒ cu aplicaÈ›ia web PWA.
- **Personalizare CompletÄƒ Preseturi & Control RÄƒzgÃ¢ndire Cazan (Finalizat 14.10.2026):**
  - **Personalizare Preseturi:** Deblocat salvarea È™i editarea preseturilor din fabricÄƒ (`default`, `eco`, `wfh`) direct Ã®n NVS fÄƒrÄƒ a mai arunca eroare. AdÄƒugat suport pentru 4 sloturi libere de utilizator (`custom1` .. `custom4`), cu `resetPreset(presetId)` pe backend È™i buton contextual `[â†º Reset FabricÄƒ]` Ã®n UI.
  - **ProtecÈ›ie Slot `custom1` (`Bogdan`):** Suita de teste a fost mutatÄƒ pe slotul de test dedicat `custom4` cu auto-reset, protejÃ¢nd 100% presetul utilizatorului salvat pe `custom1` (`Bogdan`).
  - **RÄƒzgÃ¢ndire Cooldown & Anulare ComandÄƒ AmÃ¢natÄƒ:** Rezolvat bug-ul din `web/index.html` unde `triggerBoiler` bloca comanda dacÄƒ starea era deja `ON`. Utilizatorul poate apÄƒsa din nou pe â€žPorneÈ™te Centralaâ€ pentru a anula instantaneu oprirea programatÄƒ sau poate folosi noul buton `[âœ• AnuleazÄƒ]` din bannerul `#boiler-pending-banner` (apelÃ¢nd `/api/boiler/cancel`).
- **Remediere Audit Chrome DevTools & Lighthouse (FSM Admin, Tesla Mode & Panou Principal) (Finalizat 14.10.2026):**
  - **FSM Admin Overlap Fix:** Decalat nodul `S3` la `y=24..60` È™i `S4` la `y=76..164` Ã®n SVG (16px separare curatÄƒ, 0 coliziune). ÃŽn CSS: `.fsm-grid` cu `gap: 1.5rem` (1.25rem mobil), `.fsm-node` cu `overflow: hidden; z-index: 1;` È™i `z-index: 10;` pe hover/active.
  - **Accesibilitate & Contrast WCAG AA/AAA:** Ridicat textele muted de la `#6a707a` / `#475161` la `#cbd5e1` È™i `#f1f5f9` (contrast > 9:1 pe `#0a0f1d`), opacitate carduri inactive crescutÄƒ la 0.65.
  - **PerformanÈ›Äƒ TBT (Total Blocking Time < 40ms):** Decuplat randarea schemelor complexe (`updateTeslaSchema` È™i `updateFsmDiagram` executÄƒ exclusiv pe tab-ul vizibil, nu la fiecare tick de fundal); boot sequence Ã®ncapsulat Ã®n `requestAnimationFrame`.
  - **ToleranÈ›Äƒ ReÈ›ea & Polling Adaptiv:** AdÄƒugat pill de sincronizare `#pill-net` (`ðŸŸ¢ Live` / `ðŸŸ¡ 1.4s` / `ðŸ”´ Reconnect...`), Ã®nlocuit intervalul fix de 3.5s cu polling adaptiv (`scheduleNextPoll`) relaxat la 5.0s pe latenÈ›e mari (>1.2s) È™i backoff la 6.0s pe erori.
  - **Ergonomie MobilÄƒ & Touch Targets:** Butoanele `.btn-install`, `.mode-btn`, `.btn-tesla`, `.view-tab-btn`, `.lvl-btn` ridicate la `min-height: 42pxâ€“44px; padding: 10px 14pxâ€“16px;`.
  - **Punct 1 (Contrast Cromatic):** `#64748b` Ã®nlocuit cu `#94a3b8` (>5.8:1 contrast pe `#0f172a`/`#0a0f1d`), `#0284c7` pe butoane active Ã®nlocuit cu `#0369a1` (5.93:1 contrast pe text alb, 100% WCAG AA).
  - **Punct 2 (Form Controls Labels):** Atribute `aria-label` È™i `title` adÄƒugate pe switch-urile toggle `#sw-schedule-en`, `#sw-preheat-en`, `#sw-window-en`, inputurile de timp (`#mtime-*`, `#time-*`), selectoarele din modale È™i pe butoanele stepper de temperaturÄƒ.
  - **Punct 3 (Semantic Landmark):** Containerul `.container` transformat Ã®n `<main class="container" role="main">`.
  - **Punct 4 (Ergonomie MobilÄƒ & Touch Targets):** AdÄƒugatÄƒ regula `@media (max-width: 520px)` cu flex-wrap pe `.header-pills` È™i `min-height: 38px` pe `.btn-small` È™i `.tab-btn`.
  - **Punct 5 (ToleranÈ›Äƒ LatenÈ›Äƒ Proxy):** Timeout-ul `AbortController` din `refresh()` mÄƒrit la 4000ms, cu curÄƒÈ›are sigurÄƒ Ã®n `finally` È™i blocare ciclu concurent `if (isRefreshing) return;`.
- **DirectivÄƒ ArhitecturalÄƒ Advisor (Argon Maximum) â€” Remediere UI Kiosk, Stateflow FSM & SchemÄƒ TermicÄƒ (Finalizat 14.10.2026):**
  - **Problema 1: Panou Principal, Editare Ancore Circadiene & Naming Zona 3:**
    * Corectat bug-ul de pe `POST /api/schedule/anchor`: tratare robustÄƒ case-insensitive (`WAKE` / `wake`), parsare atÃ¢t URL params cÃ¢t È™i body JSON, conversie 12h AM/PM Ã®n 24h `HH:MM`. Modificarea ancorei active sincronizeazÄƒ instant setpoint-urile de zonÄƒ È™i forÈ›eazÄƒ re-evaluarea orarului.
    * Naming corectat: Zona 3 (GPIO25) este garantatÄƒ ca **"Dormitor Matrimonial"** (nu "Baie mare"), cu migrare forÈ›atÄƒ Ã®n NVS din C++.
    * Re-arhitecturare Kiosk: Eliminat dezastrul vizual (grila cu 4 carduri dense) din coloana stÃ¢ngÄƒ a panoului principal; mutat editorul de ancore Ã®ntr-o **Matrice TabelarÄƒ CompactÄƒ (4 Ancore Ã— 3 Zone)** deschisÄƒ printr-un modal dedicat (`modal-schedule-matrix`), cu auto-save debounced (800ms) È™i butoane per rÃ¢nd cu feedback vizual verde (`âœ“ Salvat Ã®n NVS`).
    * AdÄƒugat banner dinamic cu numÄƒrÄƒtoare inversÄƒ (`next-anchor-banner`) pentru urmÄƒtoarea ancorÄƒ circadianÄƒ.
  - **Problema 2: Firul Logic & StÄƒrile (FSM Admin) â€” Stateflow Ierarhic pe 2 Planuri Ã®n SVG:**
    * ÃŽnlocuit grid-ul plat 1â€“9 cu o diagramÄƒ orientatÄƒ SVG pe 2 planuri cu sÄƒgeÈ›i vectoriale (`<path marker-end="url(#fsm-arrow)">`):
      - **Planul A: Master Plant & Hydronic FSM** (Cazan Gaz + PompÄƒ GPA): tranziÈ›ii orientate $S0 \rightarrow S1 \rightarrow S2 \rightarrow (S3 \mid S4) \rightarrow S5 \rightarrow S6 \rightarrow S1$ cu substÄƒrile 4.1 (Min Burn Guard 60s), 4.2 (Steady Burn), 4.3 (Pre-Heat) È™i stÄƒrile speciale S7 (Anti-Seize), S8 (Manual Hold).
      - **Planul B: Zone Parallel FSM**: 3 benzi orizontale paralele pentru Z1 Living (GPIO32), Z2 Copil (GPIO33), Z3 Matrimonial (GPIO25), plus casetÄƒ de interblocÄƒri locale (Geam deschis, ProtecÈ›ie Ã®ngheÈ›, DTC-101 Failsafe).
    * PÄƒstrate toate cele 9 ID-uri de noduri pentru compatibilitate 100% cu ASPICE SWE.5 È™i suita CDP (`fsm-node-idle`, `fsm-node-demand`, `fsm-node-wax`, `fsm-node-manual-hold`, `fsm-node-active-heat`, `fsm-node-overrun`, `fsm-node-cooldown`, `fsm-node-frost`, `fsm-node-window`).
  - **Problema 3: Dinamizarea Schemei Termice (Tesla Mode):**
    * Topologie realÄƒ cu **7 circuite de È™apÄƒ** (Z1: Buclele 1, 2, 3; Z2: Buclele 6, 7; Z3: Buclele 4, 5) + ramurÄƒ de **Bypass Hidraulic Permanent (Portprosop BÄƒi)** legatÄƒ la senzorul BLE `Bathroom` (22.6Â°C).
    * Progres dinamic dilatare cearÄƒ actuatoare (`0% -> 100% [openSec/120s]`) pe baza parametrului `openSec` din `/api/zones`, cu bare de progres dedicate È™i pornirea animaÈ›iei de curgere la >30% deschidere (`openSec >= 36`).
    * Model termodinamic dinamic Tur / Retur inerÈ›ial (`[Estimat InerÈ›ial]`) fÄƒrÄƒ temperaturi fixe hardcodate.
  - **Verificare & Calitate:** PlatformIO build curat (Flash 1.20 MB, 61.4%), 63/63 teste PASS (100.0%) pe `tools/test_system.py`, 10/10 teste PASS (100.0%) pe `tools/browser_live_test.py` Ã®n Chrome nativ.
- **Sincronizare Aprindere Cazan, 3 Niveluri de Control UI & Anti-Spam (Rezolvat 14.10.2026):**
  - **Failsafe Power-Loss Invariant:** La pornirea din prizÄƒ (`begin()`), se emite imediat `sendBoilerOff(true)` burst pe 433.92 MHz È™i se stocheazÄƒ `OFF` Ã®n NVS. Acest lucru garanteazÄƒ oprirea fizicÄƒ a flÄƒcÄƒrii dupÄƒ orice cÄƒdere de tensiune sau reboot È™i sincronizeazÄƒ receptorul bistabil cu ecranul.
  - **CoadÄƒ ExecuÈ›ie AmÃ¢natÄƒ (Pending Actions):** La comutarea stÄƒrii Ã®n perioada de anti-short-cycling guard (60s ardere / 60s pauzÄƒ), serverul Ã®nregistreazÄƒ `PENDING_OFF` sau `PENDING_ON` È™i returneazÄƒ HTTP 429 cu `pending: true`, `target`, `waitSec`.
  - **ExecuÈ›ie AutomatÄƒ la Expirare:** ÃŽn bucla `loop()`, `boilerRadio.update()` verificÄƒ disponibilitatea transmisiei È™i executÄƒ comanda fÄƒrÄƒ intervenÈ›ia utilizatorului. ApÄƒsarea comenzii inverse anuleazÄƒ imediat acÈ›iunea amÃ¢natÄƒ (`pendingCancelled: true`).
  - **UI Kiosk ReacÈ›ional:** Badge pulsatoriu portocaliu (`.badge-pending`), banner informativ dedicat È™i contor de cooldown la secundÄƒ (`â³ Xs rÄƒmase`).
- **Sincronizare Preseturi & UX Ancore Dinamice (Rezolvat 14.10.2026):**
  - **Sincronizare Setpoint-uri Zone cu Orarul:**
    * La aplicarea oricÄƒrui preset (`wfh`, `eco`, `default`) sau resetare, `_configs[i].targetTemp` din fiecare zonÄƒ este sincronizat automat cu È›intele ancorei active din noul preset.
    * Cardurile din dreapta (`Zone Termice PardosealÄƒ`) afiÈ™eazÄƒ acum `effectiveTarget` (temperatura realÄƒ comandatÄƒ de Bio-Schedule), eliminÃ¢nd discrepanÈ›a vizualÄƒ Ã®ntre orar È™i zone.
    * Butoanele `+`/`-` din cardurile de zone propagÄƒ ajustarea instantaneu Ã®n ancora activÄƒ curentÄƒ din Bio-Schedule (`setAnchorTarget`), fÄƒcÃ¢nd trecerea automatÄƒ pe profilul `custom` È™i determinÃ¢nd termostatul sÄƒ reacÈ›ioneze fÄƒrÄƒ latenÈ›Äƒ.
  - **AfiÈ™aj Ancore Contextuale (Home Office / WFH):**
    * CÃ¢nd presetul activ este `wfh`, intervalul de zi (ancora tehnicÄƒ `away`, 08:30â€“17:00) nu mai afiÈ™eazÄƒ derutant `ðŸš¶ PLECAT (AWAY) - Casa este goalÄƒ`, ci `ðŸ’¼ HOME OFFICE (ZI)` cu descrierea `Lucru de acasÄƒ & confort termic optim de birou`.
    * Pentru presetul `eco`, ancora afiÈ™eazÄƒ `ðŸƒ VACANÈšÄ‚ / ECO (ZI)` cu descrierea `TemperaturÄƒ economicÄƒ redusÄƒ (economie gaz)`.
    * ÃŽn editorul de orar din stÃ¢nga jos, cardul ancorei de zi Ã®È™i adapteazÄƒ eticheta dinamic (`ðŸ’¼ Home Office` / `ðŸƒ Zi Eco` / `ðŸš¶ Plecat`).
- **InterfaÈ›Äƒ GraficÄƒ PWA Kiosk & Bio-Schedule Dashboard (web/index.html):**
  - **PWA Nativ cu Suport WebAPK:** Manifest complet cu `display: standalone`, `theme_color: #080c14`, pictograme fizice rasterizate PNG (`icon-192.png`, `icon-512.png` cu masturi maskable/any) È™i vectoriale SVG. Service Worker non-blocant (`/sw.js`) cu precaching È™i cache passthrough pentru API-uri.
  - **Buton Instalare & AsistenÈ›Äƒ Kiosk:** Buton tactil `ðŸ“² InstaleazÄƒ App` integrat cu `beforeinstallprompt` È™i dialog modal detaliat pentru adÄƒugarea ca scurtÄƒturÄƒ/aplicaÈ›ie nativÄƒ pe tablete Android/Chrome via `chrome://flags` secure origins.
  - **Layout Kiosk Responsive pe 2 Coloane:**
    * Col 1: Cazan Gaz (indicator erou flacÄƒrÄƒ ON/OFF, comutator AUTOMATIC / MANUAL, comenzi), Smart Bio-Schedule (comutatoare master, Pre-Heat anticipativ, Geam deschis), Card AncorÄƒ CircadianÄƒ activÄƒ (`ðŸŒ™ Sleep`, `ðŸŒ… Wake`, `ðŸ’¼ Home Office`, `ðŸ¡ Home`), Selector rapid Preset-uri chips (`standard`, `eco`, `wfh`, `custom1`, `custom2`), Modal salvare preset NVS È™i Editor ancore L-V vs Weekend.
    * Col 2: Card Zone Termice PardosealÄƒ (Dormitor, Copil, Baie mare cu steppere `+`/`-`, È›inte bio, stare valve NC), Card Telemetrie Senzori Xiaomi BLE cu badge-uri dBm (`EXCELENT`, `BUN`, `MARGINAL`, `SLAB`), Card DiagnozÄƒ Sistem (Heap RAM, Uptime, Wi-Fi RSSI, Flash OTA liber) È™i linkuri de navigare rapidÄƒ cÄƒtre Telnet Console È™i Setup Passport.
- **Smart Bio-Schedule, FereastrÄƒ DeschisÄƒ & Pre-Heat Anticipativ (Task 26 - Finalizat & Extins cu Presets):**
  - **4 Ancore Naturale de ViaÈ›Äƒ Customizabile:** Modificate ancorele circadiene: `Sleep`, `Wake`, `Away`, `Home` pentru a fi complet customizabile atÃ¢t la nivel de orÄƒ (`HH:MM`), cÃ¢t È™i temperaturÄƒ per zonÄƒ (`z1`, `z2`, `z3`).
  - **Catalog Preset-uri & Salvare NVS (Cerere Utilizator):**
    * Preset-uri din fabricÄƒ (read-only): `default` (Standard/Confort), `eco` (VacanÈ›Äƒ/18.0Â°C), `wfh` (Work From Home / 22.5Â°C).
    * Sloturi salvabile Ã®n NVS: `custom1`, `custom2` (salvare orar curent cu etichetÄƒ customizabilÄƒ, stocare Ã®n namespace `"presets"`).
    * Endpoint-uri REST dedicate: `GET /api/schedule/presets`, `POST /api/schedule/preset/apply`, `POST /api/schedule/preset/save`, `POST /api/schedule/reset`, `POST /api/schedule/anchor`.
    * Comenzi consolÄƒ adÄƒugate: `SCHEDULE RESET`, `SCHEDULE PRESET <id>`.
    * CLI extins (`esp_cli.py schedule ...`) cu timeout robust de 5.0s.
    * SuitÄƒ de teste SWE.5 extinsÄƒ la 17 teste (adÄƒugate TC16 È™i TC17) cu 100% PASS.
    * **Decuplare UI:** ModificÄƒrile vizuale pe `web/index.html` È™i `web/setup.html` au fost amÃ¢nate deliberat pentru urmÄƒtorul prompt, conform instrucÈ›iunilor utilizatorului.
  - **Open Window Smart Pause Guard:** Istoric circular de pantÄƒ termicÄƒ. ScÄƒdere >1.2Â°C Ã®n <5 min forÈ›eazÄƒ valva Ã®n PAUZÄ‚ timp de 30 min pentru a elimina risipa de gaz la aerisire.
  - **Predictive Pre-Heat:** Calcul dinamic al inerÈ›iei È™apei cu factor meteo $k_{weather}$ de la Open-Meteo, anticipÃ¢nd pornirea cÄƒldurii cu 30â€“120 minute Ã®nainte de trezire sau sosire acasÄƒ.
  - **NVS Anti-Wear Flash:** StructurÄƒ binarÄƒ stocatÄƒ Ã®n namespace-ul `"sched"` È™i `"presets"` cu comparaÈ›ie `memcmp()` Ã®nainte de scriere.
  - **ConsolÄƒ CuratÄƒ & Zero Spam Meteo:** Eliminat logurile repetate de vreme la 15 min; actualizÄƒrile periodice sunt redirecÈ›ionate pe Serial UART.
  - **Non-blocking Socket Timeout:** Setat timeout de 1500ms pe conectarea HTTP Open-Meteo pentru a preveni orice stall Ã®n `loop()`.
- **Eliminare Word Wrap pe Mobil, Scalare Font (`A-`/`A+`) & Watchdog Feed la OTA (Task 25):**
  - **Identificare Root-Cause:** Clasa CSS `.term-container` impunea `white-space: pre-wrap; word-break: break-all;`. Pe ecrane de telefon (<450px), liniile tabelelor de diagnostic È™i `STATUS` (de 70-100 caractere) erau rupte arbitrar la mijlocul cuvintelor pe 2-3 rÃ¢nduri, fÄƒcÃ¢nd citirea impracticabilÄƒ.
  - **Formatare Monospace RigidÄƒ (No-Wrap):** Setat `white-space: pre; word-break: normal;` Ã®n `.term-container` cu `overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`. Tabelele Ã®È™i menÈ›in alinierea perfectÄƒ pe coloane, permiÈ›Ã¢nd scroll orizontal fluid cu gesturi tactile pe telefon.
  - **Comutator Interactiv de Word Wrap (OpÈ›ional, Memorat Ã®n LocalStorage):** AdÄƒugat checkbox Ã®n footer: `<input type="checkbox" id="wrapToggle"> Wrap`. Utilizatorul poate activa manual ruperea rÃ¢ndurilor (`.wrap-enabled`) dacÄƒ doreÈ™te, starea fiind salvatÄƒ persistent Ã®n `localStorage` (`termWrap`).
  - **Scalare DinamicÄƒ a Fontului Terminalului (`A-` / `A+`):** Butoane tactile `A-` È™i `A+` Ã®n footer pentru ajustare finÄƒ (0.52rem - 1.20rem). Pe ecrane mobile (<640px), fontul implicit coboarÄƒ automat la `0.70rem` pentru o densitate crescutÄƒ a datelor. Dimensiunea aleasÄƒ este salvatÄƒ persistent Ã®n `localStorage` (`termFontSize`).
  - **Design Responsive Mobile (`@media (max-width: 640px)`):** Compactate marginile, padding-ul, butoanele rapide (`qbtn`) È™i bara de input pentru a maximiza suprafaÈ›a utilÄƒ pe smartphone.
  - **Watchdog Feed la ÃŽncÄƒrcare OTA (`main.cpp`):** AtaÈ™at `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` Ã®nainte de `httpUpdater.setup()`, resetÃ¢nd periodic Task Watchdog Timer-ul pe fiecare pachet HTTP de firmware scris Ã®n flash È™i eliminÃ¢nd complet riscul de watchdog reboot la actualizÄƒri Web OTA.
- **Remediere Scurgere Memorie Heap (`DTC-402`), NimBLE `setMaxResults(0)` & Auto-Clear Histerezis (Task 24):**
  - **Identificare Root-Cause:** La 4h de uptime, heap-ul liber scÄƒzuse de la 127 KB la 51 KB declanÈ™Ã¢nd `DTC-402`. `NimBLEScan` acumula pe heap (`m_maxResults = 0xFF`) toate pachetele BLE de la telefoane/ceasuri cu adrese MAC rotative (MAC randomization) fÄƒrÄƒ a le elibera, scanarea fiind continuÄƒ.
  - **Rezolvare NimBLE:** Apelat `_pBLEScan->setMaxResults(0)` È™i `_pBLEScan->clearResults()`. NimBLE executÄƒ callback-ul `onResult` È™i elibereazÄƒ instantaneu dispozitivele (`pScan->erase()`), stabilizÃ¢nd memoria ca fiind 100% constantÄƒ Ã®n timp (zero leak).
  - **Auto-Clear Histerezis:** `DTC-402` se stinge automat cÃ¢nd heap-ul depÄƒÈ™eÈ™te 75 KB; `DTC-401` se stinge cÃ¢nd RSSI Wi-Fi depÄƒÈ™eÈ™te -78 dBm. Scorul de sÄƒnÄƒtate a revenit la 100% [EXCELLENT], Free Heap stabil la ~125 KB.
- **Aliniere TabelarÄƒ pe 2 Coloane, Categorii A-E, CurÄƒÈ›are POST/INIT & Link TelinkMiFlasher (Task 23):**
  - **Tabel Aliniat pe 2 Coloane cu Litere A-E & Aerisire cu Newlines:** Categoriile din `STATUS` sunt prefixate alfabetic (`[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`), separate prin linii libere Ã®ntre secÈ›iuni È™i Ã®ntre cardurile camerelor.
  - **POST 100% Hardware & Local:** Eliminat `Real-Time Clock` din tabelul POST (serviciu de reÈ›ea); POST raporteazÄƒ exclusiv cele 8 verificÄƒri locale definitive (Network, RAM, Actuators, Pump, Bluetooth, Boiler RF, Watchdog, Web API).
  - **Raportare AsincronÄƒ UnificatÄƒ sub `[INIT]` cu Asterisc:** Sarcinile de fundal sunt declarate sub `[INIT] Background Tasks Dispatched:`, iar evenimentele ulterioare adoptÄƒ formatul `[INIT]   * Outdoor Weather: Synchronized...`, `[INIT]   * Real-Time Clock: Synchronized...`, `[INIT]   * Sensor Discovery: Acquired (X/Y)...` È™i `[INIT]   * Sensor Discovery: All X room sensors acquired in Ys!`.
  - **Link Configurare Senzori BLE Xiaomi:** Integrat hyperlink cÄƒtre instrumentul web oficial TelinkMiFlasher (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) Ã®n Web Console (`/console`), Setup Wizard (`/setup`), Dashboard (`/`) È™i manualul de comenzi (`HELP`) pentru reglarea puterii de transmisie RF (Tx Power).
  - **Reformatare ExplicitÄƒ:** `Sensor Status: Timeout` (radio) È™i `Loop Status: Sensor Timeout (Failsafe OFF)` (hidraulic).
  - **Mapare Culori Case-Insensitive `/gi` Ã®n Consola Web:** Toate stÄƒrile de dupÄƒ `:` sunt colorate viu corespunzÄƒtor, cu suport pentru etichetele `[A-E. ...]`. `Open-Meteo` este protejat de regex via negative lookahead.
  - **Paced Socket Streaming (`tools/ota_upload.py`):** Transfer cadenÈ›at cu pauze de 12ms/pachet (~110 KB/s), rezolvÃ¢nd definitiv blocajele TCP È™i permiÈ›Ã¢nd scrierea stabilÄƒ Ã®n flash.
  - **Watchdog Extins:** Timeout TWDT setat la 90s.
- **Hardware Telemetry, Per-Zone Telemetry Cards & Console Color Tuning (Task 22):**
  - **Telemetrie Hardware:** AdÄƒugat `[CPU]` (Model ESP32-D0WD-V3, 2 Cores @ 240 MHz, Rev 3, Die Temp via `temperatureRead()`, Uptime), `[MEMORY]` (RAM liber/total, Watermark minim de la boot, Cel mai mare bloc contiguu), `[FLASH]` (NVS Active, Flash 4MB @ 40MHz, Firmware size È™i marjÄƒ liberÄƒ OTA).
  - **Carduri Zonale pe 4 Linii:** Redenumit Ã®n `[HEATING ZONES & ROOM TELEMETRY]`. Fiecare zonÄƒ afiÈ™eazÄƒ toate datele (valvÄƒ, temperaturÄƒ, umiditate, status, senzor BLE, MAC, baterie, semnal cu min/max, pachete, vechime) structurat fÄƒrÄƒ aglomerare orizontalÄƒ.
  - **Eliminare Tabel `[SENSORS]`:** Tabelul separat a fost complet eliminat din `STATUS`.
  - **Simbolul Grad Celsius (`Â°C`):** Folosit simbolul `Â°C` nativ UTF-8 peste tot, cu `charset=utf-8` Ã®n HTTP headers È™i `esp_cli.py`.
  - **ConsolÄƒ Web StilizatÄƒ:** Eticheta `[WEATHER]`, anotimpul (`Autumn`), temperaturile `Â°C` È™i etichetele hardware (`[CPU]`, `[MEMORY]`, `[FLASH]`) sunt evidenÈ›iate Ã®n culori vii.
- **Acronym Elimination, Unified Mapping & RTC Resilience (Task 21):**
  - **Eliminare Acronime Tehnice:** ÃŽnlocuit complet `GPA` (`Secondary Pump on Relay 4`), `TWDT` (`Hardware Watchdog Armed: 60s timeout`), `NC Polarity` (`Normally Closed [NC], User-Configured in NVS`), `NimBLE Active` (`Bluetooth Receiver active, listening`).
  - **Timp Real & RezilienÈ›Äƒ Offline FÄƒrÄƒ Wi-Fi:** AdÄƒugat `Boot Time: YYYY-MM-DD HH:MM:SS` Ã®n banner; anunÈ› explicit la sincronizarea ceasului NTP; documentat comportamentul timer-ului RTC hardware intern pe ESP32 (menÈ›ine ceasul chiar dacÄƒ Wi-Fi este indisponibil 2 zile; controlul termic este 100% autonom local).
  - **Integrare Servicii Ã®n POST:** `Real-Time Clock`, `Outdoor Weather` È™i `Sensor Boot Warmup` monitorizate direct Ã®n tabelul Power-On Self-Test.
  - **Unificare Tabel Zone-Senzori (`STATUS`):** IntegratÄƒ maparea completÄƒ a senzorului (Nume, MAC, Baterie %, Semnal dBm + Badge) direct Ã®n fiecare rÃ¢nd din tabelul `[ZONES] Heating Loops & Sensor Mapping`, eliminÃ¢nd confuzia dintre denumirea senzorului È™i a camerei.
  - **EvidenÈ›iere VizualÄƒ Web Console:** Stilizare automatÄƒ cuvinte-cheie (`OFF`, `ON`, `IDLE`, `DEMAND`, `AUTOMATIC`, `MANUAL`, `WARMUP`, `PASS`, `FAIL`) È™i link badges.
- **Boot Sensor Warmup Grace Period & Tabular Status (Task 20):**
  - **Grace Period la Boot (Max 5 min / 300s):** EliminatÄƒ alarma falsÄƒ instantanee de timeout la pornirea la rece; starea dedicatÄƒ `ZONE_WARMUP` aÈ™teaptÄƒ balizele BLE (30â€“60s) ale termometrelor asociate.
  - **IeÈ™ire AnticipatÄƒ la Sincronizare CompletÄƒ:** CÃ¢nd toÈ›i senzorii asociaÈ›i au fost detectaÈ›i (ex. 3/3 Ã®n 30â€“60s), warmup-ul se finalizeazÄƒ imediat (`[SCANNING COMPLETE - ALL ACQUIRED]`) È™i buclele trec Ã®n modul operaÈ›ional fÄƒrÄƒ Ã®ntÃ¢rziere.
  - **Decuplare IzolatÄƒ & Ridicare DTC-101:** DacÄƒ un senzor nu raporteazÄƒ Ã®n 5 minute, se ridicÄƒ codul `DTC-101`, se izoleazÄƒ doar zona respectivÄƒ, iar sistemul porneÈ™te Ã®ncÄƒlzirea pe camerele funcÈ›ionale.
  - **Tabel Aliniat Bucle & Senzori (`STATUS` & `BLE`):** Coloane fixe cu aliniere milimetricÄƒ pentru `ID`, `Zone Name`, `Current Temp`, `Target Temp`, `Status`, `Actuator Valve (Relay)` È™i registrul termometrelor cu semnal Min/Cur/Max dBm, pachete È™i vÃ¢rstÄƒ.
  - **EtichetÄƒ ClarÄƒ ProtecÈ›ii (`[PROTECTIONS]`):** `Sensor Offline Failsafe: 10 min (all zones) | Pump Anti-Seize: Weekly (Ran: ... | Next in ...)`.
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
  - Dedicated CLI tool [`tools/esp_cli.py`](file:///d:/_git/smart-home-heating/tools/esp_cli.py).
  - Automated System Verification Test Suite ([`tools/test_system.py`](file:///d:/_git/smart-home-heating/tools/test_system.py) / `python tools/esp_cli.py test`): 14 end-to-end integration test cases verifying ASPICE SWE.5 with 100.0% pass rate.
  - Live execution report: [`docs/system_test_report.md`](file:///d:/_git/smart-home-heating/docs/system_test_report.md).
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
  - Sincronizare NTP timp (Europe/Bucharest) È™i vreme prin Open-Meteo REST API (`TimeAndWeatherService`).
- **Hydronic Safety Hardening & Circulation Pump Control:**
  - Automated secondary circulation pump controller on Relay 4 (`GPIO26`) with interlock safety and 180s pump overrun (with keeper zone).
  - Configurable actuator polarity: NC (Normally Closed) vs NO (Normally Open) via NVS (`act_nc`).
  - Emergency Frost Protection Guard (<12Â°C) forcing heating demand even in manual/off mode.
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

