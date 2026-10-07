# Plan de Lucru Activ (Octombrie 2026) – Smart Home Heating

Acest document este planul de referință pentru implementarea controlerului autonom multi-zonă ESP32, integrat cu senzorii Xiaomi BLE (BTHome v2), emițătorul RF pentru centrală și interfața LittleFS cu schemă interactivă SVG.

---

## 🧠 Sinteză Arhitectură & Decizii Tehnice

1. **Hardware & Control Relee (CONFIRMAT & COMANDAT):**
   - **Placă:** Modul dedicat **ESP32 cu 4 Relee (Songle 10A 250VAC)**, alimentare 5V / 230V, optocuploare și conectori de expansiune GPIO.
   - **Alocare Relee (Cele 3 Zone Termice conform schemei hidraulice):**
     - **Releul 1:** Zona Living + Bucătărie + Dining + Vestibul (comandă robineții 1, 2, 3 în paralel).
     - **Releul 2:** Zona Camera Copilului / Dormitor mijloc (comandă robineții 6, 7 în paralel).
     - **Releul 3:** Zona Dormitor Matrimonial (comandă robineții 4, 5 în paralel).
     - **Releul 4:** Rezervă liberă (auxiliar / eventuală separare viitoare bucătărie/living).
   - **Bypass hidraulic & Protecție pompă:** Cele 2 băi au calorifere portprosop pe perete (fără circuite în pardoseală), cu capete termostatice mecanice deschise permanent. Asigură circulație continuă și răcire chiar dacă toate cele 3 zone de pardoseală sunt închise.

2. **Comandă Radio Centrală Termică (CONFIRMAT & IDENTIFICAT):**
   - Protocol OOK/ASK 433.92 MHz reversat complet din capturile de analizor logic (`control_RF_Centrala.ino` cu 410 tranziții, preambul 65 ms, tabele `ondurations` și `offdurations`).
   - Hardware emisie/recepție: 2 seturi **SYN115 (TX) + SYN480R (RX)** comandate (cip PLL cu quartz + receptor superheterodin -107 dBm).
   - Emițătorul SYN115 se conectează direct pe un pin GPIO al ESP32 (fără releu mecanic).

3. **Temperatură Exterioară & Compensare Climatică (HIBRID):**
   - **Canal Primar Radio:** Stație meteo **Tronic 4-LD7396-2** (IAN `515492_2510`), 433.92 MHz recepționată via receptorul SYN480R.
   - **Canal Secundar Fallback:** **Open-Meteo REST API** prin Wi-Fi (testat și validat: 13.0°C Google/Open-Meteo vs 13.3°C senzorul fizic de Nord).

4. **Interfață Utilizator (Dashboard Web integrat):**
   - Găzduit 100% în flash-ul ESP32 prin LittleFS (zero cloud, fără dependențe externe).
   - Schemă vectorială SVG derivată direct din dimensiunile și pereții din `input/Odaia.html`.

---

## ✅ Todo & Status Sarcini

- [x] **1. Specificație placă relee & alocare pini GPIO** (Finalizat 06.10.2026).
      *Rezultat:* Confirmată placa ESP32 cu 4 relee. 3 relee alocate zonelor (Living+Bucătărie, Copil, Matrimonial), 1 rezervă.

- [x] **2. Identificare & recuperare protocol radio centrală** (Finalizat 06.10.2026).
      *Rezultat:* Recuperat codul funcțional `control_RF_Centrala.ino`, analiza Excel și capturile Sigrok/Saleae. Copiate în repo. Comandate modulele radio SYN115/SYN480R.

- [ ] **3. Implementare driver software `BoilerRadioTrigger` (RMT / Timer pe ESP32)** (Planificat).
      *Plan:* Clasa C++ care reia cele 410 tranziții și le transmite hardware non-blocant pe pinul DATA al SYN115.

- [ ] **4. Scanner nativ BLE pe ESP32 (decodare BTHome v2)** (Planificat).
      *Plan:* Utilizarea `NimBLE-Arduino` pentru scanare pasivă a senzorilor Xiaomi LYWSD03MMC din cele 3 zone.

- [ ] **5. Feed hibrid & fallback MQTT Samsung Galaxy A6** (Planificat).
      *Plan:* Preluare temperaturi prin MQTT de la A6 când semnalul BLE este atenuat de pereți.

- [ ] **6. Controler termic multi-zonă & logica de protecție (Failsafe)** (Planificat).
      *Plan:* `ThermalZoneManager` cu histerezis, interlock delay 120-180s pentru deschiderea actuatorilor NC, pump overrun și anti-short-cycling.

- [ ] **7. Generare schemă interactivă SVG din `Odaia.html` & Dashboard LittleFS** (Planificat).
      *Plan:* Extragerea poligoanelor celor 3 zone din `Odaia.html` și asamblarea paginii web responsive.

- [ ] **8. Decodor stație meteo Tronic 433MHz & client Open-Meteo API** (Planificat).
      *Plan:* Integrarea recepției radio via SYN480R pe GPIO interrupt și fallback HTTP Open-Meteo.

---

## 🔒 Puncte deschise (Toate soluționate pe 06.10.2026!)

- **[x] OP-1: Tipul plăcii de relee.** Soluționat: Placă integrată ESP32 cu 4 relee (comandată pe eMAG).
- **[x] OP-2: Detalii comandă modul radio centrală.** Soluționat: Tren impulsuri 410 pași pe pin GPIO către emițător SYN115 433MHz.
- **[x] OP-3: Compartimentarea apartamentului & circuitele.** Soluționat: 3 zone termice (7 circuite pardoseală), băile pe calorifere libere (bypass hidraulic).
- **[x] OP-4: Sursa temperaturii exterioare.** Soluționat: Receptor SYN480R pentru Tronic 4-LD7396-2 + fallback Wi-Fi Open-Meteo.
