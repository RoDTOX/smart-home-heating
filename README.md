# Smart Home Heating - DIY Multi-Zone ESP32 Controller

An autonomous, low-cost hydronic climate control system powered by an **ESP32** microcontroller. Designed for multi-room heating control with 230V NC electrothermal manifold actuators, integration with open-source Xiaomi BLE thermometers (BTHome v2), a reverse-engineered wireless boiler trigger, and an embedded LittleFS web interface with an interactive SVG apartment floorplan.

---

## 📖 Documentație & Planificare

- 📋 **[Plan Dinamic & Task Tracker (docs/plan_oct2026.md)](docs/plan_oct2026.md)** – Planul activ de lucru, preanaliza inginerească, constatări și sarcini `Todo` / `Done`.
- 📝 **[Changelog & Jurnal Verificări (docs/changelog_oct2026.md)](docs/changelog_oct2026.md)** – Jurnalul detaliat al funcționalităților implementate și testate.
- 🧠 **[Memory Bank (.agent/memory-bank/)](.agent/memory-bank/)** – Memoria persistentă a asistentului AI (context produs, tipare arhitecturale, tehnologie, decizii).

---

## 🏛️ Arhitectură Generală

```text
[ Senzori Xiaomi BLE LYWSD03MMC (camere) ]
       │ (BTHome v2 Advertising Beacons)
       ├─────────────────────────────────┐
       ▼                                 ▼ (fallback semnal slab)
[ ESP32 Smart Heating Controller ]    [ Samsung Galaxy A6 ("Cinderella") ]
  ├── NimBLE-Arduino Scanner            ├── BTSnoop Scanner & Mosquitto MQTT
  ├── ThermalZoneManager Engine         ├── PostgreSQL & Grafana Telemetry
  │     ├── Hysteresis Loop             └── Telemetry Bridge (WiFi MQTT)
  │     ├── Actuator Opening Delay              │
  │     └── Boiler Demand Interlock             │
  ├── Web Server (LittleFS PWA) ◄───────────────┘
  ├── Relay Driver (Actuatoare 230V NC)
  └── Boiler Radio Trigger Driver (Semnal Sniffat)
        ▼
   [ Centrala Termică (Flacără ON/OFF) ]
```

---

## 🚀 Principii Cheie

1. **Autonomie 100% la nivel de ESP32:** Sistemul funcționează independent de conexiunea la internet, routerul Wi-Fi sau telefonul A6.
2. **Siguranță hidraulică & Interlock:** Centrala nu pornește niciodată cu toate robinetele închise. Se aplică o întârziere termică (2–3 min) pentru deschiderea completă a actuatorilor electrotermici cu parafină.
3. **Disipare căldură reziduală (Pump Overrun):** Cel puțin un circuit rămâne deschis timp de 3 minute după oprirea flăcării pentru protecția schimbătorului centralei.
4. **Protecție Anti-Short-Cycling:** Timp minim de ardere (3 min) și pauză minimă între aprinderi (5 min).
5. **Dashboard Web local (LittleFS):** Schemă interactivă SVG a apartamentului, comenzi rapide per cameră și compatibilitate PWA mobil.
