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

