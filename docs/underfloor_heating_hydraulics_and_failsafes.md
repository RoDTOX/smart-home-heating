# Ghid Tehnic de Hidraulică, Componente Speciale & Failsafes pentru Încălzirea în Pardoseală

> **Proiect:** Sistem Inteligent Autonom de Încălzire Multi-Zonă (Smart Home Heating)  
> **Destinat:** Instalatori autorizați, tehnicieni de service și proprietari de locuințe  
> **Hardware:** Placă ESP32-Relay X4 V1.1 (240MHz, 4MB Flash, IP: `192.168.1.20`)  
> **Data:** 09 Octombrie 2026 | Versiune Specificație: 2.0 (Failsafes & Hydraulic Hardening)

---

## 1. Introducere: Realitatea Instalațiilor din Teren

Instalațiile de încălzire în pardoseală (și cele mixte: pardoseală + radiatoare) diferă substanțial de la un apartament la o casă individuală sau un duplex. În timp ce un apartament mic poate avea doar o centrală murală conectată direct la distribuitor, casele și apartamentele moderne includ adesea **grupuri de pompare și amestec (GPA)**, **pompe secundare de recirculare**, **supape de presiune diferențială (bypass)** și **termostate mecanice de siguranță la supratemperatură**.

Pentru ca produsul nostru să fie un succes comercial „plug-and-play” out-of-the-box, firmware-ul și logica de comandă sunt proiectate să suporte toate aceste configurații fără modificări de cod, oferind flexibilitate totală și siguranță absolută.

---

## 2. Anatomia Distribuitorului de Pardoseală & Componentele Speciale

```text
       TUR de la Centrală (ex. 50-65°C) ───┐
                                          │
                                          ▼
                         ┌─────────────────────────────────┐
                         │ Grup de Pompare & Amestec (GPA) │
                         │ - Vană Termostatică de Amestec  │
                         │   (reduce apa la 35-40°C)       │
                         │                                 │
                         │ - Pompa Secundară de Circulație │ ◄── Comandată de RELEUL 4
                         │   (Wilo / Grundfos 230V)        │     (GPIO26 pe ESP32)
                         └────────────────┬────────────────┘
                                          │
                                          ▼
   TUR DISTRIBUITOR (Apă la 38°C) ────────┼───────────────────────┐
   [ Termostat Contact Siguranță 55°C ] ──┘                       │
   [ Debitmetre Reglaj Circuite 1..7 ]                            │
       │           │           │                                  │
    Bucla 1     Bucla 2     Bucla 3                             Bypass
    (Living)    (Copil)   (Matrimonial)                      Diferențial
       │           │           │                                  │
       ▼           ▼           ▼                                  ▼
   RETUR DISTRIBUITOR ────────────────────────────────────────────┘
   [ Ventile cu Actuatoare Electrotermice 230V (NC sau NO) ]
   (Comandate de Releele 1, 2, 3: GPIO32, GPIO33, GPIO25)
                                          │
                                          ▼
       RETUR spre Centrală (apă răcită la 28-32°C)
```

### 2.1. Actuatoare Electrotermice 230V: NC (Normally Closed) vs NO (Normally Open)
- **Actuatoare NC (95% din piață):** Fără tensiune electrică, arcul intern menține ventilul închis. Când primesc 230V de la releu, elementul termic din ceară/parafină se încălzește și deschide complet ventilul în **120–180 secunde**.
- **Actuatoare NO (5% din piață):** Fără tensiune, ventilul este deschis. Când primesc 230V, ventilul se închide.
- **Suport Firmware Universal:** Firmware-ul include setarea nativă `actuatorType: NC / NO` (salvată persistent în NVS). Comutarea se face dintr-un singur clic în Web Setup (`/setup`), prin API REST (`/api/setup/actuator-type?type=nc|no`) sau prin CLI (`python tools/esp_cli.py actuator nc`).

