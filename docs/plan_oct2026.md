# Plan de Lucru Activ (Octombrie 2026) â€“ Smart Home Heating

Acest document este planul de referinÈ›Äƒ pentru implementarea controlerului autonom multi-zonÄƒ ESP32, integrat cu senzorii Xiaomi BLE (BTHome v2), emiÈ›Äƒtorul RF pentru centralÄƒ È™i interfaÈ›a LittleFS cu schemÄƒ interactivÄƒ SVG.

---

## ðŸ§  SintezÄƒ ArhitecturÄƒ & Decizii Tehnice

1. **Hardware & Control Relee (CONFIRMAT & COMANDAT):**
   - **PlacÄƒ:** Modul dedicat **ESP32 cu 4 Relee (Songle 10A 250VAC)**, alimentare 5V / 230V, optocuploare È™i conectori de expansiune GPIO.
   - **Alocare Relee (Cele 3 Zone Termice conform schemei hidraulice):**
     - **Releul 1:** Zona Living + BucÄƒtÄƒrie + Dining + Vestibul (comandÄƒ robineÈ›ii 1, 2, 3 Ã®n paralel).
     - **Releul 2:** Zona Camera Copilului / Dormitor mijloc (comandÄƒ robineÈ›ii 6, 7 Ã®n paralel).
     - **Releul 3:** Zona Dormitor Matrimonial (comandÄƒ robineÈ›ii 4, 5 Ã®n paralel).
     - **Releul 4:** RezervÄƒ liberÄƒ (auxiliar / eventualÄƒ separare viitoare bucÄƒtÄƒrie/living).
   - **Bypass hidraulic & ProtecÈ›ie pompÄƒ:** Cele 2 bÄƒi au calorifere portprosop pe perete (fÄƒrÄƒ circuite Ã®n pardosealÄƒ), cu capete termostatice mecanice deschise permanent. AsigurÄƒ circulaÈ›ie continuÄƒ È™i rÄƒcire chiar dacÄƒ toate cele 3 zone de pardosealÄƒ sunt Ã®nchise.

2. **ComandÄƒ Radio CentralÄƒ TermicÄƒ (CONFIRMAT & IDENTIFICAT):**
   - Protocol OOK/ASK 433.92 MHz reversat complet din capturile de analizor logic (`control_RF_Centrala.ino` cu 410 tranziÈ›ii, preambul 65 ms, tabele `ondurations` È™i `offdurations`).
   - Hardware emisie/recepÈ›ie: 2 seturi **SYN115 (TX) + SYN480R (RX)** comandate (cip PLL cu quartz + receptor superheterodin -107 dBm).
   - EmiÈ›Äƒtorul SYN115 se conecteazÄƒ direct pe un pin GPIO al ESP32 (fÄƒrÄƒ releu mecanic).

3. **TemperaturÄƒ ExterioarÄƒ & Compensare ClimaticÄƒ (HIBRID):**
   - **Canal Primar Radio:** StaÈ›ie meteo **Tronic 4-LD7396-2** (IAN `515492_2510`), 433.92 MHz recepÈ›ionatÄƒ via receptorul SYN480R.
   - **Canal Secundar Fallback:** **Open-Meteo REST API** prin Wi-Fi (testat È™i validat: 13.0Â°C Google/Open-Meteo vs 13.3Â°C senzorul fizic de Nord).

4. **InterfaÈ›Äƒ Utilizator (Dashboard Web integrat):**
   - GÄƒzduit 100% Ã®n flash-ul ESP32 prin LittleFS (zero cloud, fÄƒrÄƒ dependenÈ›e externe).
   - SchemÄƒ vectorialÄƒ SVG derivatÄƒ direct din dimensiunile È™i pereÈ›ii din `input/Odaia.html`.

---

## âœ… Todo & Status Sarcini

- [x] **1. SpecificaÈ›ie placÄƒ relee & alocare pini GPIO** (Finalizat 06.10.2026).
      *Rezultat:* ConfirmatÄƒ placa ESP32 cu 4 relee. 3 relee alocate zonelor (Living+BucÄƒtÄƒrie, Copil, Matrimonial), 1 rezervÄƒ.

- [x] **2. Identificare & recuperare protocol radio centralÄƒ** (Finalizat 06.10.2026).
      *Rezultat:* Recuperat codul funcÈ›ional `control_RF_Centrala.ino`, analiza Excel È™i capturile Sigrok/Saleae. Copiate Ã®n repo. Comandate modulele radio SYN115/SYN480R.

- [ ] **3. Implementare driver software `BoilerRadioTrigger` (RMT / Timer pe ESP32)** (Planificat).
      *Plan:* Clasa C++ care reia cele 410 tranziÈ›ii È™i le transmite hardware non-blocant pe pinul DATA al SYN115.

- [ ] **4. Scanner nativ BLE pe ESP32 (decodare BTHome v2)** (Planificat).
      *Plan:* Utilizarea `NimBLE-Arduino` pentru scanare pasivÄƒ a senzorilor Xiaomi LYWSD03MMC din cele 3 zone.

- [ ] **5. Feed hibrid & fallback MQTT Samsung Galaxy A6** (Planificat).
      *Plan:* Preluare temperaturi prin MQTT de la A6 cÃ¢nd semnalul BLE este atenuat de pereÈ›i.

- [ ] **6. Controler termic multi-zonÄƒ & logica de protecÈ›ie (Failsafe)** (Planificat).
      *Plan:* `ThermalZoneManager` cu histerezis, interlock delay 120-180s pentru deschiderea actuatorilor NC, pump overrun È™i anti-short-cycling.

- [ ] **7. Generare schemÄƒ interactivÄƒ SVG din `Odaia.html` & Dashboard LittleFS** (Planificat).
      *Plan:* Extragerea poligoanelor celor 3 zone din `Odaia.html` È™i asamblarea paginii web responsive.

- [ ] **8. Decodor staÈ›ie meteo Tronic 433MHz & client Open-Meteo API** (Planificat).
      *Plan:* Integrarea recepÈ›iei radio via SYN480R pe GPIO interrupt È™i fallback HTTP Open-Meteo.

---

## ðŸ”’ Puncte deschise (Toate soluÈ›ionate pe 06.10.2026!)

- **[x] OP-1: Tipul plÄƒcii de relee.** SoluÈ›ionat: PlacÄƒ integratÄƒ ESP32 cu 4 relee (comandatÄƒ pe eMAG).
- **[x] OP-2: Detalii comandÄƒ modul radio centralÄƒ.** SoluÈ›ionat: Tren impulsuri 410 paÈ™i pe pin GPIO cÄƒtre emiÈ›Äƒtor SYN115 433MHz.
- **[x] OP-3: Compartimentarea apartamentului & circuitele.** SoluÈ›ionat: 3 zone termice (7 circuite pardosealÄƒ), bÄƒile pe calorifere libere (bypass hidraulic).
- **[x] OP-4: Sursa temperaturii exterioare.** SoluÈ›ionat: Receptor SYN480R pentru Tronic 4-LD7396-2 + fallback Wi-Fi Open-Meteo.
