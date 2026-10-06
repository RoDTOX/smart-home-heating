# Plan: Sistem Inteligent de Încălzire (Smart Heating) - Octombrie 2026

Plan de proiect dinamic și registru de sarcini inginerești pentru controlerul DIY multi-zonă de încălzire.
Se actualizează iterativ în urma discuțiilor, analizelor tehnice, testelor pe banc și feedback-ului din teren.

**Status 06.10.2026:** Preanaliză inițială finalizată. Arhitectură agreată (controller autonom ESP32 + Web UI în LittleFS + punte MQTT/Grafana pe Samsung A6 + senzori Xiaomi BLE BTHome v2). Punctele 1–8 sunt schițate la `### Todo`; așteptăm detaliile hardware la `### Puncte deschise`.

Căile de fișiere sunt relative la rădăcina proiectului (`smart-heating/`).
Ce s-a implementat și verificat se consemnează detaliat în [`docs/changelog_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/changelog_oct2026.md).

---

## Arhitectură, Constatări (Findings) & Configurare Inițială (06.10.2026)

### Preanaliză / Prezentare Generală a Sistemului

1. **Autonomie completă la nivel de ESP32 (Edge Controller):**
   - Microcontroller: **ESP32** (Dual-Core Xtensa, Wi-Fi 2.4 GHz + Bluetooth 4.2 BLE).
   - Rol principal: Rulează independent state machine-ul termic, bucla de histerezis, protecțiile hidraulice și serverul Web din memoria flash.
   - Reziliență: Dacă pică rețeaua Wi-Fi, routerul sau telefonul Galaxy A6, încălzirea continuă să funcționeze 100% autonom și în deplină siguranță.

2. **Ingestie date senzori (Sistem Hibrid BLE + MQTT):**
   - **Canal Primar (Direct pe ESP32):** Scanner BLE nativ pe ESP32 care ascultă pasiv pachetele de tip broadcast `BTHome v2` necriptate emise de senzorii Xiaomi LYWSD03MMC. Fără pairing, consum minim, baterie neschimbată 2 ani.
   - **Canal Secundar (Fallback distanță mare):** Telefonul Galaxy A6 (`smart-home-ble-gateway`) trimite prin MQTT local (`localhost:1883` / broker Wi-Fi) datele camerelor unde semnalul direct BLE până la distribuitor este atenuat de pereții de beton.

3. **Hidraulică și Acționare:**
   - Distribuitor / Calorifere: 5–6 zone acționate prin relee la 230V cu actuatoare electrotermice NC (Normal Închis).
   - Întârziere termică (Interlock Delay): Temporizare de siguranță de 2–3 minute între alimentarea actuatorului și comanda de pornire a flăcării centralei, permițând actuatorului cu parafină să se deschidă fizic.
   - Comandă flacără centrală: Semnalul radio reverse-engineered (sniffat) este comandat prin releu dedicat sau pin GPIO.
   - Disipare căldură reziduală (Pump Overrun): Cel puțin un circuit rămâne deschis timp de minim 3 minute după oprirea flăcării pentru protecția schimbătorului primar al centralei.

4. **Interfață Utilizator (Dashboard Web integrat):**
   - Găzduit 100% în flash-ul ESP32 prin LittleFS (zero cloud, fără dependențe externe de internet sau CDN).
   - Schemă vectorială interactivă (SVG) a apartamentului cu temperaturi reale, umiditate, starea fiecărui actuator și setpoint per cameră.
   - Design responsive (PWA - se instalează direct pe ecranul telefonului Android/iOS).
   - Grafice locale pe ultimele 24 de ore (buffer circular în RAM/Flash); istoric extins sincronizat în PostgreSQL/Grafana pe telefonul A6.

---

### Todo

- [ ] **1. Specificație placă relee & alocare pini GPIO** (06.10.2026).
      *Constatare:* Avem 5–6 zone cu actuatoare electrotermice 230V NC + 1 comandă centrală termică.
      *Preanaliză:* Este necesară o placă cu 8 relee comandată de ESP32 (ex. placă integrată ESP32-WROOM cu 8 relee și optocuploare sau modul de relee cuplat la un ESP32 DevKit).
      *Plan:* Stabilirea alocării releelor (Releele 1–6: Actuatori Zone; Releul 7: Comandă Radio Centrală; Releul 8: Auxiliar/Rezervă), sursa de alimentare (230V AC -> 5V DC) și maparea pinilor GPIO.

- [ ] **2. Driver comandă modul radio centrală (semnal sniffat)** (06.10.2026).
      *Constatare:* Semnalul digital I/O al receptorului radio al centralei a fost capturat cu sniffer logic.
      *Preanaliză:* Stabilirea naturii electrice a comenzii: simulare contact uscat (releu NO/COM), impuls logic 3.3V/5V dintr-un pin GPIO sau tren de impulsuri serial.
      *Plan:* Crearea modulului software `BoilerRadioTrigger` cu suport pentru impuls/menținere, confirmare stare și timeout de siguranță failsafe.

- [ ] **3. Scanner nativ BLE pe ESP32 (decodare BTHome v2)** (06.10.2026).
      *Constatare:* Senzori Xiaomi LYWSD03MMC rulați cu firmware custom (PVVX / BTHome v2).
      *Preanaliză:* Utilizarea bibliotecii `NimBLE-Arduino` pentru scanare pasivă non-blocantă a pachetelor de advertising BLE.
      *Plan:* Implementarea serviciului `BleSensorScanner` care decodează temperatura (rezoluție 0.01°C), umiditatea și nivelul bateriei direct din broadcast-urile BTHome v2.

- [ ] **4. Feed hibrid & fallback MQTT Samsung Galaxy A6** (06.10.2026).
      *Constatare:* Telefonul Galaxy A6 are deja broker Mosquitto și baza de date activă în `smart-home-ble-gateway`.
      *Preanaliză:* În apartamentele din beton armat, unele camere pot fi la limita razei BLE a ESP32-ului montat în cutia distribuitorului.
      *Plan:* Implementarea logicii hibride: fiecare zonă își actualizează temperatura din BLE dacă a primit pachet în ultimele 90s; altfel preia valoarea din topicul MQTT publicat de A6. Dacă ambele lipsesc > 30 min, zona intră pe avarie.

- [ ] **5. Controler termic multi-zonă & logica de protecție (Failsafe)** (06.10.2026).
      *Preanaliză:* Prevenirea cavitației pompei, a comutării dese a flăcării (short-cycling) și a supraîncălzirii.
      *Plan:* Implementarea modulului `ThermalZoneManager`:
      - Setpoint per cameră, histerezis configurabil (implicit ±0.3°C).
      - Delay de deschidere actuator (120–180s) înainte de aprinderea flăcării.
      - Timp minim de funcționare (3 min) și pauză minimă între porniri (5 min) anti-short-cycling.
      - Disipare căldură reziduală: menținerea a cel puțin unui circuit deschis timp de 180s după oprirea flăcării.

- [ ] **6. Interfață Web integrată & schemă interactivă apartament (SVG)** (06.10.2026).
      *Constatare:* Aplicație vizuală care arată temperatura fiecărei camere, schema apartamentului, starea fiecărui circuit din pardoseală/calorifer, programe orare și control la distanță.
      *Preanaliză:* Tehnologie HTML5 / CSS / Vanilla JS compactată în memoria LittleFS a ESP32-ului.
      *Plan:* Construirea unui dashboard modern:
      - Schemă vectorială (SVG) a apartamentului cu zone interactive.
      - Indicatori live de temperatură, umiditate, slider setpoint și iluminare dinamică a circuitelor active.
      - Selector de moduri: `Confort`, `Eco`, `Noapte`, `Protecție Îngheț / Plecat`, `Manual`.
      - API REST / WebSocket (`/api/status`, `/api/zones`, `/api/config`) pentru sincronizare instantanee.

- [ ] **7. Integrare temperatură exterioară (compensare climatică)** (06.10.2026).
      *Constatare:* Disponibil senzor radio exterior de la stația meteo Tronic (Lidl) 433MHz sau adăugarea unui senzor Xiaomi de exterior.
      *Preanaliză:* Temperatura exterioară ($T_{ext}$) permite ajustarea curbei de încălzire sau a avansului pornirii când este ger.
      *Plan:* Arhitectură modulară pentru intrarea exterioară: Opțiunea A = Senzor Xiaomi pus la exterior (BLE/MQTT); Opțiunea B = Receptor RF 433MHz; Opțiunea C = Fallback Open-Meteo REST API când există conexiune internet.

- [ ] **8. Export telemetrie & dashboard Grafana pe Samsung A6** (06.10.2026).
      *Constatare:* Sinergie completă între funcționarea locală a ESP32 și istoricul de lungă durată din telefonul Galaxy A6.
      *Plan:* ESP32 publică periodic starea pe MQTT (`smart-heating/zones`, `smart-heating/boiler`, `smart-heating/actuators`) pentru salvare automată în PostgreSQL și afișare în Grafana.

---

### Puncte deschise (Open points)

- **[ ] OP-1: Tipul plăcii de relee cu ESP32.** Ce placă avem disponibilă pe banc (ex: placă dedicată cu 8 relee cu ESP32-WROOM integrat sau modul extern cu 8 relee cuplat la un ESP32 DevKit)?
- **[ ] OP-2: Detalii comandă modul radio centrală.** Cum este transmis semnalul sniffat? (Contact de releu care simulează butonul telecomenzii originale, sau impuls digital/tren de date serial trimis pe un fir logic de 3.3V/5V direct în emițător)?
- **[ ] OP-3: Compartimentarea apartamentului & denumirea zonelor.** Care sunt cele 5–6 zone (ex. Living, Dormitor Matrimonial, Birou/Copil, Bucătărie, Baie, Hol)? Avem nevoie de denumiri și poziționare aproximativă pentru schema SVG.
- **[ ] OP-4: Sursa temperaturii exterioare.** Începem cu un senzor Xiaomi pus la geam/balcon (cel mai rapid, decodare instantanee BTHome v2) sau explorăm interfațarea receptorului 433MHz pentru Tronic?
