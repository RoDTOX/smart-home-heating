# Arhitectură Cloud Fleet Management, Acces Remote Client & Tesla-Style Service Mode

**Proiect:** Sistem Inteligent Autonom de Încălzire Multi-Zonă (Smart Home Heating)  
**Versiune:** 1.0 (Arhitectură Tehnică & Specificație Serviciu la Distanță)  
**Data elaborării:** 08 Octombrie 2026  
**Status:** Specificație Activă de Arhitectură & Implementare Modul Diagnoză  

---

## 1. Viziunea Sistemului: „Tesla Service Mode” pentru Încălzire Rezidențială

La fel cum vehiculele **Tesla** mențin o telemetrie bidirecțională securizată cu cloud-ul (raportare stare în timp real, diagnostic la distanță înainte ca proprietarul să știe că există o defecțiune, actualizări software OTA și acces în „Service Mode” pentru tehnician), sistemul nostru de încălzire rezolvă cele două mari provocări ale clienților:

1. **Accesul Clientului din Afara Casei (Fără setări de router):**
   - Clientul are Wi-Fi în locuință de la Digi/Orange/Vodafone (în spatele CGNAT, fără IP public static, fără deschidere de porturi).
   - Sistemul trebuie să permită reglarea temperaturii și verificarea casei de pe telefon din mașină, de la birou sau din vacanță, instantaneu și în siguranță.
2. **Asistență Tehnică la Distanță & Mentenanță Proactivă (Tesla Service Mode):**
   - Când un client sună: *„Parcă dormitorul copilului e mai rece azi”*, nu mai este nevoie de deplasare fizică de 2 ore în trafic pentru a constata o baterie descărcată sau un actuator blocat mecanic.
   - Inginerul deschide **Consola de Service Fleet**, se conectează securizat pe nodul clientului, declanșează suita de autodiagnosticare (`SELFTEST`), verifică timpii de deschidere ai robineților, măsoară RSSI-ul senzorilor și poate forța comutarea releelor sau trimiterea de pachete RF pe loc, explicându-i clientului la telefon exact ce se întâmplă.
   - Dacă a apărut o versiune nouă de firmware (optimizări histerezis, suport senzori noi), se transmite comanda **Remote OTA Push** direct de acasă!

---

## 2. Arhitectura de Conectivitate: Tunel Outbound Securizat (Zero Router Config)

### Problema Tehnică:
Routerele rezidențiale blochează orice conexiune inițiată din exterior (*Inbound Firewall / CGNAT*). Un client obișnuit nu știe și nu are voie să configureze Port Forwarding, DMZ sau DDNS.

### Soluția Inginerească: Tunel Inversat Inițiat de ESP32 (*Outbound Reverse Tunnel*)

```text
  ┌─────────────────────────────────────────────────────────────┐
  │                 LOCUINȚA CLIENTULUI                         │
  │                                                             │
  │  [ ESP32 Controler ] ──── (Conexiune Outbound TLS/WSS) ─┐   │
  │  (192.168.1.x intern)                                    │   │
  │                                                         │   │
  │  [ Router Client ] (NAT / CGNAT fără port deschis)       │   │
  └─────────────────────────────────────────────────────────┼───┘
                                                            │
                                  Interfață Securizată      │ (Trece liber prin
                                  Outbound Port 443/8883    │  orice firewall!)
                                                            ▼
                              ┌───────────────────────────────────┐
                              │     CLOUD FLEET HUB               │
                              │     (Hetzner / Oracle Cloud)      │
                              │  - Broker MQTT cu TLS (Port 8883) │
                              │  - Reverse WebSocket Gateway      │
                              │  - API Fleet Management & Auth    │
                              └─────────────────┬─────────────────┘
                                                │
                  ┌─────────────────────────────┴─────────────────────────────┐
                  ▼                                                           ▼
      ┌───────────────────────┐                                   ┌───────────────────────┐
      │   APLICAȚIE CLIENT    │                                   │  CONSOLĂ SERVICE PRO  │
      │   (Telefon Mobil 4G)  │                                   │  (Inginer de Acasă)   │
      │ - Control temperatură │                                   │ - Tesla Service Mode  │
      │ - Vizualizare camere  │                                   │ - Diagnoză DTC & Logs │
      │ - Notificări baterii  │                                   │ - Remote OTA Firmware │
      └───────────────────────┘                                   └───────────────────────┘
```

