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

### 3.8. Acces Cloud la Distanță, Diagnoză Flotă & Service Mode (FR-10)
- **FR-10.1 (Traversare Automată NAT/Firewall fără Port-Forwarding):** Dispozitivul ESP32 inițiază o conexiune criptată de ieșire (Outbound TLS WebSocket/MQTTS pe portul 443/8883) către un Server Central Cloud. Funcționează instantaneu pe orice rețea rezidențială (Digi, Orange, Vodafone CGNAT) fără configurare de router sau IP public.
- **FR-10.2 (Standard Diagnostic Trouble Codes - DTC):** Microcontrollerul monitorizează continuu starea subsistemelor și înregistrează coduri standardizate de eroare:
  - `DTC-101`: `ERR_SENSOR_TIMEOUT` (lipsă telemetrie cameră >20 min).
  - `DTC-102`: `WARN_BATTERY_LOW` (baterie CR2032 senzor sub 20%).
  - `DTC-103`: `ERR_TEMP_OUT_OF_BOUNDS` (citire aberantă senzor <5°C sau >45°C).
  - `DTC-201`: `WARN_HIGH_CYCLE_RATE` (comutări excesive relee/oră).
  - `DTC-202`: `ERR_ACTUATOR_STUCK_SHUT` (cerere căldură >90 min fără creștere de temperatură - posibil robinet blocat).
  - `DTC-401`: `WARN_WIFI_SIGNAL_DEGRADED` (RSSI < -82 dBm).
  - `DTC-402`: `WARN_HEAP_FRAGMENTATION` (memorie RAM liberă < 60 KB).
- **FR-10.3 (Service Mode & Phone Troubleshooting):** Mod de mentenanță activabil securizat la distanță (`/api/diag/service?enable=true` sau `DIAG SERVICE ON`):
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

### 3.12. Control Pompă Secundară & Failsafes Hidraulice pentru Grupuri de Amestec GPA (FR-14)
- **FR-14.1 (Control Automat Pompă de Recirculare pe Releu 4 / GPIO26):** Comandă automată dedicată pompei secundare din grupul de pompare și amestec (GPA) specific caselor, duplexurilor sau distribuitoarelor cu pompă adițională.
- **FR-14.2 (Interblocare Hidraulică Anti-Cavitație):** Pompa secundară pornește DOAR DUPĂ ce cel puțin un actuator termic a atins cursa completă de deschidere. Elimină riscul funcționării pompei pe circuite închise (fără debit).
- **FR-14.3 (Post-circulație & Disipare Căldură Reziduală - 180s):** După stingerea flăcării centralei, pompa continuă să funcționeze 3 minute, menținând deschisă ultima zonă activă (*keeper zone*) pentru a răci schimbătorul de căldură și a preveni șocul termic.
- **FR-14.4 (Exercițiu Automat de Vară Anti-Gripare - Anti-Seize 20s):** Dacă pompa și încălzirea nu au fost active timp de 7 zile consecutive, sistemul deschide automat toate actuatoarele și pornește pompa timp de 20 de secunde. Previne depunerea de calcar și blocarea mecanică a rotorului pompei în extrasezon.

### 3.13. Compatibilitate Universală Actuatoare NC vs NO (FR-15)
- **FR-15.1 (Polaritate Electrică Configurabilă în NVS):** Setare selectabilă din software (`act_nc: bool`) prin REST API (`/api/setup/actuator-type`) și CLI:
  - **NC (Normally Closed - Implicit):** Releul se anclanșează (230V aplicat) la cerere de căldură pentru deschidere.
  - **NO (Normally Open):** Releul rămâne neanclanșat pentru deschidere și se alimentează doar când zona este oprită.
- **FR-15.2 (Compatibilitate cu Orice Distribuitor de pe Piață):** Permite montarea kitului pe instalații existente fără a schimba actuatoarele funcționale ale clientului (fie ele NC sau NO).

