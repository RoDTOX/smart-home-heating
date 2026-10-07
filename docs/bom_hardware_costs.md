# Evidență Componente Hardware, Bucăți & Costuri Proiect (Bill of Materials - BOM)

**Proiect:** Sistem Inteligent de Încălzire Multi-Zonă (Smart Home Heating)
**Data actualizării:** 07 Octombrie 2026
**Monedă de referință:** RON (Leu Românesc)
**Documente corelate:**
- Cerințe & Funcționalități: [`docs/requirements_and_features.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/requirements_and_features.md)
- Schema & Pini Placă: [`docs/hardware_esp32_relay.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/hardware_esp32_relay.md)
- Plan de Dezvoltare: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md)

---

## 1. Sumar Financiar & Situație Buget

| Categorie Buget | Număr Articole | Cost Total Estimat (RON) | Observații / Stadiu |
|---|:---:|:---:|---|
| **Piese Achiziționate / În Uz (Comandate eMAG & Reutilizate)** | 9 repere | **~193.00 RON** | Placă ESP32-Relay X4 V1.1 (DOIT ESP32-32E N4) - 85lei, module RF 25lei set (optional), senzori BLE 4x25lei, telefon gateway (cost 0) |
| **Piese Rămase de Comandat (Actuatoare Distribuitor)** | 7 bucăți | **~280.00 – 385.00 RON** | 7x Actuatoare electrotermice 230V NC M30x1.5 |
| **Materiale Electrice Mărunte (WAGO, Cablaj, Carcasă)** | 3 repere | **~50.00 RON** | Conectori WAGO 221, cablu 0.75 mm², șuruburi |
| **TOTAL INVESTIȚIE PROIECT COMPLET (Estimativ)** | — | **~523.00 – 628.00 RON** | Sistem autonom complet pentru 3 zone (7 circuite) |

### 💡 Comparație de Cost cu Soluțiile Comerciale de pe Piață:
| Soluție de Piață | Nr. Zone | Dependență Cloud | Cost Comercial (RON) | Economie Realizată de Proiectul Nostru |
|---|:---:|:---:|:---:|:---:|
| **Sistemul Nostru DIY ESP32** | **3 zone (7 circuite)** | **Zero (100% Local)** | **~550 RON** | — |
| **Salus IT600 Smart Heating** | 3 zone | Da (Cloud Salus) | ~2.650 RON | **Economie de ~2.100 RON (79%)** |
| **Tado Smart Thermostat Multi-Zone** | 3 zone | Da (Abonament anual) | ~2.900 RON | **Economie de ~2.350 RON (81%)** |
| **Honeywell Evohome Wi-Fi** | 3 zone | Da (Cloud Total Connect) | ~4.200 RON | **Economie de ~3.650 RON (87%)** |

---

## 2. Tabelul BOM Detaliat (Bill of Materials)

Legendă Stare:
- 🟢 **În uz** = Piesa se află fizic pe masa de lucru sau montată, funcțională.
- 🟡 **Comandat** = Comandă plasată (eMAG / curier în tranzit).
- 🔵 **Reutilizat** = Piese existente în locuință (reutilizate cu cost 0 RON).
- ⚪ **Planificat** = Piese ce urmează a fi achiziționate pentru montajul final pe distribuitor.