### De ce această arhitectură este infailibilă:
1. **Zero configurare pe routerul clientului:** O conexiune outbound pe portul `443` (HTTPS/WSS) sau `8883` (MQTTS) este permisă implicit de 100% din routerele și providerii din România.
2. **Criptare End-to-End:** Conexiunea este securizată cu TLS 1.3 și certificate unice per dispozitiv (`Device-ID` + cheie simetrică/token JWT).
3. **Bandwidth neglijabil:** Heartbeat MQTT de doar 120 bytes o dată la 30-60 secunde (sub 5 MB trafic lunar pe Wi-Fi-ul clientului).

---

## 3. Matricea de Diagnoză: Coduri Standardizate DTC (Diagnostic Trouble Codes)

Pentru un diagnostic rapid la telefon, sistemul implementează un registru standardizat de coduri de avarie, inspirat din industria auto:

| Cod DTC | Denumire Semnal | Cauză Rădăcină (Root Cause) | Acțiune de Service la Distanță |
|:---:|---|---|---|
| **`DTC-101`** | `ERR_SENSOR_TIMEOUT` | Senzorul BLE dintr-o cameră nu a emis telemetrie de > 20 minute. | Verificare RSSI anterior, verificare dacă a fost mutat sau dacă bateria e moartă. |
| **`DTC-102`** | `WARN_BATTERY_LOW` | Bateria CR2032 a unui termometru a scăzut sub 20% (< 2.6V). | Notificare proactivă trimisă clientului înainte ca senzorul să se oprească. |
| **`DTC-103`** | `ERR_TEMP_OUT_OF_BOUNDS` | Temperatură raportată sub 5°C sau peste 45°C (spurious reading). | Decuplare automată a senzorului din bucla de calcul; trecere pe temperatură fallback. |
| **`DTC-201`** | `WARN_HIGH_CYCLE_RATE` | Un releu de zonă a comutat de mai mult de 8 ori în ultima oră. | Mărire dinamică a histerezisului la 0.4°C pentru a proteja actuatorul de uzură. |
| **`DTC-202`** | `ERR_ACTUATOR_STUCK_SHUT` | Zona cere căldură de > 90 min, dar temperatura camerei continuă să scadă. | Alarmă robinet închis manual pe distribuitor, actuator blocat sau aer în circuit. |
| **`DTC-203`** | `ERR_ACTUATOR_STUCK_OPEN` | Zona a oprit releul de > 60 min, dar temperatura continuă să crească alert. | Actuator termic blocat pe deschis sau ac acționat mecanic blocat. |
| **`DTC-301`** | `ERR_BOILER_NO_RESPONSE` | Comandă RF de pornire trimisă, dar temperatura turului nu crește după 30 min. | Re-trimitere tren RF 433 MHz; verificare alimentare receptor centrală. |
| **`DTC-302`** | `WARN_BOILER_RAPID_CYCLING`| Cerere de aprindere flacără repetată la sub 3 minute de la oprire. | Activare forțată a interlock-ului de anti-short cycling (minim 180s pauză). |
| **`DTC-401`** | `WARN_WIFI_SIGNAL_DEGRADED`| Semnal Wi-Fi client mai slab de -82 dBm (risc deconectare). | Sugerare repoziționare antenă sau verificare dacă s-a mutat routerul de către client. |
| **`DTC-402`** | `WARN_HEAP_FRAGMENTATION` | Memorie RAM heap liberă a scăzut sub 60 KB. | Restart planificat non-disruptiv al stivei Wi-Fi în interval de idle termic. |

