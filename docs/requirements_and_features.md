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

---

## 4. Cerințe Non-Funcționale (Non-Functional Requirements)

### 4.1. Siguranță & Comportament la Avarie (NFR-1: Safety & Failsafe)
- **NFR-1.1 (Autonomie Totală la Edge):** Toate deciziile de deschidere/închidere relee și pornire/oprire centrală sunt executate local în memoria ESP32. Căderea conexiunii Wi-Fi, a routerului, a brokerului MQTT sau a telefonului gateway **NU întrerupe încălzirea**.
- **NFR-1.2 (Failsafe Cădere Senzor):** Dacă o zonă nu primește nicio citire de temperatură validă timp de peste 30 de minute, zona este marcată cu avarie, releul zonei este închis preventiv și se emite alertă în loguri pentru a evita supraîncălzirea necontrolată.
- **NFR-1.3 (Protecție la Îngheț):** Chiar dacă o zonă este dezactivată sau setată pe minim, dacă temperatura scade sub 12°C, sistemul forțează încălzirea de protecție la îngheț.
- **NFR-1.4 (Pornire Sigură la Revenirea Curentului):** După o pană de curent, releele pornesc în mod implicit în stare `LOW` (OPRITE), iar centrala este considerată oprită până la prima evaluare termică stabilă.

### 4.2. Performanță & Eficiență Resurse (NFR-2)
- **NFR-2.1 (Consum Energie):** Placa ESP32 consumă sub 1.2W la priză în regim continuu.
- **NFR-2.2 (Memorie RAM Liberă):** Arhitectura C++ pe ESP32 păstrează peste 230 KB de memorie heap liberă în orice moment (fără scurgeri de memorie la streaming de loguri sau parsare JSON).
- **NFR-2.3 (Răspuns Rapid):** Timpul de răspuns al API-ului web local este sub 50 ms.

### 4.3. Calitate Cod & Standarde (NFR-3)
- **NFR-3.1 (English Codebase Standard):** Toate fișierele sursă C++, anteturile, variabilele, comentariile, textele UI și logurile din consolă sunt redactate **100% în limba engleză**.
- **NFR-3.2 (Modularitate):** Fiecare subsistem este decuplat în clase C++ de sine stătătoare (`NetworkConsole`, `ThermalZoneManager`, `BoilerRadioTrigger`, `MqttTelemetryClient`).

---

## 5. Matrice de Trasabilitate (Cerințe vs Componente Software)

| Cod Cerință | Denumire Cerință | Modul / Fișier Implementat | Stare Validare |
|:---:|---|---|:---:|
| **FR-1** | Histerezis & Reglaj Multi-Zonă | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp) | Validat Live |
| **FR-2** | Actuator Interlock Delay (120s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L224) | Validat Live |
| **FR-3** | Pump Overrun Heat Dissipation (120s) | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L240) | Validat Live |
| **FR-4** | Anti-Short-Cycling Boiler Guard | [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp#L274) | Validat Live |
| **FR-5** | Comandă Radio Centrală 433.92 MHz | [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) | Validat Live |
| **FR-6** | Telemetrie Xiaomi BLE via MQTT | [`src/MqttTelemetryClient.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/MqttTelemetryClient.cpp) | Validat Live |
| **FR-7** | Consolă Virtuală 2-Way (Telnet & Web) | [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) | Validat Live |
| **FR-8** | Dashboard Web & REST API | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp#L42) | Validat Live |
| **FR-8.3**| Actualizare Firmware Web OTA | [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp#L512) | Validat Live (6.4s) |
| **NFR-1** | Operare Autonomă fără Cloud | Arhitectură nativă firmware ESP32 | Validat Live |