### 2.2. Pompa Secundară de Recirculare / Grupul de Amestec GPA (Releul 4 / GPIO26)
- **De ce există?** Traseele din pardoseală au sute de metri de țeavă PEX cu rezistență hidraulică mare. Pompa internă din centrala murală nu are debitul și presiunea necesare pentru a asigura circulația corectă prin vana de amestec. Din acest motiv, instalatorii adaugă o pompă dedicată la distribuitor.
- **Problema din teren:** Fără un controler inteligent, mulți proprietari lasă pompa băgată permanent în priză (24/7). Acest lucru duce la:
  1. Risipă inutilă de curent (45–65W continuu = ~40 kWh/lună aruncați).
  2. Uzură mecanică prematură a rotorului.
  3. **Cavitație și suprapresiune:** Când toate actuatoarele camerelor sunt închise, pompa împinge împotriva unui perete închis, provocând zgomote ascuțite în distribuitor și risc de ardere a motorului.
- **Soluția Implementată pe ESP32:**
  - Releul 4 (`GPIO26`) comandă automat alimentarea fazei (230V) a pompei secundare.
  - **Interlock de Pornire Pompă:** Pompa pornește DOAR DUPĂ ce cel puțin un actuator de zonă este fizic deschis (starea `ZONE_HEATING`), garantând că circuitul hidraulic are debit asigurat.
  - **Post-Circulație (Pump Overrun):** Când cererea de căldură încetează, pompa continuă să funcționeze 120–180 de secunde pentru a disipa căldura din cazan și pentru a egaliza temperatura în pardoseală.
  - **Keeper Zone:** Pe durata post-circulației, ultimul actuator activ rămâne deschis pentru a asigura bucla de circulație a apei.

### 2.3. Termostatul Mecanic de Siguranță la Contact (55°C - High-Limit Thermostat)
- **Ce este?** Un termostat mecanic cu bimetal montat cu colier metalic direct pe țeava de tur din cupru/pex a distribuitorului, reglat la 50–55°C.
- **De ce este critic?** În pardoseală nu trebuie să intre niciodată apă la peste 50°C (ar crăpa șapa, ar dezlipi gresia și ar degrada parchetul laminat/stratificat). Dacă vana termostatică mecanică de amestec se blochează accidental pe deschis, apa din centrală (la 65-75°C) ar intra direct în pardoseală!
- **Conectare Recomandată:** Contactul său NC (Normal Închis) se leagă în serie pe alimentarea generală a pompei de distribuitor și a centralei. Dacă turul sare de 55°C, contactul se desface mecanic instantaneu, tăind pompa indiferent de starea electronicii.

### 2.4. Supapa de Presiune Diferențială (Bypass Hidraulic)
- O supapă cu arc calibrat (0.2–0.3 bar) montată între colectorul de tur și cel de retur.
- În cazul în care toate actuatoarele se închid brusc înainte de oprirea pompei, presiunea deschide mecanic supapa, permițând apei să circule în scurtcircuit la capătul distribuitorului.
- *Notă:* În apartamentul de test, rolul de bypass hidraulic permanent este asigurat de cele 2 radiatoare portprosop din băi, care nu au actuatoare și rămân permanent deschise.

---

## 3. Matricea Failsafes-urilor de Siguranță Firmware (NFR-1)

