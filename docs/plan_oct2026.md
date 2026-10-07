# Plan de Lucru Activ (Octombrie 2026) – Smart Home Heating

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

- [ ] **10. Scanner BLE Nativ pe ESP32 pentru Senzorii Xiaomi (BTHome v2) & Punte MQTT/Grafana** (Planificat).
      *Plan:* Deoarece noua placă conține modulul **ESP32-WROOM-32E cu radio Bluetooth integrat**, activăm scannerul pasiv BLE (NimBLE) direct pe ESP32. Preia direct telemetria termometrelor Xiaomi (Living, Copil, Baie, Matrimonial), devine complet independent de telefon și poate publica la rândul său datele prin MQTT spre Grafana / PostgreSQL.

- [x] **11. Consolă Virtuală Serială Bidirecțională prin Wi-Fi (Telnet TCP:23 & Web Console /console)** (Finalizat & Validat Live 07.10.2026).
      *Rezultat:* Eliminată complet dependența de cabluri USB sau operare „legată la ochi”:
        1. **Server Telnet TCP pe portul 23 (`WiFiServer(23)`):** Implementat în [`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h) și [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp). Suportă conexiuni remote din PlatformIO (`monitor_port = tcp://192.168.1.20:23`), PuTTY sau scripturi CLI, cu prompt interactiv `> ` și istoric.
        2. **Consolă Web 2-Way la `/console`:** Interfață web modernă (dark theme) integrată în Web Server, cu terminal live (ring buffer 8KB), streaming sincronizat fără pierderi, auto-scroll, istoric de comenzi cu săgețile tastaturii (Up/Down) și butoane rapide de acțiune (`STATUS`, `ON`, `OFF`, `AUTO`, `MANUAL`, `HELP`). Compatibilă 100% atât pe acces direct LAN (`http://192.168.1.20/console`) cât și prin reverse proxy Galaxy A6 (`http://galaxy-a6/heating/console`).
        3. **Control Bidirecțional Verificat Live:** Comenzile transmise din Telnet sau din Web Console (`STATUS`, `ON`, `OFF`, `SET <zone> <temp>`, `AUTO`, `MANUAL`, `HELP`) sunt executate instantaneu pe cip, iar răspunsurile și toate logurile de sistem (RF, telemetrie MQTT, relee zone) sunt difuzate sincron către Web, Telnet și Serial USB.
        4. **Flashuit & Validat Live via Web OTA:** Compilat și încărcat prin OTA în 6.43 secunde. Răspunsurile la `STATUS` și `HELP` testate cu succes pe ambele canale.

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
| **`smart-aquarium-esp8266`** | [`tools/nexus/nexus_portal.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-aquarium-esp8266/tools/nexus/nexus_portal.html) | Adăugat cardul 4 **Smart Heating** (status live, butoane, link-uri). | Sincronizat deja pe serverul Galaxy-A6 (`/var/www/nexus-portal/index.html`). |
| **`smart-heating`** (curent) | `platformio.ini`, `include/`, `src/`, `docs/`, `.agent/` | Implementat driver RF, firmware test, Web OTA și documentație. | 3 commit-uri locale în avans față de `origin/main` + fișiere noi. Nu se face push fără confirmare. |
| **`smart-home-ble-gateway`** | `services/` | Telemetria senzorilor Xiaomi via broker MQTT Mosquitto local. | Urmează integrarea clientului MQTT (Sarcina 6). Nicio modificare încă. |

