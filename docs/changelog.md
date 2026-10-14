# Changelog: Sistem Inteligent de Încălzire (Smart Heating)

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md).

## [13.10.2026] - Arhitectură Web GZIP Build-Time, Optimizare Flash (-176 KB) & Latență Consolă <50ms

### Optimizare Memorie Flash & Arhitectură Web (Modelul Smart Aquarium)
- **Decuplare Interfețe Web în Fisiere Dedicate (`web/`):**
  - Extrase paginile web din string-urile C++ în fișiere curate HTML/CSS/JS: [`web/index.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/index.html) (Dashboard interactiv), [`web/setup.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/setup.html) (Setup Wizard & Passport) și [`web/console.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/console.html) (Live Web Terminal).
- **Pre-Build Script Automat (`tools/build_web.py`):**
  - Integrat în `platformio.ini` (`extra_scripts = pre:tools/build_web.py`). Comprimă paginile cu `gzip -9` generând headere PROGMEM:
    * `include/dashboard_html_gz.h`: 13.8 KB -> 3.7 KB (27.3%).
    * `include/setup_html_gz.h`: 26.4 KB -> 7.3 KB (27.8%).
    * `include/console_html_gz.h`: 12.8 KB -> 4.4 KB (34.6%).
    * Total pachet web în flash: **doar 15.5 KB** (față de >53 KB text brut).
- **Reducere Masivă Dimensiune Binar & Eliberare Heap:**
  - Binarul de firmware a scăzut de la **1,304,000 bytes la 1,127,833 bytes (1.10 MB, 57.4% flash)** — economie de **176 KB**!
  - Eliminat peste 250 de linii de concatenare C++ `String html += ...` din `main.cpp` și `NetworkConsole.cpp`.
  - Paginile sunt servite cu `server.sendHeader("Content-Encoding", "gzip");` și `server.send_P(200, "text/html", ...);`, eliminând complet alocările dinamice pe heap la încărcarea paginilor (Free Heap stabil la **132 KB**).

### Consolă Web Bidirecțională: Latență <50ms & Delimitatori Vizuali
- **Răspuns Direct în POST (`NetworkConsole.cpp`):**
  - La trimiterea unei comenzi prin Web Console (`POST /api/console/send`), noul output este inclus direct în corpul răspunsului HTTP 200 via `getLogsSince()`.
  - Delimitare vizuală clară: adăugat chenar distinctiv `.cmd-start` cu gradient dark-blue și bordură stânga cyan `#38bdf8` pentru promptul comenzilor (`[CMD] > %s`), plus footer `.cmd-end`.
- **Eliminare Root-Cause Latență 3s (TCP SYN Collision & Buffer Bottlenecks):**
  - *Frontend (`web/console.html`):* Înlocuit `setInterval(fetchLogs, 2000)` necoordonat cu `scheduleNextPoll()` adaptiv și `AbortController`. La trimiterea oricărei comenzi, polling-ul este oprit instant, iar cererea de fundal în zbor este anulată cu `abort()`, prevenind coliziunea a două conexiuni TCP simultane pe serverul WebServer single-threaded (ce declanșa timeout-ul Windows TCP SYN de 3.0 secunde).
  - *Optimizare Scriere Batch (`src/NetworkConsole.cpp`):* Înlocuit ciclul caracter-cu-caracter din `write(buffer, size)` și `rawWrite()` cu scriere pe chunk-uri de linie și `memcpy` direct în ring buffer.
  - *Accelerare Serial UART (`src/main.cpp`):* Mărit bufferul TX hardware `Serial.setTxBufferSize(4096)` pentru a preveni blocarea CPU la 115200 baud la afișarea comenzilor lungi (`STATUS`).
  - *Optimizare `getLogsSince()`:* Înlocuit ciclul `output += char` cu două apeluri de slice `output.concat(...)` (<0.05 ms).
  - *Timestamp Caching:* Memorare în cache a șirului de timestamp pe secunda curentă, eliminând zeci de apeluri redundante de `localtime_r`.

### Verificare și Conformitate
- **Validare SWE.5:** Suita `tools/test_system.py` a trecut cu **100% PASS (15/15 teste)**.
- **Flash OTA:** Actualizare confirmată pe ESP32 (`192.168.1.20`) în 17 secunde cu HTTP 200.

## [13.10.2026] - Smart Bio-Schedule (4 Ancore Naturale), Fereastră Deschisă & Pre-Heat Anticipativ (Sarcina 26)

### Motor Orar Circadian (Smart Bio-Schedule & Ancore de Viață)
- **Eliminare Matrice Orare Rigide (168 celule) în Favoarea a 4 Ancore Naturale:**
  - Înlocuită paradigma clasică a termostatelor programabile complicate cu 4 stări organice de confort:
    * `Sleep` (somn: noapte odihnitoare 20.0–21.0°C).
    * `Wake` (trezire: confort termic dimineața 22.0–22.5°C).
    * `Away` (plecat: regim Eco când casa e nelocuită 19.5–20.0°C).
    * `Home` (acasă: confort termic de seară 21.5–23.0°C).
  - Profiluri separate configurabile pentru Luni–Vineri (zile lucrătoare) și Sâmbătă–Duminică (weekend).
  - Evaluare orară precisă la fiecare 30 secunde corelată cu ceasul de sistem NTP.

### Gardă Inteligentă Geam Deschis (Open Window Smart Pause Guard)
- **Detecție Derivă Termică Rapidă:**
  - Istoric circular de eșantioane de temperatură (10 eșantioane la interval de 30s).
  - Dacă temperatura dintr-o cameră scade cu mai mult de **1.2°C în sub 5 minute**, sistemul deduce deschiderea unei ferestre pentru aerisire.
  - Valva zonei respective este forțată automat în starea **ÎNCHIS (PAUZĂ) timp de 30 de minute**, prevenind pierderea masivă de căldură și consumul inutil de gaz. Anunțat în Network Console și în telemetria `STATUS`.

### Algoritm Anticipativ de Pre-Încălzire (Predictive Pre-Heat cu Inerție Termică)
- **Modelare Inerție Șapă & Corecție Meteo Exterioară:**
  - Șapa de beton a încălzirii în pardoseală are un timp de răspuns de 1.5–3 ore.
  - Algoritmul calculează timpul de avans necesar în funcție de diferența de temperatură $\Delta T = T_{target} - T_{current}$ și de factorul meteo exterior preluat de la Open-Meteo:
    $$k_{weather} = 1.0 + \max(0.0, (15.0 - T_{outdoor}) \times 0.04)$$
  - Pornirea anticipată se declanșează cu 30–120 minute înainte de ancorele `Wake` și `Home`, astfel încât temperatura dorită este atinsă exact când utilizatorul se trezește sau ajunge acasă.

### Persistență NVS Anti-Uzură Flash & Interfață REST/CLI
- **Namespace `"sched"` cu Comparație `memcmp()`:**
  - Structura completă de 136 octeți este stocată în NVS flash.
  - Flash-ul este scris strict când utilizatorul modifică un parametru (`memcmp != 0`), garantând zero uzură flash la funcționare de lungă durată.
- **REST Endpoints & Comenzi CLI:**
  - `GET /api/schedule` și `POST /api/schedule`: citire și configurare JSON.
  - `POST /api/schedule/enable?state=true|false`: comutare rapidă.
  - Comenzi interactive adăugate în consolă (`SCHEDULE`, `SCHEDULE ON/OFF`, `SCHEDULE PREHEAT ON/OFF`, `SCHEDULE WINDOW ON/OFF`).
  - Subcomanda `python tools/esp_cli.py schedule` adăugată în utilitarul CLI.

### Validare Live & Curățare Zgomot Consolă
- **Eliminat Spam Meteo:**
  - Actualizările periodice de vreme la 15 minute nu mai poluează consola virtuală cu `[INIT]   * Outdoor Weather:`, fiind anunțate strict la boot și redirecționate pe Serial UART de debug.
- **Optimizare Flash OTA:**
  - Minificat CSS-ul paginilor web (`main.cpp`, `NetworkConsole.cpp`, `WebSetupPage.h`), aducând binarul la **1.24 MB** (1,305,280 bytes), cu 1.3 KB sub limita de sector `maxSketchSpace`.
- **Suită de Testare ASPICE SWE.5:**
  - Adăugat testul automat `SWE.5-TC15: Smart Bio-Schedule Contract, Anchors & Failsafe Guard (FR-26)`.
  - Toate cele 15 teste de sistem trecute cu **100.0% PASS** în 15.65 secunde.

## [12.10.2026] - Eliminare Word Wrap pe Mobil, Scalare Font (`A-`/`A+`) & Watchdog Feed la OTA (Sarcina 25)

### Eliminare Word Wrap pe Telefon & Scroll Orizontal Fluid (`NetworkConsole`)
- **Cauză Formatare Necoalizată pe Mobil:**
  - Clasa CSS `.term-container` utiliza `white-space: pre-wrap; word-break: break-all;`. Pe ecrane înguste de smartphone (<450px), liniile tabelelor de diagnostic și `STATUS` (de 70-100 de caractere) erau rupte la fiecare 35-40 de caractere, fărâmițând cuvintele și coloanele pe 2-3 rânduri și făcând citirea tabelelor impracticabilă.
- **Soluție Formatare Monospace Rigidă (No-Wrap):**
  - Setat `white-space: pre; word-break: normal;` în `.term-container` cu `overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`. Tabelele își mențin alinierea perfectă pe coloane, permițând scroll orizontal fluid cu gesturi tactile pe telefon.
- **Comutator Interactiv de Word Wrap (Opțional, Memorat în LocalStorage):**
  - Adăugat checkbox în bara de control de jos: `<input type="checkbox" id="wrapToggle"> Wrap`. Utilizatorul poate activa manual ruperea rândurilor dacă dorește, starea fiind salvată persistent în `localStorage` (`termWrap`).
- **Scalare Dinamică a Fontului Terminalului (`A-` / `A+`):**
  - Adăugate butoane tactile `A-` și `A+` în footer pentru ajustarea în trepte fine a dimensiunii fontului terminalului (între `0.52rem` și `1.20rem`).
  - Pe ecrane mobile (<640px), fontul implicit coboară automat la `0.70rem` pentru o densitate crescută a datelor. Dimensiunea aleasă este salvată persistent în `localStorage` (`termFontSize`).
- **Design Responsive Mobile (`@media (max-width: 640px)`):**
  - Compactate marginile, padding-ul, butoanele rapide (`qbtn`) și bara de input pentru a maximiza zona vizibilă pe smartphone.

### Protecție Watchdog la Încărcare OTA (`main.cpp`):**
- **Feed Hardware Watchdog pe Fiecare Bloc Primit:**
  - Atașat callback-ul `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` direct înainte de `httpUpdater.setup()`.
  - Resetează periodic Task Watchdog Timer-ul pe fiecare pachet HTTP de firmware scris în flash, eliminând riscul de restart TWDT în timpul actualizărilor mari de firmware.

## [12.10.2026] - Remediere Scurgere Memorie Heap (`DTC-402`), NimBLE `setMaxResults(0)` & Auto-Clear Histerezis (Sarcina 24)

### Remediere Scurgere Memorie Heap în Scannerul NimBLE (`BleSensorScanner`)
- **Cauză Rădăcină (Root-Cause Diagnoză Live):**
  - La 4 ore de la pornire, memoria RAM liberă a scăzut de la 127 KB la 51 KB, declanșând `[DIAG-ALERT] DTC-402 [WARNING]: Free heap memory low (51 KB)`.
  - În `NimBLE-Arduino`, scanarea continuă fără setarea explicită a limitei (`_pBLEScan->setMaxResults(0)`) păstrează implicit `m_maxResults = 0xFF` (nelimitat). Fiecare pachet BLE de la dispozitive din vecinătate cu MAC rotativ (telefoane mobile, smartwatch-uri, televizoare inteligente) genera un obiect `new NimBLEAdvertisedDevice` stocat pe heap în `m_scanResults.m_deviceVec`.
  - Eliberarea memoriei (`pScan->erase()`) era condiționată de `m_maxResults == 0`. Fără această setare, vectorul a acumulat peste 70 KB de date pe heap în 4 ore.
- **Soluție Implementată:**
  - În [`BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp): apelat `_pBLEScan->setMaxResults(0)` la inițializare (procesare exclusiv prin callback-ul `onResult`, cu eliberare instantanee din heap fără stocare în vector) și adăugat `_pBLEScan->clearResults()` la restartarea scanării.
  - Consumul de memorie pentru subsistemul Bluetooth este acum 100% constant în timp (zero memory leak).
- **Auto-Clear cu Histerezis pentru DTC-uri de Sistem (`DiagnosticEngine`):**
  - Implementat mecanism automat de stingere pentru `DTC-402` (când memoria liberă depășește 75 KB) și `DTC-401` (când semnalul Wi-Fi depășește -78 dBm), restabilind automat Scorul de Sănătate la **100% [EXCELLENT]** odată ce condiția a fost rezolvată.
- **Optimizare Streaming OTA & Unbuffered Logging (`tools/ota_upload.py`):**
  - Adăugat `sys.stdout.reconfigure(line_buffering=True)` și timeout adaptiv de 6s la recepția răspunsului HTTP, eliminând blocajele de stdout pe Windows.
- **Validare Live pe Hardware:**
  - Firmware încărcat prin Web OTA (`192.168.1.20`), Free Heap stabilizat la **125 KB**, codul `DTC-402` stins, Health Score **100%**. Toate cele 14 teste ASPICE SWE.5 trecute cu **100.0% PASS**.

## [12.10.2026] - Aliniere Tabelară pe 2 Coloane, Eliminare SYNCING din POST & Colorare Exhaustivă a Stărilor (Sarcina 23)

### Format Tabelar Structural pe Fiecare Subsistem & Aliniere Milimetrică (`STATUS`)
- **Carduri Tabelare Dedicate pentru Fiecare Subsistem:**
  - Înlocuită înșiruirea densă de text și pipe-uri de după `[CMD] > STATUS` cu tabele clare și aerisite pe 2 coloane:
    * `[SYSTEM & CONTROLLER HARDWARE]`: Firmware, Git, Serial, Boot Cause, Processor, Uptime, Silicon Die Temp, Free Memory, Flash Memory, Firmware Size.
    * `[BOILER, PUMP & HYDRAULIC ACTUATION]`: Burner Flame, Operation Mode, Heat Demand, Controller IP, Actuator Type, Circulation Pump, Frost Guard, Pump Relay State.
    * `[WEATHER & OUTDOOR ENVIRONMENT]`: Outdoor Temp, Climate Season, Outdoor Humidity, Weather Service, Telemetry Age, Weather Status.
    * `[SAFETY & FAILSAFE INTERLOCKS]`: Sensor Failsafe, Hardware Watchdog (90s TWDT), Pump Anti-Seize, Next Exercise.
- **Distanțare Generoasă a Coloanelor & Corectare Deplasare Stânga UTF-8:**
  - Identificată cauza deplasării la stânga cu 1-2 caractere: `snprintf` conta octeții UTF-8 ai simbolului de grad `°` (`\xC2\xB0` = 2 octeți) în loc de lățimea vizuală pe ecran (1 caracter), ceea ce scurta spațierea cu 1 caracter pe liniile de temperatură.
  - Implementat algoritmul `utf8VisibleLen()` care calculează lungimea pe baza caracterelor vizibile, garantând că separatorul `|` și coloana a 2-a pornesc la fix aceeași coloană pe fiecare rând.
  - Mărită distanța dintre coloane la o lățime fixă generoasă (`col1TotalWidth = 48`), asigurând 8–20 spații clare de separare.

### Optimizare Scanare BLE (Ciclu de Utilizare 90%) & Reducere Timp Warmup
- **Sensibilitate Sporită Prin Pereți Multipli (`include/Config.h`):**
  - Mărit `BLE_SCAN_WINDOW_MS` de la 50ms la 90ms la un interval de 100ms (ciclu de utilizare crescut de la 50% la 90%).
  - Receptorul NimBLE ascultă acum 90% din timp canalele BLE de publicitate, capturând balizele senzorului îndepărtat din Baie Mare (după 4 pereți) în 39s–64s în loc de 293s.
- **Deblocare Bucle Operaționale în Warmup:**
  - Zonele care și-au achiziționat senzorul (ex. Living, Camera Copilului) nu mai sunt blocate la pornire timp de 5 minute în așteptarea senzorilor îndepărtați; buclele achiziționate își reiau imediat controlul termostatic normal, în timp ce zonele în așteptare rămân în siguranță cu valvele închise (`ZONE_WARMUP`).


### Eliminare Linii `SYNCING` din POST & Raportare Asincronă Progresivă sub `[INIT]`
- **Power-On Self-Test (POST) 100% Hardware & Local:**
  - Eliminat `Real-Time Clock` din tabelul POST (fiind un serviciu dependent de rețea NTP asincronă), păstrând în POST strict subsistemele hardware și stările locale verificate definitiv la pornire (Network, RAM, Actuators, Pump, Bluetooth, Boiler RF, Watchdog, Web API).
- **Bloc Asincron de Inițializare `[INIT] Background Tasks Dispatched:`:**
  - Imediat sub tabelul POST, sarcinile asincrone de fundal sunt declarate clar cu asterisc `*`:
    * `  * Real-Time Clock:  Background NTP sync initiated (pool.ntp.org)`
    * `  * Outdoor Weather:  Background sync initiated (Open-Meteo REST API)`
    * `  * Sensor Discovery: Continuous BLE scanner active (Warmup grace period: 300s)`
- **Raportare Asincronă Unificată cu Asterisc (`[INIT]   * ...`):**
  - Mesajele ulterioare de progres și finalizare ale sarcinilor de fundal adoptă identic prefixul `[INIT]   * `, eliminând aparițiile nesistematice:
    * `[INIT]   * Outdoor Weather:  Synchronized: 15.4 °C, 80.0 % RH (Open-Meteo API)`
    * `[INIT]   * Real-Time Clock:  Synchronized: YYYY-MM-DD HH:MM:SS (Europe/Bucharest via NTP)`
    * `[INIT]   * Sensor Discovery: Acquired (1/3): Baie mare -> 23.0 °C at 7s`
    * `[INIT]   * Sensor Discovery: All 3 room sensors acquired in 125s! System fully operational.`

### Numerotare Categorii `STATUS` (A - E), Aerisire cu Newline-uri & Link Configurare Senzori
- **Prefixe Categorii & Separare Vizuală:**
  - Categoriile din `STATUS` sunt numerotate alfabetic cu `[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`.
  - Introdusă o linie liberă (newline) între fiecare categorie tabelară și între fiecare zonă (Zone 1, Zone 2, Zone 3) pentru lizibilitate maximă.
- **Link Integrat Instrument Configurare Senzori BLE (TelinkMiFlasher):**
  - Adăugat hyperlink direct către instrumentul web oficial de configurare Xiaomi BLE (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) în antetul consolei Web (`/console`), în Setup Wizard (`/setup`), în Dashboard-ul principal (`/`) și în manualul de comenzi (`HELP`), permițând utilizatorului mărirea puterii de transmisie radio (RF Tx Power până la +5..+9 dBm) pentru senzorii aflați după pereți groși.

### Optimizare Paced Streaming OTA & Extindere Watchdog
- **Streaming Socket Paced (`tools/ota_upload.py`):**
  - Implementat transfer cadențat cu pauze de 12ms per pachet de 1436 bytes (~110 KB/s), prevenind epuizarea bufferelor TCP `pbuf` din stiva lwIP a ESP32 și eliminând blocajele de retransmisie TCP. Timpul de upload este stabilizat la ~13.5 secunde.
- **Hardware Watchdog Mărit la 90s:** Oferă marjă generoasă de siguranță la scrierea flash pe durata procedurilor OTA.

## [12.10.2026] - Telemetrie Hardware Extinsă (CPU, RAM, NVM, Temp Cip), Carduri Zonale & Stilizare Vreme (Sarcina 22)

### Telemetrie Hardware Detaliată (Procesor, Memorie, NVM, Temperatură Cip)
- **Info CPU & Temperatură Siliciu (`[CPU]`):**
  - Model cip, nuclee, frecvență și revizie: `ESP32-D0WD-V3 (2 Cores @ 240 MHz, Rev 3)`.
  - Temperatură internă a pastilei de siliciu în timp real: `Die Temp: %.1f °C` via `temperatureRead()`.
  - Timp de funcționare formatat uman: `Uptime: %uh %02um %02us`.
- **Info Memorie RAM & Stabilitate Heap (`[MEMORY]`):**
  - Memorie RAM liberă / totală: `%u KB free / %u KB total`.
  - Nivel minim atins de la boot (watermark de stabilitate): `Lowest: %u KB`.
  - Cel mai mare bloc contiguu alocabil (indicator anti-fragmentare): `Max Block: %u KB`.
- **Info Stocare NVM & Flash Partitioning (`[FLASH]`):**
  - Mărime și frecvență flash: `%u MB @ %u MHz`.
  - Spațiu firmware ocupat și marjă liberă pentru update OTA: `Firmware: %.2f MB (OTA Margin: %.2f MB free)`.
  - Stare NVS (memorie non-volatilă): `NVS: Active`.

### Redesign Complet Zone & Eliminare Tabel Separat `[SENSORS]`
- **Eliminare Redundanță:** Tabelul `[SENSORS]` a fost scos definitiv din ieșirea comenzii `STATUS`.
- **Carduri Structurate pe Fiecare Cameră (`[HEATING ZONES & ROOM TELEMETRY]`):**
  - Fiecare zonă afișează un bloc complet pe 4 linii cu toate datele termice, hidraulice și radio:
    - `* Temperature:` Target (`%.1f °C`), Current (`%.1f °C`), Umiditate (`%.1f % RH`).
    - `* Loop Control:` Stare buclă (`Idle (Temp OK)`, `Warmup`, `Heating Demand`, `Sensor Timeout`), Actuator (`OPEN` / `CLOSED`), Releu (`Relay GPIOxx ON/OFF`).
    - `* Sensor Radio:` Nume senzor asociat, Adresă MAC (`[AA:BB:CC:DD:EE:FF]`), Nivel baterie (`%`).
    - `* RF Telemetry:` Nivel semnal și calificativ (`-61 dBm [EXCELLENT]`), Min/Max semnal (`Min: .. / Max: .. dBm`), Număr pachete recepționate, Vechime telemetrie (`Last seen: %us ago`).
- **Simbol Corect Grad Celsius (`°C`):** Toate temperaturile afișează simbolul `°C` nativ UTF-8, cu header `text/plain; charset=utf-8` pentru prevenirea afișării de caractere fantomă (`Â°C`).

### Evidențiere Vizuală Web Console (`/console`)
- **Stilizare etichetă `[WEATHER]` & Anotimp:** Eticheta `[WEATHER]` este recunoscută case-insensitive și colorată în cyan aprins (`.t-wth`), iar anotimpul (`Autumn`) este stilizat bold.
- **Stilizare Automată Temperaturi & Hardware:** Toate valorile în `°C` sunt colorate automat în cyan bold (`.t-temp`), iar etichetele `[CPU]`, `[MEMORY]`, `[FLASH]`, `[Zone x]` au culori dedicate de mare contrast.

## [12.10.2026] - Eliminare Acronime, Unificare Mapare Zone-Senzori, Integrare POST & Clarificare Reziliență Offline RTC (Sarcina 21)

### Eliminare Acronime Tehnice & Umanizare Text Consolă
- **Excludere Jargon Neclar din Console Banner & Self-Test:**
  - Înlocuit acronimul `GPA` cu descrierea funcțională exactă: `Secondary Pump on Relay 4: ENABLED`.
  - Înlocuit acronimul `TWDT` cu formularea intuitivă: `Hardware Watchdog Armed: 60s timeout`.
  - Înlocuită formularea tehnică `NC Polarity` cu explicarea clară a provenienței setării: `Normally Closed [NC], User-Configured in NVS`, evidențiind faptul că polaritatea provine din configurația salvată de utilizator în memoria flash NVS.
  - Înlocuit `NimBLE Active` cu `Bluetooth Receiver active, listening`.

### Boot Time, Sincronizare Ceas & Funcționare Offline Fără Wi-Fi
- **Afișare Timp Boot în Banner:**
  - Adăugată linia `Boot Time: YYYY-MM-DD HH:MM:SS` în antetul de pornire al sistemului.
  - Adăugat mesaj dedicat la achiziția de timp real: `[Time] Real-time clock synchronized: YYYY-MM-DD HH:MM:SS (Europe/Bucharest via NTP)`.
  - Adăugat mesaj de confirmare meteo cu etichetă standardizată: `[Weather] Outdoor weather synchronized: ...`.
- **Comportament Offline Documentat (Rezistență la căderi de Wi-Fi):**
  - ESP32 integrează un timer RTC hardware intern. Odată sincronizat ceasul la pornire (sau via SoftAP/NTP), ceasul hardware menține data și ora chiar dacă conexiunea Wi-Fi cade complet timp de zile întregi (derivă minimă de 1–2 secunde/zi).
  - Algoritmul termic, siguranțele cazanului, interblocarea hidraulică și citirea senzorilor BLE sunt 100% autonome și rulează local în timp real, fără nicio dependență de internet sau cloud.

### Integrare Servicii în Power-On Self-Test (POST)
- **Extindere Tabel Diagnostic la Pornire:**
  - `Real-Time Clock: PASS (YYYY-MM-DD HH:MM:SS)` integrat direct în tabelul POST.
  - `Outdoor Weather: SYNCING (Connecting to Open-Meteo API...)` inclus ca serviciu monitorizat în POST.
  - `Sensor Boot Warmup: SYNCING (Grace period active: max 5 min)` inclus în lista de verificare inițială.

### Unificare Tabel `STATUS` (Mapare Directă Zonă - Senzor)
- **Eliminare Ambiguitate Nume Senzori vs Nume Zone:**
  - Integrată maparea completă a senzorului direct în fiecare rând al buclei termice (`[ZONES] Heating Loops & Sensor Mapping`):
    - `ID`, `Zone Name`, `Target`, `Current`, `Assigned Sensor (Name / MAC)`, `Bat`, `Signal (RSSI & Calificativ)`, `Status`, `Actuator Valve`.
  - Astfel, chiar dacă senzorul BLE are un nume diferit față de zona căreia îi este alocat, legătura este imediat evidentă la o singură privire.
  - Tabelul `[SENSORS]` rămâne sub el ca registru compact de telemetrie pentru diagnostic de pachete primite și semnal minim/maxim.

### Evidențiere Vizuală / Culori în Consola Web (`/console`)
- **Stilizare Automată Cuvinte-Cheie Stare:**
  - Adăugate clase CSS de contrast înalt în `NetworkConsole.cpp`: `.t-ok`, `.t-off`, `.t-idle`, `.t-demand`, `.t-auto`, `.t-man`, `.t-warm`, alături de badge-urile de semnal `.b-exc`, `.b-good`, `.b-marg`, `.b-poor`.
  - Parsare automată regex în `colorize(line)` pentru cuvinte-cheie de stare ce urmează după `:` (`OFF`, `ON`, `IDLE`, `DEMAND`, `AUTOMATIC`, `MANUAL`, `WARMUP`, `PASS`, `FAIL`), făcându-le să sară direct în ochi în interfața web.

## [12.10.2026] - Perioadă de Warmup Senzori la Boot (Max 5 min), Diagnoză DTC-101 & Format Tabelar Consolă (Sarcina 20)

### Perioadă de Grație Warmup Senzori la Pornire (Max 5 min / 300s)
- **Eliminare Alarme Premature la Boot la Rece:**
  - La pornirea plăcii ESP32, senzorii BLE Xiaomi emit balize la intervale de 30–60 secunde (sau mai rar în funcție de firmware). Înainte, sistemul semnala instantaneu `Sensor Timeout (Failsafe OFF)` la secundele 0..30.
  - Implementată starea dedicată `ZONE_WARMUP` și mecanismul de grație la boot (`_bootWarmupActive = true`, temporizator de 300s / 5 minute):
    - Pe durata perioadei de warmup, buclele termice afișează `WARMUP` și `Warmup (%us/300s)`.
    - Pornirea încălzirii/arzătorului este blocată preventiv până când toți senzorii asociați transmit prima citire validă, protejând echilibrul hidraulic al casei (excepție: protecția de urgență la îngheț `<12°C` care acționează imediat).
  - **Ieșire Anticipată la Detectarea Tuturor Senzorilor:**
    - Imediat ce toți senzorii asociați (ex: 3/3) au fost detectați (la secunda 20..60), warmup-ul se încheie instantaneu: `Boot warmup COMPLETE in %us! All %u paired sensors acquired. System fully operational.`
  - **Decuplare Izolată și Ridicare DTC-101 la Expirare:**
    - Dacă trec 5 minute și anumiți senzori nu au emis balize (ex. baterie descărcată sau distanță prea mare), sistemul ridică automat codul de eroare `DTC-101` (`"Zone X sensor missing at boot warmup cutoff"`), îl salvează în memoria NVS a motorului de diagnoză și pornește încălzirea exclusiv pentru zonele complet funcționale.

### Format Tabelar cu Coloane Fixe pentru Comanda `STATUS` și `BLE`/`SENSORS`
- **Tabel Aliniat Bucle Încălzire Pardoseală (`[ZONES]`):**
  - Antet cu lățimi fixe: `ID`, `Zone Name`, `Current Temp`, `Target Temp`, `Status`, `Actuator Valve (Relay GPIO)`.
  - Toate datele sunt aliniate vertical milimetric, ușor de citit pe orice consolă sau terminal web.
- **Tabel Aliniat Registru Termometre Cameră (`[SENSORS]`):**
  - Indică starea clară a achiziției: `[SCANNING: %us/300s in progress]` în warmup, respectiv `[SCANNING COMPLETE - ALL ACQUIRED]` când toți senzorii sunt prezenți.
  - Tabel aliniat cu coloane: `Sensor Name`, `MAC Address`, `Temp`, `Humidity`, `Battery`, `Signal (Min/Cur/Max dBm)`, `Packets`, `Last Seen`.
- **Clarificare Linie de Protecții (`[PROTECTIONS]`):**
  - Redenumită eticheta generică `[SAFETY]` în `[PROTECTIONS]`: `Sensor Offline Failsafe: 10 min (all zones) | Pump Anti-Seize: Weekly (Ran: ... | Next in ...)`.

## [12.10.2026] - Rafinare Completă Output Consolă, Eliminare Zgomot Boot & Categorisire Comenzi (Sarcina 19)

### Redirecționare Jurnale Interne & Eliminare Zgomot Boot
- **Pristine Boot Banner & POST la Pornire:**
  - Redirecționate toate cele 8 mesaje interne de inițializare a modulelor (`ThermalZoneManager`, `TimeAndWeatherService`, `DevicePassport`, `BleSensorScanner`, `DiagnosticEngine`, TWDT Watchdog) exclusiv pe portul serial hardware UART (`Serial.println` / `Serial.printf`).
  - Buffer-ul consolei virtuale (`netConsole` - Telnet TCP:23 și Web `/console`) debutează instantaneu cu titlul oficial al controlerului și tabelul Power-On Self-Test (POST).
  - Integrat statusul serviciilor Web & API (`HTTP Port 80, Telnet Port 23`) direct în tabelul POST, eliminând mesajul separat post-boot.
  - Actualizat indicatorul scannerului BLE la pornire: `PASS (NimBLE Active, listening)`.

### Reziliență Meteo & Eliminare Alerte False
- **Handshake TLS Stabilizat & Backoff Silențios:**
  - Mărit timeout-ul socket-ului HTTP Open-Meteo la 5000ms.
  - Introdusă o pauză inițială de 5 secunde la pornirea sistemului pentru a permite stabilizarea stivei TCP/IP și a ceasului NTP înainte de prima interogare meteo.
  - Avertizările temporare de rețea (ex. HTTP -11 la pornire la rece) sunt direcționate către portul serial hardware UART, eliminând poluarea consolei utilizatorului; logul în consolă apare doar la sincronizare confirmată sau la comanda `WEATHER`.

### Îmbunătățire Funcțională Comenzi Consolă & Deduplicare
- **Eliminare Mesaje Duplicate la Execuție:**
  - Curățat dublajul de mesaje la comenzile `ON`/`OFF`, `AUTO`/`MANUAL` și `SET <z> <temp>`, lăsând un singur mesaj clar de confirmare emis direct de modulul de control.
- **Indicatori Calitate Semnal (Link Quality Badges) în `STATUS` și `BLE`:**
  - Comenzile `STATUS` și `BLE` includ calificativul calității conexiunii pentru fiecare termometru de cameră: `[EXCELLENT]` (>= -70 dBm), `[GOOD]` (-71..-80 dBm), `[MARGINAL]` (-81..-88 dBm), `[POOR]` (< -88 dBm).
  - Adăugată linie dedicată pentru starea vremii exterioare în sumarul comenzii `STATUS`.
- **Redesign Complet Structură & Claritate `STATUS`:**
  - **Umanizare Parametri Siguranță (`[SAFETY]`):** Eliminat timestamp-ul brut în format UNIX epoch (`1791792220`). Afișat în format uman: `Sensor Failsafe: 10 min (600s) | Anti-Seize: Weekly (Ran: 2026-10-12 11:03 | Next in 6d)`.
  - **Header Dedicat & Formatare Bucle Termice (`[ZONES]`):** Separată afișarea zonelor sub antetul `[ZONES] Underfloor Loops (3):`, eliminând confuzia de subordonare la `[SAFETY]`.
  - **Afișare Clară a Actuatoarelor & Temperaturilor:** Indică starea valvei (`Valve: CLOSED (GPIO32 OFF)` / `Valve: OPEN (GPIO32 ON)`), iar în caz de senzor deconectat afișează deterministic `--.- C` și `Sensor Timeout (Failsafe OFF)`.
  - **Secțiune Separată Termometre (`[SENSORS]`):** Marcată distinct cu `[SENSORS] Room Thermometers (...)` în loc de eticheta generică `[STATUS]`.
  - **Etichetă Dedicată Boiler (`[BOILER]`):** Starea arzătorului și a memoriei libere grupată sub `[BOILER] Burner Flame: OFF | Demand: IDLE | Mode: AUTOMATIC | IP: ... | Free Heap: 120 KB`.
- **Manual de Comenzi Structurat (`HELP` / `?`):**
  - Reorganizată lista comenzilor în 4 secțiuni logice: `[THERMAL & BOILER CONTROL]`, `[SYSTEM & SAFETY CONFIG]`, `[SENSORS & ENVIRONMENT]`, `[DIAGNOSTICS & SYSTEM]`.
- **Suport Comandă `CLEAR` / `CLS`:**
  - Implementată metoda `netConsole.clear()` ce resetează buffer-ul circular și trimite secvența ANSI `\033[2J\033[H` pentru curățarea terminalului Telnet / Serial.
- **Culori Extinse în Web Terminal:**
  - Reguli de colorare CSS pentru badge-urile `[EXCELLENT]`, `[GOOD]`, `[MARGINAL]`, `[POOR]`, `[STATUS]`, `[SYSTEM]`, `[BOILER]`, `[HARDWARE]`, `[SAFETY]`, `[ZONES]`, `[SENSORS]`, `[Ready]`.

## [12.10.2026] - Îmbunătățire Consolă, Reziliență Meteo, Anti-Seize pe Epoch & Timeout Senzori Configurabil (Sarcina 18)

### Consolă de Rețea & Web Terminal Profesional
- **Timestamp-uri NTP Reale pe Fiecare Linie:**
  - Adăugate marcaje de timp `[HH:MM:SS]` (sau `[+HH:MM:SS]` la pornire înainte de sincronizarea NTP) la începutul fiecărei linii noi în Serial, Telnet TCP:23 și Web Console ([`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h), [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp)).
- **Tag-uri Colorate în Web Terminal (`/console`):**
  - Badge-uri stilizate CSS și funcție JavaScript `colorize()` pentru categorii: `[Thermal]`, `[Weather]`, `[BLE]`, `[Time]`, `[Passport]`, `[DIAG]`, `[CMD]`, `ERROR`, `WARN`, `SUCCESS`.
- **Eliminare Poluare Nginx 502 / 504 în Terminal:**
  - Scriptul JavaScript din `/console` verifică explicit codul HTTP de răspuns (`r.ok && r.status === 200`) înainte de inserarea logurilor în DOM, eliminând afișarea paginilor HTML de eroare de la proxy-ul Nginx.

### Serviciu Meteo & Reziliență Open-Meteo
- **Prevenire Buclă Rapidă Erori HTTP -11:**
  - Mărit timeout-ul socket-ului HTTP de la 1500ms la 3000ms ([`src/TimeAndWeatherService.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/TimeAndWeatherService.cpp)).
  - Migrat la `JsonDocument` (ArduinoJson 7).
  - Introdus backoff de 60 de secunde la eșec: în cazul în care serverul Open-Meteo nu răspunde la timp, reîncercarea are loc după 60s în loc de reîncercare continuă la fiecare iterație de loop.

### Calibrare Exercițiu Anti-Blocare (Anti-Seize) pe Timp Real Calendaristic
- **Eliminare Porniri False de 20s la Reboot:**
  - Înlocuit temporizatorul bazat pe `millis()` cu UNIX epoch calendaristic real (`time_t nowEpoch = time(nullptr)`) salvat în NVS (`last_seize_ep`) ([`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp)).
  - La prima pornire a plăcii cu NTP activ, dacă nu există o rulare anterioară înregistrată, se memorează timestamp-ul curent și se programează următorul ciclu la 7 zile (604.800 secunde).

### Failsafe Timeout Senzori Configurabil (`DTC-101`)
- **Parametru Configurat Dinamic & Persistență NVS:**
  - Implementat parametru configurabil `_sensorTimeoutSec` (implicit 600s / 10 min, domeniu valid 60s - 7200s) salvat în NVS (`sns_tout`).
  - Endpoint REST dedicat `/api/setup/sensor-timeout` (GET/POST) și comandă consolă `TIMEOUT <sec>` în [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp).
  - La depășirea timeout-ului pe o zonă, se declanșează `DTC-101`, iar bucla termică trece automat în siguranță (actuator OFF).

### Indicatori Calitate Semnal BLE (Link Quality Indicators)
- **Asistență Instalare pe Teren:**
  - Evaluare automată a nivelului RSSI: `EXCELLENT` (>= -72 dBm), `GOOD` (-73..-82 dBm), `MARGINAL` (-83..-88 dBm), `POOR` (< -88 dBm).
  - Afișat în tabelul de senzori din Setup Wizard Web ([`include/WebSetupPage.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/WebSetupPage.h)) și în utilitarul CLI ([`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) `status` și `ble`).

### Versionare Firmware Baseline & Diagnostic POST la Boot (v1.2.0)
- **Versionare Formală & Trasabilitate Hardware ([`include/Version.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Version.h)):**
  - Implementat antet SemVer `v1.2.0`, timestamp exact de compilare (`__DATE__ " " __TIME__`) și hash scurt de commit Git (`8065534`).
  - Expus în `/api/boiler/status` și afișat în header-ul utilitarului CLI (`python tools/esp_cli.py status`).
- **Mesaj de Boot Profesional & Power-On Self-Test (POST):**
  - Eliminat meniul lung de 17 linii cu comenzi la startup; înlocuit cu antet curat de sistem și raportare cauză resetare (`esp_reset_reason()`: Cold Boot vs OTA Reboot vs TWDT).
  - Rulare automată a testului integrat POST la fiecare pornire: raportează starea Wi-Fi, memoria Heap liberă, actuatoarele & pompa GPA, scannerul pasiv NimBLE, emițătorul radio RF 433 și scorul general de sănătate (100% EXCELLENT).
- **Corecție Completă Duplicare Consolă Web:**
  - Adăugat antetul HTTP `Access-Control-Expose-Headers: X-Log-Seq` permițând JavaScript-ului din browser să citească corect poziția cursorului de loguri.
  - Implementat mecanism de blocare concurentă (`isFetching`) și debouncing comenzi (350ms) în Web Console (`/console`), eliminând dublarea comenzilor `STATUS`.
  - Detecție automată a restartării plăcii (`newSeq < lastSeq`), afișând un separator clar `--- [SYSTEM REBOOT DETECTED / NEW FIRMWARE RUNNING] ---` în browser.

### Verificare SWE.5 Extinsă (14/14 PASS - 100.0%)
- Extinsă suita automată de testare pe PC ([`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py)) de la 12 la 14 cazuri de testare:
  * `SWE.5-TC13`: Contract Failsafe Timeout Senzori (`FR-18`) - PASS.
  * `SWE.5-TC14`: Identitate Baseline & Versionare Firmware v1.2.0 (`FR-21`) - PASS.
- Rulată complet împotriva plăcii fizice la `192.168.1.20`. Rezultat: **100.0% PASS (14/14 teste trecute)**. Raport actualizat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).

## [09.10.2026] - Eliminare Bloatware MQTT, Curățare Consolă & Suită Automată Testare Sistem ASPICE SWE.5 (Sarcina 17)

### Eliminare Bloatware & Decuplare Hardware Autonomă
- **Eliminare Totală Modul MQTT & Dependențe Galaxy-A6:**
  - Șters fișierele de cod `MqttTelemetryClient.cpp` și `MqttTelemetryClient.h`.
  - Eliminat flag-ul de compilare `-D MQTT_MAX_PACKET_SIZE=512` din [`platformio.ini`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/platformio.ini) și dezactivat legătura bibliotecii `PubSubClient`.
  - Curățat toate referințele MQTT din [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp), [`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp) și [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp).
  - Sistemul funcționează 100% autonom prin radio Bluetooth NimBLE pasiv fără a depinde de un server/telefon Galaxy-A6 sau bridge extern.

### Curățare Consolă & Confort Acustic
- **Fără Spam de Pachete BLE:**
  - Eliminat orice print automat nesolicitat al pachetelor de la senzori din consola Serial / Telnet TCP:23.
  - Datele senzorilor sunt afișate exclusiv la cererea explicită a utilizatorului (`STATUS`, `BLE`, `SENSORS` sau apel REST).

### Suită de Testare Automată pe PC (ASPICE SWE.5)
- **Implementat Runner de Testare Sistem (`tools/test_system.py` & `esp_cli.py test`):**
  - Fără mock-uri artificiale în microcontroler: testele rulează de pe PC direct împotriva plăcii fizice la `192.168.1.20`.
  - 12 cazuri de testare acoperind:
    * `SWE.5-TC01`: Telemetrie stare sistem & latență sub 200ms (`FR-1`, `NFR-1.1`, `NFR-1.5`) - PASS.
    * `SWE.5-TC02`: Pașaport digital de punere în funcțiune NVS (`FR-13.1`, `FR-13.2`) - PASS.
    * `SWE.5-TC03`: Scanner pasiv NimBLE & decodare Xiaomi (`FR-4.1`, `FR-4.2`) - PASS.
    * `SWE.5-TC04`: Suită integrată self-test 6 subsisteme (`FR-10.1`, `FR-10.2`) - PASS.
    * `SWE.5-TC05`: Polaritate actuatoare NC/NO & persistență NVS (`FR-15.1`, `FR-15.2`) - PASS.
    * `SWE.5-TC06`: Pompă recirculare grup amestec GPA (`FR-14.1`, `FR-14.2`) - PASS.
    * `SWE.5-TC07`: Filtru idempotență anti-spam no-op (`FR-17.1`) - PASS.
    * `SWE.5-TC08`: Gardă anti-short-cycling 60s & HTTP 429 (`FR-17.3`, `FR-17.4`) - PASS.
    * `SWE.5-TC09`: Detecție abuz spam & cod `DTC-201` (`FR-17.5`) - PASS.
    * `SWE.5-TC10`: Calibrare temperatură țintă zonă (`FR-1.2`, `FR-1.3`) - PASS.
    * `SWE.5-TC11`: Sincronizare NTP & telemetrie vreme (`FR-11.1`, `FR-11.2`) - PASS.
    * `SWE.5-TC12`: Feedback bidirecțional consolă rețea (`FR-9.1`, `FR-9.2`) - PASS.
  - Rată de succes: **100.0% (12/12 teste trecute)** în 4.02 secunde. Raport generat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).

## [09.10.2026] - Failsafe Anti-Spam, Rate-Limiting Cazane & Diagnostic DTC-201 (Sarcina 16)

### Protecție Anti-Spam & Anti-Short-Cycling Conform V-Model ASPICE (FR-17, NFR-1.6)
- **Filtru Idempotent (FR-17.1):**
  - Comenzile redundante pentru aceeași stare (de ex. comenzi repetate `OFF` sau `ON`) răspund instantaneu în 0 ms cu status 200 și câmpul `idempotent: true`, omițând rafala radio RF 433.92 MHz pentru a reduce poluarea electromagnetică și consumul.
- **RF Transmitter Burst Cooldown (FR-17.2):**
  - Temporizare hardware obligatorie de minim 3 secunde între orice două transmisii RF consecutive pentru a proteja etajul de emisie și a garanta recepția curată.
- **Garda Anti-Short-Cycling a Arzătorului (FR-17.3, NFR-1.6):**
  - Implementat în [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) și verificat la nivel de API în [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp).
  - Durată minimă de ardere continuă: 60 secunde înainte de a permite oprirea (`OFF`).
  - Pauză minimă de repaus între cicluri: 60 secunde după oprire înainte de a permite repornirea (`ON`).
- **Răspuns Standard HTTP 429 Too Many Requests (FR-17.4):**
  - Răspuns REST standardizat cu cod HTTP 429, payload JSON explicit (`error`, `waitSec`, `boiler`) indicând exact secundele rămase până la deblocare.
- **Detecție Abuz Rapid & Cod Diagnoză DTC-201 (FR-17.5):**
  - 3 tentative respinse într-o fereastră de 10 secunde ridică automat codul `DTC-201: Rapid boiler toggling / spam detected` cu severitate WARNING, scăzând scorul de sănătate al plăcii la 90%.
- **Opțiune de Forțare pentru Urgență / Service (`--force`):**
  - Parametrul `?force=true` sau comutarea în Mod Service permite inginerului de service să suprascrie interlock-ul în situații critice.
- **Actualizare Procedură Web OTA cu Watchdog Calibrat la 30s:**
  - Rezolvat conflictul dintre durata de scriere a flash-ului SPI (~12s) și Task Watchdog Timer prin procedura stepped flash (binar de tranziție -> binar complet). TWDT rulează acum la 30 secunde în regim normal.
- **Verificare Completă SWE.4 / SWE.5:**
  - Toate scenariile (idempotență, refuz 429 cu countdown, spam DTC-201, bypass `--force`, curățare DTC) au fost rulate și validate live pe hardware (`192.168.1.20`) prin [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).

## [09.10.2026] - Hardening Firmware, Failsafes Hidraulice & Pompă Recirculare Pardoseală (Sarcina 15)


### Failsafes de Siguranță & Compatibilitate Hidraulică Universală
- **Control Automat Pompă Secundară (Grup Amestec GPA / Releu 4 GPIO26):**
  - Implementat driver inteligent pentru pompa distribuitorului de pardoseală în [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp).
  - *Interlock Pompă:* Previne cavitația și funcționarea în gol; pompa pornește exclusiv după deschiderea fizică a cel puțin unui actuator.
  - *Post-circulație (Pump Overrun):* Pompa continuă să ruleze timp de 180s după oprirea flăcării pe ultima zonă activă (*keeper zone*) pentru a disipa căldura reziduală și a preveni șocul termic în schimbător.
  - *Exercițiu Anti-Blocare de Vară (Anti-Seize):* Rulare automată săptămânală de 20s în sezonul cald pentru prevenirea gripării rotorului. Validat complet pe hardware via CLI (`tools/esp_cli.py exercise`) și endpoint `/api/diag/exercise`.
- **Compatibilitate Universală Actuatoare NC vs NO:**
  - Suport nativ selectabil via NVS (`act_nc`) și API (`/api/setup/actuator-type`) pentru actuatoare Normal Închis (NC) și Normal Deschis (NO).
- **Protecție Forțată la Îngheț (<12°C):**
  - Failsafe de urgență ce forțează cererea termică chiar dacă sistemul este comutat pe MANUAL sau oprit.
- **Izolare Avarie Senzor (`DTC-101`):**
  - La pierderea semnalului peste 30 min, zona afectată este decuplată imediat pentru a preveni supraîncălzirea șapei, iar starea se recuperează instant la reluarea pachetelor.
- **Hardware Task Watchdog Timer (ESP32 TWDT 30s):**
  - Bucla principală `loop()` este monitorizată de watchdog-ul hardware nativ ESP-IDF (`esp_task_wdt_init(30, true)`).
- **Extindere Utilitar CLI (`tools/esp_cli.py`):**
  - Adăugate comenzile `pump <on|off|enable|disable>`, `actuator <nc|no>`, `exercise` și afișare completă de stare hidraulică în `status`.
- **Ghid Tehnic & Hidraulic Dedicat:**
  - Creat documentul [`docs/underfloor_heating_hydraulics_and_failsafes.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/underfloor_heating_hydraulics_and_failsafes.md) cu scheme și bune practici pentru instalatori (termostat de contact la 55°C, supape de bypass diferențial).

## [09.10.2026] - Arhitectură Decuplată Headless (API-First), Eliminare Latență UI & Asistent Configurare NVS

### Arhitectură & Optimizare Motor Termic
- **Tranziție către Arhitectură Headless (API-First):**
  - Decuplat complet logica de bază a microcontrolerului de designul interfeței grafice pentru a asigura stabilitate maximă, depanare simplă și libertate deplină în alegerea viitoare a frontend-ului client/admin.
  - Consolidat catalogul complet de endpoint-uri REST/JSON (`/api/boiler/status`, `/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/mode`, `/api/boiler/on`, `/api/boiler/off`, `/api/ble/sensors`, `/api/setup/passport`, `/api/setup/time`, `/api/diag/status`, `/api/diag/selftest`, `/api/diag/service`).
  - Toate endpoint-urile răspund sincron în sub **70–120 ms**.
- **Rezolvare Cauze Lag & Comenzi Pierdute:**
  - *Optimizare DOM / Script:* Eliminat `location.reload()` pe toate butoanele (`+`, `-`, mod, flacără). Înlocuit cu actualizări optimiste instantanee în DOM (0 ms timp de răspuns vizual) și transmisiuni asincrone în fundal.
  - *Rupere Buclă Recursivă:* Corectat feedback-loop-ul între ingestia `BleSensorScanner` și `MqttTelemetryClient` care aglomera stiva și cauza instabilitate.
  - *Non-Blocking Timeouts:* Setat timeout scurt (1s) pe clientul socket MQTT și 1.5s pe interogarea HTTP Open-Meteo pentru a preveni orice blocaj al buclei `loop()`.
- **Selectare Inteligentă a Locației (Zero Coordonate Manuale):**
  - Implementat căutare live cu auto-suggest prin Open-Meteo Geocoding pentru orice localitate (oraș, comună, sat) din România.
  - Integrat selector rapid cu cele 41 de reședințe de județ (funcționare 100% offline) și buton de auto-detecție prin GPS/IP cu 1 click.
- **Normalizare Indecși Zone:**
  - Endpoint-urile `/api/zones/setpoint` și `/api/zones/toggle` acceptă acum în mod nativ atât identificatori bazați pe 1 (`id=1..3`), cât și indecși bazați pe 0 (`id=0..2`).
- **Validare Utilitar CLI (`tools/esp_cli.py`):**
  - Testat complet fluxul de interogare și control prin CLI: `status`, `set`, `selftest`, `diag`, `ble`, `passport`, `time`.

## [08.10.2026] - Scanner BLE Nativ Pasiv pe ESP32 (BTHome v2 & PVVX) cu Redundanță Hibridă MQTT

### Arhitectură & Dezvoltare Software
- **Integrare NimBLE-Arduino în `lib/`:**
  - Vendored local în `lib/NimBLE-Arduino` pentru a garanta compilare 100% offline și reproductibilă.
  - Înlocuit stiva greoaie Bluedroid cu NimBLE, reducând flash-ul consumat de la ~1.65 MB la doar **1.08 MB (1.128.681 bytes)** și utilizarea de RAM la 65 KB (19.8%).
  - Permite continuarea actualizărilor de firmware wireless via Web OTA (`/update`) chiar pe partițiile implicite de 1.25 MB.
- **Clasă Dedicată `BleSensorScanner`:**
  - Implementată în [`include/BleSensorScanner.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BleSensorScanner.h) și [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp).
  - Scanner pasiv (`setActiveScan(false)`): ascultă pachetele de broadcast fără să transmită nimic în eter, menținând zero consum suplimentar de baterie pe termometre și zero poluare RF.
  - Parametri de scanare: `interval = 100ms`, `window = 50ms` (duty cycle de 50% pentru coexistență nativă optimă cu transceiverul Wi-Fi pe frecvența comună de 2.4 GHz).
- **Parsare Automată Multi-Protocol:**
  - **BTHome v2 (UUID `0xFCD2`):** Decodifică TLV objects: ID 0x01 (baterie %), ID 0x02 (temperatură int16 scaled 0.01), ID 0x03 (umiditate uint16 scaled 0.01).
  - **PVVX / ATC Custom Format (UUID `0x181A`):** Decodifică telemetria completă (temperatură, umiditate, tensiune baterie și procent).
  - **Mapping Inteligent:** Mapează adresele MAC cunoscute la camere (`A4:C1:38:A4:C1:DD` -> Living Room, `A4:C1:38:84:A1:E8` -> Kids Room, `A4:C1:38:94:0F:44` -> Bathroom); senzorii noi necunoscuți sunt înregistrați automat ca `BLE-XXXX`.
- **Canal Dual Redundant & Fuziune Telemetrie:**
  - Senzorii detectați direct prin BLE pe ESP32 au prioritate (`BLE-Direct (ESP32)`), oferind autonomie totală chiar dacă telefonul gateway, brokerul MQTT sau conexiunea Wi-Fi cad.
  - Pachetele primite prin MQTT de pe Galaxy A6 (`MQTT-Gateway (A6)`) servesc ca backup seamless dacă un senzor este plasat într-o zonă ecranată (ex. gresie/faianță Baie).
- **API REST, Consolă & Utilitar CLI:**
  - Adăugat endpoint REST `/api/ble/sensors` cu starea scanerului, lista senzorilor, pachete decodificate, RSSI și vârstă.
  - Adăugată comanda `BLE` (sau `SENSORS`) în consola Telnet TCP:23 și în Web Console `/console`.
  - Actualizat utilitarul CLI [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) cu comanda `python tools/esp_cli.py ble`.

### Verificare Live pe Hardware
- **Web OTA Flash:** Firmware-ul compilat a fost încărcat wireless prin Web OTA (`http://192.168.1.20/update`) în doar 15 secunde.
- **Validare Live Pachet direct în eter:**
  - Living Room (`A4:C1:38:A4:C1:DD`): recepționat direct cu RSSI **-64 dBm**, 23.0°C, 54.6% umiditate, baterie 93%.
  - Kids Room (`A4:C1:38:84:A1:E8`): recepționat direct cu RSSI **-67 dBm / -69 dBm**, 22.9°C, 51.2% umiditate, baterie 100%.
  - Ambele pachete au fost recepționate și decodificate live fără nicio eroare de CRC.

## [07.10.2026] - Consolă Virtuală Serială Bidirecțională prin Wi-Fi (Telnet TCP:23 & Web Console /console)

### Arhitectură & Dezvoltare Software
- **Clasă Dedicată `NetworkConsole`:** Implementată în [`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h) și [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) moștenind clasa `Print`. Permite rutarea transparentă a tuturor ieșirilor de loguri (`netConsole.printf`, `netConsole.println`) către:
  1. Serial fizic prin USB (dacă este conectat).
  2. Conexiunea activă Telnet TCP pe portul 23.
  3. Ring buffer în memorie RAM (8 KB) pentru afișarea în browser.
- **Server Telnet TCP pe portul 23:**
  - Ascultă conexiuni remote via `WiFiServer(23)`.
  - Configurat `monitor_port = tcp://192.168.1.20:23` în `platformio.ini`, permițând rularea directă a `pio device monitor` fără cablu USB și fără drivere de port COM virtual în Windows.
  - Oferă prompt interactiv `> `, suport pentru comenzi cu backspace și ecou local.
- **Consolă Web 2-Way la `/console`:**
  - Pagină web dedicată cu stil modern (dark theme), terminal negru cu auto-scroll, bară de introducere comenzi cu memorie de istoric (tastele Up/Down), butoane rapide de acțiune (`STATUS`, `START BOILER`, `STOP BOILER`, `AUTO MODE`, `MANUAL MODE`, `HELP`, `CLEAR`).
  - Suportă automat atât accesul direct LAN (`http://192.168.1.20/console`) cât și accesul prin reverse proxy Galaxy A6 (`http://galaxy-a6/heating/console` sau prin Tailscale).
  - API de streaming rapid cu antet HTTP `X-Log-Seq` pentru transfer incremental fără pierderi și fără overhead de serializare.
- **Control Bidirecțional 2-Way Total:**
  - Comenzile primite prin Web Console sau Telnet (`1`, `0`, `AUTO`, `MANUAL`, `SET <zone> <temp>`, `STATUS`, `HELP`) sunt executate direct pe procesor.
  - Răspunsurile și evenimentele de sistem (telemetrie primită de la senzori, pornire/oprire boiler prin RF, declanșare relee actuatoare) se văd instantaneu în consola web și în Telnet.

### Verificare Live
- Compilat și încărcat via Web OTA în 6.43 secunde.
- Testat bidirecțional din script Python (`scratch/test_console.py`):
  - Web Console `/console` accesibilă cu HTTP 200 OK.
  - Comanda `STATUS` trimisă prin API-ul web a generat instantaneu raportul complet cu zonele, temperaturile și heap-ul liber (241 KB).
  - Socket-ul Telnet TCP pe portul 23 a răspuns la comanda `HELP` cu meniul complet de comenzi.

## [07.10.2026] - Recepție Placă Nouă ESP32, Migrare 100% Curată la ESP32, Traducere Engleză & Validare Live Web OTA

### Hardware & Programare Fizică
- **Recepționată Noua Placă:** ESP32-Relay X4 V1.1 cu modul **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3 Revision 3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
- **Verificare Conexiune Serială (COM6):** Folosită vechea placă NodeMCU ca adaptor USB-to-UART transparent (pin `RST` tras la `GND`). Interogare reușită prin `esptool.py` confirmând cipul, memoria SPI Flash de 4MB și identificatorii interni.
- **Scriere Firmware Inițial:** Firmware-ul a fost flashuit complet la 460800 baud în 13.1 secunde (toate segmentele verificate cu hash: bootloader, partitions, boot_app0, firmware.bin).
- **Documentare Hardware:** Creat [`docs/hardware_esp32_relay.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/hardware_esp32_relay.md) cu schema completă de pini, cablare passthrough și procedură OTA.

### Curățare Arhitectură Software & Traducere 100% în Engleză
- Conform cerinței explicite de arhitectură curată, s-au eliminat complet toate ramurile și directivele `#ifdef ESP8266` / `#elif defined(ESP32)`.
- Codul este dedicat 100% și nativ arhitecturii **ESP32**:
  - `include/Config.h`: Pini direct definiți pentru ESP32 (Relee `GPIO32, 33, 25, 26`, LED `GPIO23`, RF Data `GPIO19`). Nivel activ relee: `HIGH`.
  - `include/MqttTelemetryClient.h` & `src/MqttTelemetryClient.cpp`: Utilizare directă a `<WiFi.h>` și generare ID client pe baza Efuse MAC (`ESP32-SmartHeating-<mac>`).
  - `src/BoilerRadioTrigger.cpp`: Rutină OOK curată folosind `digitalWrite` fără registre hardware ESP8266 (`GPOS`/`GPOC`) sau apeluri de watchdog specifice 8266.
  - `src/ThermalZoneManager.cpp`: Stări, loguri și comentarii traduse în engleză (`Idle (Temp OK)`, `Heating Active`, `Actuator Warming Up`, `Disabled`).
  - `src/main.cpp`: Utilizare directă a bibliotecilor native ESP32 `<WiFi.h>`, `<WebServer.h>`, `<HTTPUpdateServer.h>`, `WiFi.setHostname`. Toate șirurile UI (titlu, butoane, etichete zone, telemetrie), meniul CLI (`HELP`/`STATUS`) și logurile Serial traduse în **100% limba engleză**.
  - `platformio.ini`: Mediu implicit unic `default_envs = esp32dev` (`platform = espressif32`, `board = esp32dev`, `upload_speed = 460800`).

### Validare Operațională Live & Web OTA
- Placa a pornit autonom, s-a conectat la Wi-Fi `SBC_WiFi` și a primit adresa IP **`192.168.1.20`**.
- Sistemul a stabilit instantaneu conexiunea cu brokerul Mosquitto de pe Galaxy A6 (`192.168.1.28:1883`) și ingerează în timp real telemetria de la senzorii Xiaomi BLE (Living Room 23.0°C, Kids Room 23.3°C, Bathroom 22.1°C).
- Heap liber: **235 KB** (un salt masiv față de cei 28 KB disponibili pe ESP8266).
- **Web OTA Funcțional:** Actualizarea noului binar complet tradus în limba engleză a fost trimisă wireless prin HTTP POST multipart la `http://192.168.1.20/update` și scrisă pe cip în doar **6.55 secunde**, urmată de reboot automat și reluarea funcționării fără cabluri.
- Actualizat proxy-ul Nginx (`nexus_nginx.conf`) pe Galaxy-A6 către noua adresă `192.168.1.20`.

## [07.10.2026] - Cablare Hardware Emițător RF, Driver C++ & Flash pe ESP8266

### Hardware & Măsurători
- **Validare Pinout Modul Emițător RF 433.92 MHz:**
  - Testat cu multimetrul pe pad-urile de test (`V`, `GND`):
    - **Fir Negru:** GND $\rightarrow$ Conectat la pinul **G** (GND) ESP8266.
    - **Fir Galben:** VCC (+3.3V) $\rightarrow$ Conectat la pinul **3V** (+3.3V) ESP8266.
    - **Fir Verde:** DATA $\rightarrow$ Conectat la pinul **D1** (GPIO5) ESP8266.
    - **Pin 2 (nelipit):** Punte internă cu VCC (Galben).
  - Păstrată placa PCB verde intactă pentru a beneficia de antena perimetrică acordată la 433.92 MHz.
- **Clarificare Arhitectură Microcontroller:**
  - Sistemul țintă este **ESP8266** (ESP-12F / LC-Relay-ESP12-4R-MV cu 4 relee). Telemetria BLE a senzorilor Xiaomi LYWSD03MMC este asigurată extern de gateway-ul Samsung Galaxy A6 via broker MQTT Mosquitto.

### Software & Firmware
- **PlatformIO Setup:** Creat fișierul [`platformio.ini`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/platformio.ini) (`board = esp12e`, `framework = arduino`, 4M1M flash).
- **Configurație Centralizată:** Adăugat fișierul [`include/Config.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Config.h) pentru alocarea pinilor și a rețelei.
- **Driver Radio `BoilerRadioTrigger`:**
  - Creat [`include/BoilerRadioTrigger.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BoilerRadioTrigger.h) și [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp).
  - Include tabelele PROGMEM cu 409 tranziții, preambul de 65 ms, tren de impulsuri OOK de ~131 ms generat prin bit-banging direct pe registrele `GPOS` / `GPOC` și `ESP.wdtFeed()` pentru prevenirea reseturilor WDT.
- **Test Bench Firmware ([`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp)):**
  - Serial CLI interactiv la 115200 baud (`1`/`ON`, `0`/`OFF`, `S`/`STATUS`, `?`/`HELP`).
  - Web Server responsive pe portul 80 cu butoane mari de comandă.
  - Endpoint-uri REST: `/api/boiler/on`, `/api/boiler/off`, `/api/boiler/status`.
  - Flash Web OTA integrat pe `/update` (actualizare facilă prin Wi-Fi fără cablu USB).
- **Validare Flash & Conexiune:**
  - Firmware-ul a fost scris pe `COM6`.
  - Modulul ESP8266 s-a conectat automat la rețeaua Wi-Fi la IP-ul **`192.168.1.18`**, cu 48.4 KB Free Heap și timp de răspuns instantaneu la cererile HTTP.
- **Validare Funcțională pe Viu cu Centrala Termică:**
  - Testat declanșarea flăcării centralei direct din interfața web: semnalul radio de 433.92 MHz a fost recepționat de centrală, care a reacționat instantaneu la pornire/oprire.
- **Integrare Galaxy-A6 Gateway & Acces la Distanță Tailscale:**
  - Configurat Nginx reverse proxy pe nodul Samsung Galaxy A6 (`cinderella`) pentru subpath-ul `/heating/` direcționat către `http://192.168.1.18/`.
  - Adaptat frontend-ul din `src/main.cpp` pentru URL-uri relative dinamice, permițând funcționarea identică atât pe IP-ul local direct cât și prin proxy.
  - Actualizat portalul centralizat Smart Services Portal (`http://galaxy-a6/` / `http://192.168.1.28/`): adăugat cardul **Smart Heating** (cu pictogramă 🔥, monitorizare de stare în timp real la 5 secunde, afișare mod flacără, uptime și link rapid la senzorii climatici din Grafana).
  - Validat accesul extern securizat prin rețeaua **Tailscale** (`http://100.83.135.74/heating/`).
- **Client Telemetrie MQTT Mosquitto ([`include/MqttTelemetryClient.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/MqttTelemetryClient.h), [`src/MqttTelemetryClient.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/MqttTelemetryClient.cpp)):**
  - Integrat local `PubSubClient` și `ArduinoJson` (v7.0.4).
  - Configurat persistent brokerul Mosquitto de pe Galaxy-A6 să accepte conexiuni externe pe `0.0.0.0:1883` prin `/etc/mosquitto/conf.d/teslamate.conf`.
  - Ingestie live a senzorilor Xiaomi LYWSD03MMC (Living Room, Kids Room, Bathroom) cu temperatură, umiditate, nivel baterie și vârstă telemetrie.
- **Controler Termic Multi-Zonă Decuplat ([`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp)):**
  - Structură decuplată și cod modular pentru cele 3 zone termice (Living, Copil, Matrimonial).
  - Relee actuatori NC pe GPIO16, GPIO14, GPIO12 cu nivel configurabil (`RELAY_ACTIVE_LEVEL = LOW`).
  - Evaluare termică autonomă cu histerezis (±0.3°C), interlock delay pentru deschidere completă actuator ceară NC înainte de aprinderea flăcării, pump overrun și protecție anti-short cycling a cazanului.
  - Moduri de lucru selectabile (AUTOMAT vs MANUAL), reglaj temperatură țintă din interfață și API REST extins (`/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/zones/sensor`, `/api/mode`).
  - Firmware compilat și încărcat cu succes pe ESP8266 (`COM6`), testat live în rețea.
- **Achiziție Hardware Panou de Perete (Kiosk Display):**
  - Cumpărată tableta **Samsung Galaxy Tab A9 (SM-X115, 4G LTE + Wi-Fi, 4GB RAM, 64GB stocare)** la prețul de **450 lei** (OLX).
  - Avantaje cheie validate: ecran 8.7" PLS LCD imun la burn-in, procesor MediaTek Helio G99 6nm de performanță înaltă, slot 4G de rezervă și limitare nativă a încărcării la 85% pentru funcționare 24/7 conectată la alimentare.

---

## [06.10.2026] - Inițializare Arhitectură & Configurare Proiect

### Adăugat
- **Inițializare Repository:** Creare repository git local și structurare directoare (`docs/`, `.agent/`).
- **Planificare Inginerească:** Redactare plan dinamic în limba română în [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md) conform metodologiei de lucru din `3dvehsim`.
- **Filtre Git:** Adăugare fișier [`.gitignore`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/.gitignore) pentru PlatformIO, C++, LittleFS și secrete locale.
- **Arhitectură de Sistem Agreată:**
  - Microcontroller principal: **ESP32** (autonomie 100% la nivel de nod, scanner BLE nativ integrat pentru senzori Xiaomi LYWSD03MMC BTHome v2).
  - Punte de telemetrie & Backup: Integrare bidirecțională cu Samsung Galaxy A6 (`smart-home-ble-gateway`) prin brokerul MQTT Mosquitto local.
  - Siguranță hidraulică: Interlock delay (120–180s) pentru deschiderea actuatorilor termici 230V NC înainte de pornirea flăcării cazanului + pump overrun de 180s pentru disiparea energiei reziduale.
  - Interfață Web: Găzduită local în flash-ul ESP32 prin LittleFS cu schemă vectorială interactivă SVG a apartamentului.
- **Sincronizare GitHub Remote:** Configurat remote origin și publicat branch-ul `main` pe [`https://github.com/RoDTOX/smart-home-heating`](https://github.com/RoDTOX/smart-home-heating) incluzând planul, changelog-ul și întregul Memory Bank persistent.


## [2026-10-06] - Sesiunea de SearÄƒ: Recuperare Proiect RF, Cartografiere HidraulicÄƒ & AchiziÈ›ie Hardware

### AdÄƒugat & Recuperat
- **Identificat È™i salvat proiectul RF de control al centralei:**
  - Sursa funcÈ›ionalÄƒ Arduino Uno: `control_RF_Centrala.ino` (410 tranziÈ›ii, preambul 65 ms, tabele `ondurations` È™i `offdurations`).
  - Analiza detaliatÄƒ a duratelor de impulsuri Ã®n nanosecunde/microsecunde: `ONOFF_OK_Signal_FiltereByNanosec.xlsx`.
  - Capturile digitale Sigrok / PulseView (`.sr`, `.pvs`), Saleae Logic (`.sal`) È™i dump-urile brute (2 MHz, text/binar).
  - Toate fiÈ™ierele au fost structurate Ã®n folderul `input/boiler_rf_trigger/` din repository.
- **Cartografiere hidraulicÄƒ completÄƒ din `Odaia.html` È™i prezentarea PPTX:**
  - Cele 7 circuite ale distribuitorului au fost mapate pe cele 3 zone termice principale:
    - **Zona 1 (Living + BucÄƒtÄƒrie):** RobineÈ›ii 1, 2, 3 (3 actuatoare NC Ã®n paralel pe Releul 1).
    - **Zona 2 (Camera Copilului):** RobineÈ›ii 6, 7 (2 actuatoare NC Ã®n paralel pe Releul 2).
    - **Zona 3 (Dormitor Matrimonial):** RobineÈ›ii 4, 5 (2 actuatoare NC Ã®n paralel pe Releul 3).
    - **Releul 4:** RÄƒmÃ¢ne rezervÄƒ liberÄƒ.
  - S-a stabilit cÄƒ cele 2 bÄƒi nu au circuite de pardosealÄƒ, avÃ¢nd calorifere portprosop deschise permanent, asigurÃ¢nd **bypass hidraulic natural** pentru pompa centralei.
- **Identificare staÈ›ie meteo exterioarÄƒ:**
  - Model Tronic **4-LD7396-2** (IAN `515492_2510`), funcÈ›ioneazÄƒ pe 433.92 MHz OOK/ASK.
  - S-a validat acurateÈ›ea compensÄƒrii climatice: 13.0Â°C Google/Open-Meteo vs 13.3Â°C senzorul fizic de Nord.

### AchiziÈ›ii Hardware Efectuate (Comandate pe eMAG)
- **1x PlacÄƒ ESP32 cu 4 Relee (Songle 10A 250VAC)**, control Wi-Fi/BLE, conectori GPIO expansiune (cod AI1254).
- **2x Seturi Radio 433MHz (SYN115 emiÈ›Äƒtor + SYN480R receptor superheterodin)**, antene sfert de undÄƒ 17.3 cm.

### Conexiune & Repository
- Configurat accesul SSH la GitHub (`git@github.com:RoDTOX/smart-home-heating.git`).
- Validat `git ls-remote` fÄƒrÄƒ erori.
