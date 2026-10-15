# Plan de Lucru Activ – Smart Home Heating

Acest document este planul de referință pentru implementarea controlerului autonom multi-zonă ESP32, integrat cu senzorii Xiaomi BLE (BTHome v2), emițătorul RF pentru centrală și interfața LittleFS cu schemă interactivă SVG.

---

## 🧠 Sinteză Arhitectură & Decizii Tehnice

1. **Hardware & Control Relee (CONFIRMAT & RECEPȚIONAT FIZIC):**
   - **Placă recepționată:** Modul **ESP32_Relay X4 V1.1 (AC/DC powered)** cu alimentare hibridă (AC 90-250V sau DC 7-30V / 5V), relee Songle 10A 250VAC, optocuploare și conectori expansiune 10x2.
   - **Microcontroller:** Cip integrat **DOIT WIFI MODEL ESP32-32E N4 (ESP32-WROOM-32E)**, Dual-Core 240MHz, 4MB Flash, Wi-Fi + Bluetooth BLE integrat nativ.
   - **Alocare Pini Placă Relee (Conform schemei hardware):**
     - **Releul 1:** `GPIO32` – Zona Living + Bucătărie (robineții 1, 2, 3 în paralel).
     - **Releul 2:** `GPIO33` – Zona Camera Copilului (robineții 6, 7 în paralel).
     - **Releul 3:** `GPIO25` – Zona Dormitor Matrimonial (robineții 4, 5 în paralel).
     - **Releul 4:** `GPIO26` – Rezervă liberă.
     - **Status LED:** `GPIO23`.
     - **Emițător Radio 433.92 MHz:** Conectare pin DATA pe pin liber expus pe headerul 10x2 (ex: `GPIO19`).
     - **Header Programare:** Pini dedicați `5V`, `TX`, `RX`, `GND`, `GND`, `GPIO0` cu buton fizic `IO0` pe PCB.
   - **Bypass hidraulic & Protecție pompă:** Cele 2 băi au calorifere portprosop pe perete (fără circuite în pardoseală), cu capete termostatice mecanice deschise permanent. Asigură circulație continuă și răcire chiar dacă toate cele 3 zone de pardoseală sunt închise.

2. **Comandă Radio Centrală Termică (CONFIRMAT & IDENTIFICAT):**
   - Protocol OOK/ASK 433.92 MHz reversat complet din capturile de analizor logic (`control_RF_Centrala.ino` cu 410 tranziții, preambul 65 ms, tabele `ondurations` și `offdurations`).
   - Hardware emisie/recepție: 2 seturi **SYN115 (TX) + SYN480R (RX)** comandate (cip PLL cu quartz + receptor superheterodin -107 dBm).
   - Emițătorul SYN115 se conectează direct pe un pin GPIO al ESP32 (fără releu mecanic).

3. **Temperatură Exterioară & Compensare Climatică (HIBRID):**
   - **Canal Primar Radio:** Stație meteo **Tronic 4-LD7396-2** (IAN `515492_2510`), 433.92 MHz recepționată via receptorul SYN480R.
   - **Canal Secundar Fallback:** **Open-Meteo REST API** prin Wi-Fi (testat și validat: 13.0°C Google/Open-Meteo vs 13.3°C senzorul fizic de Nord).

4. **Interfață Utilizator & Panou de Perete (Dashboard Kiosk Dedicat):**
   - **Hardware Panou Perete:** Achiziționată tableta **Samsung Galaxy Tab A9 4G LTE (SM-X115)** (8.7" PLS LCD anti-burn-in, procesor MediaTek Helio G99 6nm, modem 4G LTE backup, funcție nativă One UI *Protect Battery* 80/85% pentru alimentare 24/7).
   - **Aplicație Kiosk:** Fully Kiosk Browser cu blocare PIN anti-copii, trezire automată la apropiere (Motion Wake via cameră frontală).
   - **Dashboard Integrat:** PWA / HTML5 responsiv găzduit local în ecosistem (schemă SVG din `input/Odaia.html`, carduri Tesla, Încălzire, Acvariu, Meteo).

---

## 🏛️ Viziune Comercială, Scalabilitate (10 → 10.000 Useri) & Echipa Virtuală de Experți

Pentru a asigura tranziția de la un sistem home-made la un **produs comercial scalabil de clasă industrială/automotive**, dezvoltarea este structurată pe roluri experte bine definite, conformitate de proces și decuplare arhitecturală strictă.

### 👥 1. Matricea Echipei de Lucru Experte (Roluri & Responsabilități)
* **CEO (Chief Executive Officer):** Direcție strategică, viziune de piață, prioritizare funcționalități și validare direcție comercială.
* **CTO (Chief Technology Officer):** Arhitectură tehnică integrată, alegere stack tehnologic, audit performanță și scalabilitate.
* **CFO (Chief Financial Officer):** Analiză costuri BOM per unitate (BOM < 45 EUR), strategie de preț pachet hardware + software, optimizare costuri cloud.
* **Expert Plan Master:** Păstrarea riguroasă a foii de parcurs, trasabilitate cerințe $\leftrightarrow$ implementare, sincronizare continuă a documentației (`docs/plan.md`, `.agent/memory-bank/`).
* **Expert System Engineer:** Inginerie de sistem la standarde automotive (ASPICE SYS.1–SYS.5), mapare subsisteme (E/E, RF, BLE, Wi-Fi, HVAC), alocare resurse și definire interfețe ICD (Interface Control Documents).
* **Expert SW Dev (Embedded C++ / IoT):** Firmware determinist, fără memory leaks, non-blocking (FSM), conformitate cu reguli de siguranță (Watchdog TWDT, limite NVS, interblocări hardware).
* **Expert HW & Instalator Electrician:** Siguranță instalație 230V, capacitate de rupere relee (curenți de anclanșare sarcini inductive), selecție actuatoare NC/NO, scheme hidraulice și bypass.
* **Expert User Experience (UX):** Ergonomie, navigare intuitivă, simplificarea configurării pentru utilizatorul non-tehnic (zero efort de învățare).
* **Expert UI/UX (Design & Visuals):** Interfață modernă, estetică premium (stil Tesla HVAC / Apple Home), micro-animații fluide, mod noapte/zi, lizibilitate Kiosk de la 2 metri.
* **Expert User Tester & End-User Tester:** Testare dinamică în condiții reale de viață (temperaturi resimțite de familie, feedback copii/soție, testare la limită).
* **Process ISO/CEE & Safety/Cybersecurity Expert:** Conformitate marcaj CE (compatibilitate electromagnetică EMC, directiva joasă tensiune LVD), standarde termostate (EN 60730), izolare socket-uri și securitate BLE/Wi-Fi.
* **Scaling Expert:** Arhitectură pregătită pentru saltul **10 $\rightarrow$ 100 $\rightarrow$ 1.000 $\rightarrow$ 10.000 utilizatori** (management de flotă, update-uri OTA sigure, agregare telemetrie în cloud Grafana/InfluxDB, multi-tenancy).
* **Expert Sales & Expert Marketing:** Prezentare atractivă către clienți, Digital Twin 3D / scanare apartament, propunere de valoare centrată pe economie la factura de gaz și confort maxim.

> [!IMPORTANT]
> **Diviziunea Muncii:**
> * **Agentul AI (Copilot / Antigravity):** Duce tot greul tehnic de documentație, specificații tehnice, cod C++, teste automate, trasabilitate și diagrame logice.
> * **Utilizatorul & Clientul:** Se concentrează exclusiv pe experiența de utilizare (UI/UX), utilitate practică, confortul real din casă și ușurința de instalare/onboarding.

---

### 🛡️ 2. Disciplina de Dezvoltare Decuplată (Zero-Risc & Viteza F5)
1. **ESP32 (Backend C++ Headless):**
   * Răspunde exclusiv de siguranță și expune API-ul REST JSON (`/api/...`).
   * Testarea logicii noi se face rapid în consolă (`tools/esp_cli.py`, `/console` sau teste automate).
2. **Frontend Decuplat Local (`tools/dev_server.py`):**
   * Dezvoltarea interfeței se face 100% local (`python tools/dev_server.py` -> `http://localhost:8080`).
   * Browserul local citește datele reale de la ESP32 (`192.168.1.20`) prin CORS.
   * Modifici HTML/CSS -> Salvezi -> Dai F5 în 50ms.
   * **Zero flash pe cip, zero riscuri de corupere flash, zero timp pierdut cu compilarea.**
   * Doar la milestone-uri finale, când interfața este aprobată de client, se rulează `tools/build_web.py`.

---

## ✅ Todo & Status Sarcini

- [x] **1. Specificație placă relee & alocare pini GPIO** (Finalizat 06.10.2026, Migrat la ESP32 pe 07.10.2026).
      *Rezultat:* Recepționată placa nouă de distribuție **ESP32-Relay X4 V1.1** cu modul **DOIT WiFi ESP32-32E N4 (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash)**. Codul a fost curățat 100% pentru ESP32 nativ (fără ramuri `#ifdef ESP8266`). Documentația completă a hardware-ului, pinout-ului și procedurii de programare se găsește în [`docs/hardware_esp32_relay.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/hardware_esp32_relay.md). Pini relee alocați:
        - Releu 1: `GPIO32` (Living + Bucătărie + Hol)
        - Releu 2: `GPIO33` (Camera Copilului)
        - Releu 3: `GPIO25` (Dormitor Matrimonial)
        - Releu 4: `GPIO26` (Rezervă / Pompă / Bypass)
        - LED Stare: `GPIO23` | Emisie RF 433: `GPIO19`

- [x] **2. Identificare & recuperare protocol radio centrală** (Finalizat 06.10.2026, Cablat & Confirmat 07.10.2026).
      *Rezultat:* Recuperat codul funcțional `control_RF_Centrala.ino`, analiza Excel și capturile Sigrok/Saleae.
      *Validare Hardware Pinout & Cablaj (07.10.2026):*
        - **Fir Negru:** GND -> Conectat la pinul **G** (GND) ESP8266.
        - **Fir Galben:** VCC (+3.3V) -> Conectat la pinul **3V** (3.3V) ESP8266.
        - **Fir Verde:** DATA (OOK) -> Conectat la pinul **D1** (GPIO5) ESP8266.
        - **Pin 2 modul RF (nelipit):** Punte internă cu VCC (Galben).

- [x] **3. Implementare driver software `BoilerRadioTrigger` pentru ESP8266 & Validare Centrală** (Finalizat 07.10.2026).
      *Rezultat:* Driverul C++ ([`include/BoilerRadioTrigger.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BoilerRadioTrigger.h), [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp)) a fost compilat și încărcat pe ESP8266 (`COM6`). Generează trenul OOK de 409 tranziții pe pinul D1 (GPIO5) cu bit-banging hardware.
      *Validare Live Centrală:* Testat pe viu cu centrala termică prin interfața web — centrala răspunde și pornește/oprește flacăra conform comenzilor radio!

