# Multi-Tenant IoT SaaS Platform Architecture (mysmarthome.cc)

Acest document reprezintă viziunea arhitecturală oficială și consolidată a platformei **MySmartHome.cc** (actualizat 24.10.2026) pentru tranziția de la un home server individual la o **platformă comercială scalabilă de servicii rezidențiale inteligente la cheie**.

---

## 🏛️ 1. Portofoliul de Produse & Servicii ale Ecosistemului

Brandul **MySmartHome.cc** reunește o suită modulară de automatizări hardware premium pentru locuință, funcționând 100% autonom la nivel local și agregată într-un portal cloud modern:

| Modul Ecosistem | Hardware Fizic | Rol Operațional Local | Conectare Cloud / SaaS |
| :--- | :--- | :--- | :--- |
| 🔥 **Smart Heating** | ESP32-Relay X4, actuatoare NC, senzori BLE, emițător radio 433MHz cazan | Buclă histerezis, protecție hidraulică, detecție geam deschis, economie 15–25% gaz | Telemetrie MQTT, reglaj temperaturi de la distanță, alerte pe telefon |
| 🐠 **Smart Aquarium** | Placă relee ESP8266/ESP32, senzori temperatură apă, nivel lichid | Control iluminare orară, pompe filtrare, încălzitor/răcire automată | Monitorizare apă în timp real, alerte de avarie pompă/temperatură critică |
| 🚗 **Smart Vehicle** | TeslaMate Docker container pe server | Monitorizare autonomie, stare baterie, cicluri de încărcare și costuri | Tab dedicat în portalul unificat pentru proprietar |

---

## 🌐 2. Topologie Domenii & Rutare Cloudflare

```
                            [ INTERNET / CLOUDFLARE ]
                                        │
         ┌──────────────────────────────┼──────────────────────────────┐
         ▼                              ▼                              ▼
  mysmarthome.cc                app.mysmarthome.cc              mqtt.mysmarthome.cc
(Vitrină Comercială)         (Portalul Unificat PWA)           (Gateway Flotă Hardware)
  • Prezentare produse           • Single App Shell              • Mosquitto MQTT peste TLS
  • Calculator economie gaz      • Login Google / Apple / Email  • Ingestie telemetrie outbound
  • Formular lead-uri montaj     • Tab-uri: Încălzire / Acvariu  • Comenzi în timp real
  • Simulator interactiv live    • Mod Client vs Mod Admin       • Zero porturi pe router client
  • 100% Public (Fără login)     • Kiosk Tableta + Mobil PWA     • Conexiuni securizate token
```

---

## 👥 3. Roluri Utilizatori & Separarea Client vs SuperAdmin

Platforma oferă două perspective distincte de operare:

1. **Client Rezidențial (Standard):**
   * Se autentifică prin Google, Apple sau Email Magic Link.
   * Vede exclusiv locuința sa și doar modulele hardware achiziționate (ex: dacă a cumpărat doar încălzirea, vede doar tab-ul de încălzire).
   * Interfață ergonomică fără elemente tehnice de server.
2. **Bogdan (SuperAdmin & Proprietar Sistem):**
   * **Mod Personal (Casa Mea):** Experiență completă de client cu toate gadgeturile active (Încălzire 3 zone, Acvariu cu relee, TeslaMate).
   * **Mod Fleet Ops (Management Afacere):** 
     - Lista clienților activi și a locuințelor instalate.
     - Monitorizarea stării hardware a plăcilor din teren (semnal Wi-Fi RSSI, DTC coduri de eroare, versiune firmware).
     - **Tesla-Style Remote Service:** Posibilitatea de a deschide la distanță consola de diagnoză a oricărui client pentru asistență telefonică rapidă.
     - Lista lead-urilor noi sosite din formularul comercial de pe vitrină.

---

## 🛠️ 4. Fluxul de Onboarding, Instalare & Asistență la Cheie

1. **Montaj Fizic (Sub 90 minute):**
   * Tehnicianul instalează placa de relee în cutia distribuitorului și senzorii pe pereți.
2. **Asociere & Înregistrare:**
   * Clientul intră pe `mysmarthome.cc` $\rightarrow$ `app.mysmarthome.cc`, se loghează cu Google.
   * Introduce codul de asociere de pe cutia echipamentului (ex: `SH-2026`).
3. **Vrăjitorul de Configurare (Setup Wizard):**
   * Mapează fiecare releu pe camera dorită (Releul 1 = Living, Releul 2 = Copil, etc.).
   * Buton de **Testare Fizică Relee** cu puls de 2 secunde direct în interfață pentru confirmare vizuală/auditivă pe viu.