### 3.14. Protecție Forțată la Îngheț & Izolare Failsafe Senzori (FR-16)
- **FR-16.1 (Emergency Frost Guard < 12°C):** Declanșare automată de urgență a căldurii dacă temperatura oricărei încăperi coboară sub 12°C, suprascriind inclusiv modurile MANUAL sau OFF.
- **FR-16.2 (Izolare Avarie Senzor DTC-101):** Oprirea imediată a actuatorului zonei la lipsa telemetriei peste 30 de minute (protejează parchetul de supraîncălzire în caz de baterie epuizată a termometrului) și restaurarea instantanee a stării normale de îndată ce sosesc pachete valide.

### 3.15. Protecție Anti-Spam & Interblocare de Siguranță Arzător Cazan (FR-17)
- **FR-17.1 (Filtru de Idempotență & No-Op Bypass):**
  - Dacă o comandă de pornire (`ON`) este primită când starea curentă a centralei este deja `ON`, sau o comandă de oprire (`OFF`) când starea este deja `OFF`:
  - Comanda este tratată instantaneu ca no-op fără a bloca procesorul și fără a transmite pachete radio fizice în eter (zero poluare RF).
  - Răspunsul HTTP REST returnează succes idempotent (`200 OK` cu `{"status":"ok","boiler":"ON","idempotent":true}`).
- **FR-17.2 (RF Transmission Cooldown - 3 secunde):**
  - Emițătorul radio 433.92 MHz impune un interval minim de liniște de 3.000 ms (`MIN_RF_COOLDOWN_MS`) între oricare două trenuri de impulsuri RF consecutive.
  - Orice solicitare de transmisie în cadrul acestei ferestre este respinsă la nivelul driverului fizic.
- **FR-17.3 (Interblocare Anti-Short-Cycling pentru Comenzi Manuale & REST API):**
  - **Timp Minim de Ardere Continuă (Min Run Time - 60s):** Odată ce flacăra cazanului a fost aprinsă (`ON`), nicio comandă manuală de oprire (`OFF`) nu este acceptată timp de minimum 60 de secunde (asigură stabilizarea arderii și evacuarea completă a gazelor arse).
  - **Timp Minim de Pauză Arzător (Min Pause Time - 60s):** Odată ce flacăra a fost stinsă (`OFF`), nicio comandă manuală de pornire (`ON`) nu este acceptată timp de minimum 60 de secunde (previne aprinderea repetată pe amestec nears și uzura bobinelor vanei de gaz).
  - *Override de Urgență:* În caz de depanare tehnică sau avarie, modul Service (`Service Mode`) sau un parametru explicit (`force=true`) permite suprascrierea interblocării de către un operator autorizat.
- **FR-17.4 (Răspuns Determinist HTTP 429 & Feedback CLI):**
  - Dacă o comandă este respinsă de interblocarea anti-spam / anti-short-cycling, serverul web returnează răspuns determinist HTTP `429 Too Many Requests` cu timpul de așteptare restant:
    `{"status":"rate_limited","error":"Anti-short-cycling guard active","waitSec":45,"currentBoilerState":"ON"}`.
  - Utilitarul CLI [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) interceptează starea și afișează avertismentul vizual în terminal fără excepții.
- **FR-17.5 (Detecție Spam Abuziv & Declanșare Avarie DTC-201):**
  - Dacă se înregistrează mai mult de 3 încercări respinse de comutare rapidă într-o fereastră de 10 secunde (detecție spam repetat de la utilizator sau buclă infinită dintr-un script extern):
  - Sistemul înregistrează codul de diagnosticare `DTC-201` (`WARN_RELAY_SHORT_CYCLING` / `WARN_BOILER_SPAM_DETECTED`) și emite avertisment în consola de rețea.
- **FR-17.6 (Protecție Watchdog TWDT & Zero Blocaj CPU):**
  - Pre-validarea comenzilor se execută în sub 1 ms. Comenzile respinse nu invocă funcția `transmitBurst()`, garantând că CPU-ul nu blochează întreruperile (`noInterrupts()`) și că Watchdog-ul Hardware TWDT (30s) este alimentat normal.

