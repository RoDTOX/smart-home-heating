# Controler Inteligent de Încălzire Multi-Zonă (Smart Home Heating)

Un sistem autonom și economic de control termohidraulic multi-zonă, dezvoltat pe microcontrolerul **ESP32**. Conceput pentru reglajul independent al temperaturii pe 5–6 camere cu actuatoare electrotermice 230V NC, integrare directă cu senzori de temperatură Xiaomi BLE (firmware open-source BTHome v2), comandă radio sniffată pentru pornirea/oprirea flăcării centralei termice și interfață web găzduită local în memoria flash (LittleFS), incluzând schema vectorială interactivă a apartamentului.

---

## 📖 Documentație & Planificare

- 📑 **[Cerințe de Sistem & Funcționalități (docs/requirements_and_features.md)](docs/requirements_and_features.md)** – Documentul de cerințe (PRD), specificații tehnice, compartimentare zone, reguli de siguranță hidraulică și matrice de trasabilitate.
- 🧪 **[Raport Testare Automată Sistem ASPICE SWE.5 (docs/system_test_report.md)](docs/system_test_report.md)** – Rezultatele testelor automate end-to-end rulate de pe PC împotriva plăcii (100% PASS).
- 💧 **[Ghid Hidraulică Pardoseală & Failsafes (docs/underfloor_heating_hydraulics_and_failsafes.md)](docs/underfloor_heating_hydraulics_and_failsafes.md)** – Ghid pentru grupuri de amestec GPA, pompă recirculare, actuatoare NC/NO și siguranță șapă.
- 💰 **[Evidență Piese, Bucăți & Costuri - BOM (docs/bom_hardware_costs.md)](docs/bom_hardware_costs.md)** – Bill of Materials detaliat cu piese în uz, comandate, reutilizate și planificate, prețuri unitare, totaluri și ghid de achiziție actuatoare.
- 📊 **[Studiu de Piață, Concurență & Evaluare ROI (docs/market_analysis_and_roi.md)](docs/market_analysis_and_roi.md)** – Evaluare viabilitate comercială, profil de piață, comparație Salus/Tado/Honeywell, unit economics și strategii de monetizare.
- 💼 **[Plan de Vânzări, Side-Hustle & Registru TODO (docs/plan_sales.md)](docs/plan_sales.md)** – Strategie de lansare locală, ofertă la cheie cu montaj, scripturi de conversie, procedură de montaj în 90 min și registru TODO.
- ☁️ **[Arhitectură Cloud Fleet, Diagnoză & Service Mode (docs/remote_fleet_and_service_mode.md)](docs/remote_fleet_and_service_mode.md)** – Conectivitate inversată Outbound TLS, coduri DTC, Service Mode pentru suport telefonic la distanță și Fleet OTA.
- 🔌 **[Documentație Hardware & Pinout Placă (docs/hardware_esp32_relay.md)](docs/hardware_esp32_relay.md)** – Specificații placă ESP32-Relay X4 V1.1, alocare pini GPIO, procedură passthrough și tabel dispozitive rețea.
- 📋 **[Plan Dinamic & Registru de Sarcini (docs/plan.md)](docs/plan.md)** – Planul activ de lucru, preanaliza inginerească, constatări și sarcini `Todo` / `Done`.
- 📝 **[Jurnal de Modificări & Verificări (docs/changelog.md)](docs/changelog.md)** – Jurnalul detaliat al funcționalităților implementate, testate și validate.
- 🤖 **[Instrucțiuni & Disciplină Inginerie AI (AGENTS.md)](AGENTS.md)** – Principiile Ponytail (Ladder of Laziness, YAGNI, anti-bloat), protocoale de memorie și siguranță.
- 🧠 **[Bancă de Memorie Persistentă (.agent/memory-bank/)](.agent/memory-bank/)** – Memoria persistentă de context pentru asistenții AI (scopul produsului, tipare arhitecturale, context tehnic, decizii).

---

## 🏛️ Arhitectură Generală (100% Autonomă & Fără Bloatware)