4. **Liniște & Asistență la Distanță (Garanție 30 de Zile):**
   * Clientul are 30 de zile garanție cu banii înapoi dacă nu este complet mulțumit de confortul termic.
   * Asistență gratuită de la distanță: update-uri OTA, citire erori DTC și calibrare histerezis prin telefon, fără vizite deranjante în casă.

---

## 📱 5. Ergonomie Multi-Device: Tableta de Perete vs Mobil Remote

* **Tableta din Perete (Samsung Galaxy Tab A9 Kiosk Landscape):**
  * Afișaj panoramic dintr-o singură privire: toate zonele de încălzire, starea acvariului, temperatura exterioară și ceasul, fără scroll.
  * Motion Wake: ecranul se aprinde discret la apropierea utilizatorului.
* **Telefonul Mobil (PWA Remote Portrait):**
  * Aceeași identitate vizuală și aceleași componente responsive.
  * Secțiune superioară dedicată acțiunilor rapide de la distanță:
    * Buton mare: **[🏠 Acasă]** vs **[✈️ Mod Plecat / Eco (18°C)]**.
    * Status centrală și status pompe/filtre acvariu.
    * Scroll fluid pentru reglajul fiecărei camere în parte.

---

## 🔔 6. Politica Inteligentă de Alerte (Zero Spam)

Notificări trimise pe telefon strict la evenimente de valoare:
1. 🪟 **Aerisire inteligentă:** *„Geam deschis în Dormitor. Căldura a fost pusă pe pauză automat pentru economie de gaz.”*
2. 🔋 **Baterie senzor scăzută:** *„Bateria senzorului din Camera Copilului este sub 15%. Vă recomandăm înlocuirea CR2032.”*
3. 🐠 **Siguranță Acvariu:** *„Atenție: Temperatura apei a depășit 28°C. Sistemul a decuplat încălzitorul.”*
4. 🛠️ **Tesla Service Diagnoză:** *„Detectat cod eroare DTC-102 (Senzor offline). Sistemul a trecut pe histerezis de siguranță.”*

---

* **`mysmarthome.cc` (Public Showcase):** Site-ul de prezentare a afacerii și produselor. Vizitatorii externi, potențialii clienți sau partenerii văd catalogul de produse fără nicio barieră de autentificare.
* **`app.mysmarthome.cc` (Client Web App & Gateway):** Aplicația web unificată (PWA / Web / viitoare aplicație mobilă). Utilizatorii se autentifică cu email și parolă. Backend-ul filtrează datele strict pe baza `tenant_id`:
  - Clientul 0 vede panoul de control global al flotei și serviciile sale personale.
  - Clientul N vede doar cele 3-4 zone termice ale locuinței sale.
* **`mqtt.mysmarthome.cc` / `api.mysmarthome.cc` (Device Ingestion & Fleet Management):** Punctul de intrare pentru plăcile ESP32 din teren. Conexiunile sunt inițiate exclusiv **outbound** din locuințele clienților către server, funcționând prin orice router, NAT sau conexiune mobilă 4G/5G fără configurare pe routerul clientului.

---

## ⚡ 3. Cerințe Esențiale pentru Firmware-ul ESP32 (`SmartHeating C++`)

1. **Model Dual de Funcționare (Dual-Mode):**
   - **Mod Autonom (Offline First):** ESP32 controlează releele, cazanul, bucla de histerezis, senzorii BLE și geamul deschis chiar dacă pică internetul sau serverul central. Siguranța fizică rămâne 100% la margine (*edge*).
   - **Mod Conectat (Cloud Fleet):** Placa se conectează prin client MQTT securizat (TLS/WSS) către `mqtt.mysmarthome.cc` folosind `device_id` și `device_secret` persistate în NVS (`DevicePassport`).
2. **Telemetrie Structurată (Subiecte MQTT):**
   - Publicare la fiecare 30–60s sau la schimbare de stare: `devices/{device_id}/telemetry` (temperaturi, setpoint, relee, flacără, cădere geam, DTC-uri).
   - Abonare la comenzi: `devices/{device_id}/commands` (modificare setpoint, reluare după geam, mod absență/eco).
3. **Firmware Update OTA de la Distanță (Fleet Management):**
   - Placa se abonează la `devices/{device_id}/ota`. Când serverul publică un release nou de firmware, ESP32 descarcă binarul prin HTTPS, verifică hash-ul SHA-256 și aplică actualizarea autonom.

---

## 🎨 4. Cerințe Esențiale pentru Frontend / UI/UX

1. **Decuplare de IP-ul Local:**
   - Interfața web **nu trebuie să aibă hardcodat IP-ul `192.168.1.20`**.
   - În modul local (Kiosk Tab A9 în casa ta), folosește `/api/...` relativ sau IP-ul local.
   - În modul cloud (`app.mysmarthome.cc`), interfața interoghează API-ul serverului central prin token JWT, iar serverul transmite starea primită prin MQTT de la placa asociată clientului respectiv.