| Nivel Siguranță | Mecanism Hardware / Software | Comportament la Anomalie | Cod Diagnoză |
| :--- | :--- | :--- | :---: |
| **Microcontroller Hang** | Hardware Task Watchdog (TWDT 8s) | Reset hardware automat al ESP32 în max 8 secunde; toate releele repornesc în starea sigură `LOW`. | `SYS-RST` |
| **Avarie / Pierdere Senzor** | Timeout Telemetrie (>30 min) | Zona este decuplată forțat (`demand=false`), actuatorul este închis pentru a preveni supraîncălzirea necontrolată a șapei. | `DTC-101` |
| **Baterie Descărcată** | Monitorizare CR2032 (<20%) | Avertizare proactivă înainte ca termometrul să se oprească complet. | `DTC-102` |
| **Temperatură Aberantă** | Citire sub 5°C sau peste 45°C | Izolare citire eronată pentru prevenirea deciziilor greșite. | `DTC-103` |
| **Protecție la Îngheț** | Frost Guard Urgență (<12°C) | Declanșare automată căldură și deschidere actuatoare chiar dacă modul este pe `MANUAL` sau `OFF`. | `FROST-GUARD` |
| **Protecție Cazan Gaz** | Anti-Short-Cycling Burner Guard | Minim 3 min funcționare continuă flacără / Minim 5 min pauză între reaprinderi pentru protejarea vanei de gaz. | `BURNER-GUARD` |
| **Gripare Mecanică Ax Pompă**| Exercițiu Anti-Blocare (Summer) | Rulare automată 20 secunde o dată la 7 zile în extra-sezon pentru spargerea depunerilor de calcar/magnetită. | `ANTI-SEIZE` |
| **Pierdere Conexiune Wi-Fi**| Autonomie Totală la Edge | Microcontrolerul continuă să regleze încălzirea 100% autonom prin BLE și histerezis local. | `NET-OFFLINE` |

### 3.1. Decuplare FSM Relee & Anulare Instantanee Warmup (Instant Warmup Abort)
- **Problema Tehnică:** Actuatoarele termoelectrice NC au nevoie de 120–180s pentru a deschide complet ventilul prin dilatarea elementului de ceară/parafină. Când o zonă cere căldură (`demand = true`), intră în starea `ZONE_WARMUP` și alimentează actuatorul la 230V. Dacă în acest interval utilizatorul reduce setpoint-ul (manual sau din Home Assistant) sau temperatura ambientală crește natural, condiția de cerere devine nulă (`demand = false`).
- **Comportament de Siguranță Firmware:**
  1. Starea FSM a zonei este imediat retrogradată din `ZONE_WARMUP` în `ZONE_IDLE`.
  2. **Invariant Hardware Releu Orfan:** Releul fizic al zonei este forțat instantaneu la nivel inactiv (`digitalWrite(pin, LOW)` pentru actuatoare NC), tăind alimentarea de 230V. Vana se închide fără a rămâne deschisă fantomă în instalație.
  3. **Protecție Cazan Gaz:** Centrala termică nu primește comanda de aprindere prin radio 433 MHz dacă toate zonele își anulează faza de warmup, eliminând pornirile inutile pe circuite fără circulație hidraulică activă (anti-dry firing).

### 3.2. Post-Circulație Inteligentă Condiționată de Flacără (Intelligent Overrun)
- **Problema Tehnică:** Pompa de distribuitor (GPA, Releul 4) include o funcție de post-circulație de 180 secunde (`PUMP_OVERRUN_MS = 180000`) pentru a disipa căldura acumulată în schimbătorul primar al cazanului după stingerea flăcării. Dacă cererea a fost inițiată, cazanul a intrat în armare prealabilă (`PENDING_ON`), dar cererea a fost anulată înainte ca flacăra să se aprindă efectiv (`_flameStartTimeMs == 0`), rularea pompei timp de 3 minute ar genera zgomot, uzură mecanică inutilă a rotorului și răcirea șapei.
- **Soluție Firmware (`BoilerRadioTrigger`):**
  - Temporizarea de post-circulație este declanșată **STRICT CONDIȚIONAT** de faptul că flacăra a fost aprinsă efectiv (`_flameStartTimeMs > 0`).
  - Dacă starea cazanului este anulată înainte de aprindere (ex. prin comanda `/api/boiler/cancel` sau prin stingerea tuturor cererilor din camere), cazanul revine direct în `IDLE` cu `overrunRemainingSec = 0`.
  - Când flacăra a funcționat efectiv, ultimul circuit activ (`_lastDemandZoneIndex`) este menținut forțat deschis pe întreaga durată a celor 180 secunde de post-circulație („Keeper Zone”), garantând că pompa secundară nu împinge împotriva unui colector închis.