---

### 3.3. Failsafe-uri Avansate, Calibrare Senzori & Versionare (FR-18 - FR-21)
- **FR-18 (Failsafe Timeout Senzori Configurabil):**
  - Utilizatorul / instalatorul poate seta durata maximă acceptată de lipsă semnal de la un senzor termic înainte de decuplarea automată a zonei afectate (failsafe OFF cu `DTC-101`).
  - Domeniu valid: 60s – 7200s (implicit: 600s / 10 min), salvat persistent în NVS (`sns_tout`).
  - Accesibil prin REST API (`GET/POST /api/setup/sensor-timeout?timeoutSec=...`) și consolă CLI (`tools/esp_cli.py timeout <sec>`).
  - Test de verificare: `SWE.5-TC13`. Status: `[VERIFIED]`.
- **FR-19 (Exercițiu Anti-Blocare Calendaristic pe UNIX Epoch):**
  - Ciclul săptămânal de 20s de exercițiu al pompei GPA și al vanelor este programat pe baza timpului real calendaristic (`time_t nowEpoch = time(nullptr)`) și salvat în NVS (`last_seize_ep`).
  - Elimină declanșările premature la repornirea alimentării (reboot/cold boot); la prima sincronizare NTP, dacă nu există istoric, data este memorată și următorul ciclu este decalat cu 7 zile (604.800 secunde).
  - Test de verificare: `SWE.5-TC06`. Status: `[VERIFIED]`.
- **FR-20 (Indicatori Calitate Semnal BLE - Link Quality):**
  - Evaluare automată pe clase de intensitate radio pentru fiecare senzor Xiaomi detectat:
    * `EXCELLENT` ($\ge -72$ dBm)
    * `GOOD` ($-73$ .. $-82$ dBm)
    * `MARGINAL` ($-83$ .. $-88$ dBm)
    * `POOR` ($< -88$ dBm)
  - Afișat în tabelul Setup Wizard Web (`/setup`) și utilitarul CLI (`tools/esp_cli.py ble` și `status`) pentru asistență imediată la montajul fizic în locuință.
  - Test de verificare: `SWE.5-TC03`. Status: `[VERIFIED]`.
- **FR-21 (Versionare Firmware & Identitate Baseline):**
  - Fiecare imagine de firmware compilată încorporează număr de versiune SemVer (`APP_VERSION`, ex: `1.2.0`), timestamp compilare (`APP_BUILD_DATE " " APP_BUILD_TIME`), hash commit Git (`APP_GIT_COMMIT`) și cauza exactă de resetare hardware (`esp_reset_reason()`).
  - Expuse în `/api/boiler/status`, utilitarul CLI (`tools/esp_cli.py status`), antetul comenzii `STATUS` și mesajul de boot (POST) la pornirea plăcii.
  - Test de verificare: `SWE.5-TC14`. Status: `[VERIFIED]`.
- **FR-22 (Perioadă de Grație Warmup Senzori la Boot & Activare Selectivă):**
  - La pornirea plăcii ESP32 (cold boot sau reboot software), sistemul acordă o fereastră de grație de maximum 5 minute (300 secunde) în starea dedicată `ZONE_WARMUP` pentru recepționarea balizelor BLE pasive (interval de emisie 30–60s).
  - Comenzile automate de încălzire și aprinderea arzătorului sunt suspendate preventiv pe durata warmup-ului pentru a proteja echilibrul hidraulic al locuinței (cu excepția protecției de urgență la îngheț `<12°C` care acționează imediat).
  - **Ieșire Anticipată (Early Exit):** De îndată ce toți senzorii asociați (ex: 3/3) au fost detectați, warmup-ul se oprește imediat (`[SCANNING COMPLETE - ALL ACQUIRED]`) și buclele devin 100% operaționale fără timp mort.
  - **Decuplare Selectivă & Ridicare DTC-101 (Timeout Cutoff):** Dacă trec 5 minute și anumiți senzori nu au emis balize (baterie descărcată sau distanță prea mare):
    * Se ridică automat codul `DTC-101` (`"Zone X sensor missing at boot warmup cutoff"`) salvat persistent în NVS;
    * Se izolează în failsafe OFF exclusiv buclele nefuncționale;
    * Sistemul pornește încălzirea normal doar pentru zonele complet funcționale.
  - Test de verificare: `SWE.5-TC03`, `TC13`. Status: `[VERIFIED]`.
