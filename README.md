# Controler Inteligent de Încălzire Multi-Zonă (Smart Home Heating)

Un sistem autonom și economic de control termohidraulic multi-zonă, dezvoltat pe microcontrolerul **ESP32**. Conceput pentru reglajul independent al temperaturii pe 5–6 camere cu actuatoare electrotermice 230V NC, integrare directă cu senzori de temperatură Xiaomi BLE (firmware open-source BTHome v2), comandă radio sniffată pentru pornirea/oprirea flăcării centralei termice și interfață web găzduită local în memoria flash (LittleFS), incluzând schema vectorială interactivă a apartamentului.

---

## 📖 Documentație & Planificare

- 📑 **[Cerințe de Sistem & Funcționalități (docs/requirements_and_features.md)](docs/requirements_and_features.md)** – Documentul de cerințe (PRD), specificații tehnice, compartimentare zone, reguli de siguranță hidraulică și matrice de trasabilitate.
- 💰 **[Evidență Piese, Bucăți & Costuri - BOM (docs/bom_hardware_costs.md)](docs/bom_hardware_costs.md)** – Bill of Materials detaliat cu piese în uz, comandate, reutilizate și planificate, prețuri unitare, totaluri și ghid de achiziție actuatoare.
- 📊 **[Studiu de Piață, Concurență & Evaluare ROI (docs/market_analysis_and_roi.md)](docs/market_analysis_and_roi.md)** – Evaluare viabilitate comercială, profil de piață, comparație Salus/Tado/Honeywell, unit economics și strategii de monetizare.
- 💼 **[Plan de Vânzări, Side-Hustle & Registru TODO (docs/plan_sales.md)](docs/plan_sales.md)** – Strategie de lansare locală, ofertă la cheie cu montaj, scripturi de conversie, procedură de montaj în 90 min și registru TODO.
- ☁️ **[Arhitectură Cloud Fleet, Diagnoză & Tesla-Style Service Mode (docs/remote_fleet_and_service_mode.md)](docs/remote_fleet_and_service_mode.md)** – Conectivitate inversată Outbound TLS, coduri DTC, Service Mode pentru suport telefonic la distanță și Fleet OTA.
- 🔌 **[Documentație Hardware & Pinout Placă (docs/hardware_esp32_relay.md)](docs/hardware_esp32_relay.md)** – Specificații placă ESP32-Relay X4 V1.1, alocare pini GPIO, procedură passthrough și tabel dispozitive rețea.
- 📋 **[Plan Dinamic & Registru de Sarcini (docs/plan.md)](docs/plan.md)** – Planul activ de lucru, preanaliza inginerească, constatări și sarcini `Todo` / `Done`.
- 📝 **[Jurnal de Modificări & Verificări (docs/changelog.md)](docs/changelog.md)** – Jurnalul detaliat al funcționalităților implementate, testate și validate.
- 🧠 **[Bancă de Memorie Persistentă (.agent/memory-bank/)](.agent/memory-bank/)** – Memoria persistentă de context pentru asistenții AI (scopul produsului, tipare arhitecturale, context tehnic, decizii).

---

## 🏛️ Arhitectură Generală

```text
[ Senzori Xiaomi BLE LYWSD03MMC (fiecare cameră) ]
       │ (Broadcast BLE BTHome v2 - interval 10s-20s)
       ├─────────────────────────────────┐
       ▼                                 ▼ (fallback dacă semnalul direct e slab)
[ ESP32 - Controler Termic Central ]   [ Samsung Galaxy A6 ("Cinderella") ]
  ├── Scanner BLE Nativ (NimBLE)        ├── Scanner BTSnoop & Broker Mosquitto MQTT
  ├── Motor Termic (ThermalZoneManager)  ├── Bază de date PostgreSQL & Grafana
  │     ├── Buclă Histerezis (±0.3°C)    └── Punte Telemetrie (MQTT pe rețeaua locală)
  │     ├── Întârziere Deschidere Valvă          │
  │     └── Interlock Protecție Centrală         │
  ├── Server Web Local (LittleFS PWA) ◄──────────┘
  ├── Driver Relee (Actuatoare 230V NC)
  └── Driver Comandă Radio Centrală (Semnal Sniffat)
        ▼
   [ Centrală Termică (Comandă Flacără ON/OFF) ]
```

---

## 🚀 Principii Cheie & Siguranță Hidraulică

1. **Autonomie Totală la Nivel de ESP32:**
   - Toată logica de decizie, protecție și control rulează pe ESP32. Dacă pică routerul Wi-Fi, conexiunea la internet sau telefonul A6, încălzirea continuă fără nicio întrerupere.
2. **Interlock Deschidere Actuatoare (120–180s):**
   - Actuatoarele electrotermice cu ceară/parafină au nevoie de 2–3 minute pentru a deschide complet circuitul. Centrala nu este pornită până când cel puțin o valvă nu este fizic deschisă, protejând pompa de circulație împotriva suprapresiunii și cavitației.
3. **Disipare Căldură Reziduală (Pump Overrun - 180s):**
   - La atingerea temperaturii setate și oprirea flăcării centralei, ultimul actuator activ rămâne deschis încă 3 minute pentru a disipa căldura din schimbătorul de căldură primar.
4. **Protecție Anti-Short-Cycling:**
   - Previne uzura centralei prin impunerea unui timp minim de funcționare a flăcării (3 min) și a unei pauze minime între reaprinderi (5 min).
5. **Ingestie Hibridă de Date (Reziliență Maximă):**
   - ESP32 ascultă pasiv semnalele BLE ale senzorilor. Dacă o cameră îndepărtată are pereți groși de beton și semnal slab la distribuitor, temperatura este preluată automat prin MQTT de la telefonul A6.
6. **Arhitectură Decuplată Headless (API-First & Utilitare CLI):**
   - Microcontrolerul expune un catalog complet de API-uri REST/JSON (`/api/zones`, `/api/boiler/status`, `/api/ble/sensors`, `/api/diag/status`, `/api/setup/passport`) cu timp de răspuns sub 100 ms.
   - Orice interfață viitoare (PWA, aplicație mobilă sau cloud) funcționează independent de logica de pe cip.
   - Pachet complet de automatizare și depanare CLI în linia de comandă: [`tools/esp_cli.py`](tools/esp_cli.py) (`status`, `set`, `selftest`, `diag`, `ble`, `passport`, `time`).