### 3.3. Tăiere Hidraulică la Detectare Fereastră Deschisă (Open Window Hydronic Cutoff)
- **Problema Tehnică:** Aerisirea camerei în sezonul rece provoacă o scădere bruscă a temperaturii aerului. Un termostat clasic ar forța deschiderea circuitului și arderea masivă de gaz pentru a încerca să încălzească aerul de afară. Datorită inerției șapei (2–4 ore), încălzirea pompată în timpul aerisirii ar duce la o supraîncălzire sufocantă a camerei după închiderea geamului.
- **Soluție Firmware (`ThermalZoneManager`):**
  - Algoritmul calculează derivata de temperatură pe o fereastră mobilă de 10 minute (`WINDOW_HISTORY_SLOTS = 5`, eșantioane la fiecare 2 minute).
  - Prag de detecție dinamic configurabil în intervalul sigur `[0.8°C .. 2.0°C]` (default `1.2°C`). Valorile sub 0.8°C sunt refuzate pentru a evita declanșările false cauzate de curenți slabi sau oscilații normale ale senzorului BLE.
  - La confirmarea căderii de temperatură:
    1. Zona intră în starea de protecție `windowOpen = true`.
    2. Cererea termică a camerei este suprimată forțat (`demand = false`).
    3. Releul electrotermic asociat este decuplat instantaneu la 0V (actuatorul NC închide ventilul buclei).
    4. Încălzirea pe camera respectivă rămâne blocată timp de 30 de minute (`WINDOW_PAUSE_MS = 1800000`) sau până la deblocarea manuală prin `/api/zones/window/clear?zone=X` ori din comutatorul dedicat din Home Assistant.

---

## 4. Ghid de Cablare Electrică la Montaj

Placa **ESP32-Relay X4 V1.1** este alimentată la 230V AC (sau 5V DC micro-USB). Cele 4 relee au contacte normal deschise (NO) și comune (COM) capabile să comute până la 10A la 250V AC.

### Schemă de conexiuni recomandată:

```text
                  FAZĂ 230V AC (de la panoul de siguranțe al locuinței)
                                     │
                                     ├────────────────────────────┐
                                     │ (Siguranță fuzibilă 2A-4A) │
                                     │                            │
                    ┌────────────────┴────────────────┐           │
                    │      ESP32-Relay X4 V1.1        │           │
                    │  Terminal Alimentare AC: L & N  │           │
                    └────────────────┬────────────────┘           │
                                     │                            │
     ┌───────────────────────────────┼────────────────────────────┤
     ▼ COM Releu 1                   ▼ COM Releu 2                ▼ COM Releu 4
[ Releu 1 - GPIO32 ]            [ Releu 2 - GPIO33 ]         [ Releu 4 - GPIO26 ]
     ▼ NO                            ▼ NO                         ▼ NO
Fază spre Actuator NC 1         Fază spre Actuator NC 2      Fază spre Pompa Distribuitor
(Living & Bucătărie)            (Camera Copilului)           (Grup de Amestec GPA)
     │                               │                            │
     ▼                               ▼                            ▼
  Actuator 1                      Actuator 2                    Pompă
     │                               │                            │
     └───────────────────────────────┴────────────────────────────┘
                                     │
                                     ▼
                  NUL 230V AC (Comun pentru toți consumatorii)
```

---

## 5. Comenzi Rapide CLI pentru Diagnoză & Punere în Funcțiune

Pentru verificare instantanee fără browser la montaj:

```bash
# Verificare stare completă (zone, pompă, actuatoare, senzori)
python tools/esp_cli.py status

# Verificare sau configurare tip actuator (NC = Normally Closed, NO = Normally Open)
python tools/esp_cli.py actuator
python tools/esp_cli.py actuator nc

# Verificare sau configurare releu pompă de circulație
python tools/esp_cli.py pump
python tools/esp_cli.py pump enable

# Declanșare manuală ciclu test anti-gripare (20 secunde deschidere actuatoare + pompă)
python tools/esp_cli.py exercise

# Raport autodiagnostic și verificare coduri de eroare DTC
python tools/esp_cli.py selftest
python tools/esp_cli.py diag
```