- **FR-23 (Format Tabelar Aliniat & Registru Calitate Semnal Consolă):**
  - Răspunsurile comenzilor `STATUS`, `BLE` și `SENSORS` sunt structurate în tabele cu lățimi fixe și aliniere verticală milimetrică:
    * Bucle pardoseală (`[ZONES]`): `ID`, `Zone Name`, `Current Temp` (sau `WARMUP`), `Target Temp`, `Status`, `Actuator Valve (Relay GPIO)`.
    * Registru termometre (`[SENSORS]`): `Sensor Name`, `MAC Address`, `Temp`, `Humidity`, `Battery`, `Signal (Min/Cur/Max dBm)`, `Packets`, `Last Seen`.
    * Linie dedicată de protecții: `[PROTECTIONS] Sensor Offline Failsafe: 10 min (all zones) | Pump Anti-Seize: Weekly (Ran: ... | Next in ...)`.
    * Indicator clar stare achiziție: `[SCANNING: %us/300s in progress]` vs `[SCANNING COMPLETE - ALL ACQUIRED]`.
  - Test de verificare: `SWE.5-TC12`. Status: `[VERIFIED]`.

- **FR-24 (Telemetrie Hardware Avansată - CPU, RAM, NVM, Temp Cip):**
  - Monitorizare hardware directă în comanda `STATUS`:
    * Procesor: Model cip, număr nuclee, frecvență și temperatură internă die (`Die Temp: %.1f °C` via `temperatureRead()`);
    * Memorie RAM: Memorie heap liberă / totală, watermark minim atins de la boot (`Lowest: %u KB`) și cel mai mare bloc contiguu alocabil (`Max Block: %u KB`) pentru diagnosticarea timpurie a fragmentării;
    * Memorie Non-Volatilă Flash: Mărime și frecvență flash, stare NVS (`Active`), spațiu ocupat de firmware și marjă liberă pentru update OTA (`OTA Margin: %.2f MB free`).
  - Test de verificare: `SWE.5-TC01`, `TC14`. Status: `[VERIFIED]`.
- **FR-25 (Carduri Zonale Integrate & Simbol Nativ UTF-8 `°C`):**
  - Eliminarea tabelelor separate aglomerate; fiecare cameră este reprezentată printr-un card complet integrând atât parametrii hidraulici și termici (temperatură țintă, curentă, umiditate, stare buclă, stare valvă, releu asociat), cât și parametrii radio ai senzorului BLE asociat (denumire, adresă MAC, baterie, semnal cu link badge, număr pachete, vechime).
  - Toate temperaturile utilizează simbolul standard UTF-8 `°C`, servit cu header `text/plain; charset=utf-8` pentru redare corectă în orice terminal.
  - Test de verificare: `SWE.5-TC01`, `TC12`. Status: `[VERIFIED]`.
- **FR-26 (Stilizare & Mapare Culori Exhaustivă Web Console `/console`):**
  - Recunoaștere case-insensitive (`/gi`) în scriptul de colorare din browser pentru toate cuvintele de stare:
    * Verde aprins (`.t-ok`): `PASS`, `Online`, `Enabled`, `Running`, `Active`, `Open`, `ON`;
    * Roșu alertă (`.t-off`): `FAIL`, `Offline`, `Disabled`, `Stopped`, `Closed`, `OFF`, `Timeout`;
    * Albastru (`.t-idle`): `Idle`, `Standby`;
    * Galben cald (`.t-warm`): `Warmup`, `Syncing`, `Scanning...`;
    * Portocaliu (`.t-demand`): `Demand`, `Heating`;
    * Violet / Indigo (`.t-auto`, `.t-man`): `Automatic`, `Auto`, `Manual`.
  - Negative lookahead `(?!-)` pe `OPEN` pentru protejarea numelui `Open-Meteo`.
  - Test de verificare: `SWE.5-TC12`. Status: `[VERIFIED]`.