```text
[ Senzori Xiaomi BLE LYWSD03MMC (fiecare cameră) ]
       │ 
       │ (Broadcast BLE BTHome v2 & PVVX/ATC direct în aer)
       ▼
[ ESP32 - Controler Termic Central (ESP32-Relay X4 V1.1) ]
  ├── Scanner BLE Nativ Pasiv (NimBLE - zero RF emissions, zero baterie consumată)
  ├── Motor Termic Multi-Zonă (ThermalZoneManager)
  │     ├── Buclă Histerezis (±0.3°C)
  │     ├── Interlock Deschidere Actuatoare (NC/NO)
  │     ├── Control Pompă Recirculare GPA (Releu 4) cu Overrun 180s
  │     ├── Protecție la Îngheț (<12°C) & Izolare Avarie Senzor (DTC-101)
  │     └── Exercițiu Anti-Gripare Săptămânal (20s)
  ├── Gardă Anti-Spam & Anti-Short-Cycling (BoilerRadioTrigger)
  │     ├── Filtru Idempotență (0 ms, zero RF inutil)
  │     ├── Interblocare Arzător 60s (Min Run / Min Pause) -> HTTP 429
  │     └── Detecție Abuz Spam -> DTC-201 Warning
  ├── Diagnostic Engine & Service Mode la Distanță (DTC-101 .. DTC-402)
  ├── Pașaport Digital NVS & Sincronizare NTP / Vreme Open-Meteo
  ├── Server Web Local (Port 80: REST API, Setup Wizard, OTA Flash)
  ├── Consolă Virtuală Silențioasă (Telnet TCP:23 & Web /console)
  ├── Driver Relee (Actuatoare Pardoseală 230V NC/NO)
  └── Emițător Radio 433.92 MHz (DATA pe GPIO19)
        ▼
   [ Centrală Termică pe Gaz (Comandă Flacără ON/OFF) ]
```

---

## 🚀 Principii Cheie & Siguranță Hidraulică

1. **Autonomie Totală & Zero Bloatware:**
   - Sistemul nu depinde de brokeri MQTT, bridge-uri externe sau servere suplimentare. Totul este procesat nativ direct pe ESP32 prin radio Bluetooth NimBLE.
2. **Interlock Deschidere Actuatoare (120–180s):**
   - Actuatoarele electrotermice cu ceară/parafină au nevoie de 2–3 minute pentru a deschide complet circuitul. Centrala și pompa secundară pornesc doar după ce cel puțin un circuit este deschis, prevenind cavitația și suprapresiunea.
3. **Control Pompă Grup de Amestec (GPA) & Post-circulație (180s):**
   - Releul 4 comandă pompa de circulație dedicată a distribuitorului. La atingerea temperaturii setate, pompa continuă să circule apa încă 3 minute pe ultima zonă activă (*keeper zone*) pentru a disipa căldura reziduală.
4. **Protecție Anti-Short-Cycling & Filtru Anti-Spam (FR-17):**
   - Previne avarierea plăcii electronice a centralei prin interblocare minimă de 60s între pornire și oprire, răspunzând cu HTTP 429 Too Many Requests și înregistrând `DTC-201` la spam abuziv.
5. **Consolă Silențioasă & Confort:**
   - Consola TCP:23 / Serial nu difuzează pachete de la senzori în fundal; datele sunt listate strict la cerere explicită.
6. **Arhitectură Decuplată Headless & Testare Automată ASPICE SWE.5:**
   - Microcontrolerul expune un catalog complet de API-uri REST/JSON (`/api/zones`, `/api/boiler/status`, `/api/ble/sensors`, `/api/diag/status`, `/api/setup/passport`) cu timp de răspuns sub 100 ms.
   - Suită completă de testare automată de pe PC: `python tools/esp_cli.py test` sau `python tools/test_system.py` cu raport de verificare 100% PASS în [`docs/system_test_report.md`](docs/system_test_report.md).
   - Utilitar complet CLI: [`tools/esp_cli.py`](tools/esp_cli.py) (`status`, `set`, `selftest`, `diag`, `ble`, `passport`, `time`, `pump`, `actuator`, `exercise`, `test`, `monitor`).