2. **Identitate Vizuală Scalabilă:**
   - Componentele de UI (cardurile camerelor, tumblerul de gaz, schema hidraulică SVG) trebuie să fie modulare, permițând afișarea unui număr arbitrar de zone (de la 1 la 8 camere în funcție de locuința clientului).

---

## 🏗️ 5. Arhitectură Multi-Distribuitor Master-Slave (ESP-NOW)

În case mari sau vile cu etaj (unde există distribuitoare hidraulice separate pe parter și etaj), se aplică următoarele principii:

1. **Un controller ESP32 per distribuitor fizic:**
   - Evită cablarea de 230V între etaje; actuatoarele se leagă direct în cutia distribuitorului local.
   - Fiecare controller își citește senzorii de temperatură de pe etajul său (BLE sau Zigbee via satelit).
2. **Roluri Master / Slave:**
   - **Controller Master:** Cel montat la distribuitorul cel mai apropiat de centrala termică (sau cel echipat cu emițătorul radio 433 MHz SYN115 / releul de flacără).
   - **Controllere Slave:** Controllerele de pe celelalte etaje.
3. **Comunicație directă via ESP-NOW:**
   - Slave-urile transmit cererea de căldură (heat_demand: true/false) direct către Master prin protocolul **ESP-NOW** (wireless peer-to-peer 2.4 GHz).
   - **Rezistență extremă la defecțiuni:** Comunicarea funcționează chiar dacă routerul Wi-Fi al casei este scos din priză, dacă parola de Wi-Fi este schimbată de client sau dacă rețeaua locală este căzută.
   - Masterul centralizează cererile, aplică delay-ul de siguranță hidraulică (180s pentru deschiderea vanelor) și aprinde cazanul. Când toate vanele de pe toate etajele s-au închis, Masterul stinge cazanul.

---

## 👥 6. Model de Date Multi-Proprietate (Multi-Tenant Hierarchy)

`
[ User Account ] (Login via Google / Apple / Email PIN)
       │
       ├── Property 1: "Apartament București"
       │         │
       │         └── Controller (Master): ESP32 Living Manifold
       │                   ├── Zone 1: Living & Bucătărie (Relay 1, BLE Sensor)
       │                   ├── Zone 2: Cameră Copil (Relay 2, BLE Sensor)
       │                   └── Zone 3: Matrimonial (Relay 3, BLE Sensor)
       │
       └── Property 2: "Casă Sinaia"
                 ├── Controller (Master): ESP32 Distribuitor Parter (433MHz Cazan)
                 │         ├── Zone 1: Living Parter
                 │         └── Zone 2: Bucătărie & Dining
                 │
                 └── Controller (Slave): ESP32 Distribuitor Etaj (ESP-NOW către Master)
                           ├── Zone 3: Dormitor 1
                           └── Zone 4: Dormitor 2
`

---

## 💼 7. Strategie Comercială & Poziționare în Piață (Dual Channel)

| Criteriu | Tado / Salus | Nabu Casa / Home Assistant | **SmartHome Heating (mysmarthome.cc)** |
|---|---|---|---|
| **Model Cost** | 4,000-8,000 Lei kit + Abonament lunar (€3.99/lună) | Hardware separat + Abonament (\/an) | **Cost unic pachet hardware + montaj, ZERO abonament lunar** |
| **Instalare** | DIY dificil pe distribuitor | Exclusiv DIY / Tehnic / YAML | **Pachet la cheie cu montaj fizic direct pe distribuitor** |
| **Control Inerție Pardoseală** | On/off generic sau algoritmi TRV calorifer | Necesită scrierea de automatizări manuale | **Nativ: protecție condensare, interblocare 180s, detecție geam dT/dt** |
| **Canale de Vânzare** | Retail online | Entuziaști DIY | **B2C (retrofit locuințe existente) + B2B (parteneriate instalatori/dezvoltatori)** |

---

## ⚙️ 8. Backend Multi-Tenant pe Server: PocketBase

Pentru microserviciul de backend pe pp.mysmarthome.cc:
* **Tehnologie:** **PocketBase** (scris în Go cu SQLite embedded în mod WAL).
* **Consum resurse:** ~20 MB binar, **doar ~15 MB RAM** pe serverul ThinkPad L590.
* **Funcționalități native:**
  - Autentificare OAuth2 (Google, Apple, email/parolă).
  - WebSockets / Server-Sent Events în timp real (pentru update instant de temperaturi pe telefon).
  - Panou administrativ web inclus pentru gestiunea utilizatorilor, caselor și dispozitivelor.
  - Sincronizare bidirecțională cu brokerul MQTT mosquitto local.