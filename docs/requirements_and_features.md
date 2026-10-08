# Specificație de Cerințe de Sistem & Funcționalități (Requirements & Features)

**Proiect:** Sistem Inteligent Autonom de Încălzire Multi-Zonă (Smart Home Heating)  
**Data elaborării:** 07 Octombrie 2026  
**Status:** În dezvoltare activă (Hardware & Software de bază validate live)  
**Documente corelate:**
- Plan & Roadmap: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md)
- Jurnal Modificări: [`docs/changelog.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/changelog.md)
- Documentație Hardware: [`docs/hardware_esp32_relay.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/hardware_esp32_relay.md)
- Evidență Piese & Costuri (BOM): [`docs/bom_hardware_costs.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/bom_hardware_costs.md)

---

## 1. Scopul și Viziunea Sistemului (Product Vision & Problem Statement)

### 1.1. Problema Reală
În instalațiile rezidențiale clasice de încălzire în pardoseală, un singur termostat de cameră (de regulă plasat în living) dictează pornirea și oprirea întregii centrale termice pe gaz. Acest lucru generează două deficiențe majore:
1. **Dezechilibru termic sever între camere:** În timp ce livingul atinge temperatura dorită și oprește centrala, dormitoarele rămân reci; invers, dacă termostatul este mutat în dormitor, livingul devine supraîncălzit.
2. **Defecțiunea termostatului wireless original:** În cazul defectării unității de emisie a vechiului termostat fără fir, utilizatorul este forțat să comande manual centrala sau să cumpere un kit comercial scump.
3. **Costul prohibitiv al soluțiilor comerciale multi-zonă:** Sistemele consacrate de la Tado, Honeywell Evohome, Netatmo sau Danfoss Ally pentru 3–5 zone depășesc cu ușurință 2.500 – 4.500 RON și depind obligatoriu de servere cloud externe și abonamente lunare.

### 1.2. Soluția & Viziunea Noastră
Construirea unui sistem autonom, fiabil și cu costuri minime, capabil să:
- Regleze independent temperatura în fiecare cameră a apartamentului prin acționarea electrotermică a celor 7 circuite de încălzire în pardoseală de pe distribuitor.
- Comande wireless centrala termică pe gaz prin emițătorul radio de 433.92 MHz (semnal decodificat și reprodus fidel pe microcontroler).
- Funcționeze **100% local și autonom**, independent de internet, cloud extern sau de funcționarea routerului Wi-Fi.
- Ofere interfață web modernă, acces la distanță prin VPN privat (Tailscale) și consolă virtuală serială prin rețea pentru depanare fără cabluri.

---

## 2. Topologia Apartamentului, Zone Termice & Circuite Hidraulice

Distribuitorul de încălzire conține **7 robineți termostatabili** pe tur/retur, mapați pe **3 zone termice distincte** + 2 băi cu calorifere libere (fără pardoseală):

```text
                                [ DISTRIBUITOR CENTRAL 7 ROBINEȚI ]
                                                │
         ┌──────────────────────────────┬───────┴──────────────────────┬────────────────────────┐
         ▼                              ▼                              ▼                        ▼
     [ ZONA 1 ]                     [ ZONA 2 ]                     [ ZONA 3 ]              [ BYPASS ]
  Living & Bucătărie             Camera Copilului             Dormitor Matrimonial         2x Băi
  Robineții: #1, #2, #3          Robineții: #6, #7            Robineții: #4, #5       Calorifere libere
  3 actuatoare NC (Paralel)      2 actuatoare NC (Paralel)    2 actuatoare NC (Paralel) (Fără pardoseală)
  Comandate de RELEUL 1          Comandate de RELEUL 2        Comandate de RELEUL 3     Bypass natural
  Senzor: Living Room BLE        Senzor: Kids Room BLE        Senzor: Matrimonial BLE   permanent
```

### Detalierea Zonelor Termice:
| Zonă Termică | Încăperi Deservite | Nr. Bucle Pardoseală | Robineți Distribuitor | Actuator & Releu | Senzor Principal Asignat |
|---|---|:---:|:---:|---|---|
| **Zona 1** | Living + Bucătărie + Dining + Vestibul | 3 | Robineții 1, 2, 3 | 3x Actuatoare NC 230V $\rightarrow$ **Releu 1 (GPIO32)** | Senzor BLE `Living Room` (`A4:C1:38:A4:C1:DD`) |
| **Zona 2** | Camera Copilului (Dormitor mijloc) | 2 | Robineții 6, 7 | 2x Actuatoare NC 230V $\rightarrow$ **Releu 2 (GPIO33)** | Senzor BLE `Kids Room` (`A4:C1:38:84:A1:E8`) |
| **Zona 3** | Dormitor Matrimonial | 2 | Robineții 4, 5 | 2x Actuatoare NC 230V $\rightarrow$ **Releu 3 (GPIO25)** | Senzor BLE `Matrimonial` (sau senzor dedicat) |
| **Rezervă** | Pompă adițională / Căldură baie | — | — | Contact liber de potențial $\rightarrow$ **Releu 4 (GPIO26)** | Neasignat (Canal liber) |
| **Bypass** | Baia Mare + Baia Mică | — | — | Calorifere portprosop fără actuatoare | Asigură circulația minimă pentru pompa centralei |

---

## 3. Cerințe Funcționale de Sistem (System Functional Requirements)

### 3.1. Reglaj Termic & Algoritm Histerezis (FR-1)
- **FR-1.1 (Buclă Histerezis):** Fiecare zonă termică va evalua continuu diferența dintre temperatura curentă ($T_{crt}$) și temperatura țintă ($T_{setpoint}$):
  - **Cerere de căldură (ON):** Când $T_{crt} \le T_{setpoint} - Histerezis$ (implicit: $T_{setpoint} - 0.3^\circ\text{C}$).
  - **Oprire cerere (OFF):** Când $T_{crt} \ge T_{setpoint} + Histerezis$ (implicit: $T_{setpoint} + 0.3^\circ\text{C}$).
- **FR-1.2 (Reglaj Temperatură):** Utilizatorul poate modifica temperatura țintă independent per zonă în trepte de 0.5°C (interval acceptat: 15.0°C – 30.0°C) din interfața web, API sau consolă.
- **FR-1.3 (Comutare Mod de Operare):**
  - **Mod AUTOMAT:** Zonele și centrala sunt controlate exclusiv de algoritmul termic pe baza senzorilor.
  - **Mod MANUAL:** Utilizatorul poate forța oricare releu sau centrala ON/OFF pentru mentenanță sau teste.

### 3.2. Siguranță Hidraulică & Interblocare Hardware (FR-2, FR-3, FR-4)
- **FR-2 (Actuator Opening Interlock):**
  - Actuatoarele electrotermice cu ceară (NC 230V) necesită între 120 și 180 de secunde pentru cursa completă de deschidere mecanică.
  - **Regulă critică:** Comanda de pornire a flăcării centralei NU se trimite niciodată înainte ca cel puțin un actuator să fi terminat timpul de interblocare (`DEFAULT_ACTUATOR_DELAY_MS = 120s`).
  - Previne funcționarea pompei de circulație în regim de cavitație pe circuite complet închise.
- **FR-3 (Pump Overrun & Heat Dissipation):**
  - La atingerea temperaturii dorite în toate camerele, flacăra centralei este oprită imediat prin radio.
  - Actuatoarele termice ale ultimelor zone active sunt menținute deschise timp de încă 120–180 de secunde (`DEFAULT_PUMP_OVERRUN_MS`), permițând pompei centralei să circule agentul termic și să disipeze energia acumulată în schimbătorul primar de căldură.
- **FR-4 (Anti-Short-Cycling Guard):**
  - Pentru a proteja vana de gaz și arzătorul centralei de uzură prematură, sistemul interzice repornirea flăcării la un interval mai scurt de 3–5 minute (`DEFAULT_MIN_BOILER_OFF_MS`) de la ultima oprire.

### 3.3. Comandă Radio Centrală pe 433.92 MHz (FR-5)
- **FR-5.1 (Compatibilitate Semnal Original):** Sistemul reproduce cu precizie la nivel de microsecundă semnalul radio decodificat de la vechiul termostat de cameră:
  - Modulație: OOK / ASK pe frecvența 433.92 MHz.
  - Structură tren: 409 tranziții de nivel logic, preambul de liniște de 65 ms, tren de date de ~131 ms și postambul de liniște de 65 ms.
- **FR-5.2 (Antenă & Hardware Transmisie):** Folosirea plăcii fiice RF cu antenă perimetrică acordată sau modul SYN115 cu antenă calibrată de sfert de undă (17.3 cm cupru) pentru acoperirea sigură a întregului apartament prin pereți de beton.

### 3.4. Telemetrie & Ingestie Senzori de Mediu (FR-6)
- **FR-6.1 (Senzori Suportați):** Termometre digitale Xiaomi Mijia LYWSD03MMC (ecran LCD, firmware custom ATC / BTHome v2).
- **FR-6.2 (Parametri Monitorizați):** Temperatură (rezoluție 0.1°C), Umiditate relativă (rezoluție 1%), Nivel baterie (%), Calitate semnal (RSSI dBm).
- **FR-6.3 (Arhitectură Hibridă de Ingestie):**
  - **Canal Local (Nativ ESP32):** Scanare BLE pasivă direct pe cipul ESP32-WROOM-32E folosind librăria optimizată NimBLE.
  - **Canal Secundar / Fallback (MQTT):** Preluare pachete transmise prin brokerul Mosquitto de pe telefonul gateway Galaxy A6 (`home/sensors/ble/#`).

### 3.5. Consolă Virtuală Serială Bidirecțională (2-Way Network Console) (FR-7)
- **FR-7.1 (Telnet TCP pe portul 23):** Server Telnet activ permanent pe microcontroler, permițând monitorizarea wireless directă din terminal (`pio device monitor`) cu `monitor_port = tcp://192.168.1.20:23`.
- **FR-7.2 (Consolă Web la `/console`):** Pagină web dedicată (dark theme) cu afișare în timp real a logurilor (ring buffer 8 KB în RAM), istoric de comenzi (taste Up/Down), butoane rapide de acțiune și streaming fără pierderi (`X-Log-Seq`).
- **FR-7.3 (Control Bidirecțional):** Orice comandă trimisă din consolă (`STATUS`, `ON`, `OFF`, `AUTO`, `MANUAL`, `SET <z> <temp>`, `HELP`) este executată de procesor, iar răspunsul este transmis sincron către Web, Telnet și Serial USB.

### 3.6. Interfață Utilizator & Acces la Distanță (FR-8)
- **FR-8.1 (Dashboard Principal Web):** Pagină responsive optimizată pentru mobil și desktop la `http://192.168.1.20/` (și prin reverse proxy Galaxy A6 `http://galaxy-a6/heating/`):
  - Stare flacără centrală (ON/OFF).
  - Selector mod AUTOMAT / MANUAL.
  - Carduri individuale pentru fiecare zonă cu afișare temperatură curentă, temperatură țintă, butoane +/- 0.5°C și stare releu.
  - Listă telemetrie senzori de cameră cu baterie și semnal.
- **FR-8.2 (Acces de la Distanță Securizat):** Accesibil de oriunde din afara locuinței prin rețeaua criptată **Tailscale VPN** (`http://100.83.135.74/heating/`).
- **FR-8.3 (Actualizări Wireless Web OTA):** Pagină dedicată de încărcare firmware securizată cu parolă la `http://192.168.1.20/update` (durată flash sub 7 secunde).

### 3.7. Compensare Climatică & Temperatură Exterioară (FR-9)
- Preluare temperatură exterioară prin receptor radio SYN480R de la senzorul stației meteo Tronic 4-LD7396-2 de pe peretele de Nord.
- Fallback automat pe API-ul meteo gratuit Open-Meteo prin Wi-Fi dacă senzorul fizic este offline.

### 3.8. Acces Cloud la Distanță, Diagnoză Flotă & Tesla-Style Service Mode (FR-10)
- **FR-10.1 (Traversare Automată NAT/Firewall fără Port-Forwarding):** Dispozitivul ESP32 inițiază o conexiune criptată de ieșire (Outbound TLS WebSocket/MQTTS pe portul 443/8883) către un Server Central Cloud. Funcționează instantaneu pe orice rețea rezidențială (Digi, Orange, Vodafone CGNAT) fără configurare de router sau IP public.
- **FR-10.2 (Standard Diagnostic Trouble Codes - DTC):** Microcontrollerul monitorizează continuu starea subsistemelor și înregistrează coduri standardizate de eroare:
  - `DTC-101`: `ERR_SENSOR_TIMEOUT` (lipsă telemetrie cameră >20 min).
  - `DTC-102`: `WARN_BATTERY_LOW` (baterie CR2032 senzor sub 20%).
  - `DTC-103`: `ERR_TEMP_OUT_OF_BOUNDS` (citire aberantă senzor <5°C sau >45°C).
  - `DTC-201`: `WARN_HIGH_CYCLE_RATE` (comutări excesive relee/oră).
  - `DTC-202`: `ERR_ACTUATOR_STUCK_SHUT` (cerere căldură >90 min fără creștere de temperatură - posibil robinet blocat).
  - `DTC-401`: `WARN_WIFI_SIGNAL_DEGRADED` (RSSI < -82 dBm).
  - `DTC-402`: `WARN_HEAP_FRAGMENTATION` (memorie RAM liberă < 60 KB).
- **FR-10.3 (Tesla-Style Service Mode & Phone Troubleshooting):** Mod de mentenanță activabil securizat la distanță (`/api/diag/service?enable=true` sau `DIAG SERVICE ON`):
  - Permite depanatorului, în timpul convorbirii telefonice cu clientul, să suspende temporar deciziile automate, să acționeze manual fiecare actuator pe rând, să testeze emisia radio către cazan și să inspecteze telemetria în timp real.
- **FR-10.4 (Suita Onboard Self-Test):** Rutină integrată de autodiagnostic (`/api/diag/selftest` sau `DIAG SELFTEST`) care verifică în mai puțin de 500 ms memoria Heap, Wi-Fi, stiva BLE NimBLE, bridge-ul MQTT, engine-ul termic și emițătorul RF 433 MHz, returnând un Health Score procentual (0-100%).
- **FR-10.5 (Actualizare Firmware de la Distanță - Remote Fleet OTA):** Posibilitate de actualizare fără fir a firmware-ului de la birou/acasă pentru întreaga flotă de clienți, prin tunelul securizat cloud.

### 3.9. Procedură de Configurare la Client & Cartografiere Dinamică Senzori BLE (FR-11)
- **FR-11.1 (Produs Comercial Fără MAC-uri Hardcodate):** Sistemul nu depinde de MAC-uri sau denumiri pre-programate în codul sursă. Orice senzor BLE compatibil (BTHome v2 sau PVVX/ATC) despachetat din cutie este detectat automat în aer.
- **FR-11.2 (Asistent Vizual de Instalare & Descoperire Senzori - Setup Wizard):** Interfață web dedicată la `/setup` și endpoint `/api/setup/discovered-sensors`:
  - Afișează toți senzorii detectați în eter, ordonați în timp real după intensitatea semnalului RSSI (senzorul ținut în mână de instalator lângă distribuitor apare primul).
  - Indică adresa MAC, temperatura curentă, nivelul bateriei și dacă este deja asociat unei zone.
- **FR-11.3 (Cartografiere Flexibilă Zone <-> Relee <-> Senzori):**
  - Instalatorul sau clientul asignează printr-un singur clic fiecare senzor găsit la Zona Termică dorită (ex: Zona 1 = Living, Releu GPIO32, Senzor `A4:C1:38:...`).
  - Posibilitate de a numi camerele personalizat și de a seta pragurile de temperatură țintă per zonă.
- **FR-11.4 (Persistență Non-Volatilă NVS / Preferences):** Toate mapările zonă-senzor, numele încăperilor și setările se salvează în memoria flash non-volatilă (ESP32 Preferences / NVS). Sistemul rezistă la reporniri, căderi de tensiune și deconectări de la rețea.

### 3.10. Sincronizare Automată Dată, Timp & Vreme prin Wi-Fi (FR-12)
- **FR-12.1 (Sincronizare Oră & Dată via NTP):** La conectarea la Wi-Fi, microcontrolerul sincronizează automat ceasul intern prin Network Time Protocol (NTP, `pool.ntp.org` / `time.google.com`) cu fusul orar al României (`EET-2EEST,M3.5.0/3,M10.5.0/4`), gestionând automat trecerea la ora de vară/iarnă.
- **FR-12.2 (Detectare Sezon & Regim de Funcționare):** Sistemul deduce automat anul, luna, ziua, ora și determină dacă se află în **Sezonul de Încălzire** (ex. 15 Octombrie – 15 Aprilie) sau în **Extra-sezon** (regim de vară, cu exercițiu automat de mișcare periodică a vanelor pentru prevenirea blocării axului mecanic).
- **FR-12.3 (Vreme Exterioară prin Wi-Fi Open-Meteo REST API):** Preluare automată a temperaturii exterioare la fiecare 15 minute prin apel HTTP REST către Open-Meteo, pentru reglaj predictiv și compensare climatică chiar și în absența sondei radio externe.
- **FR-12.4 (Selectare Inteligentă a Locației - Zero Input Coordonate GPS):** 
  - Fără introducere manuală de latitudine/longitudine (zero fricțiune pentru utilizator sau instalator).
  - Căutare inteligentă cu auto-suggest live pentru orice oraș, comună sau sat din România (interogare directă Geocoding Open-Meteo).
  - Listă rapidă predefinită cu toate cele 41 de reședințe de județ + București (funcționare 100% offline).
  - Buton dedicat „📍 Detectează (GPS/IP)” care preia automat coordonatele și localitatea cu 1 singur clic de pe telefon sau laptop.

### 3.11. Pașaport Digital de Instalare & Jurnal Mentenanță Dispozitiv (FR-13)
- **FR-13.1 (Identificator Hardware & Număr Serial Unic):** Generat automat pe baza MAC-ului nativ al cipului ESP32 (ex: `SH-20500D1A7214`).
- **FR-13.2 (Data & Ora Primei Puneri în Funcțiune - First Commissioning):** La prima instalare la client, sistemul înregistrează automat în memoria NVS timestamp-ul exact obținut prin NTP (an, lună, zi, oră).
- **FR-13.3 (Metadate Client & Instalație):** Salvare persistentă a fișei tehnice de montaj:
  - Numele clientului / Număr de contact.
  - Localitate / Oraș / Comună & Adresă (selectată inteligent sau auto-detectată).
  - Jurnal notițe instalator: model centrală pe gaz, tip distribuitor și filet robineți (M30x1.5), număr circuite pardoseală, data ultimei revizii.
  - Contor timp total de viață (Life Uptime, zile de funcționare continuă de la instalare).
- **FR-13.4 (Acces & Editare Pașaport):** Endpoint dedicat `/api/setup/passport` (GET/POST) și card integrat în interfața `/setup` pentru acces facil de pe mobil la montaj.

---

## 4. Cerințe Non-Funcționale (Non-Functional Requirements)

### 4.1. Siguranță & Comportament la Avarie (NFR-1: Safety & Failsafe)
- **NFR-1.1 (Autonomie Totală la Edge):** Toate deciziile de deschidere/închidere relee și pornire/oprire centrală sunt executate local în memoria ESP32. Căderea conexiunii Wi-Fi, a routerului, a brokerului MQTT sau a conexiunii cloud **NU întrerupe încălzirea**.
- **NFR-1.2 (Failsafe Cădere Senzor):** Dacă o zonă nu primește nicio citire de temperatură validă timp de peste 30 de minute, zona este marcată cu avarie, releul zonei este închis preventiv și se emite codul `DTC-101` pentru a evita supraîncălzirea necontrolată.
- **NFR-1.3 (Protecție la Îngheț):** Chiar dacă o zonă este dezactivată sau setată pe minim, dacă temperatura scade sub 12°C, sistemul forțează încălzirea de protecție la îngheț.
- **NFR-1.4 (Pornire Sigură la Revenirea Curentului):** După o pană de curent, releele pornesc în mod implicit în stare `LOW` (OPRITE), iar centrala este considerată oprită până la prima evaluare termică stabilă.

### 4.2. Performanță & Eficiență Resurse (NFR-2)
- **NFR-2.1 (Consum Energie):** Placa ESP32 consumă sub 1.2W la priză în regim continuu.
- **NFR-2.2 (Memorie RAM Liberă):** Arhitectura C++ pe ESP32 păstrează peste 230 KB de memorie heap liberă în orice moment (fără scurgeri de memorie la streaming de loguri sau parsare JSON).
- **NFR-2.3 (Răspuns Rapid):** Timpul de răspuns al API-ului web local este sub 50 ms.

### 4.3. Calitate Cod & Standarde (NFR-3)
- **NFR-3.1 (English Codebase Standard):** Toate fișierele sursă C++, anteturile, variabilele, comentariile, textele UI și logurile din consolă sunt redactate **100% în limba engleză**.
- **NFR-3.2 (Modularitate):** Fiecare subsistem este decuplat în clase C++ de sine stătătoare (`NetworkConsole`, `ThermalZoneManager`, `BoilerRadioTrigger`, `MqttTelemetryClient`, `BleSensorScanner`, `DiagnosticEngine`, `DevicePassport`, `TimeAndWeatherService`).

---

## 5. Matrice de Trasabilitate (Cerințe vs Componente Software)

| Cod Cerință | Denumire Cerință | Modul / Fișier Implementat | Stare Validare |
|:---:|---|---|:---:|
| **FR-1** | Histerezis & Reglaj Multi-Zonă | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | Validat Live |
| **FR-2** | Actuator Interlock Delay (120s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L224) | Validat Live |
| **FR-3** | Pump Overrun Heat Dissipation (120s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L240) | Validat Live |
| **FR-4** | Anti-Short-Cycling Boiler Guard | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L274) | Validat Live |
| **FR-5** | Comandă Radio Centrală 433.92 MHz | [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) | Validat Live |
| **FR-6** | Telemetrie Xiaomi BLE (Nativ NimBLE + MQTT) | [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp) | Validat Live |
| **FR-7** | Consolă Virtuală 2-Way (Telnet & Web) | [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) | Validat Live |
| **FR-8** | Dashboard Web & REST API | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp#L42) | Validat Live |
| **FR-8.3**| Actualizare Firmware Web OTA | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp#L512) | Validat Live (6.4s) |
| **FR-10**| Diagnoză DTC & Tesla Service Mode | [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp) | Validat Live |
| **FR-11**| Setup Wizard & Mapare Dinamică Senzori | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) & [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | Validat Live |
| **FR-12**| Sincronizare NTP Dată, Timp & Vreme Wi-Fi | [`src/TimeAndWeatherService.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/TimeAndWeatherService.cpp) | Validat Live |
| **FR-13**| Pașaport Digital Dispozitiv & Jurnal Montaj | [`src/DevicePassport.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DevicePassport.cpp) | Validat Live |
| **NFR-1** | Operare Autonomă fără Cloud | Arhitectură nativă firmware ESP32 | Validat Live |