- **FR-27 (Format Tabelar Structural pe Fiecare Subsistem & Aliniere `utf8VisibleLen`):**
  - Ieșirea comenzii `STATUS` este împărțită în blocuri tabelare dedicate pe 2 coloane paralele numerotate alfabetic (`[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`), aerisite prin linii goale (newlines) între categorii și între cardurile camerelor.
  - Implementat algoritmul `utf8VisibleLen()` care calculează lungimea pe baza caracterelor vizibile pe ecran (compensând octeții suplimentari ai simbolului de grad `°` din UTF-8), garantând că delimitatorul `|` și a doua coloană încep la fix aceeași poziție fără deplasare de 1-2 caractere.
  - Integrare link oficial de configurare și reglare putere de emisie senzori Xiaomi BLE (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) în Web Console, Setup Wizard, Web Dashboard și `HELP`.
  - Test de verificare: `SWE.5-TC12`. Status: `[VERIFIED]`.
- **FR-28 (Sensibilitate Ridicată NimBLE prin Pereți & Warmup Non-Blocant):**
  - Creșterea ferestrei de scanare NimBLE la 90ms dintr-un interval de 100ms (90% duty cycle) în `include/Config.h`, permițând captarea rapidă a senzorilor din încăperi îndepărtate (după 4 pereți) în 39s–64s în loc de 293s.
  - Buclele de încălzire care și-au achiziționat deja senzorul la pornire își reiau imediat funcționarea termostatică normală dacă au cerere termică, fără a bloca întreaga locuință în așteptarea senzorilor îndepărtați (care rămân în siguranță cu vana închisă în `ZONE_WARMUP`).
  - Test de verificare: `SWE.5-TC03`, `TC13`. Status: `[VERIFIED]`.

---

## 4. Cerințe Non-Funcționale (Non-Functional Requirements)

### 4.1. Siguranță & Comportament la Avarie (NFR-1: Safety & Failsafe)
- **NFR-1.1 (Autonomie Totală la Edge):** Toate deciziile de deschidere/închidere relee și pornire/oprire centrală sunt executate local în memoria ESP32. Căderea conexiunii Wi-Fi, a routerului, a brokerului MQTT sau a conexiunii cloud **NU întrerupe încălzirea**.
- **NFR-1.2 (Failsafe Cădere Senzor):** Dacă o zonă nu primește nicio citire de temperatură validă timp de peste `sensorTimeoutSec` (implicit 600s), zona este marcată cu avarie, releul zonei este închis preventiv și se emite codul `DTC-101` pentru a evita supraîncălzirea necontrolată.
- **NFR-1.3 (Protecție la Îngheț):** Chiar dacă o zonă este dezactivată sau setată pe minim, dacă temperatura scade sub 12°C, sistemul forțează încălzirea de protecție la îngheț.
- **NFR-1.4 (Pornire Sigură la Revenirea Curentului):** După o pană de curent, releele pornesc în mod implicit în stare `LOW` (OPRITE), se emite burst RF `BOILER STOP`, iar sistemul intră în perioada de Warmup fără a porni nicio comandă termică nevalidată.
- **NFR-1.5 (Hardware Task Watchdog Timer - TWDT 90s):** Supraveghere hardware activă nativă ESP-IDF pe bucla `loop()` cu resetare automată la blocaj software. Calibrat la 90s pentru a asigura o marjă de 100% la transferul și scrierea blocurilor flash în timpul actualizărilor Web OTA (1.28 MB) fără resetări premature.
- **NFR-1.6 (Limită Ciclu Termic Arzător Cazan - Min 120s per Ciclu):** Indiferent de originea comenzilor (algoritm automat, REST API, Home Assistant sau consolă CLI), frecvența maximă permisă de ciclare a flăcării este limitată la cel mult 1 ciclu per 120 secunde (60s arsură minimă + 60s pauză minimă), prevenind avarierea vanei de gaz și intrarea centralei în avarie mecanică (*Flame Lockout*).
- **NFR-1.7 (Invarianți Failsafe Termohidraulic la Pană de Tensiune & Defecțiuni):**
  - *Pană de curent în timpul arderii:* Electrovalva de gaz a cazanului (solenoid 230V) închide etanș admisia de gaz în <50 ms; actuatoarele NC se închid mecanic prin arc; căldura reziduală din arzător este absorbită de volumul de apă din schimbătorul primar (1.5–2L) fără risc de fierbere/suprapresiune. Supapa mecanică de 3 bar reprezintă bariera finală de siguranță.
  - *Avarie cazan (Lockout EN 298):* Modulul autonom al centralei blochează aprinderea și ignoră orice contact de termostat/RF. Microcontrolerul ESP32 detectează stagnarea termică via `DTC-102`.
  - *Actuator blocat pe închis:* Distribuitorul include vană de by-pass diferențial cu arc (sau pompă GPA modulantă pe $\Delta P$), prevenind cavitația și supraîncălzirea motorului pompei.