| Nr. | Denumire Componentă | Model / Specificație Tehnică | Buc. | Stare | Sursă / Magazin | Preț Unitar (RON) | Preț Total (RON) | Rol în Sistem / Observații |
|:---:|---|---|:---:|:---:|:---:|:---:|:---:|---|
| **1** | **Placă Controller ESP32 cu 4 Relee** | **ESP32-Relay X4 V1.1** (DOIT ESP32-32E N4, 240MHz, 4MB Flash, 4x Relee Songle 10A 250V optocuplate, alimentare AC 90-250V / DC 7-30V) | 1 | 🟢 În uz | eMAG (cod `AI1254`) | 72.00 | **72.00** | Creierul sistemului (`192.168.1.20`). Comandă actuatoarele și emițătorul RF. |
| **2** | **Emițător Radio Centrală 433.92 MHz** | Placă fiică originală recuperată din vechiul termostat wireless, antenă perimetrică integrată, cablu 3 fire (GND, 3.3V, DATA pe GPIO19) | 1 | 🟢 În uz | Reutilizat (din vechiul termostat) | 0.00 | **0.00** | Trimite trenul de impulsuri (409 pași OOK) pentru pornirea/oprirea flăcării cazanului. |
| **3** | **Seturi Radio 433.92 MHz (SYN115 + SYN480R)** | 2x Emițător SYN115 PLL + 2x Receptor SYN480R Superheterodin, antenă sfert de undă 17.3 cm | 2 | 🟡 Comandat | eMAG | 14.50 | **29.00** | Receptorul SYN480R va fi folosit pentru decodarea stației meteo Tronic exterioare. |
| **4** | **Termometre Digitale Cameră BLE** | **Xiaomi Mijia LYWSD03MMC** (Ecran LCD, senzor Sensirion, Bluetooth BLE BTHome v2, firmware ATC) | 4 | 🟢 În uz | eMAG / AliExpress | 20.00 | **80.00** | Amplasate în: Living, Camera Copilului, Baie, Matrimonial. |
| **5** | **Baterii Litiu pentru Termometre** | Baterii pastilă tip CR2032 (3V Litiu) | 4 | 🟢 În uz | Kaufland / Dedeman | 3.00 | **12.00** | Autonomie 1.5 – 2 ani cu firmware-ul BTHome v2 (interval transmisie 15-20s). |
| **6** | **Stație Meteo Exterioară** | **Tronic 4-LD7396-2** (IAN `515492_2510`), 433.92 MHz ASK, senzor exterior pe peretele de Nord | 1 | 🔵 Reutilizat | Existentă în locuință | 0.00 | **0.00** | Sursă primară pentru compensarea climatică și curbă de încălzire. |
| **7** | **Server Gateway & Broker MQTT** | **Samsung Galaxy A6 2018** ("Cinderella", Android 10, Termux, Mosquitto 1883, Nginx 80, Tailscale) | 1 | 🔵 Reutilizat | Telefon vechi scos din uz | 0.00 | **0.00** | IP `192.168.1.28`. Bridge MQTT, proxy Nginx, acces la distanță Tailscale fără abonamente. |
| **8** | **Încărcător & Cablu Alimentare Server** | Adaptor 5V 1.5A + cablu microUSB de calitate | 1 | 🔵 Reutilizat | Existent în locuință | 0.00 | **0.00** | Alimentare continuă server Galaxy A6. |
| **9** | **Actuatoare Electrotermice Distribuitor** | **Actuator Electrotermic NC 230V, filet M30 x 1.5** (Normal Închis, cursă 3-4 mm, consum ~2W în menținere):<br>• 3 buc: Zona 1 (Robineții 1, 2, 3)<br>• 2 buc: Zona 2 (Robineții 6, 7)<br>• 2 buc: Zona 3 (Robineții 4, 5) | 7 | ⚪ Planificat | eMAG / Mathaus / Leroy Merlin (modele: Beok / Salus T30NC / Watts) | ~45.00 – 55.00 | **~315.00 – 385.00** | Montează pe robineții distribuitorului de pardoseală. Deschid buclele la cererea de căldură. |
| **10** | **Conectori Rapizi WAGO** | WAGO seria 221 (cu pârghie):<br>• 4x WAGO 221-413 (3 poli)<br>• 2x WAGO 221-415 (5 poli) | 1 set | ⚪ Planificat | Dedeman / Hornbach | 25.00 | **25.00** | Legarea sigură în paralel a actuatoarelor pe fiecare releu (3 buc Living, 2 buc Copil, 2 buc Matrimonial). |
| **11** | **Cablaj Electric Distribuție** | Cablu flexibil din cupru MYYM 2x0.75 mm² sau 3x0.75 mm² (cca. 5 metri) | 5 m | ⚪ Planificat | Dedeman | 3.50 | **17.50** | Conexiune de la tabloul electric / priză la placa de relee și distribuitor. |
| **12** | **Carcasă / Cutie Protecție Tablou** | Cutie distribuitor existentă sau tablou aplicat IP40 (4-6 module) | 1 | 🔵 Reutilizat | Cutia metalică a distribuitorului | 0.00 | **0.00** | Placa de relee se montează ferit în cutia distribuitorului de pardoseală. |