- [x] **4. Integrare în Portalul Centralizat Galaxy-A6 & Acces la Distanță Tailscale** (Finalizat 07.10.2026).
      *Rezultat:* Integrat controllerul de încălzire în Smart Services Portal (`http://galaxy-a6/` / `http://192.168.1.28/`):
        - Adăugat card dedicat **Smart Heating** (cu flacără 🔥, status live polling și link direct).
        - Configurat Nginx reverse proxy pe Galaxy-A6 cu rută dedicată `/heating/` -> `http://192.168.1.18/`.
        - Permite controlul centralei atât local în LAN cât și de la distanță din afara casei prin rețeaua privată sigură **Tailscale** (`http://100.83.135.74/heating/`).

- [x] **5. Arhitectură telemetrie senzori BLE** (Clarificat 07.10.2026).
      *Rezultat:* ESP8266 nu deține radio Bluetooth; scanarea senzorilor Xiaomi LYWSD03MMC (BTHome v2) este asigurată 24/7 de gateway-ul dedicat Samsung Galaxy A6 (`smart-home-ble-gateway`), care publică datele prin brokerul MQTT local (Mosquitto).

- [x] **6. Integrare client MQTT pe ESP8266** (Finalizat & Validat Live 07.10.2026).
      *Rezultat:* Modulul decuplat [`include/MqttTelemetryClient.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/MqttTelemetryClient.h) și [`src/MqttTelemetryClient.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/MqttTelemetryClient.cpp) a fost compilat folosind bibliotecile locale `PubSubClient` și `ArduinoJson` v7. Brokerul Mosquitto de pe Galaxy-A6 a fost setat persistent să asculte pe `0.0.0.0:1883`. ESP8266 ingerează live în memorie telemetria termometrelor Xiaomi (Living Room, Kids Room, Bathroom).