### 4.2. Performanță & Eficiență Resurse (NFR-2)
- **NFR-2.1 (Consum Energie):** Placa ESP32 consumă sub 1.2W la priză în regim continuu.
- **NFR-2.2 (Memorie RAM Liberă):** Arhitectura C++ pe ESP32 păstrează peste 115 KB de memorie heap liberă în orice moment (cu stiva NimBLE activă).
- **NFR-2.3 (Răspuns Rapid):** Timpul de răspuns al API-ului web local este sub 100 ms.
- **NFR-2.4 (Ergonomie Consolă & Fără Spam):** Fiecare linie din consolă include timestamp NTP `[HH:MM:SS]`. Web Console include filtrare automată împotriva duplicării comenzilor și elimină poluarea cu pagini de eroare HTML (Nginx 502).
- **NFR-2.5 (Paced Socket Streaming OTA):** Încărcarea firmware-ului prin scriptul `tools/ota_upload.py` utilizează transfer cadențat cu pauze de 12ms per pachet (~110 KB/s), prevenind epuizarea bufferelor TCP `pbuf` din stiva lwIP a ESP32 și asigurând o scriere stabilă în flash în sub 14 secunde fără retransmisii sau căderi de conexiune.

### 4.3. Calitate Cod & Standarde (NFR-3)
- **NFR-3.1 (English Codebase Standard):** Toate fișierele sursă C++, anteturile, variabilele, comentariile, textele UI și logurile din consolă sunt redactate **100% în limba engleză**.
- **NFR-3.2 (Modularitate):** Fiecare subsistem este decuplat în clase C++ de sine stătătoare (`NetworkConsole`, `ThermalZoneManager`, `BoilerRadioTrigger`, `BleSensorScanner`, `DiagnosticEngine`, `DevicePassport`, `TimeAndWeatherService`).

---

## 5. Matrice de Trasabilitate (Cerințe vs Componente Software vs Teste SWE.5)