---

## 3. Ghid de Achiziție pentru Actuatoarele Rămase (Reperele #9)

Pentru echiparea completă a distribuitorului cu 7 robineți, este necesară achiziționarea a **7 actuatoare electrotermice**.

### Specificații Tehnice Obligatorii:
1. **Tip contact:** **NC (Normal Închis / Normally Closed)**
   - *De ce:* În lipsa tensiunii de comandă (releu oprit sau cădere de curent), vana este închisă. Încălzirea pornește doar când releul aplică tensiune de 230V.
2. **Tensiune de alimentare:** **230V AC (50 Hz)**
   - *De ce:* Releele Songle de pe placa ESP32 comută direct faza de 230V a rețelei, fără a fi nevoie de un transformator suplimentar greoi de 24V.
3. **Filet de montare:** **M30 x 1.5 mm**
   - *De ce:* Este standardul industrial universal pentru distribuitoarele de încălzire în pardoseală din România (Giacomini, Purmo, Rehau, Oventrop, Caleffi, Tiemme).
4. **Timp de reacție:** 2.5 – 3.5 minute (timp standard pentru elementul termic cu ceară).

### Modele Recomandate și Prețuri Piață:
- **Opțiunea Economică:** *Actuator electrotermic Beok RZ-AN 230V NC M30x1.5* (~35 – 45 RON/buc pe eMAG/AliExpress).
  - Cost total 7 bucăți: **~245 – 315 RON**.
- **Opțiunea Premium:** *Actuator electrotermic Salus T30NC 230V M30x1.5* (~55 – 65 RON/buc pe eMAG/Dedeman).
  - Cost total 7 bucăți: **~385 – 455 RON**.
- **Opțiunea Tehnică cu indicator de cursă:** *Watts 22CX 230V NC* (~50 – 60 RON/buc).

---

## 4. Jurnal de Achiziții & Bonuri Fiscale

| Dată Achiziție | Număr Comandă / Magazin | Componente Achiziționate | Valoare Factură (RON) | Document / Notă |
|---|---|---|:---:|---|
| **06.10.2026** | Comandă eMAG | 1x Placă ESP32 cu 4 Relee (cod AI1254)<br>2x Seturi Radio SYN115/SYN480R | ~101.00 | Livrată și recepționată pe 07.10.2026 |
| **Anterior** | Achiziție inițială senzori | 4x Xiaomi LYWSD03MMC + baterii CR2032 | ~92.00 | Flashtuiți cu firmware BTHome v2, funcționali |
| **07.10.2026** | Reutilizare componente | Placă RF boiler, Galaxy A6, încărcător, stație meteo Tronic | 0.00 | Zero costuri adiționale |
| *În așteptare* | Urmează (Distribuitor) | 7x Actuatoare NC 230V M30x1.5 + conectori WAGO | *est. ~350 - 400* | Înainte de montajul pe robineții distribuitorului |

---

*Notă: Dacă prețurile exacte din facturile eMAG diferă cu câțiva lei de valorile standard estimate mai sus, tabelul poate fi actualizat direct pentru a reflecta evidența contabilă exactă până la ultimul ban.*
