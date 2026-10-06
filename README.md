# Controler Inteligent de Încălzire Multi-Zonă (Smart Home Heating)

Un sistem autonom și economic de control termohidraulic multi-zonă, dezvoltat pe microcontrolerul **ESP32**. Conceput pentru reglajul independent al temperaturii pe 5–6 camere cu actuatoare electrotermice 230V NC, integrare directă cu senzori de temperatură Xiaomi BLE (firmware open-source BTHome v2), comandă radio sniffată pentru pornirea/oprirea flăcării centralei termice și interfață web găzduită local în memoria flash (LittleFS), incluzând schema vectorială interactivă a apartamentului.

---

## 📖 Documentație & Planificare

- 📋 **[Plan Dinamic & Registru de Sarcini (docs/plan_oct2026.md)](docs/plan_oct2026.md)** – Planul activ de lucru, preanaliza inginerească, constatări și sarcini `Todo` / `Done`.
- 📝 **[Jurnal de Modificări & Verificări (docs/changelog_oct2026.md)](docs/changelog_oct2026.md)** – Jurnalul detaliat al funcționalităților implementate, testate și validate.
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
6. **Interfață Web Integrată (Zero Cloud):**
   - Stocată direct pe ESP32 în LittleFS. Oferă schemă interactivă SVG a apartamentului, comenzi rapide per cameră, grafice locale 24h și compatibilitate PWA (instalare pe ecranul telefonului dintr-un click).