- [x] **7. Controler termic multi-zonă de bază & decuplat (`ThermalZoneManager`)** (Finalizat & Validat Live 07.10.2026).
      *Rezultat:* Implementat modulul decuplat [`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h) și [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp):
        - Gestionează cele 3 zone termice de pardoseală (Living, Copil, Matrimonial) pe pinii de releu GPIO32, GPIO33, GPIO25 (Releu 4 rezervă pe GPIO26).
        - Reglează temperatura prin buclă de histerezis (±0.3°C), interlock pentru deschiderea actuatorilor NC, pump overrun și anti-short cycling.
        - Permite mod AUTOMAT / MANUAL, reglaj fin temperatură țintă din UI și asociere dinamică a senzorilor prin API REST (`/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/zones/sensor`, `/api/mode`).

- [x] **7.1. Migrare Arhitecturală 100% Nativă pe Placa Nouă ESP32 (ESP32-Relay X4 V1.1)** (Finalizat & Validat Live 07.10.2026).
      *Rezultat:*
        - Identificat cipul hardware fizic: **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash SPI, 320KB RAM, MAC `20:50:0d:1a:72:14`).
        - Curățat complet codebase-ul de branching legacy `#ifdef ESP8266` (trecut 100% pe API-uri native ESP32: `<WiFi.h>`, `<WebServer.h>`, `<HTTPUpdateServer.h>`, registre GPIO native).
        - Toate comentariile, logurile Serial, interfața Web Dashboard și răspunsurile API REST au fost traduse în **100% limba engleză** (localizarea în limba română urmând a fi încărcată din JSON extern).
        - Flash inițial efectuat via USB-UART Passthrough NodeMCU la 460800 baud.
        - Placa rulează autonom pe Wi-Fi (`SBC_WiFi`) la **`192.168.1.20`** cu heap liber 235 KB (față de doar 28 KB pe vechiul ESP8266!).
        - Firmware-ul actualizat în limba engleză a fost flashuit și verificat wireless prin **Web OTA** (`http://192.168.1.20/update`) în doar 6.55 secunde!
        - Proxy Nginx Nexus de pe Galaxy-A6 redirecționat către noua adresă `192.168.1.20`.

- [ ] **8. Generare schemă interactivă SVG din `Odaia.html` & Dashboard LittleFS** (Planificat).
      *Plan:* Extragerea poligoanelor celor 3 zone din `Odaia.html` și asamblarea paginii web responsive.

- [ ] **9. Decodor stație meteo Tronic 433MHz & client Open-Meteo API** (Planificat).
      *Plan:* Integrarea recepției radio via SYN480R pe GPIO interrupt și fallback HTTP Open-Meteo.

- [x] **10. Scanner BLE Nativ pe ESP32 pentru Senzorii Xiaomi (BTHome v2 & PVVX)** (Finalizat & Validat Live pe Hardware 08.10.2026).
      *Rezultat:* Deoarece placa recepționată conține modulul **ESP32-WROOM-32E cu radio Bluetooth integrat**, s-a implementat și activat scannerul pasiv nativ BLE bazat pe biblioteca optimizată `NimBLE-Arduino` (vendored local în `lib/` pentru build 100% offline):
        - **Modul dedicat `BleSensorScanner`:** Implementat în [`include/BleSensorScanner.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BleSensorScanner.h) și [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp).
        - **Parsare automată protocoale:** Decodifică atât pachete **BTHome v2** (Service UUID `0xFCD2`), cât și formatul custom **PVVX / ATC** (Service UUID `0x181A`).
        - **Amprentă memorie optimizată:** Firmware-ul complet (Wi-Fi + WebServer + WebOTA + Telnet + MQTT + NimBLE) ocupă doar **1.08 MB (1.128.681 bytes)** din partiția de 1.92 MB (min_spiffs) și doar 65 KB RAM (19.8%), permițând încărcarea instantanee prin Web OTA (`/update`) în 15 secunde fără fir!
        - **Redundanță & Fallback hibrid:** Senzorii detectați direct prin BLE (Living Room cu RSSI -64 dBm, Kids Room cu RSSI -67 dBm) sunt actualizați instantaneu pe cip. Dacă un senzor se află după pereți groși sau la distanță (ex. Baie), pachetele primite prin brokerul MQTT de pe Galaxy A6 asigură continuitatea fără întreruperi.
        - **Interfețe & Diagnoză:** Adăugat endpoint REST `/api/ble/sensors`, comandă dedicată `BLE` / `SENSORS` în consola Telnet & Web, plus suport în utilitarul CLI (`python tools/esp_cli.py ble`).

- [x] **11. Consolă Virtuală Serială Bidirecțională prin Wi-Fi (Telnet TCP:23 & Web Console /console)** (Finalizat & Validat Live 07.10.2026).
      *Rezultat:* Eliminată complet dependența de cabluri USB sau operare „legată la ochi”:
        1. **Server Telnet TCP pe portul 23 (`WiFiServer(23)`):** Implementat în [`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h) și [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp). Suportă conexiuni remote din PlatformIO (`monitor_port = tcp://192.168.1.20:23`), PuTTY sau scripturi CLI, cu prompt interactiv `> ` și istoric.
        2. **Consolă Web 2-Way la `/console`:** Interfață web modernă (dark theme) integrată în Web Server, cu terminal live (ring buffer 8KB), streaming sincronizat fără pierderi, auto-scroll, istoric de comenzi cu săgețile tastaturii (Up/Down) și butoane rapide de acțiune (`STATUS`, `ON`, `OFF`, `AUTO`, `MANUAL`, `HELP`). Compatibilă 100% atât pe acces direct LAN (`http://192.168.1.20/console`) cât și prin reverse proxy Galaxy A6 (`http://galaxy-a6/heating/console`).
        3. **Control Bidirecțional Verificat Live:** Comenzile transmise din Telnet sau din Web Console (`STATUS`, `ON`, `OFF`, `SET <zone> <temp>`, `AUTO`, `MANUAL`, `HELP`) sunt executate instantaneu pe cip, iar răspunsurile și toate logurile de sistem (RF, telemetrie MQTT, relee zone) sunt difuzate sincron către Web, Telnet și Serial USB.
        4. **Flashuit & Validat Live via Web OTA:** Compilat și încărcat prin OTA în 6.43 secunde. Răspunsurile la `STATUS` și `HELP` testate cu succes pe ambele canale.

- [x] **12. Diagnostic Engine & Service Mode la Distanță** (Finalizat & Validat Live 08.10.2026).
      *Rezultat:* Sistem integrat de asistență tehnică la distanță pentru clienți și mentenanță flotă fără deplasare:
        1. **Documentație Arhitectură Cloud & Service Mode:** Redactat [`docs/remote_fleet_and_service_mode.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/remote_fleet_and_service_mode.md) detaliind tunelul securizat Outbound TLS (port 443/8883) fără port-forwarding sau IP public.
        2. **Modul C++ `DiagnosticEngine`:** Implementat în [`include/DiagnosticEngine.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/DiagnosticEngine.h) și [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp).
        3. **Coduri Standardizate DTC (Diagnostic Trouble Codes):** `DTC-101` (Timeout senzor), `DTC-102` (Baterie CR2032 sub 20%), `DTC-103` (Temperatură aberantă), `DTC-201` (Ciclare excesivă relee), `DTC-202` (Actuator/robinet blocat închis), `DTC-401` (Wi-Fi degradat), `DTC-402` (Fragmentare RAM).
        4. **Suită Integrată Self-Test & Health Score:** Endpoint `/api/diag/selftest` și comanda `DIAG SELFTEST` testează în 200 ms toate cele 6 subsisteme (Wi-Fi, Heap, BLE, MQTT, Zone, RF 433) și returnează un Health Score (100% [EXCELLENT]).
        5. **Comutare Service Mode:** Permite inginerului să preia controlul de la distanță (`/api/diag/service?enable=true` sau `DIAG SERVICE ON/OFF`), să suspende temporar deciziile automate, să acționeze manual actuatoarele în timpul convorbirii cu clientul și să diagnosticheze instant defecțiunea.
        6. **Integrare CLI & Consola:** Adăugate comenzile `DIAG`, `DIAG SELFTEST`, `DIAG CLEAR`, `DIAG SERVICE ON/OFF` în Telnet/Web Console și subcomenzile `diag` și `selftest` în utilitarul `tools/esp_cli.py`.

- [x] **13. Asistent de Configurare la Client (Setup Wizard), Mapare Dinamică Senzori BLE, Pașaport Digital NVS & Sincronizare NTP/Vreme Wi-Fi** (Finalizat & Validat Live 09.10.2026).
      *Rezultat:* Dispozitivul funcționează complet ca un produs comercial "out of the box", fără MAC-uri sau setări hardcodate în cod:
        1. **Asistent Vizual de Instalare la `/setup` (`WebSetupPage.h`):** Pagină web modernă (dark theme) stocată în memoria flash PROGMEM (zero consum RAM), cu scanare live a spectrului BLE, asociere printr-un singur clic a senzorilor la zonele 1, 2, 3 și testare individuală relee.
        2. **Cartografiere Dinamică & Persistență NVS:** În `ThermalZoneManager`, configurările de zonă (nume, adrese MAC ale senzorilor, setpoints, histerezis, pini relee) se salvează în memoria flash non-volatilă (NVS prin `<Preferences.h>`) și rezistă la reporniri și pene de curent.
        3. **Pașaport Digital Dispozitiv (`DevicePassport`):** Generează automat număr serial unic pe baza MAC-ului cipului (ex. `SH-14721A0D5020`), memorează data și ora primei puneri în funcțiune la client, datele clientului, adresa fizică, coordonatele GPS și notițele tehnice ale instalatorului.
        4. **Ceas Sincronizat Wi-Fi NTP & Vreme Open-Meteo (`TimeAndWeatherService`):** Sincronizează automat timpul prin NTP (fus orar România Europe/Bucharest cu oră de vară/iarnă), calculează calendarul, anul, ziua din săptămână, sezonul (Heating Active vs Summer Standby) și interoghează periodic temperatura exterioară prin Open-Meteo REST API.
        5. **Integrare Utilitare & CLI:** Adăugate comenzile `PASSPORT`, `TIME`, `WEATHER` în Network Console și în utilitarul de terminal (`python tools/esp_cli.py passport` și `python tools/esp_cli.py time`).
        6. **Validat Live pe Hardware:** Firmware optimizat și compilat (1.26 MB), flashuit prin Web OTA în 14 secunde, validat cu succes la `http://192.168.1.20/setup`.

- [x] **14. Arhitectură Decuplată Headless (API-First Focus), Eliminare Latență UI & Utilitar CLI** (Finalizat & Validat Live 09.10.2026).
      *Rezultat:* Reorientare strategică: separarea fermă a logicii backend/microcontroller de detaliile instabile de interfață grafică:
        1. **Catalog API REST / JSON Consolidat:** Toate funcționalitățile sistemului (termostate, zone, cazan, telemetrie, diagnoză, pașaport, NTP) sunt expuse ca endpoint-uri REST rapide (`/api/boiler/status`, `/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/mode`, `/api/boiler/on`, `/api/boiler/off`, `/api/ble/sensors`, `/api/setup/passport`, `/api/setup/time`, `/api/diag/status`, `/api/diag/selftest`). Timp de răspuns măsurat sub **70–120 ms**.
        2. **Eliminare Lag & Reîncărcări Forțate:** Înlocuit mecanismul lent de `location.reload()` din UI cu actualizări optimiste instantanee în DOM (0 ms latență vizuală) și apeluri POST asincrone.
        3. **Rupere Buclă Recursivă & Non-Blocking Timeouts:** Eliminat apelul recursiv dintre scannerul BLE și clientul MQTT. Setat timeout de 1s pe socketul MQTT și 1.5s pe HTTP Open-Meteo pentru a garanta că bucla principală `loop()` nu este niciodată blocată.
        4. **Selectare Inteligentă Locație fără Coordonate GPS:** Căutare live cu auto-suggest prin Open-Meteo Geocoding pentru orice localitate din România, dropdown offline cu toate cele 41 de reședințe de județ și auto-detecție prin GPS/IP cu 1 click.
        5. **Normalizare Indecși Zone:** Endpoint-urile de zone acceptă nativ atât identificatori bazați pe 1 (`id=1..3`), cât și indecși bazați pe 0 (`id=0..2`).
- [x] **15. Failsafes de Siguranță, Hardening Firmware & Suport Hidraulic Universal pentru Instalații de Pardoseală** (Finalizat 09.10.2026).
      *Obiectiv:* Asigurarea compatibilității cu orice distribuitor de pardoseală existent (inclusiv case/duplexuri cu pompe secundare și grupuri de amestec GPA) și maximizarea siguranței fizice:
        1. **Control Pompă Secundară de Recirculare / Distribuitor via Releu 4 (`GPIO26`):**
           - Comandă automată pentru pompa de circulație a grupului de amestec (GPA) cu configurare NVS (`pump_en`).
           - *Interlock Pompă:* Pompa pornește DOAR DUPĂ ce cel puțin un actuator este deschis, prevenind cavitația și funcționarea împotriva circuitelor complet închise.
           - *Post-circulație Pompă (Overrun):* Pompa continuă să circule apa încă 3 minute după stingerea flăcării pentru disiparea căldurii reziduale (menținând deschisă ultima zonă activă - *keeper zone*).
           - *Exercițiu Anti-Blocare de Vară (Anti-Seize):* Rulare automată timp de 20 de secunde o dată pe săptămână în sezonul cald pentru prevenirea depunerilor de calcar/magnetită pe rotorul pompei. Validat live pe releu cu revenire automată la `IDLE [OFF]`.
        2. **Compatibilitate Actuatoare NC vs NO (Normally Closed / Normally Open):**
           - Setare configurabilă în NVS (`act_nc: bool`) pentru suport nativ atât pentru actuatoarele standard NC (230V aplicat = deschidere), cât și pentru actuatoarele NO (fără tensiune = deschis). Validat live prin API (`/api/setup/actuator-type`) și CLI.
        3. **Protecție Forțată la Îngheț (Frost Protection Guard < 12°C):**
           - Declanșare automată de urgență a căldurii dacă temperatura oricărei camere scade sub 12°C, chiar dacă sistemul este pe mod `OFF` sau `MANUAL`.
        4. **Izolare Avarie Senzor (`DTC-101`) & Decuplare Preventivă:**
           - Dacă o zonă nu primește telemetrie validă timp de peste 30 de minute, zona este oprită forțat pentru a proteja șapa și parchetul împotriva supraîncălzirii necontrolate, emițând codul `DTC-101`.
        5. **Hardware Task Watchdog Timer (ESP32 TWDT 30s):**
           - Activare watchdog hardware nativ ESP-IDF pe bucla principală `loop()`, garantând restart automat curat în caz de blocaj de rețea sau crash, calibrat la 30s pentru toleranță flash write.
        6. **Ghid & Arhitectură Hidraulică pentru Instalatori:**
           - Documentat complet în [`docs/underfloor_heating_hydraulics_and_failsafes.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/underfloor_heating_hydraulics_and_failsafes.md): grupuri de amestec GPA, termostat de contact la 55°C, supapă bypass diferențial și scheme de conexiune electrică 230V.

- [x] **16. Failsafe Anti-Spam & Interblocare de Siguranță Arzător Cazan (ASPICE V-Model SWE.1 - SWE.5)** (Finalizat & Validat Live pe Hardware 09.10.2026).
      *Rezultat:* Eliminarea completă a riscului de avarie hardware cazan (*Flame Lockout*), epuizare bobine vană de gaz și blocare CPU ESP32 cauzate de comenzi repetate abuzive (spam manual, scripturi externe, clicuri rapide Web/CLI):
        1. **SWE.1 Cerințe Software (FR-17 & NFR-1.6):** Specificat complet în [`docs/requirements_and_features.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/requirements_and_features.md).
           - *Filtru de Idempotență (FR-17.1):* Comenzi repetate identice (ON când este deja ON) tratate ca no-op instantaneu (0 ms, zero RF, `idempotent: true`).
           - *RF Cooldown (FR-17.2):* Minim 3 secunde liniște între trenuri de impulsuri RF.
           - *Interblocare Cazan (FR-17.3 & NFR-1.6):* Minim 60 secunde ardere continuă (Min Run Time) și minim 60 secunde pauză (Min Pause Time) chiar și în mod MANUAL/API.
           - *Răspuns Determinist (FR-17.4):* Răspuns HTTP `429 Too Many Requests` cu `waitSec` restant și detalii explicite în CLI.
           - *Detecție Avarie DTC-201 (FR-17.5):* Declanșare automată a codului `DTC-201` cu severitate WARNING la spam repetat (>3 încercări / 10s) și scăderea scorului de sănătate la 90%.
           - *Zero Blocaj CPU (FR-17.6):* Refuz în pre-validare fără `noInterrupts()`, TWDT protejat la 30s.
        2. **SWE.2 Design Arhitectural & Interfețe:** Modul `BoilerRadioTrigger` (metoda `canTransmit(targetState, &waitSec, &isIdempotent)` + `sendBoilerOn/Off(force)`), rutare Web Server cu cod 429, actualizare `ThermalZoneManager::setBoilerManual`, interceptare CLI.
        3. **SWE.3 Implementare Cod:** Modificare [`include/BoilerRadioTrigger.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BoilerRadioTrigger.h), [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp), [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) și [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).
        4. **SWE.4 / SWE.5 Verificare & Testare la Spam:**
           - Validat pe hardware (`192.168.1.20`) prin `python tools/esp_cli.py`:
             * Comanda `off` repetată returnează instantaneu: `[i] Centrala este deja in starea ceruta (OFF). Transmisia RF a fost omisa (idempotent no-op).` (FR-17.1 PASS).
             * Comanda `on` dată la sub 60s după oprire returnează HTTP 429 cu timp rămas: `[!] Comanda respinsa de siguranta (HTTP 429): Anti-short-cycling guard active. Timp minim ramas: 48 secunde.` (FR-17.3, FR-17.4 PASS).
             * 3 comenzi rapide spam ridică `DTC-201` în sistemul de diagnoză (`Health Score: 90%`, `DTC-201 WARNING: Rapid boiler toggling / spam detected`) (FR-17.5 PASS).
             * Opțiunea `--force` (`python tools/esp_cli.py on --force`) suprascrie interlock-ul în caz de urgență / intervenție service (PASS).
             * Curățare DTC via `/api/diag/clear` restabilește Scorul de Sănătate la 100% (PASS).

- [x] **17. Eliminare Bloatware MQTT, Curățare Consolă & Suită Automată de Testare pe PC (ASPICE SWE.5)** (Finalizat & Validat Live pe Hardware 09.10.2026).
      *Rezultat:* Sistemul a fost decuplat 100% de bridge-ul extern Galaxy-A6, transformându-se într-un produs comercial autonom complet:
        1. **Eliminare Bloatware MQTT:** Șterse `src/MqttTelemetryClient.cpp` și `include/MqttTelemetryClient.h`, eliminat flag-ul de compilare `-D MQTT_MAX_PACKET_SIZE=512`, unlinked `PubSubClient`. Economisit RAM și Flash, zero socket-uri externe deschise inutil.
        2. **Decuplare 100% Autonomă pe BLE Nativ:** Toți senzorii de cameră Xiaomi LYWSD03MMC sunt interceptați direct prin spectrul radio Bluetooth integrat în ESP32-WROOM-32E via `NimBLE-Arduino`.
        3. **Consolă Curată (Zero Spam Nesolicitat):** Eliminat afișarea automată a pachetelor BLE la fiecare recepție. Consola Telnet/Serial rămâne complet tăcută; telemetria camerelor este afișată exclusiv la cerere (`STATUS`, `BLE`, `SENSORS` sau interogare REST).
        4. **Suită Automată de Testare Sistem PC (`tools/test_system.py` & `esp_cli.py test`):**
           - Fără mock-uri sau endpoint-uri artificiale de test în ESP32: testele rulează de pe PC direct împotriva plăcii la `192.168.1.20`.
           - Execută automat cele 12 cazuri de testare de integrare (`SWE.5-TC01` - `SWE.5-TC12`) acoperind telemetria, pașaportul, BLE, self-testul, polaritatea actuatoarelor, pompa GPA, idempotența, garda la 60s, spam DTC-201, calibrarea setpoint-urilor, NTP și Network Console.
           - Rată de succes: **100.0% (12/12 PASS)** în 4.02 secunde. Raport generat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).

- [x] **18. Îmbunătățire Consolă, Reziliență Meteo, Anti-Seize pe Epoch & Timeout Senzori Configurabil** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Timestamp-uri reale NTP `[HH:MM:SS]` pe fiecare linie în consolă.
        - Open-Meteo socket timeout mărit la 5000ms cu 5s settling delay la boot (zero erori HTTP -11).
        - Exercițiu de vară anti-gripare bazat pe UNIX epoch calendaristic real (`time_t nowEpoch = time(nullptr)`) în NVS (zero porniri false la reboot).
        - Failsafe timeout senzori configurabil din REST și CLI (default 600s / 10 min, `FR-18`).

- [x] **19. Rafinare Completă Output Consolă, Eliminare Zgomot Boot & Categorisire Comenzi** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Pristine boot banner: 8 loguri interne redirecționate exclusiv pe UART hardware; titlul și tabelul POST sunt primele elemente afișate pe consola virtuală.
        - Eliminat dublajul de comenzi în consolă la `ON`/`OFF`, `AUTO`/`MANUAL` și `SET`.
        - Adăugat manual structurat de comenzi (`HELP` / `?`) pe 4 categorii funcționale.
        - Suport pentru comanda `CLEAR` / `CLS` (ANSI terminal reset).
        - Integrare indicatori calitate semnal (`Link Quality Badges`) în `STATUS` și `BLE`.

- [x] **20. Perioadă de Grație Warmup Senzori la Boot (Max 5 min), Diagnoză DTC-101 & Format Tabelar Consolă** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Implementat `ZONE_WARMUP` și grace period de 300s la boot: încălzirea/arzătorul este oprită preventiv în primele secunde până la stabilizarea primelor balize BLE pasive (fără alarme false premature).
        - Ieșire anticipată (*Early Exit*): Imediat ce toți senzorii asociați au sosit (ex: 3/3 în 47s), warmup-ul se oprește instantaneu și buclele devin 100% active.
        - La expirare (300s): Dacă un senzor nu a apărut, se ridică automat `DTC-101` în NVS, bucla defectă este decuplată în failsafe OFF, iar sistemul dă drumul căldurii pe camerele complet funcționale.
        - Format tabelar aliniat vertical pentru bucle (`[ZONES]`), termometre (`[SENSORS]` cu min/cur/max dBm) și etichetă clară de protecții (`[PROTECTIONS]`).
        - Toate cele 14 teste ASPICE SWE.5 trecute cu 100% PASS în 6.41s.

- [x] **21. Eliminare Acronime Tehnice, Unificare Mapare Zone-Senzori, Servicii în POST & Reziliență RTC Offline** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Excluse acronimele tehnice din POST și status (`GPA` -> `Secondary Pump on Relay 4`, `TWDT` -> `Hardware Watchdog`, `NC Polarity` -> `Normally Closed [NC], User-Configured in NVS`, `NimBLE Active` -> `Bluetooth Receiver active`).
        - Adăugată data/ora reală de boot în banner și integrat ceasul hardware RTC în POST cu rezistență dovedită offline (zero dependență de internet sau cloud).
        - Unificată maparea senzorilor BLE direct pe buclele de încălzire, eliminând ambiguitatea dintre denumirea zonei și numele termometrului.
        - Reguli CSS de colorare high-contrast pentru cuvintele-cheie de stare în Web Console.

- [x] **22. Telemetrie Hardware Extinsă (CPU Die Temp, RAM Lowest/MaxBlock, NVM/Flash), Carduri Zonale & Stilizare Vreme** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Telemetrie hardware completă în comanda `STATUS`: cip ESP32, nuclee, frecvență, revizie, temperatură siliciu (`Die Temp`), uptime, memorie RAM (liberă, lowest watermark, max contiguous block), memorie flash și marjă OTA.
        - Redesign complet bucle: tabelul redundant `[SENSORS]` eliminat; fiecare cameră dispune de un card complet pe 4 linii (temperatură, control hidraulic valvă/releu, radio BLE cu MAC/baterie, telemetrie RF cu RSSI min/cur/max, pachete și vechime).
        - Toate temperaturile utilizează simbolul standard UTF-8 `°C`.
        - Eticheta `[WEATHER]` stilizată distinct în Web Console, iar anotimpul afișat bold.

- [x] **23. Format Tabelar pe Subsisteme, Aliniere `utf8VisibleLen`, NimBLE 90% Duty Cycle & Warmup Non-Blocant, Paced Socket OTA** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Structurarea ieșirii `STATUS` în tabele clare pe 2 coloane numerotate alfabetic (`[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`), separate prin linii goale (newlines) între categorii și între cardurile zonale pentru lizibilitate maximă.
        - Eliminată deplasarea la stânga cu 1-2 caractere prin implementarea funcției `utf8VisibleLen()` care compensează octeții multi-byte ai caracterului `°` (`\xC2\xB0`).
        - Eliminat serviciile asincrone din tabelul POST (unde rămân strict verificările hardware/locale 100% PASS); integrat `* Real-Time Clock` sub `[INIT] Background Tasks Dispatched:` alături de Weather și Sensor Discovery.
        - Formatare unificată cu prefixul `[INIT]   * ` pentru toate actualizările asincrone (sincronizare ceas NTP, vreme Open-Meteo, achiziție progresivă balize senzori și finalizare warmup).
        - Integrare hyperlink către instrumentul oficial de configurare senzori BLE Xiaomi (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) în Web Console, Setup Wizard, Dashboard și `HELP` pentru reglarea puterii de transmisie RF (Tx Power).
        - Fereastră de scanare NimBLE mărită la 90ms / 100ms interval (90% duty cycle) în `include/Config.h`, reducând timpul de captură a senzorului din Baie Mare (după 4 pereți) de la 293s la 39s–64s.
        - Warmup non-blocant: buclele care și-au achiziționat senzorul pornesc imediat controlul termostatic dacă este nevoie de căldură, fără a aștepta senzorii îndepărtați.
        - Colorare exhaustivă în `/console` prin regex case-insensitive (`/gi`) pe toate stările de după `:`, suport prefixe `[A-E. ...]`, inclusiv stilizarea delimitatorilor de coloană `|`.
        - Paced socket streaming în `tools/ota_upload.py` (1436-byte chunks, 12ms sleep, ~110 KB/s), prevenind epuizarea bufferelor TCP `pbuf` din lwIP și asigurând flash stabil în ~13.5 secunde.
        - Watchdog TWDT mărit la 90s pentru toleranță maximă la scrierea flash.

- [x] **24. Remediere Scurgere Memorie Heap (`DTC-402`), NimBLE `setMaxResults(0)` & Auto-Clear Histerezis** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Identificată și izolată pe viu cauza scăderii lente a memoriei heap (de la 127 KB la 51 KB după 4 ore): `NimBLEScan` stoca implicit nelimitat (`m_maxResults = 0xFF`) toate pachetele BLE de la dispozitivele din jur cu adrese MAC rotative (telefoane, smartwatch-uri), ne-eliberând memoria deoarece scanarea este continuă.
        - Setat `_pBLEScan->setMaxResults(0)` la inițializare conform contractului NimBLE (procesare directă pe callback și ștergere instantanee cu `erase()`), eliminând complet acumularea de dispozitive în RAM și stabilizând consumul de memorie ca fiind 100% plat în timp.
        - Adăugat mecanism de auto-clear cu histerezis în `DiagnosticEngine` pentru `DTC-402` (clear la >75 KB) și `DTC-401` (clear la >-78 dBm Wi-Fi).
        - Testat și validat pe viu: firmware flashuit prin OTA, Free Heap stabil la **125 KB**, codul `DTC-402` stins, Health Score **100% [EXCELLENT]**, suita ASPICE SWE.5: **14/14 PASS (100.0%)**.



- [x] **25. Eliminare Word Wrap pe Mobil, Scalare Font (`A-`/`A+`), Scroll Orizontal Fluid & Watchdog Feed la OTA** (Finalizat & Validat Live pe Hardware 12.10.2026).
      *Rezultat:*
        - Identificată cauza ruperii tabelelor pe telefoane mobile: clasa CSS `.term-container` impunea `white-space: pre-wrap; word-break: break-all;`, forțând rândurile tabelelor de 70-100 de caractere să se fragmenteze pe 2-3 rânduri pe ecrane înguste (<450px).
        - Înlocuit cu formatare rigidă monospace no-wrap (`white-space: pre; word-break: normal;`), acompaniată de scroll orizontal și vertical nativ fluid pe ecrane tactile (`overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`). Tabelele `STATUS`, bannerele POST și `[INIT]` își păstrează alinierea geometrică impecabilă.
        - Adăugat checkbox comutator `Wrap` în footer-ul consolei (`wrapToggle`), permițând activarea opțională a ruperii de rând (`.wrap-enabled`) cu persistență în `localStorage` (`termWrap`).
        - Adăugate butoane de scalare a mărimii textului `A-` și `A+` în footer pentru ajustare fină (0.52rem - 1.20rem), cu memorare în `localStorage` (`termFontSize`).
        - Reguli responsive `@media (max-width: 640px)` cu font implicit mai mic (`0.70rem` vs `0.82rem`) și margini compactate pentru lizibilitate maximă pe smartphone.
        - Atașat callback-ul `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` înainte de `httpUpdater.setup()`, resetând periodic Task Watchdog Timer-ul pe fiecare pachet HTTP de firmware scris în flash și eliminând complet riscul de watchdog reboot la actualizări Web OTA.
        - Validat live pe hardware: firmware v1.2.0 (Build Oct 12 2026 20:47:28) încărcat pe ESP32 (`192.168.1.20`), verificată interfața `/console` și confirmată trecerea tuturor celor 14 teste ASPICE SWE.5 cu **100% PASS**.

- [x] **26. Backend Orar Termic Inteligent (Smart Bio-Schedule), Fereastră Deschisă & Pre-Heat Anticipativ** (Finalizat & Validat Live pe Hardware 13.10.2026).
      *Rezultat:*
        - *Smart Bio-Schedule (4 Ancore Naturale):* Implementat motor de programare bazat pe ritmul circadian și 4 stări de viață naturale: `Sleep` (somn: noapte odihnitoare 20.0–21.0°C), `Wake` (trezire: confort dimineața 22.0–22.5°C), `Away` (plecat: mod Eco cât casa e goală 19.5–20.0°C), `Home` (seară: confort în familie 21.5–23.0°C). Profiluri configurabile distincte pentru Luni–Vineri (Weekday) și Sâmbătă–Duminică (Weekend).
        - *Detecție Fereastră Deschisă (Open Window Smart Pause Guard):* Monitorizare viteză de scădere temperatură prin istoric de eșantioane circulare. La detectarea unei căderi bruște (>1.2°C în sub 5 minute), valva zonei este forțată în starea ÎNCHIS timp de 30 minute pentru prevenirea risipei inutile de gaz. Anunțat în consolă și marcat în telemetrie (`PAUSED (Open Window)`).
        - *Pre-Heat Anticipativ cu Inerție Termică:* Calcul dinamic al avansului necesar pornirii prealabile a încălzirii în funcție de diferența de temperatură și factorul meteorologic exterior $k_{weather} = 1.0 + \max(0.0, (15.0 - T_{ext}) \times 0.04)$, activând pre-încălzirea șapei cu 30–120 minute înainte de ancorele de confort (`Wake` și `Home`).
        - *Persistență NVS Anti-Uzură Flash:* Structură binară salvată în namespace-ul `"sched"`, protejată prin comparație binară `memcmp()` (scrieri doar la modificări reale de parametri).
        - *REST API & Comenzi Consolă:* Endpoint-uri `/api/schedule` (GET/POST) și `/api/schedule/enable` (POST); comenzi interactive `SCHEDULE`, `SCHEDULE ON/OFF`, `SCHEDULE PREHEAT ON/OFF`, `SCHEDULE WINDOW ON/OFF` în Network Console și integrare completă în raportul `STATUS` (Secțiunea `[D. SMART BIO-SCHEDULE & PRE-HEAT]`).
        - *CLI & Suită de Testare:* Adăugat subcomanda `schedule` în `tools/esp_cli.py` și testul automat `SWE.5-TC15` în `tools/test_system.py`. Validat live pe hardware: **15/15 PASS (100.0%)** în 15.65 secunde. Zero spam de vreme pe consola virtuală.

- [x] **26.1. Arhitectură Web GZIP Build-Time, Optimizare Masivă Flash (-176 KB) & Latență Consolă <50ms** (Finalizat 13.10.2026).
      *Rezultat:*
        - *Modelul Smart Aquarium aplicat cu succes:* Paginile web au fost extrase din C++ în fișiere dedicate în directorul `web/` (`index.html`, `setup.html`, `console.html`).
        - *Compilare cu GZIP Automat:* Creat scriptul `tools/build_web.py` integrat în `platformio.ini` (`extra_scripts = pre:tools/build_web.py`), care comprimă cu `gzip -9` toate paginile în antete PROGMEM (`dashboard_html_gz.h`, `setup_html_gz.h`, `console_html_gz.h`). Întregul pachet web ocupă doar **15.5 KB** în flash.
        - *Eliberare 176 KB Flash & Zero Spike Heap:* Binarul a scăzut de la 1,304,000 bytes la **1,127,833 bytes (1.10 MB, 57.4% flash)**, creând un spațiu liber de peste 178 KB sub plafonul strict de sector. Eliminat peste 250 de linii de concatenare C++ din `main.cpp` și `NetworkConsole.cpp`. Free Heap stabil la **132 KB**.
        - *Consolă Web Ultra-Rapidă & Delimitatori:* Comenzile trimise prin Web Console returnează datele direct în corpul răspunsului POST (<50ms). Adăugați delimitatori vizuali `.cmd-start` cu cyan glow, `.cmd-end` și `.t-sep`.
        - *Conformitate ASPICE SWE.5:* Toate cele 15 teste din `tools/test_system.py` au trecut cu **100.0% PASS**. Flash OTA confirmat cu succes.

- [x] **26.2. Eliminare Latență Consolă (TCP Anti-Collision, Non-Blocking UART), Shell Interactiv REPL CLI & Specificație REST API** (Finalizat 14.10.2026).
      *Rezultat:*
        - *Root-Cause Latență 3s Rezolvat:* Identificat timeout-ul de 3.0s ca fiind generat de retransmisia Windows TCP SYN cauzată de coliziunea cererilor de polling `setInterval` cu `sendCmd` pe serverul single-threaded al ESP32. Rezolvat prin introducerea `AbortController` și `scheduleNextPoll()` adaptiv în `web/console.html`, plus eliminarea forțării `Connection: close` care declanșa reseturi de socket (WinError 10054).
        - *UART Serial Non-Blocking & Scriere Batch:* Implementat `Serial.availableForWrite()` în `NetworkConsole::rawWrite` și `Serial.setTxBufferSize(4096)` în `main.cpp`. CPU-ul ESP32 nu mai este blocat de transmisia la 115200 baud la rularea comenzilor lungi (`STATUS`). Scriere batch și `memcpy` rapid în ring buffer.
        - *Utilitar CLI Interactiv REPL (`tools/esp_cli.py`):* Rularea fără parametri (`python tools/esp_cli.py`) lansează o consolă interactivă dedicată în terminal (`smart-heating> `) cu răspuns instantaneu (<150ms) peste REST JSON: status, zone, senzori, on/off cazan, reglaj temperatură, diagnoză DTC, self-test, stream Telnet direct via port 23 (`monitor`).
        - *Specificație Completă REST API (`docs/api_reference.md`):* Redactat contractul oficial REST/JSON detaliind toate cele 22 de rute, parametri, structuri de răspuns și bune practici pentru dezvoltarea interfeței Kiosk de pe tableta Samsung Galaxy Tab A9.
        - *Validare SWE.5:* 15/15 teste PASS (100.0%) în 20.68 secunde pe firmware-ul compilat și încărcat prin OTA.

- [x] **26.3. Personalizare Ancore Bio-Schedule, Motor de Preset-uri NVS, Comenzi Consolă & Suită de 63 Teste Atomice cu Matrice de Trasabilitate (ASPICE SWE.5)** (Finalizat & Validat Live pe Hardware 14.10.2026).
      *Rezultat:*
        - *Ancore Circadiene Complet Customizabile:* Ora (`HH:MM`) și temperaturile per zonă (`z1`, `z2`, `z3`) sunt complet configurabile individual sau global prin endpoint-ul `POST /api/schedule/anchor` și comanda de consolă `SCHEDULE SET <prof> <anc> <HH:MM> [targets]`.
        - *Motor de Preset-uri (Factory & User Slots):*
          * Preset-uri de fabrică (read-only): `default` (standard confort), `eco` (18.0°C), `wfh` (work from home 22.5°C).
          * Sloturi NVS pentru utilizator: `custom1`, `custom2` (salvare orar curent cu denumire prietenoasă în namespace-ul `"presets"` cu protecție flash-wear `memcmp`).
          * Endpoint-uri REST dedicate: `GET /api/schedule/presets`, `POST /api/schedule/preset/apply`, `POST /api/schedule/preset/save`, `POST /api/schedule/reset`.
        - *Comenzi de Consolă Telnet & Web:*
          * Manual `HELP` structurat pe 3 secțiuni funcționale (Heating, Bio-Schedule, Diagnostics).
          * Adăugate comenzile: `PRESETS`, `PRESET <id>`, `SCHEDULE SAVE <slot> [name]`, `SCHEDULE SET ...`, `SCHEDULE RESET`.
          * Raportul `STATUS` afișează preset-ul activ în secțiunea `[D. SMART BIO-SCHEDULE & PRE-HEAT]`.
        - *Suită de Testare Atomică & Trasabilitate (ASPICE SWE.5):*
          * Desfăcut cele 17 teste monolitice în **63 de teste atomice independente**, eliminând aserțiunile combinate și izolând fiecare verificare hidraulică, de memorie, radio și NVS.
          * Implementat calculul automat al Matricei de Trasabilitate Bidirecțională între toate cele 33 de cerințe SWE.1 (`FR-1` .. `FR-28`, `NFR-1.1`, `NFR-1.5`, `NFR-2.2`, `NFR-3.2`) și testele atomice SWE.5.
          * Raport complet generat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md): **33/33 cerințe acoperite (100.0%)**, **0 cerințe neacoperite**, **63/63 teste PASS (100.0%)** pe hardware live (`192.168.1.20`).
        - *Decuplare UI:* Fișierele `web/index.html` și `web/setup.html` au rămas neatinse, modificările grafice fiind programate pentru pasul următor.

- [ ] **27. Telemetrie Hidraulică Avansată Tur-Retur Distribuitor ($\Delta T$), Modelare Șapă & Prevenire Overshoot:**
      - *Hardware Sonde Manifold:* Monitorizare temperatură cu 2 sonde digitale de contact (DS18B20 OneWire waterproof sau NTC) prinse cu colier și pastă termică direct pe bara de Tur și bara de Retur din caseta distribuitorului (zero cabluri prin casă, montaj în 5 minute).
      - *Calcul Live $\Delta T = T_{tur} - T_{retur}$:* Determinarea ratei reale de absorbție a căldurii de către șapă.
      - *Prevenire Supraîncălzire (Anti-Overshoot inteligent):* Când returul începe să crească și $\Delta T$ scade sub un prag (semn că șapa este saturată termic), taie flacăra centralei din timp și lasă căldura reziduală din beton să ducă aerul exact la setpoint fără overshoot (fără depășire de temperatură).
      - *Diagnoză & Protecție Failsafe (`DTC-301: Hydraulic Circulation Lock / No-Flow`):* Dacă $T_{tur}$ crește brusc (>55°C) fără ca $T_{retur}$ să reacționeze (pompă blocată, aer în distribuitor, robineți închiși), sistemul taie instantaneu flacăra radio și emite alertă de protecție.
      - *Optimizare Randament Condensare:* Monitorizare permanentă pentru ca $T_{retur}$ să rămână sub 45°C pentru condensare maximă a vaporilor de apă din gaz.
      - *Configurare Specific Șapă în NVS:* Parametri pentru tip finisaj (`Gresie` vs `Parchet laminat/stratificat`) și temperatura agentului setată în centrală.

- [ ] **28. Extindere Ecosistem Senzori & Scalabilitate Radio (BLE vs. Zigbee Sonoff SNZB-02D pentru Case Mari):**
      - *Segmentare Senzori după Tip Locuință:*
        * Apartamente (1 nivel): Păstrare senzori BLE LCD (28 lei) citiți nativ de ESP32 direct din aer (zero gateway, zero costuri extra).
        * Case mari / Parter + Etaj: Suport senzori Zigbee 3.0 cu afișaj mare (Sonoff SNZB-02D la 68 lei) profitând de rețeaua mesh pe 230V prin prize/becuri (prin mini-modul serial pe placă sau trecere viitoare pe ESP32-C6).
      - *Evaluare Senzori în Șapă de Beton:* Documentat clar că merită strict la construcții noi de la zero dacă dezvoltatorul a tras tuburi corugate pentru limitare parchet (max 28°C); la apartamente gata finisate (retrofit) este imposibil și nejustificat economic, fiind înlocuit mult mai elegant de analiza $\Delta T$ pe distribuitor.

- [x] **29. Kiosk Dashboard Dedicat PWA pentru Tableta Samsung Galaxy Tab A9 (Panou de Perete 24/7):** (Finalizat & Validat Live pe Hardware 14.10.2026).
      *Rezultat:*
        - *Arhitectură PWA (Progressive Web App) Instalabilă:*
          * Endpoint-uri native servite direct din flash PROGMEM de ESP32 (`/manifest.json`, `/sw.js`, `/icon.svg`, `/icon-192.png`).
          * Web App Manifest complet cu mod `display: standalone`, temă întunecată (`#080c14`) și pictograme rasterizate PNG (192x192) + vectoriale SVG compatibile cu motorul WebAPK Android Chrome.
          * Service Worker funcțional (`/sw.js`) cu handler de cache passthrough non-blocant.
          * Buton tactil dedicat `📲 Instalează App` integrat cu evenimentul `beforeinstallprompt`, detectare mod standalone și modal interactiv de asistență pentru tablete Kiosk pe rețele LAN HTTP (`chrome://flags` secure origins).
        - *Design Grafic Premium Responsive (2 Coloane Kiosk Galaxy Tab A9 & Mobil):*
          * Coloana 1: Erou Cazan Gaz (indicator flacără ON/OFF cu glow verde/roșu, comutator AUTOMATIC / MANUAL, comenzi rapide), Card Smart Bio-Schedule (comutatoare master Orar, Pre-Heat anticipativ, Geam deschis), Card Ancoră Circadiană activă (`🌙 Sleep`, `🌅 Wake`, `🚶 Away`, `🏡 Home`), Selector rapid Preset-uri chips (`default`, `eco`, `wfh`, `custom1`, `custom2`), Modal salvare preset-uri utilizator în NVS, Editor vizual de ancore cu tab-uri Luni-Vineri vs Weekend și steppere temperaturi.
          * Coloana 2: Card Zone Termice Pardoseală (Dormitor, Copil, Baie mare cu steppere `+`/`-`, ținte bio, stare valve electrotermice NC), Card Telemetrie Senzori Xiaomi BLE cu badge-uri dBm (`EXCELENT`, `BUN`, `MARGINAL`, `SLAB`), Card Diagnoză Sistem (Heap RAM, Uptime, Wi-Fi RSSI, Flash OTA liber) și linkuri de navigare rapidă către Telnet Console și Setup Passport.
        - *Optimizare Extremă a Memoriei Flash (-133.8 KB Economisiți):*
          * Înlocuit `HTTPClient::begin(url)` cu `HTTPClient::begin(WiFiClient&, url)` în `TimeAndWeatherService.cpp`, eliminând complet legătura linkerului cu stiva greoaie mbedtls SSL client/server (neutilizată pe HTTP local/Open-Meteo).
          * Adăugat `-D CONFIG_NIMBLE_CPP_ENABLE_RETURN_CODE_TEXT=0` și `-D CONFIG_NIMBLE_CPP_ENABLE_WARNINGS=0` în `platformio.ini`.
          * Minificare automată a codului HTML/CSS/JS la build-time în `tools/build_web.py` (eliminare comentarii și spații redundante) comprimând dashboard-ul la doar 10,988 bytes GZIP.
          * Dimensiunea binarului firmware a scăzut spectaculos de la 1,320 KB la **1,186 KB** (oferind o marjă confortabilă de **121.7 KB sub limita maximă a partiției OTA** de 1.25 MB).
        - *Validare Live pe Hardware & ASPICE SWE.5:*
          * Flash OTA executat cu succes în 41 secunde (`tools/ota_upload.py`), restart curat în 7 secunde.
          * Toate endpoint-urile PWA verificate live cu cod `200 OK`.
          * Suită completă de regresie rulată pe hardware live: **63/63 teste PASS (100.0%)**, **33/33 cerințe acoperite (100.0%)** în 39.80 secunde.

- [x] **30. Schemă Termodinamică & Hidraulică Interactivă și Animată (Stil Tesla Service Mode HVAC & Cooling Loops):** (Finalizat & Validat Live 14.10.2026)
      - *Rezultat & Arhitectură SVG Vectorială:*
        * **Tab Navigare Dedicat:** Buton rapid în headerul panoului (`🏎️ Schemă Termică (Tesla Mode)`) ce comută instantaneu vizualizarea cu persistență locală.
        * **Sub-Sistem Cazan Gaz:** Card dedicat cazanului cu flacără dinamică animată (verde la ardere, gri în standby), timer live de cooldown anti-short cycling și etichetă idempotență RF 433.92 MHz.
        * **Grup Amestec & Pompă (GPA):** Reprezentare vană de amestec și rotor de recirculare animat rotativ (`.pump-spinning`), debit 1850 RPM și afișaj interlock 120s / overrun 180s.
        * **Supermanifold & 3 Bucle Pardoseală:** Conducte tur/retur animate cu flux de particule (`teslaPipeFlow`), debitmetre individuale, actuatoare electrotermice NC (indicator dinamic 230V vs 0V NC) și căsuțe de cameră cu telemetrie live ($T_{cameră} \mid T_{target}$).
        * **Casete Telemetrie Stil Tesla:** $T_{tur}$ (48.5°C ardere / 24°C standby), $T_{retur}$ (38.0°C / 23.5°C), $\Delta T$ sorbție (10.5°C Optim / 0.5°C Echilibru) și stare pompă.
        * **Consolă Acțiuni Service Directe:** 5 butoane tactile: Purjare/Test Pompă GPA, Test RF Cazan (Ping), Test Valvă Copil, Test Valvă Living, Curățare DTC Diagnoză.
      - *Validat:* Flash OTA pe ESP32 (`192.168.1.20`) și testat automat în browser nativ (`TC-UI-13` PASS, `TC-UI-15` PASS).

- [x] **31. Diagramă Interactivă de Stări FSM & Fir Logic Live în Zona de Admin / Diagnoză:** (Finalizat & Validat Live 14.10.2026)
      - *Rezultat & Motor Decizional FSM:*
        * **Tab Navigare FSM:** Buton rapid `🧠 Fir Logic & Stări (FSM Admin)` cu afișare full-screen a motorului de stări.
        * **9 Noduri de Stare Distincte:**
          1. `IDLE / STANDBY` (toate zonele la setpoint, cazan/pompă standby)
          2. `CERERE TERMICĂ` (cel puțin o zonă cere căldură)
          3. `AȘTEPTARE ACTUATOR` (interlock ceară termică 120-180s)
          4. `MANUAL HOLD` (cerere zonă activă, cazan oprit manual)
          5. `ÎNCĂLZIRE ACTIVĂ` (flacără aprinsă via 433MHz, pompă GPA activă)
          6. `POST-CIRCULAȚIE POMPĂ` (pompă releu 4 activă 180s pe keeper zone)
          7. `GARDĂ COOLDOWN` (interdicție reaprindere rapidă 60s)
          8. `PROTECȚIE ÎNGHEȚ` (urgență sub 12°C)
          9. `GEAM DESCHIS` (cădere >1.2°C / 5min, valvă închisă 30m)
        * **Evidențiere Live Stare Activă:** Nodul curent este evidențiat dinamic cu bordură cyan electric (`.active`), badge activ și timer live decrementat secundă de secundă (ex: `19s Lockout`).
        * **Afișaj Header Fir Logic:** Indicator sintetic live în colțul din dreapta (`Stare Activă Hardware: 7. GARDĂ COOLDOWN (19s LOCKOUT)`).
      - *Validat:* 100% PASS în testul E2E de browser (`TC-UI-14` PASS) și captură live `e2e_07_fsm_live_diagram.png`.

- [x] **32. Framework Nativ de Testare Automată Browser E2E & Scenarii User (Agent-Driven Chrome CDP):** (Finalizat & Validat Live 14.10.2026)
      - *Rezultat:* Utilitar complet [`tools/browser_live_test.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/browser_live_test.py) ce controlează direct Google Chrome prin Chrome DevTools Protocol (CDP) WebSocket pe portul 9888.
      - *Scor Rulare:* **10/10 teste PASS (100.0%)** direct pe hardware live `192.168.1.20` cu capturi de ecran automate salvate în directorul de artefacte (`e2e_01_beginner_mode.png` .. `e2e_07_fsm_live_diagram.png`).

- [x] **33. Directivă Arhitecturală Advisor (Argon Maximum) & Remediere Audit Chrome DevTools / Lighthouse:** (Finalizat & Validat Live 14.10.2026)
      - *Rezultat & Remediere Arhitecturală:*
        * **Matrice Orar Circadian (4×3):** Înlocuit grila densă cu un modal tabelar compact 4 Ancore × 3 Zone, auto-save debounced (800ms) și butoane de salvare per rând cu feedback verde.
        * **FSM Stateflow pe 2 Planuri în SVG:** Planul A (Master Plant & Pompă GPA) cu săgeți vectoriale orientate și substări de ardere/cooldown, Planul B (Zone Parallel FSM) pe 3 benzi paralele cu interblocări locale.
        * **Schemă Termică Dinamică 7 Bucle Pardoseală (Tesla Mode):** Topologie reală 3 zone (7 circuite) + bypass băi permanent, calcul dinamic progres ceară actuatoare (`openSec/120s`) și model termodinamic Tur/Retur plafonat la regim de pardoseală ($36.0^\circ\text{C}\text{–}43.5^\circ\text{C}$).
        * **Remediere Audit DevTools & Lighthouse (WCAG AA + Network):** Contrast cromatic ridicat (>5.8:1), etichetare accesibilă switch-uri/inputuri (`aria-label`), landmark semantic `<main class="container" role="main">`, ergonomie mobilă `@media (max-width: 520px)` și toleranță latență reverse proxy Nginx (`AbortController` 4000ms).
      - *Validat:* 68/68 PASS (100.0%) pe ASPICE SWE.5 (`tools/test_system.py`) și 10/10 PASS (100.0%) pe CDP browser test (`tools/browser_live_test.py`).

- [ ] **34. Decuplare Arhitecturală & Optimizare Workflow (Anti-Bloat & Headless C++ First):**
      - **Problemă Identificată (15.10.2026):** Modificarea continuă a unui fișier HTML uriaș (3300+ linii), recompilarea gzip C++ în `dashboard_html_gz.h`, flash OTA wireless pe ESP32 și reboot la fiecare mică schimbare de stil sau buton este ineficientă, riscantă pentru memoria flash și generează bloatware.
      - **Noua Disciplină de Lucru (Ponytail Engineering):**
        * **Backend C++ Headless:** ESP32 răspunde exclusiv de siguranța fizică a cazanului (interblocări, TWDT, supraveghere senzori), bucla termică deterministă și servirea rapidă a API-ului REST/JSON (`/api/...`). Modificările de C++ se testează prin consolă (`/console`, Telnet, `tools/esp_cli.py`) sau suita automată `test_system.py`.
        * **Frontend Decuplat Local (Dev Server):** Prototiparea și ajustarea UI-ului se fac 100% pe mașina locală (ex: rulare prin server static local sau fișier web local conectat prin CORS la `http://192.168.1.20/`). Iterare instantanee cu F5 în browser (fără recompilare C++, fără flash OTA).
        * **Bundling Final pe Flash:** Doar după ce o interfață este complet aprobată și stabilă, se rulează `tools/build_web.py` și se include în binarul de producție.
        * **Zi de Observație Live (Test Dinamic de Comportament):** Menținerea stabilității firmware-ului v1.2.0 pentru a colecta date reale de comportament dinamic (temperatură, histerezis, timpi de răspuns).

- [ ] **35. Roadmap Comercial & Inteligență Termică (Confort, Calibrare & Cloud):**
      - **35.1. Trend Temperaturi (Săgeți Dinamice & Istoric 3h):**
        * Calcul pantă $\Delta T / \Delta t$ per senzor/zonă: afișare săgeată orientată (`↗ Încălzire`, `↘ Răcire`, `→ Stabil`) și buffer in-memory de 3 ore pentru analiza vitezei de pierdere termică.
      - **35.2. Factorul Umiditate (% RH) & Confort Bioclimatic:**
        * Integrarea umidității de la senzorii Xiaomi în calculul temperaturii resimțite (indice Humidex / ecuația Steadman). Corelarea umidității scăzute (<40%) cu senzația falsă de frig la repaus.
      - **35.3. Wizard Onboarding Configurare Client:**
        * Interfață ghidată pas-cu-pas: tip locuință (apartament / casă, etaj, calitatea izolației), volum încăperi (înălțime, suprafață), destinație cameră (dormitor, birou, living, baie, garaj, atelier, frecvență utilizare), recomandare automată a profilului termic de pornire.
      - **35.4. Meniu & Algoritm Transparent de Calibrare (Săptămâna 1):**
        * În primele 7 zile, sistemul colectează inerția șapei per cameră și emite recomandări proactive: repoziționare senzor (dacă e influențat de curenți/soare), reglare temperatură tur cazan, optimizare timp de aerisire.
      - **35.5. Modul Research Cloud (Grafana pe Google Cloud):**
        * Export securizat de telemetrie (valori senzori, timpi flacără, vreme exterioară, setpoint-uri) către instanță Grafana Cloud / InfluxDB pentru analiză aprofundată și sugestii de optimizare asistată de AI.
      - **35.6. Vizualizare 3D / Schiță Digital Twin a Locuinței:**
        * Generare schiță 2D/3D a apartamentului din video/foto/schiță cu poziționarea spațială a senzorilor și buclelor de încălzire pentru clienți.

---

## 🔮 Viitor / Nice to Have: Migrare Home Server pe ThinkPad L590 (Înlocuire Galaxy A6)

Această sarcină este opțională și va fi declanșată când Galaxy A6 („Cinderella”) dă semne de oboseală accentuată sau când se dorește centralizarea robustă a serviciilor.

- [ ] **NTH-1: Pregătire & Audit ThinkPad L590 (Intel Core Gen 8, NVMe SSD, Gigabit LAN):**
  - **Sistem de Operare:** Linux curat (Ubuntu Server 24.04 LTS sau Debian 12 Minimal) sau dual-boot. Fără interfață grafică greoaie desktop pentru a minimiza consumul.
  - **Consum Real la Priză în Idle:** Procesoarele Intel Whiskey Lake (15W TDP) coboară în stările energetice profunde (Package C-States C8/C9/C10). Cu capacul închis (`HandleLidSwitch=ignore`) și ecranul decuplat complet, consumul măsurat la priză este de **4.5W – 6.5W** (sub 5 lei/lună la Hidroelectrica/Enel, comparabil cu un încărcător de telefon sau un Raspberry Pi cu SSD).
  - **Protecție Baterie & UPS Automat:** Configurare `tlp` cu praguri de încărcare (start la 40%, stop la 50%). Bateria nu se degradează niciodată, iar în caz de cădere de tensiune în apartament serverul are 3–6 ore de autonomie completă, prevenind coruperea bazelor de date.
  - **Stack Servicii (Docker Compose):**
    - `teslamate` + `postgres:17` + `grafana`
    - `eclipse-mosquitto:2` (broker MQTT pe `0.0.0.0:1883`)
    - `ble-gateway` (daemon Python folosind adaptorul Bluetooth integrat Intel)
    - `nginx` (reverse proxy portal centralizat + Tailscale)
    - Opțional: `adguard-home` / `pi-hole` (blocare reclame la nivel de rețea întreagă).

---

## 🔒 Puncte deschise (Toate soluționate pe 06.10.2026!)

- **[x] OP-1: Tipul plăcii de relee.** Soluționat: Placă integrată ESP8266 (ESP-12F / LC-Relay-ESP12-4R-MV) cu 4 relee (comandată pe eMAG, aceeași arhitectură cu placa de test actuală).
- **[x] OP-2: Detalii comandă modul radio centrală.** Soluționat: Tren impulsuri 410 pași pe pin GPIO către emițător SYN115 433MHz.
- **[x] OP-3: Compartimentarea apartamentului & circuitele.** Soluționat: 3 zone termice (7 circuite pardoseală), băile pe calorifere libere (bypass hidraulic).
- **[x] OP-4: Sursa temperaturii exterioare.** Soluționat: Receptor SYN480R pentru Tronic 4-LD7396-2 + fallback Wi-Fi Open-Meteo.

---

## 🔗 Evidență Modificări & Dependințe Cross-Repository

Pentru a menține coerența între sistemele interconectate (Smart Services Portal de pe Galaxy-A6 și controllerul de încălzire), ținem evidența modificărilor locale care necesită commit/push pe repository-urile respective:

| Repository | Cale Fișier Modificat | Descriere Modificare | Stare Git & Remote |
| :--- | :--- | :--- | :--- |
| **`smart-aquarium-esp8266`** | [`tools/nexus/nexus_nginx.conf`](file:///c:/Users/BSeceleanu/.github_repos/smart-aquarium-esp8266/tools/nexus/nexus_nginx.conf) | Adăugat blocul `location /heating/` (proxy la `192.168.1.18`). | 2 fișiere modificate local (`git status`). Ne-împins pe remote. Se va face commit/push la confirmarea utilizatorului. |
| **`smart-heating`** (curent) | `platformio.ini`, `include/`, `src/`, `docs/`, `tools/`, `.agent/` | Implementat driver RF, scanner BLE NimBLE, motor termic cu histerezis, Telnet/Web console, diagnoză DTC & Service Mode, Pașaport digital, NTP/Meteo, utilitar CLI `esp_cli.py` și documentație completă. | Sincronizat (commit & push pe `origin/main`). |
| **`smart-home-ble-gateway`** | `services/` | Telemetria senzorilor Xiaomi via broker MQTT Mosquitto local. | Urmează integrarea clientului MQTT (Sarcina 6). Nicio modificare încă. |

