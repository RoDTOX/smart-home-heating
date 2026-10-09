# Changelog: Sistem Inteligent de Încălzire (Smart Heating)

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md).

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