| Cod Cerință | Denumire Cerință | Modul / Fișier Implementat | Test Automat SWE.5 | Stare Validare |
|:---:|---|---|:---:|:---:|
| **FR-1** | Histerezis & Reglaj Multi-Zonă | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC01`, `TC10` | 100% PASS |
| **FR-2** | Actuator Interlock Delay (120s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC05` | 100% PASS |
| **FR-3** | Pump Overrun Heat Dissipation (180s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC06` | 100% PASS |
| **FR-4** | Anti-Short-Cycling Boiler Guard (60s) | [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) | `SWE.5-TC08` | 100% PASS |
| **FR-5** | Comandă Radio Centrală 433.92 MHz | [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) | `SWE.5-TC04` | 100% PASS |
| **FR-6** | Telemetrie Xiaomi BLE (Nativ NimBLE) | [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp) | `SWE.5-TC03` | 100% PASS |
| **FR-7** | Consolă Virtuală 2-Way (Telnet & Web) | [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) | `SWE.5-TC12` | 100% PASS |
| **FR-8** | Dashboard Web & REST API | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC01` | 100% PASS |
| **FR-8.3**| Actualizare Firmware Web OTA | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC14` | 100% PASS |
| **FR-10**| Diagnoză DTC & Self-Test Suite | [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp) | `SWE.5-TC04`, `TC09` | 100% PASS |
| **FR-11**| Setup Wizard & Mapare Dinamică | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC02` | 100% PASS |
| **FR-12**| Sincronizare NTP Dată, Timp & Vreme Wi-Fi | [`src/TimeAndWeatherService.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/TimeAndWeatherService.cpp) | `SWE.5-TC11` | 100% PASS |
| **FR-13**| Pașaport Digital Dispozitiv & NVS | [`src/DevicePassport.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DevicePassport.cpp) | `SWE.5-TC02` | 100% PASS |
| **FR-14**| Pompă Recirculare GPA Releu 4 & Anti-Seize | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC06` | 100% PASS |
| **FR-15**| Compatibilitate Actuatoare NC vs NO (NVS) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC05` | 100% PASS |
| **FR-16**| Frost Guard <12°C & Izolare DTC-101 | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC06` | 100% PASS |
| **FR-17**| Anti-Spam & Interblocare Cazane (60s / 3s) | [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) | `SWE.5-TC07`, `TC08`, `TC09` | 100% PASS |
| **FR-18**| Failsafe Timeout Senzori Configurabil | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) & [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC13` | 100% PASS |
| **FR-19**| Exercițiu Anti-Seize pe UNIX Epoch | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC06` | 100% PASS |
| **FR-20**| Indicatori Calitate Semnal BLE (Link Quality) | [`include/WebSetupPage.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/WebSetupPage.h) & [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) | `SWE.5-TC03` | 100% PASS |
| **FR-21**| Versionare Firmware Baseline & Reset Reason | [`include/Version.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Version.h) & [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC14` | 100% PASS |
| **FR-22**| Boot Warmup Grace Period & Activare Selectivă | [`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h) & [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC03`, `TC13` | 100% PASS |
| **FR-23**| Format Tabelar Aliniat & Registru Calitate Semnal | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC12` | 100% PASS |
| **FR-24**| Telemetrie Hardware Avansată (CPU/RAM/NVM) | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC01`, `TC14` | 100% PASS |
| **FR-25**| Carduri Zonale Integrate & Simbol UTF-8 `°C` | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC01`, `TC12` | 100% PASS |
| **FR-26**| Stilizare & Mapare Culori Exhaustivă Web Console | [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) | `SWE.5-TC12` | 100% PASS |
| **FR-27**| Format Tabelar pe Fiecare Subsistem & Aliniere `utf8VisibleLen` | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC12` | 100% PASS |
| **FR-28**| Sensibilitate NimBLE 90% & Warmup Non-Blocant | [`include/Config.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Config.h) & [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | `SWE.5-TC03`, `TC13` | 100% PASS |
| **NFR-1.5**| Hardware Task Watchdog (TWDT 90s) | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp) | `SWE.5-TC04` | 100% PASS |
| **NFR-1.7**| Invarianți Siguranță Termohidraulică (Pană/Avarie) | Arhitectură Hardware / NC Actuators / Solenoid Gaz | Analiză FMEA & Standard EN 298 | 100% PASS |
| **NFR-2.5**| Paced Socket Streaming OTA (~110 KB/s) | [`tools/ota_upload.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/ota_upload.py) | `SWE.5-TC14` | 100% PASS |