---

## 4. Modul „Tesla Service Mode” (Ce poate face tehnicianul de la distanță)

Când tehnicianul activează modul de service pentru un client specific din Fleet Manager:

1. **Self-Test Complet Automat (`/api/diag/selftest`):**
   * Verifică starea fiecărui releu GPIO.
   * Verifică receptorul/emițătorul RF 433.92 MHz.
   * Verifică stiva NimBLE și receptionarea pachetelor de la fiecare termometru asociat.
   * Generează un raport instant cu codurile DTC active și scorul de sănătate al nodului (ex: `System Health: 98% [GOOD]`).
2. **Acționare Forțată de Relee (Manual Actuator Test):**
   * Posibilitatea de a deschide Releul 1, 2 sau 3 pe o durată prestabilită (ex. 60 de secunde) pentru a asculta la telefon dacă actuatorul electrotermic bârâie/se încălzește sau dacă debitmetrul de pe distribuitor indică debit de apă.
3. **Test Foc Radio Centrală (Manual Boiler RF Ping):**
   * Trimite pe loc pachetul radio de pornire sau oprire a centralei și ascultă confirmarea acustică din centrală de la client.
4. **Citire Terminal & Stream de Loguri (`/api/console/logs` sau Telnet Tunneled):**
   * Vizualizarea în timp real a fiecărui pachet BLE primit, a histerezisului calculat la fiecare 10 secunde și a tranzițiilor de stare.
5. **Remote Over-The-Air Update (Fleet Firmware Push):**
   * Upload direct al noului binar compilat prin tunelul securizat.
   * ESP32 verifică hash-ul SHA-256, scrie partiția inactivă OTA și repornește în 15 secunde fără întreruperea încălzirii locuinței.

---

## 5. Model de Monetizare Side-Hustle: „Fleet Care & Remote SLA”

Această capacitate tehnică transformă proiectul dintr-o simplă „vânzare de cutie cu relee” într-un **serviciu recurent cu valoare adăugată mare**:

* **Pachetul de Bază (Inclus la montaj 12 luni):**
  * Acces pe telefon de la distanță pentru client.
  * Notificări automate când se descarcă bateriile termometrelor.
* **Abonamentul „Smart Heating VIP Care” (19 – 29 RON / lună):**
  * Monitorizare proactivă 24/7 de pe serverul nostru cloud.
  * Intervenție tehnică și diagnoză la distanță garantată în sub 30 minute.
  * Înlocuire baterii inclusă o dată pe an (cost 10 lei).
  * Actualizări periodice de firmware cu algoritmi noi de economisire gaz.
  * Clientul scapă complet de grija reglajelor manuale de toamnă/iarnă!

---

## 6. Etape de Implementare în Repozitoriu

1. **Faza 1 (Nivel Nod ESP32 - Imediat):**
   - Implementare `DiagnosticEngine` ([`include/DiagnosticEngine.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/DiagnosticEngine.h), [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp)) pe ESP32.
   - Înregistrare automată a codurilor DTC, monitorizare timpi senzori, histerezis blocat, starea bateriilor și sănătatea sistemului.
   - Expunere API `/api/diag/status` și comanda `DIAG` în consola Telnet/Web.
2. **Faza 2 (Tunel Rețea & Cloud Hub):**
   - Creare serviciu ușor de releu WebSocket / MQTT broker securizat în cloud (sau pe Galaxy-A6 ca server de testare).
   - Conectare automată a ESP32 prin client WebSocket/MQTT securizat către Cloud Hub la boot.
3. **Faza 3 (Panou Web Fleet Admin & Aplicație Client):**
   - Dashboard PWA centralizat pentru vizualizarea tuturor instalațiilor active pe o hartă/listă cu status verde/galben/roșu.
