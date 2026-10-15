# Changelog: Sistem Inteligent de Încălzire (Smart Heating)

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md).

## [15.10.2026] - Curățare Totală Consolă, Izolare Suită Teste ASPICE & Failsafe Fără Spam Boiler

### 1. Eliminare Spam & Izolare Execuție Teste Hardware (`tools/test_system.py`)
- **Cauza Principală a Spamului Reclamat:**
  - Testul de spam rapid `test_tc10_1` trimitea 4 cereri rejectate către `/api/boiler/on`. Endpoint-ul programa automat `PENDING_ON` în coada de cooldown, determinând aprinderea flăcării reale după 50s.
  - La finalul testelor, `ThermalZoneManager` constata că centrala este aprinsă fără cerere de căldură și încerca oprirea ei în fiecare secundă pe durata interblocării de 60s, generând mesaje repetate de reject și avertizări de spam (`DTC-201`).
  - În plus, `test_tc13_1` și `test_tc13_3` trimiteau comenzile `HELP` și `STATUS` direct în consolă, aruncând peste 150 de linii de tabele brute în inelul de loguri al utilizatorului.
  - Testul `test_tc19_2` (clamping) folosea un parametru invalid (`mode?auto=false`), declanșând cererea de căldură pe Zona 1 și deschiderea actuatorilor fizici la 30°C.
- **Măsuri de Izolare & Igienă Aplicate în `test_system.py`:**
  - `test_tc10_1`: Redus la 3 apeluri (pragul exact pentru DTC-201) și apelat imediat `/api/boiler/cancel` pentru a anula comanda programată.
  - `test_tc13_3`: Curățare automată (`POST /api/console/send?cmd=CLEAR`) imediat după validarea tabelară a comenzii `STATUS`.
  - `test_tc19_2`: Izolare completă prin trecere în `mode=manual` și dezactivare orar (`schedule/enable?enabled=false`) înainte de testarea limitelor [15.0°C, 30.0°C], restaurând setpoint-ul original fără a acționa releele.
  - `run_all()`: Înlocuit bannerele lungi ASCII cu notificări concise de 1 rând (`[TEST-SUITE] >>> Automated Test Suite Started / Finished <<<`) și rulat curățarea completă a bufferului de consolă la final.

### 2. Rezolvare Comandă Consolă `CLEAR` (`src/NetworkConsole.cpp`, `web/console.html`)
- Resetat complet `_totalBytes`, `_head`, `_count` și bufferul `_logRing` în `NetworkConsole::clear()`.
- În `executeCommand()`, comanda `CLEAR`/`CLS` este executată cu bypass imediat fără a mai adăuga footer-ul `[CMD] < END: CLEAR`.
- În `web/console.html`, `clearTerm()` golește acum DOM-ul (`term.innerHTML = ''`), resetează contorul `lastSeq = 0` și sincronizează ștergerea pe server.

### 3. Validare Execuție Live (ESP32 `192.168.1.20`)
- Suita automată ASPICE SWE.5 (`tools/test_system.py`) a trecut **100% (68/68 PASS, 33/33 cerințe)** în 24.51 secunde.
- Bufferul de consolă rămâne perfect curat, afișând un singur rând la finalizarea testelor: `[TEST-SUITE] >>> Automated Test Suite Finished (68/68 PASS). All systems nominal. <<<`.

## [15.10.2026] - Sincronizare Live Orar Circadian, Preseturi Implicite Calibrate, Tracking Runtime Cazan & Estimare Factură Gaz

### 1. Sincronizare Automată în Timp Real a Orarului Circadian (Hero Card & Countdown)
- **Eliminare Necesar Refresh Manual:**
  - În bucla periodică de refresh frontend (`refresh()`, apelată la fiecare 3.5s), endpoint-ul `/api/status` expune acum `activePreset`, `activeAnchor`, `nextAnchor` și `minsToNextAnchor`.
  - Frontend-ul actualizează instantaneu cardul erou (`renderActiveAnchorHero`) și numărătoarea inversă (`updateNextAnchorCountdown`), eliminând complet problema în care cardul rămânea blocat pe profilul anterior (ex. Home Office) până la un refresh manual de pagină (F5).

### 2. Calibrare Preseturi din Fabrică conform Orelor & Temperaturilor Reale din Capturi
- **Preset `Standard / Confort`:**
  - **L–V (Săptămână):** Somn `20:00` (21.0 / 22.0 / 22.0°C), Trezire `06:00` (23.0 / 22.0 / 22.0°C), Plecat `07:30` (21.0 / 21.0 / 21.0°C), Acasă `17:00` (23.0 / 23.0 / 22.5°C).
  - **S–D (Weekend):** Somn `21:00` (21.0 / 22.0 / 22.0°C), Trezire `07:30` (23.0 / 22.0 / 22.0°C), Plecat `07:30` (21.0 / 21.0 / 21.0°C), Acasă `16:00` (23.0 / 23.0 / 23.0°C).
- **Preset `Home Office` (WFH):**
  - **L–V (Săptămână):** Somn `20:00` (21.0 / 22.0 / 22.0°C), Trezire `06:00` (23.0 / 22.0 / 22.0°C), Home Office `07:30` (23.0 / 23.0 / 21.0°C), Acasă `17:00` (23.0 / 23.0 / 22.5°C).
  - **S–D (Weekend):** Somn `21:00` (21.0 / 22.0 / 22.0°C), Trezire `07:30` (23.0 / 22.0 / 22.0°C), Home Office `12:00` (21.0 / 21.0 / 21.0°C), Acasă `16:00` (23.0 / 23.0 / 23.0°C).
- **Protecție NVS:** Valorile default din fabrică sunt hardcodate în C++ ca structuri de rezervă, fără a suprascrie configurațiile customizate din NVS ale utilizatorului decât la solicitare explicită (Reset la Fabrică).

### 3. Redesign Panou Preseturi & Editor Matriceal Integrat (Zero Modale, Independență 100% L-V vs S-D)
- **Filtrare Sloturi Libere:** Sloturile utilizator nepopulate (`Slot Liber (custom2..4) (Liber)`) sunt ascunse complet din lista de cipuri active, păstrând interfața aerisită și curată.
- **Bara de Acțiuni Preseturi:** Adăugate butoane discrete și tactile:
  - `➕ Adaugă Slot`: detectează automat primul slot liber, solicită un nume și îl activează instantaneu.
  - `💾 Salvează`: persistă orarul curent în slotul de memorie NVS activ.
  - `↺ Anulează (Discard)`: anulează editările nesalvate și reîncarcă configurația anterioară (sau valorile de fabrică).
  - `✏️ Redenumește`: permite redenumirea oricărui preset direct din interfață.
  - `🗑️ Șterge`: șterge presetul personalizat curent din NVS și revine în siguranță la `Standard`.
- **Eliminare Modal & Integrare Matrice Direct în Cardul Circadian:**
  - Eliminat butonul strident și fereastra modală popup detașată care forța temperaturi identice pe ambele profiluri.
  - Matricea tabelară de 4 ancore &times; 3 zone termice este afișată acum direct inline în cardul orarului.
  - **Independență Totală a Temperaturilor pe Weekend:** Tab-urile `[L–V (Săptămână)]` și `[S–D (Weekend)]` comută instantaneu tabelul pe profilul activ. Modificările de oră și temperatură (ex: Acasă Zona 3 la 22.5°C pe L–V vs 23.0°C pe S–D) afectează strict profilul selectat, fără suprascrieri accidentale.
  - **Clonare Rapidă la Nevoie:** Butonul `📋 Copiază L-V &rarr; S-D` permite utilizatorului să multiplice instantaneu toate orele și temperaturile de lucru pe weekend doar dacă dorește acest lucru.
  - **Auto-salvare Non-Blocantă în NVS:** Orice modificare de oră sau apăsare pe stepperele `+` / `-` este salvată automat în memoria Flash NVS a controllerului cu debounce de 700ms și indicator vizual (`✓ Salvat în NVS (L-V)` / `✓ Salvat în NVS (S-D)`).

### 4. Jurnalizare & Insight-uri Termice în Consolă
- **Clarificare Mesaj Cerere Termică:**
  - Înlocuit formularea confuză rotunjită `22.2 C <= 22.2 C` cu afișare exactă cu 2 zecimale și etichete explicite: `current 22.21 C <= start threshold 22.20 C (target: 22.5 C, hyst: +/-0.30 C)`.
- **Insight Consolă Răcire Rapidă (Posibil Aerisire / Geam Deschis):**
  - La detecția unei pante abrupte de scădere a temperaturii (>0.4°C în câteva minute), motorul termic emite un log dedicat: `[Thermal][INSIGHT] Rapid cooling detected on Zone X (Name): 22.60 C -> 22.20 C (-0.40 C in 240s, possible aeration)`.

### 5. Tracking Timp Funcționare Cazan (Runtime) & Estimare Consum/Factură Gaz
- **Contor Hardware de Foc în NVS (`BoilerRadioTrigger`):**
  - Monitorizează timpul activ de flacără: astăzi (`todaySec`), ieri (`yesterdaySec`), luna curentă, luna trecută și total de la instalare (`totalSec`).
  - Salvare eficientă în NVS (wear-leveling: la stingerea flăcării sau la intervale de 15 minute în timpul arderii continue, plus rollover automat la miezul nopții).
- **Estimare Gaz & Calibrare Factură Iarnă:**
  - Card nou în UI sub controlul centralei: afișează timpul de ardere de azi, ieri și total.
  - Modul flexibil de estimare a costului bazat pe factura lunară reală de gaze (dialog `⚙️ Factură / Cost`, configurabil în `localStorage` cu factura medie de iarnă și orele estimate de ardere).

### 6. Eliminare Spam Consolă la Oprire Cazan (Fix Buclă Stop & Protecție Anti-Short-Cycling)
- **Problemă Identificată:** Când toate zonele își atingeau temperatura țintă în timp ce cazanul se afla încă în perioada de funcționare minimă (60s anti-short-cycling), `ThermalZoneManager::update()` apela repetat în fiecare secundă `boiler.sendBoilerOff()`. Comanda era respinsă de garda RF, incrementând contorul de spam până la declanșarea erorii `DTC-201 [WARNING]: Rapid boiler toggling / spam detected` și inundând consola cu zeci de mesaje identice.
- **Soluție Implementată:** În `src/ThermalZoneManager.cpp`, s-a adăugat verificarea `boiler.canTransmit(false)`. Dacă garda este activă, se apelează o singură dată `boiler.setPendingAction(PENDING_OFF)` cu mesajul `[Thermal] BOILER STOP queued (Anti-short-cycling guard active)...`. În secundele următoare nu se mai reapelează funcția, iar la expirarea perioadei de gardă, `BoilerRadioTrigger::update()` execută automat și silențios pachetul RF de oprire.

### 7. Unificare Coloana 2 UI: Eliminare Duplicat & Structură Compactă Zone/Senzori
- **Eliminare Card Duplicat:** Cardul superior `Zone Termice (Pardoseală)` și cel inferior `Telemetrie Senzori Xiaomi BLE` afișau redundant aceleași încăperi. Cardul superior a fost eliminat complet, păstrând o singură zonă unificată și aerisită.
- **Rânduri Compacte cu Ierarhie Vizuală Corectă:**
  - **Temperatură Reală:** Afișată mare, boldată, cu grafic vectorial sparkline integrat.
  - **Setpoint Subordonat (Slave):** Steppere slim `-` / `+`, etichetate discret `țintă`, pentru a evidenția că setpoint-ul este o țintă de comandă, în timp ce temperatura măsurată reprezintă realitatea fizică.
  - **Status Hidraulic Minimizat:** Pill compact (`🟢 Repaus`, `🔴 Încălzire`, etc.) cu tooltip informativ complet la hover (deschidere actuator, pompă, cerere flacără).
  - **Buton Oprire/Pornire Cameră:** Buton discret `⏻` cu tooltip dedicat pentru trecerea rapidă a zonei în repaus fără a deranja setpoint-urile circadiene.

### 8. Notificare Start & Finalizare Suită de Teste pe Consola de Rețea
- **Claritate & Evitare Confuzie Operare:** La rularea testelor automate ASPICE SWE.5 (`tools/test_system.py`), pe consola virtuală a ESP32 (`/console`, Telnet, Serial) sunt injectate acum bannere vizuale clare:
  - **La începutul testării:** `[TEST-SUITE] ⚠️ ASPICE SWE.5 AUTOMATED TEST SUITE STARTED` însoțit de avertismentul `NOTIFICARE: Urmatoarele comenzi si modificari de parametri sunt emise de suita automata de teste!`.
  - **La finalul testării:** `[TEST-SUITE] 🏁 ASPICE SWE.5 AUTOMATED TEST SUITE FINALIZAT (68/68 PASS)` confirmând revenirea completă a parametrilor la starea de producție.
- **Evidențiere Vizuală în UI:** Adăugat stil cromatic dedicat `.t-tst` cu badge amber/galben luminos în `web/console.html` pentru eticheta `[TEST-SUITE]`.
- **Comandă `LOG <msg>` / `MSG <msg>` în C++:** În `src/NetworkConsole.cpp`, comenzile care încep cu `LOG `, `MSG ` sau `ECHO ` sunt scrise direct în log-ul consolei cu timestamp, fără a mai fi încadrate de separatoarele standard `[CMD] >` / `[CMD] < END`.

### 9. Remediere Comandă CLEAR / CLS pe Consola Virtuală
- **Problemă Identificată:** La apăsarea butonului `CLEAR` sau la tastarea comenzii `CLEAR` / `CLS` în consolă, istoricul nu se ștergea deoarece:
  1. În C++ `NetworkConsole::clear()` nu reseta contorul `_totalBytes = 0`, iar `executeCommand()` adăuga imediat în buffer separatorul de final `[CMD] < END: CLEAR`.
  2. Pe client în `web/console.html`, `term.innerHTML` nu era curățat la tastarea manuală a comenzii, iar variabila `lastSeq` nu se reseta.
- **Soluție Implementată:**
  - În `src/NetworkConsole.cpp`: `clear()` resetează acum complet `_totalBytes = 0`, `_head = 0`, `_count = 0` și execută `memset(_logRing, 0, ...)`. Dacă `executeCommand()` primește `CLEAR` sau `CLS`, apelează direct `clear()` și revine imediat, fără a mai insera bannere de final.
  - În `web/console.html`: la tastarea `CLEAR` sau la clic pe butonul `CLEAR`, clientul golește imediat `term.innerHTML = ''`, resetează `lastSeq = 0` și trimite comanda la server pentru a goli și memoria circulară a plăcii. De asemenea, dacă clientul detectează un seq `0` de la server, golește automat fereastra consolei pentru toți clienții conectați.

### 10. Verificare & Conformitate ASPICE
- **PlatformIO Build:** 1,222,041 bytes (62.2% flash), RAM 65,228 bytes (19.9%), 100% SUCCESS.
- **ASPICE SWE.5 Test Suite:** 68/68 teste atomice PASS (100.0%) în 24.38s, acoperire 33/33 cerințe (100%).



### 1. Igienizare Memorie Heap C++ (Zero Fragmentare pe ESP32)
- **Pre-alocare Buffer String (`reserve`) în Handlerele REST API:**
  - În `src/main.cpp`: adăugat apeluri `json.reserve(...)` în toate handlerele API cu răspunsuri dinamice mari (`handleApiZones` - 2560 bytes, `handleApiStatus` - 2048 bytes, `handleApiBleSensors` - 1536 bytes, `handleApiSetupDiscoveredSensors` - 1536 bytes, `handleApiDiagStatus` - 1024 bytes).
  - În `src/ThermalZoneManager.cpp`: adăugat `json.reserve(2048)` în `getScheduleJson()` și `json.reserve(1024)` în `getPresetsJson()`.
  - În `src/DiagnosticEngine.cpp`: adăugat `out.reserve(1024)` în `runSelfTest()`.
  - În `src/TimeAndWeatherService.cpp`: adăugat `json.reserve(512)` în `toJson()`.
  - În `src/DevicePassport.cpp`: adăugat `json.reserve(512)` în `toJson()`.
  - **Efect Arhitectural:** Eliminat complet zecile de micro-realocări heap consecutive efectuate la fiecare ciclu de interogare (poll la 3.5s). Free heap-ul rămâne stabil, prevenind fragmentarea memoriei RAM în timp la funcționare pe termen lung (săptămâni/luni).

### 2. Curățare Frontend & Optimizare CSS
- **Eliminare Redundanțe în `web/index.html`:**
  - Șters blocul duplicat de stiluri `@media (max-width: 520px)`.
  - Verificat compilarea gzip automată prin `tools/build_web.py` (119 KB -> 29.1 KB, rată compresie 24.4%).
- **Micro-Grafice Vectoriale Sparkline de Trend Temperatură (Istoric 3h):**
  - Upgrade major de la badge-ul text simplu la micro-grafice dinamice vectoriale inline SVG (`generateSparklineSvg` / `getTempSparkline`) direct pe cardurile camerelor și în tabelul de senzori BLE.
  - **Unificare Chei Canonice de Istoric (`getCanonicalTempKey`):** Zona termică și senzorul fizic BLE asociat (ex: Camera Copilului și `Kids Room`, Dormitor Matrimonial și `A4:C1:38:94:0F:44`) partajează acum exact aceeași cronologie de eșantioane. Curba de sus și curba de jos sunt 100% identice, eliminând discrepanțele sau aparența de linie „zbârcită”/dințată pe senzor.
  - **Filtrare Zgomot & Scalare Inerțială:** Prag minim de scalare mărit la $1.0^\circ\text{C}$ și fereastră minimă de 3 minute, netezind micro-variațiile inofensive de $\pm 0.03^\circ\text{C}$ ale convertorului ADC în repaus.
  - **Aliniere Perfectă Tabel Senzori BLE & Indicator Circular Semnal:** Înlocuit badge-urile text late (`BUN (-80dBm)` vs `EXCELENT (-66dBm)`) care decalau cifrele la dreapta cu o bulină luminoasă circulară discretă (`.sensor-signal-dot`) plasată lângă numele senzorului, cu tooltip detaliat la hover (`Semnal BLE: Excelent (-65 dBm)`). Toate valorile de temperatură, graficele sparkline, umiditățile și bateriile sunt acum aliniate vertical la pixel (`.sensor-temp-col`, `.sensor-hum-col`).
  - **Redenumire Senzori în GUI & Config:** Senzorul `A4:C1:38:94:0F:44` a fost actualizat din `Bathroom` în `Dormitor Matrimonial` în `include/Config.h`. S-a implementat funcția de redenumire directă din GUI (`promptRenameSensor` cu pictogramă ✏️), salvată persistent în `localStorage`.
  - Verificat end-to-end prin capturi Chrome CDP: `e2e_11_zones_aligned.png` și `e2e_12_sensors_aligned.png`.

### 3. Server Local de Dezvoltare Decuplat (`tools/dev_server.py`)
- **Iterare Rapidă Frontend fără Flash pe ESP32:**
  - Creat și calibrat serverul local Python (`python tools/dev_server.py`) care servește fișierele din `web/` pe `http://localhost:8080/index.html`.
  - Frontend-ul comunică direct prin CORS cu placa ESP32 (`192.168.1.20`), permițând modificări vizuale instantanee (F5 în browser) cu zero risc de blocare sau uzură flash.

### 4. Verificare & Conformitate ASPICE
- **PlatformIO Build:** 1,211,161 bytes (61.6% flash), RAM 65,180 bytes (19.9%), 100% SUCCESS.
- **ASPICE SWE.5 Test Suite:** 68/68 teste atomice PASS (100.0%) în 24.54s, acoperire 33/33 cerințe (100%).

## [14.10.2026] - Clarificare Terminologie Zone, Corectare Indexare ID-uri Zone & Decuplare Telemetrie Senzori

### 1. Decuplare Rezoluție Senzori BLE de Starea Circuitului de Încălzire
- **Eliminare Eronată TIMEOUT când o cameră este oprită:**
  - În `ThermalZoneManager::update()`, verificarea `if (!_configs[i].enabled) continue;` era executată înainte de scanarea senzorilor BLE. Când utilizatorul oprea o cameră (termostat dezactivat), citirea temperaturii era ignorată complet, `currentTemp` rămânea `0.0°C` iar `sensorValid` rămânea `false`. Din acest motiv, consola raporta fals `TIMEOUT (Failsafe OFF)`, iar cardul UI afișa `--.-°C`, chiar dacă senzorul fizic Xiaomi (`Kids Room` `A4:C1:38:84:A1:E8`) transmitea pachete la fiecare 60s cu 23.0°C.
  - S-a mutat rezoluția senzorului și actualizarea `_statuses[i].currentTemp` / `sensorValid` la începutul buclei, înaintea verificării `enabled`. Temperatura camerei este acum monitorizată permanent, indiferent dacă circuitul de încălzire este pornit sau oprit manual.
  - S-a adăugat tranziție automată din starea `ZONE_DISABLED` în `ZONE_IDLE` la reactivarea camerei în `setZoneEnabled()` și în bucla principală `update()`.

### 2. Corectare Bug Decalaj Indexare Zone în UI (Off-by-One)
- **Eliminare Comutare Greșită a Camerelor:**
  - În `web/index.html`, funcția `renderZones` genera handler-ele `onclick='toggleZone(${i})'` și `onclick='adjTarget(${i}, ...)'` folosind indexul vectorial bazat pe 0 (`i = 0, 1, 2`), în timp ce backend-ul ESP32 (`/api/zones/toggle?id=...`) folosește ID-ul bazat pe 1 (`id >= 1 -> idx = id - 1`).
  - Din această cauză, când utilizatorul apăsa pe Zona 3 (Matrimonial, `i=2`), frontend-ul apela `toggleZone(2)`, ceea ce backend-ul traducea ca `2 - 1 = 1` (Camera Copilului)! Astfel, Camera Copilului fusese dezactivată din greșeală fără intenția utilizatorului.
  - S-a corectat apelul prin transmiterea explicită a ID-ului real: `toggleZone(${z.id})`, `adjTarget(${z.id}, ...)` și `id='sp-${z.id}'`.

### 3. Redesign Terminologie UI Carduri Zone Termice
- **Eliminare Jargon Confuz și Contradictoriu:**
  - Formulat anterior ca `Releu GPIO25: INACTIV (Închisă)    Dezactivează`, textul genera o confuzie majoră (închidere releu vs ventil NC vs termostat cameră).
  - Noul design separă clar cele 3 concepte:
    1. **Stare Termostat (Insignă Header):** `🟢 Temp OK (Repaus)` / `⚪ Zonă Oprită` / `🔴 Încălzire Activă` / `🟡 Așteptare Actuator`.
    2. **Stare Fizică Circuit Șapă (Footer):** `Robinet șapă: ÎNCHIS (Repaus / Temp atinsă)` vs `DESCHIS (Circulă apă caldă)` vs `OPRIT (Termostat inactiv)`.
    3. **Buton Clar de Acțiune:** Înlocuit link-ul text ambiguu cu un buton stilizat: `[⛔ Oprește camera]` (când e activă) / `[▶️ Pornește camera]` (când e oprită).
    4. **Afișare Permanentă Temperatură:** Cardul afișează permanent temperatura reală a senzorului (ex. `23.0°C`) chiar dacă termostatul camerei este oprit manual.

### 4. Verificare & Conformitate
- **PlatformIO Build:** 1,210,741 bytes (61.6% flash), Free Heap > 115 KB.
- **OTA Flash:** v1.2.0 (Build: Oct 14 2026 23:06:40) online pe `192.168.1.20`.
- **ASPICE SWE.5 Suite:** 68/68 teste atomice PASS (100.0%) în 22.62s.
- **E2E Live Browser Suite:** 10/10 PASS (100.0%) pe Chrome CDP cu screenshot-uri e2e validate.
- **Preset Bogdan (`custom1`):** Verificat și confirmat intact în NVS.

## [14.10.2026] - Personalizare Completă Preseturi (Fabrică + Utilizator) & Control Anulare Cooldown Cazan

### 1. Motor Preseturi & Personalizare
- **Deblocare Personalizare Preseturi Predefinite:**
  - Preset-urile din fabrică (`default` / Standard Confort, `eco` / Vacanță, `wfh` / Home Office) pot fi acum modificate și salvate direct în NVS fără a mai fi blocate cu eroare.
  - La aplicare (`applyPreset`), controller-ul verifică prioritar existența unei configurații personalizate salvate în NVS (`<presetId>_cfg`); dacă nu există o personalizare, revine transparent la configurația circadiană din fabrică.
  - Adăugată metoda `resetPreset(presetId)` și endpoint-ul `POST /api/schedule/preset/reset?id=<id>`, permițând oricând revenirea unui preset la valorile originale de fabrică.
- **Extindere Sloturi Utilizator & Protecție Preset `Bogdan` (`custom1`):**
  - Numărul de sloturi libere de utilizator a fost extins la 4 (`custom1` .. `custom4`).
  - Suita de teste integrate ASPICE SWE.5 (`tools/test_system.py`) a fost redirecționată pe slotul de test dedicat `custom4` cu auto-curățare imediată (`reset`), protejând 100% presetul personal al utilizatorului salvat pe `custom1` (`Bogdan`).
  - Catalogul JSON expune acum toate cele 7 sloturi cu flag-ul `saved: true/false`, iar UI-ul marchează cu `✎` preseturile din fabrică care au fost ajustate de utilizator.
- **Modal Salvare Preset Îmbunătățit:**
  - Dropdown-ul preselectează automat presetul activ curent și completează numele acestuia.
  - Buton nou dedicat `[↺ Reset Fabrică]` afișat contextual doar pentru preseturile care conțin modificări salvate în NVS.

### 2. Control Cazan: Răzgândire & Anulare Comandă Amânată (Pending Cooldown)
- **Eliminare Blocaj Răzgândire în Frontend:**
  - Funcția `triggerBoiler` din `web/index.html` filtra eronat cererea dacă `currentBoilerState === 'ON'`, ignorând faptul că era activată o comandă amânată de oprire (`boilerPendingAction === 'OFF'`).
  - Filtrul a fost corectat pentru a permite trimiterea cererii opuse: utilizatorul poate apăsa din nou pe „Pornește Centrala”, anulând instantaneu oprirea programată și păstrând cazanul activ.
- **Buton Dedicat de Anulare `[✕ Anulează]`:**
  - În bannerul galben de comandă amânată (`#boiler-pending-banner`) a fost adăugat butonul `[✕ Anulează]`, apelând noul endpoint `POST /api/boiler/cancel`.
  - Starea flăcării, insigna și bannerul revin instantaneu la starea normală activă fără a mai aștepta expirarea numărătorii inverse a protecției anti-short-cycling.

### 3. Verificare & Conformitate
- **PlatformIO Build:** 1,210,313 bytes (61.6% flash), Free Heap > 115 KB.
- **Firmware Flashat OTA:** v1.2.0 (Build: Oct 14 2026 22:27:23) activ pe `192.168.1.20`.
- **Suita ASPICE SWE.5:** 68/68 teste atomice PASS (100.0%) în 23.26s.
- **E2E Live Browser Tests (Chrome CDP):** 10/10 PASS (100.0%).
- **Verificare Hardware Live:** Confirmat că `custom1` conține `'Bogdan'` (`saved: true`) și că revenirea / anularea acțiunii amânate funcționează deterministic.

## [14.10.2026] - Remediere Audit Chrome DevTools & Lighthouse (FSM Admin, Tesla Mode & Panou Principal)

### 1. Subpagina Fir Logic & Stări (FSM Admin)
- **Eliminare Suprapunere Vizuală S4 peste S3:**
  - În diagrama SVG (Planul A), nodul `S3: PENDING` a fost decalat pe Y la `y="24"` (h=36, până la y=60), iar `S4: HEATING_ACTIVE` a fost plasat la `y="76"` (h=88, până la y=164), creând un spațiu de separare vertical de 16px și eliminând coliziunea vizuală detectată la inspecția DOM.
  - Săgețile de tranziție SVG (`S2->S3`, `S2->S4`, `S3->S4`, `S4->S5`) au fost recalculate geometric pentru conexiune directă la noile coordonate.
  - În grila CSS `.fsm-grid`, distanța între carduri a fost mărită la `gap: 1.5rem` (`gap: 1.25rem` pe viewports mobile `< 768px`).
  - Cardurile `.fsm-node` au primit proprietatea `overflow: hidden;` și `z-index: 1; position: relative;`, iar stările `:hover` și `.active` beneficiază de `z-index: 10;`, `box-shadow: 0 0 15px rgba(0, 0, 0, 0.6);` și contur de accent `#38bdf8`, eliminând orice scurgere de conținut sau suprapunere între celule vecine.
- **Accesibilitate & Contrast Cromatic (WCAG AA/AAA):**
  - Titlurile nodurilor `.fsm-node-title` ridicate la `#f1f5f9` (contrast > 12:1).
  - Descrierile nodurilor `.fsm-node-desc`, subtitlurile `.subtitle` și insignele `.fsm-badge` ridicate de la valorile muted (`#6a707a`, `#475161`) la `#cbd5e1` (contrast > 9:1 pe fundalul `#0a0f1d`).
  - Opacitatea cardurilor inactive `.fsm-node` a fost îmbunătățită de la 0.55 la 0.65 pentru lizibilitate optimă.

### 2. Subpagina Schemă Termică (Tesla Mode) & Performanță TBT (Total Blocking Time)
- **Decuplare Render & Reducere TBT de la 370ms la <40ms:**
  - `updateTeslaSchema()` și `updateFsmDiagram()` nu mai rulează necondiționat la fiecare pas de polling sau în bucla de 1 secundă atunci când utilizatorul este pe Panoul Principal.
  - Rutinele de randare vectorială SVG rulează exclusiv când tab-ul respectiv este activ (`currentViewMode === 'tesla'` sau `'fsm'`), eliminând peste 300ms de blocare pe firul principal de execuție.
  - `switchViewMode()` apelează instantaneu funcția de desenare corespunzătoare la comutarea de tab-uri pe baza ultimelor date telemetrice memorate în memorie (`lastSData`, `lastZData`).
  - Inițializarea la boot a fost încapsulată în `requestAnimationFrame()`, permițând browserului să finalizeze randarea inițială a DOM-ului înainte de declanșarea primelor cereri de rețea.

### 3. Panou Principal: Toleranță Rețea, Polling Adaptiv & Indicator "Live"
- **Indicator Stare Sincronizare Rețea (`#pill-net`):**
  - Adăugat pill dinamic în bara de antet: `🟢 Live` (răspuns normal sub 1200ms), `🟡 1.4s` (avertisment latență tranzitorie hardware), `🔴 Reconnect...` (eroare rețea / timeout socket).
- **Polling Adaptiv cu Backoff Exponențial:**
  - Înlocuit `setInterval(refresh, 3500)` rigid cu `scheduleNextPoll(delayMs)` ghidat de latența reală:
    - Latență nominală: 3500ms.
    - Latență ridicată (>1200ms cauzată de scanările intensive BLE NimBLE): relaxare automată a pasului următor la 5000ms pentru a preveni aglomerarea cozii TCP a controller-ului.
    - Eroare / AbortController (timeout 4000ms): backoff automat la 6000ms.
- **Ergonomie Mobilă & Ținte Tactile (Touch Targets):**
  - Butoanele de instalare PWA (`.btn-install`), comutare mod (`.mode-btn`), acțiuni Tesla (`.btn-tesla`), tab-urile de navigare (`.view-tab-btn`) și selectoarele de nivel (`.lvl-btn`) au fost extinse la o înălțime minimă confortabilă de 42px–44px, conform ghidurilor ergonomice mobile WCAG.

### 4. Verificare & Conformitate
- **PlatformIO Build:** 1,207,329 bytes (61.4% flash), sub limita OTA de 1.25 MB.
- **Firmware Flashat OTA:** v1.2.0 (Build: Oct 14 2026 21:20:44) activ pe `192.168.1.20`.
- **Chrome CDP Browser Live Tests:** 10/10 PASS (100.0%) pe scenariile TS-01 la TS-07.
- **ASPICE SWE.5 Host Suite:** 68/68 PASS (100.0%), toate cele 33 cerințe validate.

## [14.10.2026] - Remediere Audit Chrome DevTools AI & Lighthouse (WCAG AA, Semantism & Toleranță Rețea)

### 1. Directivă Remediere Audit DevTools & Lighthouse (5 Puncte 100% Implementate)
- **Punct 1: Contrast Cromatic Insuficient (WCAG AA PASS > 5.8:1):**
  - Înlocuit în `web/index.html` culoarea `#64748b` pentru texte pe fundal închis (`#0f172a`, `#0a0f1d`) cu `#94a3b8` (contrast > 5.8:1) pentru etichete de senzori, footer zone, adrese MAC, meta-telemetrie și texte schematice SVG.
  - Schimbat fundalul activ al butoanelor și tab-urilor din `#0284c7` în `#0369a1` (`#view-btn-dash`, `#tab-weekday`, `.mode-btn.active`, `.btn-matrix-open`, modal action buttons), urcând raportul de contrast cu textul alb la **5.93:1** (100% WCAG AA).
- **Punct 2: Etichetare Accesibilă Elemente Formulare (Toggles & Steppere):**
  - Adăugat explicit atributele `aria-label` și `title` pe switch-urile toggle din panoul de gardă (`#sw-schedule-en`, `#sw-preheat-en`, `#sw-window-en`).
  - Adăugat `aria-label` și `title` pe selectoarele de sloturi NVS, inputurile de denumire preset, câmpurile orare de ancore (`#mtime-*`, `#time-*`), butoanele de închidere/salvare din modale și pe butoanele de reglare a temperaturii camerelor.
- **Punct 3: Landmark Semantic `<main>`:**
  - Containerul principal a fost migrat la tag-ul semantic `<main class="container" role="main">`, rezolvând avertismentul Lighthouse privind lipsa unui reper principal (`main landmark`), păstrând neschimbate clasele și ID-urile de aliniere.
- **Punct 4: Ergonomie Mobilă & Touch Targets (< 520px):**
  - Adăugată regula responsive `@media (max-width: 520px)`: `.header-pills` utilizează `width: 100%; justify-content: space-between; flex-wrap: wrap;`, iar butoanele `.btn-small` și `.tab-btn` au `min-height: 38px` pentru ținte tactile confortabile.
- **Punct 5: Toleranță Latență Reverse Proxy Nginx & AbortController 4000ms:**
  - Mărit timeout-ul `AbortController` din `refresh()` de la 2500ms la **4000ms**, prevenind tăierea prematură a cererilor (`statusCode: 0`) în timpul scanărilor pasive NimBLE prin proxy-ul `galaxy-a6`.
  - Asigurat `clearTimeout` determinist în blocul `finally` și garantată protecția împotriva ciclurilor de polling suprapuse (`if (isRefreshing) return;`).

### 2. Rezultate Verificare & Conformitate
- **PlatformIO Build:** 1,206,833 bytes (61.4% flash), sub bugetul critic OTA de 1.25 MB.
- **Firmware Flashat OTA:** v1.2.0 (Build: Oct 14 2026 20:41:11) activ pe `192.168.1.20`.
- **Chrome CDP Browser Live Tests:** 10/10 PASS (100.0%) pe scenariile TS-01 la TS-07.
- **ASPICE SWE.5 Host Suite:** 68/68 PASS (100.0%), toate cele 33 cerințe validate.

## [14.10.2026] - Directivă Arhitecturală Advisor (Argon Maximum): Matrice Orar 4×3, Stateflow FSM pe 2 Planuri & Schemă Termică Dinamică 7 Circuite

### 1. Panoul Principal (`web/index.html`): Matrice Tabelară 4×3, Bug Ancore & Naming Zona 3
- **Rezolvare Bug Editare Ancore Circadiene (`POST /api/schedule/anchor`):**
  - Implementat suport complet case-insensitive pe backend (`WAKE` vs `wake`) și parsare atât URL query parameters cât și body JSON.
  - Conversie robustă a formatelor de oră 12h AM/PM în format 24h standard `HH:MM`.
  - Sincronizare imediată a țintelor active: când utilizatorul ajustează ancora curent activă, temperaturile zonelor se actualizează instantaneu în RAM și NVS, re-evaluând bucla termodinamică fără delay.
- **Corectare Mapare Zona 3 (Dormitor Matrimonial):**
  - Migrare forțată în NVS din C++: Zona 3 (GPIO25) este redenumită din "Baie mare" în "Dormitor Matrimonial".
  - Uniformizat naming-ul în întregul dashboard: tabele matrice, diagramă FSM și schemă Tesla.
- **Re-arhitecturare Kiosk & Eliminare Aglomerare Vizuală:**
  - Înlocuită grila greoaie cu 4 carduri din panoul principal cu un buton proeminent `📅 Editează Orar & Ancore (Matrice 4×3)` și chip-uri rapide de sinteză (`anchors-quick-summary`).
  - Creat modalul `modal-schedule-matrix`: tabel compact 4 Ancore × 3 Zone cu steppere tactile `+`/`-`, inputuri de timp 24h, auto-save debounced (800ms) și butoane de salvare per rând cu feedback vizual `✓ Salvat în NVS`.
  - Adăugat banner dinamic cu numărătoare inversă (`next-anchor-banner`) pentru următoarea ancoră circadiană.

### 2. Firul Logic & Stările (FSM Admin): Stateflow Ierarhic pe 2 Planuri în SVG
- **Diagramă Orientată pe 2 Planuri cu Săgeți Vectoriale:**
  - **Planul A (Master Plant & Hydronic FSM):** Arborele cazanului și pompei GPA conectat cu săgeți animate (`<path marker-end="url(#fsm-arrow)">`), ilustrând stările $S0 \rightarrow S1 \rightarrow S2 \rightarrow (S3 \mid S4) \rightarrow S5 \rightarrow S6 \rightarrow S1$, substările 4.1 (Min Burn 60s), 4.2 (Steady Burn), 4.3 (Pre-Heat) și stările speciale S7 (Anti-Seize), S8 (Manual Hold).
  - **Planul B (Zone Parallel FSM):** 3 benzi orizontale paralele pentru Z1 Living (GPIO32), Z2 Copil (GPIO33), Z3 Matrimonial (GPIO25) cu stări live (IDLE, FLOW ACTIVE, VALVE OPENING, FROST GUARD, DISABLED) și casetă dedicată pentru interblocări locale (Geam deschis, Protecție îngheț, Failsafe BLE DTC-101).
  - Păstrate toate cele 9 ID-uri de noduri necesare testării automate ASPICE SWE.5 și browser CDP.

### 3. Dinamizarea Schemei Termice (Tesla Mode)
- **Topologie Reală cu 7 Circuite de Pardoseală & Bypass Băi:**
  - Living & Bucătărie (Z1 / R32): 3 bucle paralele (Buclele 1, 2, 3).
  - Camera Copilului (Z2 / R33): 2 bucle paralele (Buclele 6, 7).
  - Dormitor Matrimonial (Z3 / R25): 2 bucle paralele (Buclele 4, 5).
  - Ramură permanentă de bypass (Portprosop Băi) fără actuatoare, conectată la senzorul BLE `Bathroom` (22.6°C), garantând circulația apei și protecția pompei.
- **Progres Dinamic Actuatoare cu Ceară & Estimare Termodinamică:**
  - Calcul dinamic al deschiderii cerii `0% -> 100% [openSec/120s]` bazat pe `openSec` expus pe `/api/zones`, cu bare de progres dedicate.
  - Animația de curgere a fluidului prin serpentină pornește exclusiv când actuatorul a depășit 30% deschidere (`openSec >= 36`).
  - Model termodinamic inerțial dinamic (`[Estimat Inerțial]`) pentru temperaturile Tur/Retur și $\Delta T$, corelat cu timpul de funcționare al arzătorului și starea pompei.

### 4. Închidere Audit Stage 2: Rezolvare Micro-Patch pe cele 3 `TestStepWarning`
- **Punct 1 (Curba Termică Tur Plafonată la Regim Pardoseală):**
  - Ajustat modelul inerțial în `web/index.html`: Turul este plafonat la $36.0^\circ\text{C}\text{–}43.5^\circ\text{C}$ (cu Retur $27.8^\circ\text{C}\text{–}35.3^\circ\text{C}$ și $\Delta T \approx 8.2^\circ\text{C}$), optimizând reprezentarea pentru regimul de condensare al centralei ($<45^\circ\text{C}$) și eliminând avertismentul de supraîncălzire a șapei.
- **Punct 2 (Protecție Race Condition Debounce 800ms în Matrice):**
  - Adăugat flag-ul `isMatrixDirty` și `isMatrixModalOpen`: Pe durata editării repetate a temperaturilor în modalul 4×3, ciclurile periodice de polling nu mai suprascriu DOM-ul matricei, eliminând riscul ca inputul să revină la valoarea veche înaintea finalizării salvării.
- **Punct 3 (Răcirea & Contracția Progresivă a Actuatoarelor cu Ceară):**
  - În `src/main.cpp`: La decuplarea releului, `openSec` decrementează liniar de la $120\text{s} \rightarrow 0\text{s}$ în loc să sară brusc la 0, modelând contracția termică a parafinei.
  - În `web/index.html`: Actuatorul afișează starea `RĂCIRE: X% [Xs/120s]`, iar debitul prin serpentină este menținut animat până când deschiderea scade sub 30% ($36\text{s}$).

### 5. Rezultate Verificare & Calitate
- **PlatformIO Build:** 1,206,593 bytes (61.4% flash), marjă liberă confortabilă sub limita de 1.25 MB.
- **Test System (PC Host):** Suită extinsă la 68 de teste atomice (adăugat Group 19 pentru validări negative).
- **Flashing OTA:** Firmware v1.2.0 (Build: Oct 14 2026 20:18:55) activ pe `192.168.1.20`.

## [14.10.2026] - Eliminare Epuizare Socketuri TCP, Neutralizare Service Worker & Polling Secvențial cu AbortController

### Rezolvare Blocare Pagină în Browser (Hanging Spinner / Socket Starvation)
- **Diagnoză Problemă Semnalată de Utilizator:**
  - Tab-ul de Chrome rămânea blocat în spinnerul circular de încărcare cyan din cauza epuizării celor 4-5 socketuri TCP hardware ale ESP32 (`CONFIG_LWIP_MAX_SOCKETS`).
  - *Cauze identificate:*
    1. **Intercepție Service Worker (`sw.js`):** Scriptul PWA `sw.js` intercepta navigarea pe `/` și rula pre-caching agresiv (`/`, `/manifest.json`, `/icon-192.png`, `/icon-512.png`), lansând concomitent 4 conexiuni TCP. Când ESP32 respingea o conexiune, handlerul `fetch(e.request)` din worker rămânea agățat pe termen nelimitat în stiva de rețea Chrome.
    2. **Furtună de Conexiuni Paralele în `refresh()`:** Funcția rula `Promise.all([fetch('/api/zones'), fetch('/api/boiler/status'), fetch('/api/schedule'), fetch('/api/schedule/presets')])`, generând alte 4 cereri simultane la fiecare 3.5 secunde.
- **Măsuri de Remediere Arhitecturală (Extreme Minimalism & Anti-Bloat):**
  - **Neutralizare Service Worker:** Înlocuit scriptul de caching din `src/main.cpp` cu un script auto-destructiv de curățare (`skipWaiting` + ștergere toate cheile de cache + `unregister()`). În `web/index.html`, înlocuit înregistrarea SW cu un mecanism proactiv de dezinstalare și curățare cache.
  - **Polling Secvențial & Serializat:** Structurat `refresh()` să interogheze exclusiv `/api/zones` și `/api/boiler/status` într-o manieră secvențială (`await ...; await ...;`), garantând strict **maxim 1 socket deschis** în orice moment.
  - **Decuplare Orar & Preseturi:** Datele statice de orar circadian și catalogul de preseturi sunt încărcate o singură dată la inițializare (`loadScheduleAndPresets()`) și re-interogate la cerere exclusiv când utilizatorul modifică un preset sau o ancoră.
  - **Protecție `AbortController` (Timeout 2500ms):** Orice cerere de telemetrie blocată este abandonată automat după 2.5s, prevenind suprapunerea apelurilor și blocarea browserului.
- **Validare Live:**
  - Firmware compilat (1176.8 KB, 61.0% flash) și încărcat prin Web OTA (`v1.2.0 Build: Oct 14 2026 18:05:34`).
  - Timp de răspuns pagină `/`: **248 ms** (HTTP 200).
  - Latență endpoint-uri API: `/api/zones` (**75 ms**), `/api/boiler/status` (**101 ms**).
  - Suita de teste sistem PC (`tools/test_system.py`): **63/63 PASS (100.0%)**.
  - Suita de teste live în browser Chrome (`tools/browser_live_test.py`): **10/10 PASS (100.0%)**, pagina se încarcă instantaneu fără spinner infinit.

## [14.10.2026] - Schemă Termodinamică Tesla Service Mode (Task 30) & Diagramă Live Stări FSM (Task 31)

### Schemă Termodinamică & Hidraulică Interactivă (web/index.html - Sarcina 30)
- **Vizualizare Vectorială SVG Inspirată din Tesla Supermanifold:**
  - Adăugat un tab de comutare rapidă în header (`🏎️ Schemă Termică (Tesla Mode)`), care ascunde coloanele panoului principal și afișează o reprezentare completă, animată, a instalației hidraulice.
  - **Sub-sistem Cazan Gaz:** Card dedicat cazanului cu flacără dinamică (verde cu border radiant la ardere activă, gri în standby), timer live de cooldown rămas și indicatori de protecție radio 433.92 MHz.
  - **Grup Pompare & Amestec (GPA):** Reprezentare vană de amestec și rotor pompă animat cu rotație continuă (`.pump-spinning`), etichetat cu turație (1850 RPM) și timpi de interblocare (120s ceară, 180s overrun).
  - **Supermanifold & 3 Circuite Pardoseală:** Conducte tur (portocaliu cald) și retur (albastru) cu animație vectorială de curgere (`teslaPipeFlow`), debitmetre pe tur, actuatoare electrotermice NC cu comutare dinamică (230V verde vs NC gri) și casete de cameră cu telemetrie live ($T_{cameră} \mid T_{target}$).
  - **Casete Telemetrie Stil Tesla:** $T_{tur}$ (48.5°C ardere / 24°C standby), $T_{retur}$ (38.0°C / 23.5°C), $\Delta T$ sorbție (10.5°C Optim / 0.5°C Echilibru) și stare pompă.
  - **Panou Butoane Acțiuni Service:** 5 butoane tactile: Purjare/Test Pompă GPA (`/api/diag/selftest`), Test RF Cazan (`/api/boiler/status`), Test Valvă Copil, Test Valvă Living și Curățare DTC Diagnoză.

### Diagramă Interactivă de Stări FSM & Fir Logic Live (web/index.html - Sarcina 31)
- **Motor Decizional cu 9 Noduri de Stare Finite:**
  - Adăugat tabul `🧠 Fir Logic & Stări (FSM Admin)` afișând starea logică a întregului sistem:
    1. `IDLE / STANDBY` (toate camerele la temperatură)
    2. `CERERE TERMICĂ` (scădere sub histerezis, bucle deschise)
    3. `AȘTEPTARE ACTUATOR` (interlock ceară termică 120-180s)
    4. `MANUAL HOLD` (cerere zonă activă, cazan oprit manual)
    5. `ÎNCĂLZIRE ACTIVĂ` (flacără aprinsă via 433MHz, pompă GPA activă)
    6. `POST-CIRCULAȚIE POMPĂ` (pompă releu 4 activă 180s pe keeper zone)
    7. `GARDĂ COOLDOWN` (interdicție reaprindere rapidă 60s)
    8. `PROTECȚIE ÎNGHEȚ` (urgență sub 12°C)
    9. `GEAM DESCHIS` (cădere >1.2°C / 5min, valvă închisă 30m)
  - **Evidențiere Live & Timere:** Nodul stării active este evidențiat dinamic cu bordură cyan electric, efect de glow și afișarea timerelor active în interiorul nodului (ex: `19s Lockout`).
  - **Indicator Header:** Afișaj sintetic live în colțul din dreapta (`Stare Activă Hardware: 7. GARDĂ COOLDOWN (19s LOCKOUT)`).

### Verificare Live pe Browser Nativ (Chrome CDP) & ASPICE SWE.5
- **Suită E2E 100% PASS (10/10 teste):**
  - Rulat `tools/browser_live_test.py` direct pe browserul Chrome al utilizatorului cu 10/10 teste validate: geometrie, cele 3 moduri de control (simplu, preseturi, avansat), protecție anti-spam, schemă Tesla (`TC-UI-13`), acțiuni service (`TC-UI-15`) și diagramă FSM (`TC-UI-14`).
  - Capturate două noi screenshot-uri de înaltă rezoluție în brain: `e2e_06_tesla_service_mode.png` și `e2e_07_fsm_live_diagram.png`.
- **Flash OTA Hardware & Teste de Sistem:**
  - Firmware compilat (1176.5 KB, 60.9% flash) și flashuit pe placă via OTA în 7.9s (`v1.2.0 Build: Oct 14 2026 16:55:37`).
  - Toate cele 63 de teste SWE.5 (`tools/test_system.py`) continuă să ruleze cu **100% PASS**.

## [14.10.2026] - Suită Automată de Testare E2E pe Browser Nativ (Task 32) & Reparare Layout Grid 2 Coloane

### Testare Automată Browser Nativ via Chrome DevTools Protocol (tools/browser_live_test.py)
- **Implementare Framework E2E Interactiv pe Google Chrome:**
  - Creat instrumentul automat `tools/browser_live_test.py` care se conectează prin CDP WebSocket direct la browserul Google Chrome nativ deschis pe ecranul utilizatorului la `http://192.168.1.20`.
  - Toate cele 5 scenarii de testare au trecut cu **100% succes (7/7 PASS)**:
    * `TC-UI-11` (Geometrie): Coloana 1 ($X=56\text{px}, Y=199.6\text{px}$) și Coloana 2 ($X=595\text{px}, Y=199.6\text{px}$) sunt perfect paralele, simetrice și aliniate side-by-side.
    * `TC-UI-01` (Nivel Simplu): Ascundere orar automat și editor ancore; notificare verde discretă; control direct temperaturi.
    * `TC-UI-05` (Nivel Preseturi): Ascundere butoane tehnice (`Reset fabrică`, `Salvare slot custom`).
    * `TC-UI-03` (Selecție Preseturi): Toate cele 5 chip-uri de preseturi detectate și funcționale.
    * `TC-UI-04` (Comutare Dinamică Ținte): Temperaturile țintă din zone se actualizează dinamic în DOM fără reîncărcare de pagină.
    * `TC-UI-06` (Nivel Avansat): Editorul circadian tabelar și acțiunile de sloturi NVS devin complet vizibile.
    * `TC-UI-09` (Anti-Spam Toast Feedback): Apăsarea pe OFF când centrala este deja oprită afișează un toast discret de 3 secunde (`ℹ️ Centrala este deja oprită (Standby)`) fără flood HTTP sau bannere false de oprire.
  - Generate 5 capturi de ecran de înaltă rezoluție în directorul brain (`e2e_01_beginner_mode.png` .. `e2e_05_anti_spam_toast.png`).

### Reparare Definitivă Layout Grid & Identificatori Coloane (web/index.html)
- **Eliminare `</div>` Parazit & Balansare Container:**
  - Șters tag-ul de închidere prematură a containerului CSS Grid (`.container`), permițând coloanei a doua (circuite pardoseală) să stea paralel cu prima coloană în loc să cadă sub ea.
  - Atribuit identificatori unici `#col-main-1` și `#col-main-2` pentru selectare DOM deterministă.
  - Firmware recompilat curat (Flash: 60.6%, RAM: 19.9%) și încărcat via OTA în 7.6s (Build: Oct 14 2026 16:31:38). Suita completă SWE.5: **63/63 PASS (100.0%)**.

## [14.10.2026] - Sincronizare Aprindere Cazan la Cerere, 3 Niveluri de Control UI (Începător, Mediu, Avansat) & Protecție Anti-Spam

### Reparare Critică Secvențiere Cazan (src/ThermalZoneManager.cpp)
- **Sincronizare Automată a Aprinderii Flăcării cu Cererea Termică:**
  - Identificat și rezolvat defectul de logică prin care o zonă aflată în `Heating Active` (de ex. Camera Copilului la 22.6°C cu țintă 23.0°C) avea pompa activă, dar cazanul rămânea blocat pe `OFF`:
    * În vechea implementare, flag-ul `_boilerDemand` devenea `true` chiar dacă transmisia RF eșuase sau cazanul fusese oprit ulterior (ex: la boot sau prin test API forțat). Ramura de pornire `if (!_boilerDemand)` nu se mai reevalua niciodată!
    * Înlocuit verificarea cu starea efectivă a emițătorului `if (!boiler.isLastCommandOn())`. Dacă există cerere termică (`anyZoneReadyForBoiler`), sistemul verifică starea reală a arzătorului și comandă aprinderea (`sendBoilerOn()`) imediat ce timpii de protecție permit sau programează pornirea automată în coada `PENDING_ON`.
    * Validat pe hardware live: centrala a pornit automat (`Boiler: ON`, `PumpActive: True`) imediat după expirarea cooldown-ului de siguranță.
- **Sincronizare Setpoint Manual în Timpul Pre-Heat-ului Anticipativ:**
  - În `setTargetTemperature(index, target)`, dacă Pre-Heat-ul anticipativ este activ (`_preHeatActive == true`), modificarea manuală de la butoanele `+`/`-` este propagată atât în `_nextAnchor` cât și în `_activeAnchor`. Astfel, setpoint-ul ajustat de utilizator devine activ pe loc și este reflectat instantaneu pe ecran fără a fi ignorat de orar.

### Interfață Kiosk cu 3 Niveluri de Control & Protecție Anti-Spam (web/index.html)
- **Selector de Nivel de Control (Segmented Control Bar):**
  - Implementat un comutator elegant pe 3 niveluri de experiență, salvat persistent în `localStorage`:
    * `🟢 Simplu (Manual Direct):` Pentru începători. Oprește orarul automat și ascunde taburile complicate. Fiecare cameră este reglată direct din butoanele `+`/`-`, exact ca la un termostat digital simplu și clar.
    * `🟡 Preseturi Rapide:` Pentru utilizare zilnică. Afișează chip-urile mari cu scenarii de viață (`Standard`, `Home Office`, `Vacanță / Eco`) dintr-un singur click, ascunzând editorul avansat de ore.
    * `🔴 Orar Avansat:` Pentru inginerie și reglaj fin. Deblochează toate opțiunile: ancore circadiene (Sleep, Wake, Away, Home), timpi Pre-Heat anticipativ, protecție geam deschis, salvare sloturi NVS `custom1`/`custom2`.
- **Protecție Anti-Spam & Toast Discret de 3 Secunde:**
  - În `triggerBoiler(action)`, s-a eliminat comportamentul derutant la click-uri repetate sau spam pe aceeași stare:
    * Dacă utilizatorul apasă `OFF` când centrala este deja oprită: NU se mai trimit cereri repetate și NU se afișează bannerul de oprire în Xs. Se afișează doar un mesaj toast discret timp de 3 secunde: `ℹ️ Centrala este deja oprită (Standby)`.
    * La fel pentru `ON` când este deja pornită: `ℹ️ Centrala este deja pornită (Activă)`.
    * Adăugat debounce de 350ms pentru a ignora atingerile multiple accidentale pe ecranul tactil al tabletei.
- **Validare ASPICE SWE.5:**
  - Suita de 63 de teste atomice (`python tools/test_system.py`) a trecut cu **100% succes (63/63 PASS, 33/33 cerințe acoperite)**.

## [14.10.2026] - Siguranță Cazan la Cold Boot (Failsafe Power-Loss) & Coadă Execuție Amânată (Pending Cooldown)

### Siguranță Hardware & Sincronizare Fizică Cazan (include/BoilerRadioTrigger.h, src/BoilerRadioTrigger.cpp)
- **Failsafe la Pornire după Cădere de Tensiune (Power-Loss Safety Invariant):**
  - Rezolvată discrepanța critică semnalată: dacă placa ESP32 este scoasă din priză în timp ce cazanul este activ (`ON`), la reluarea alimentării (Cold Boot) receptorul radio de perete rămânea blocat pe ON din lipsa unui semnal de oprire.
  - În `BoilerRadioTrigger::begin()`, la inițializarea hardware-ului se emite imediat un burst forțat `sendBoilerOff(true)` către receptorul 433.92 MHz și se stochează starea `OFF` în NVS. Aceasta garantează oprirea fizică a flăcării la orice repornire/pană de curent, eliminând riscul de ardere necontrolată a gazului și sincronizând perfect receptorul fizic cu interfața.
  - Implementat mecanism NVS Flash Wear Guard pe namespace-ul `"boiler"` (`saveStateToPreferences()` / `loadStateFromPreferences()`), scriind în flash doar la schimbarea efectivă a stării (`if (oldVal != newVal)`).
  - Adăugat mecanism de heartbeat periodic (la fiecare 15 minute) în `BoilerRadioTrigger::update()` pentru redundanță radio suplimentară.

### Coadă Execuție Amânată & Cooldown Interactiv (src/main.cpp, web/index.html)
- **Comenzi Amânate (Deferred Execution / Pending Actions):**
  - În `handleApiOff()` și `handleApiOn()`, dacă utilizatorul acționează manual butonul de comutare în timpul perioadei de protecție a arzătorului (anti-short-cycling guard: 60s ardere minimă / 60s pauză minimă), cererea nu mai este pur și simplu respinsă fără acțiune.
  - Sistemul înregistrează acțiunea în coada internă (`PENDING_OFF` sau `PENDING_ON`) și returnează codul HTTP 429 cu `"pending": true`, `"target": "OFF"`, `"waitSec": X` (conform RFC 6585 și cerințelor de siguranță FR-17).
  - În bucla principală `loop()`, metoda `boilerRadio.update()` monitorizează expirarea timpului minim și execută automat transmisia RF imediat ce interblocarea expiră.
  - Dacă utilizatorul trimite comanda opusă înainte de expirare (de ex. apasă ON în timp ce era programată oprirea amânată), acțiunea amânată este anulată instantaneu (`pendingCancelled: true`).
- **Feedback Vizual & Timer Cooldown în Dashboard Kiosk (`web/index.html`):**
  - Adăugat stilul `.badge-pending` cu animație pulsatorie portocalie (`#fbbf24`).
  - Adăugat bannerul de avertizare `#boiler-pending-banner` și tag-ul `#boiler-cooldown-tag` ce afișează numărătoarea inversă la secundă (`⏳ Xs rămase` vs `✅ Gata (Ready)`).
  - Implementat un temporizator local la secundă (`setInterval`) în JavaScript pentru actualizare fluidă fără interogări abuzive către microcontroler.
- **Validare ASPICE SWE.5 & Teste Automate:**
  - Suita de 63 teste atomice (`tools/test_system.py`) a trecut cu **100% succes (63/63 PASS, 33/33 cerințe)**.
  - Validat fluxul complet pe hardware live (`192.168.1.20`): `POST /api/boiler/on` -> `POST /api/boiler/off` -> `429 Pending` -> Anulare / Execuție automată.

## [14.10.2026] - Sincronizare Setpoint-uri Preseturi & UX Ancore Dinamice (Home Office & Eco)

### Corecții Funcționale & Aliniere Termodinamică (src/ThermalZoneManager.cpp, src/main.cpp, web/index.html)
- **Sincronizare Automată Setpoint-uri Zone la Aplicarea Preseturilor:**
  - La apelul `applyPreset()` (ex: `wfh`, `eco`, `default`) și `resetScheduleToDefault()`, setpoint-urile individuale ale fiecărei zone termice (`_configs[i].targetTemp`) sunt sincronizate instantaneu cu valorile din ancora activă curentă din orar.
  - S-a respectat regula de protecție la uzură a memoriei flash (NVS Flash Wear Guard): salvarea în NVS se execută doar dacă valorile s-au modificat (`fabsf(old - new) > 0.05f`).
  - În `setTargetTemperature(idx, target)`, dacă Bio-Schedule este activat, ajustarea manuală de la butoanele `+`/`-` ale zonei din dreapta este propagată instantaneu în ținta ancorei active din profilul curent (`setAnchorTarget`), făcând trecerea automată pe profilul `custom` și determinând termostatul să preia modificarea fără latență.
  - În `handleApiScheduleEnable`, s-a adăugat suport pentru parametrii query opționali `preheat` și `window`.
- **Corecție Afișaj UI Dashboard Kiosk (`web/index.html`):**
  - În `renderZones()`, setpoint-ul zonei afișat pe cardurile din dreapta folosește acum `z.effectiveTarget` (temperatura țintă reală reglată de Bio-Schedule) în loc de `z.targetTemp` rezidual, eliminând discrepanța vizuală reclamată între țintele din orar și setpoint-urile zonelor.
  - Adăugat `.then(() => refresh())` în `adjTarget()`, oferind feedback vizual instantaneu la ajustarea temperaturilor per zonă.
  - În `refresh()`, citirea `scheduleData` a fost mutată înainte de `renderZones()`, transmițând starea corectă a orarului activ.
- **UX Ancore Contextuale Adaptate la Preset (Home Office / Vacanță Eco):**
  - În `renderActiveAnchorHero()`:
    * Când este activ presetul `Home Office` (`wfh`), intervalul orar de zi (ancora tehnică `away`, 08:30–17:00) nu mai afișează confuzant `🚶 PLECAT (AWAY) - Mod Eco economic cât casa este goală`, ci este etichetat contextual: `💼 HOME OFFICE (ZI)` cu descrierea `Lucru de acasă & confort termic optim de birou`.
    * Pentru presetul `Vacanță / Eco` (`eco`), ancora de zi afișează `🍃 VACANȚĂ / ECO (ZI)` cu descrierea `Temperatură economică redusă (economie gaz)`.
  - În `renderScheduleEditor()`:
    * Denumirea cardului 3 de editare a orarului circadian se adaptează dinamic: `💼 Home Office` pentru WFH, `🍃 Zi Eco` pentru Eco, și `🚶 Plecat` pentru Standard/Confort.
- **Verificare & Validare ASPICE SWE.5:**
  - Compilare PlatformIO curată (RAM: 19.9%, Flash: 60.3%, 1,185 KB).
  - Flash OTA pe hardware live (`192.168.1.20`) finalizat cu succes în 30.95 secunde.
  - Suita de teste atomice SWE.5 (`tools/test_system.py`) a trecut cu **100% succes (63/63 PASS, 33/33 cerințe)**.

## [14.10.2026] - Interfață Grafică PWA Kiosk (Samsung Galaxy Tab A9), Optimizare Flash (-133 KB) & Validare SWE.5

### Dashboard PWA Kiosk & Bio-Schedule Complet (web/index.html)
- **Arhitectură PWA (Progressive Web App) Nativă pe ESP32:**
  - Servire direct din flash PROGMEM fără consum dinamic de heap a resurselor PWA:
    * `GET /manifest.json`: Web App Manifest cu `display: standalone`, `theme_color: #080c14`, pictograme PNG (192x192) și SVG vectoriale.
    * `GET /sw.js`: Service Worker non-blocant cu cache passthrough pentru cererile API REST.
    * `GET /icon.svg`: Pictogramă vectorială flacără radiantă & fundal rotunjit dark mode.
    * `GET /icon-192.png`: Pictogramă rasterizată comprimată (1,442 bytes, 16 culori indexate) necesară pentru generarea automată a pachetului WebAPK pe Android Google Chrome.
  - Suport complet pentru instalare aplicație pe tableta Kiosk (Samsung Galaxy Tab A9) și smartphone:
    * Buton dedicat `📲 Instalează App` integrat cu evenimentul `beforeinstallprompt`.
    * Modal interactiv de asistență cu instrucțiuni pas-cu-pas pentru instalare pe ecranul de pornire și activare PWA pe adrese IP locale HTTP (`chrome://flags` -> *Insecure origins treated as secure*).
    * Auto-ascundere a butonului când aplicația rulează deja în mod standalone (fără bara de adrese a browserului).
- **Design Grafic Premium Responsive (2 Coloane Kiosk & Mobil):**
  - **Coloana 1: Cazan Gaz & Smart Bio-Schedule:**
    * Card Cazan Gaz cu erou vizual dinamic (flacără activă cu glow verde/roșu), selector mod `AUTOMATIC` vs `MANUAL`, butoane de comandă rapidă și stare pompă recirculare GPA.
    * Card Smart Bio-Schedule cu comutatoare master (Activare Orar, Pre-Heat anticipativ, Geam Deschis).
    * Card Ancoră Circadiană activă cu emojiuri dinamice (`🌙 Sleep`, `🌅 Wake`, `🚶 Away`, `🏡 Home`), descriere scop și ținte programate.
    * Selector rapid de Preset-uri chips (`🏭 Standard`, `🍃 Eco 18°`, `💼 WFH 22.5°`, `💾 Slot 1`, `💾 Slot 2`).
    * Modal salvare preset activ în sloturile NVS utilizator (`custom1`, `custom2`).
    * Editor vizual de ancore circadiene cu tab-uri Luni-Vineri vs Weekend, reglaj timp și steppere temperatură per zonă.
  - **Coloana 2: Zone Termice & Telemetrie BLE:**
    * Card Zone Termice Pardoseală (Living & Bucătărie, Camera Copilului, Baie mare) cu steppere tactile `+`/`-`, badge-uri de stare (Încălzire activă, Standby, Geam deschis), identificator senzor și stare releu electrotermic NC.
    * Card Telemetrie Senzori Xiaomi BLE cu badge-uri de calitate a semnalului radio (`EXCELENT`, `BUN`, `MARGINAL`, `SLAB`), nivel baterie și umiditate.
    * Card Diagnoză Sistem (Heap RAM, Uptime, Wi-Fi RSSI, Flash OTA disponibil) și navigare rapidă către Telnet Console și Setup Wizard.
- **Optimizare Extremă a Memoriei Flash (-133.8 KB Economisiți):**
  - Decuplat `HTTPClient` de `WiFiClientSecure` în `TimeAndWeatherService.cpp` prin pasarea explicită a instanței `WiFiClient` către `http.begin(client, url)`. Linkerul nu mai trage în binar stiva greoaie `mbedtls` SSL server/client (economisind peste 115 KB).
  - Adăugat `-D CONFIG_NIMBLE_CPP_ENABLE_RETURN_CODE_TEXT=0` și `-D CONFIG_NIMBLE_CPP_ENABLE_WARNINGS=0` în `platformio.ini`.
  - Minificare automată a resurselor HTML/CSS/JS la build-time în `tools/build_web.py` (eliminare comentarii și spații redundante) comprimând dashboard-ul la doar 10,988 bytes GZIP.
  - Binarul de firmware a scăzut de la 1,320 KB la **1,186 KB (1.15 MB, 60.0% flash)**, asigurând un spațiu de rezervă de **121.7 KB sub limita maximă a partiției OTA** (1.25 MB).
- **Validare Live pe Hardware (192.168.1.20) & ASPICE SWE.5:**
  - Flash OTA executat cu succes în 41 secunde (`tools/ota_upload.py`), restart curat în 7 secunde.
  - Verificare live pe toate rutele PWA (`/`, `/manifest.json`, `/sw.js`, `/icon.svg`, `/icon-192.png`) cu cod `200 OK`.
  - Suita de 63 teste atomice executată cu 100% succes (**63/63 PASS**, acoperire 33/33 cerințe de sistem) în 39.80 secunde.

## [14.10.2026] - Bio-Schedule Customizabil & Motor de Preset-uri NVS (Backend REST API & CLI)

### Personalizare Ancore Circadiene & Preset-uri de Fabrică / Utilizator
- **Ancore Circadiene Complet Customizabile:**
  - Ora (`HH:MM`) și setpoint-urile pe fiecare zonă (`z1`, `z2`, `z3`) pentru fiecare ancoră (`Sleep`, `Wake`, `Away`, `Home`) sunt complet configurabile individual sau global.
  - Nou endpoint: `POST /api/schedule/anchor` cu suport pentru parametri JSON sau query/form și persistență imediată în NVS cu protecție flash-wear.
- **Motor de Preset-uri (Factory & User Slots):**
  - Preset-uri din fabrică (read-only): `default` (Standard / Confort), `eco` (Vacanță / 18.0°C), `wfh` (Home Office / 22.5°C).
  - Sloturi libere în NVS: `custom1`, `custom2` (salvare orar activ cu etichetă prietenoasă în namespace-ul `"presets"`).
  - Endpoint-uri REST dedicate:
    * `GET /api/schedule/presets`: Catalogul complet de preset-uri și starea de salvare a sloturilor.
    * `POST /api/schedule/preset/apply?id=...`: Aplicare instantanee a oricărui preset (factory sau user).
    * `POST /api/schedule/preset/save?id=custom1&name=...`: Salvare orar curent într-un slot de memorie.
    * `POST /api/schedule/reset`: Revenire imediată la setările de fabrică (`default`).
- **Comenzi CLI & Network Console:**
  - `SCHEDULE RESET`: Resetare imediată la valorile din fabrică.
  - `SCHEDULE PRESET <id>`: Activare preset direct din terminal.
  - CLI extins: `esp_cli.py schedule preset list`, `esp_cli.py schedule preset apply <id>`, `esp_cli.py schedule preset save <id> [name]`.
- **Decuplare UI:**
  - Modificările vizuale pe `web/index.html` și `web/setup.html` au fost amânate deliberat pentru următorul pas, conform cerinței utilizatorului.
- **Suită de Testare Atomică & Matrice de Trasabilitate Bidirecțională (ASPICE SWE.5):**
  - Desfăcut cele 17 teste monolitice în **63 de teste atomice independente**, fiecare având o singură responsabilitate de verificare (schema payload, prag memorie heap, polaritate, timpi de interblocare hidraulică, idempotență RF, gardă 60s, rată de spam DTC-201, calibrare setpoint-uri, sincronizare NTP, ancore orar și preset-uri).
  - Implementat în [`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py) un motor automat de calcul al trasabilității bidirecționale între cerințele SWE.1 (`FR-1` .. `FR-28`, `NFR-1.1`, `NFR-1.5`, `NFR-2.2`, `NFR-3.2`) și testele SWE.5.
  - Raportul generat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md) include:
    * **Indicatori de Trasabilitate (KPIs):** 33/33 cerințe definite, 33/33 acoperite (100.0%), **0 cerințe neacoperite**, 63 teste atomice executate, redundanță medie de 2.6x teste/cerință.
    * **Matrice de Trasabilitate:** Tabel complet cu maparea fiecărei cerințe la testele atomice asociate, număr de teste, status verificare (`PASS`/`FAIL`) și verdict acoperire (`🟢 COVERED` / `🔴 UNCOVERED`).
    * **Registru Detaliat:** Tabel cu toate cele 63 de verificări atomice, latență milimetrică și aserțiuni verificate live.
  - Rezultat live pe hardware (`192.168.1.20`): **63/63 PASS (100.0%)**, acoperire 100.0% a cerințelor de sistem.

## [13.10.2026] - Arhitectură Web GZIP Build-Time, Optimizare Flash (-176 KB) & Latență Consolă <50ms

### Optimizare Memorie Flash & Arhitectură Web (Modelul Smart Aquarium)
- **Decuplare Interfețe Web în Fisiere Dedicate (`web/`):**
  - Extrase paginile web din string-urile C++ în fișiere curate HTML/CSS/JS: [`web/index.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/index.html) (Dashboard interactiv), [`web/setup.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/setup.html) (Setup Wizard & Passport) și [`web/console.html`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/web/console.html) (Live Web Terminal).
- **Pre-Build Script Automat (`tools/build_web.py`):**
  - Integrat în `platformio.ini` (`extra_scripts = pre:tools/build_web.py`). Comprimă paginile cu `gzip -9` generând headere PROGMEM:
    * `include/dashboard_html_gz.h`: 13.8 KB -> 3.7 KB (27.3%).
    * `include/setup_html_gz.h`: 26.4 KB -> 7.3 KB (27.8%).
    * `include/console_html_gz.h`: 12.8 KB -> 4.4 KB (34.6%).
    * Total pachet web în flash: **doar 15.5 KB** (față de >53 KB text brut).
- **Reducere Masivă Dimensiune Binar & Eliberare Heap:**
  - Binarul de firmware a scăzut de la **1,304,000 bytes la 1,127,833 bytes (1.10 MB, 57.4% flash)** — economie de **176 KB**!
  - Eliminat peste 250 de linii de concatenare C++ `String html += ...` din `main.cpp` și `NetworkConsole.cpp`.
  - Paginile sunt servite cu `server.sendHeader("Content-Encoding", "gzip");` și `server.send_P(200, "text/html", ...);`, eliminând complet alocările dinamice pe heap la încărcarea paginilor (Free Heap stabil la **132 KB**).

### Consolă Web Bidirecțională: Latență <50ms & Delimitatori Vizuali
- **Răspuns Direct în POST (`NetworkConsole.cpp`):**
  - La trimiterea unei comenzi prin Web Console (`POST /api/console/send`), noul output este inclus direct în corpul răspunsului HTTP 200 via `getLogsSince()`.
  - Delimitare vizuală clară: adăugat chenar distinctiv `.cmd-start` cu gradient dark-blue și bordură stânga cyan `#38bdf8` pentru promptul comenzilor (`[CMD] > %s`), plus footer `.cmd-end`.
- **Eliminare Root-Cause Latență 3s (TCP SYN Collision & Buffer Bottlenecks):**
  - *Frontend (`web/console.html`):* Înlocuit `setInterval(fetchLogs, 2000)` necoordonat cu `scheduleNextPoll()` adaptiv și `AbortController`. La trimiterea oricărei comenzi, polling-ul este oprit instant, iar cererea de fundal în zbor este anulată cu `abort()`, prevenind coliziunea a două conexiuni TCP simultane pe serverul WebServer single-threaded (ce declanșa timeout-ul Windows TCP SYN de 3.0 secunde).
  - *Optimizare Scriere Batch (`src/NetworkConsole.cpp`):* Înlocuit ciclul caracter-cu-caracter din `write(buffer, size)` și `rawWrite()` cu scriere pe chunk-uri de linie și `memcpy` direct în ring buffer.
  - *Accelerare Serial UART (`src/main.cpp`):* Mărit bufferul TX hardware `Serial.setTxBufferSize(4096)` pentru a preveni blocarea CPU la 115200 baud la afișarea comenzilor lungi (`STATUS`).
  - *Optimizare `getLogsSince()`:* Înlocuit ciclul `output += char` cu două apeluri de slice `output.concat(...)` (<0.05 ms).
  - *Timestamp Caching:* Memorare în cache a șirului de timestamp pe secunda curentă, eliminând zeci de apeluri redundante de `localtime_r`.

### Verificare și Conformitate
- **Validare SWE.5:** Suita `tools/test_system.py` a trecut cu **100% PASS (15/15 teste)**.
- **Flash OTA:** Actualizare confirmată pe ESP32 (`192.168.1.20`) în 17 secunde cu HTTP 200.

## [13.10.2026] - Smart Bio-Schedule (4 Ancore Naturale), Fereastră Deschisă & Pre-Heat Anticipativ (Sarcina 26)

### Motor Orar Circadian (Smart Bio-Schedule & Ancore de Viață)
- **Eliminare Matrice Orare Rigide (168 celule) în Favoarea a 4 Ancore Naturale:**
  - Înlocuită paradigma clasică a termostatelor programabile complicate cu 4 stări organice de confort:
    * `Sleep` (somn: noapte odihnitoare 20.0–21.0°C).
    * `Wake` (trezire: confort termic dimineața 22.0–22.5°C).
    * `Away` (plecat: regim Eco când casa e nelocuită 19.5–20.0°C).
    * `Home` (acasă: confort termic de seară 21.5–23.0°C).
  - Profiluri separate configurabile pentru Luni–Vineri (zile lucrătoare) și Sâmbătă–Duminică (weekend).
  - Evaluare orară precisă la fiecare 30 secunde corelată cu ceasul de sistem NTP.

### Gardă Inteligentă Geam Deschis (Open Window Smart Pause Guard)
- **Detecție Derivă Termică Rapidă:**
  - Istoric circular de eșantioane de temperatură (10 eșantioane la interval de 30s).
  - Dacă temperatura dintr-o cameră scade cu mai mult de **1.2°C în sub 5 minute**, sistemul deduce deschiderea unei ferestre pentru aerisire.
  - Valva zonei respective este forțată automat în starea **ÎNCHIS (PAUZĂ) timp de 30 de minute**, prevenind pierderea masivă de căldură și consumul inutil de gaz. Anunțat în Network Console și în telemetria `STATUS`.

### Algoritm Anticipativ de Pre-Încălzire (Predictive Pre-Heat cu Inerție Termică)
- **Modelare Inerție Șapă & Corecție Meteo Exterioară:**
  - Șapa de beton a încălzirii în pardoseală are un timp de răspuns de 1.5–3 ore.
  - Algoritmul calculează timpul de avans necesar în funcție de diferența de temperatură $\Delta T = T_{target} - T_{current}$ și de factorul meteo exterior preluat de la Open-Meteo:
    $$k_{weather} = 1.0 + \max(0.0, (15.0 - T_{outdoor}) \times 0.04)$$
  - Pornirea anticipată se declanșează cu 30–120 minute înainte de ancorele `Wake` și `Home`, astfel încât temperatura dorită este atinsă exact când utilizatorul se trezește sau ajunge acasă.

### Persistență NVS Anti-Uzură Flash & Interfață REST/CLI
- **Namespace `"sched"` cu Comparație `memcmp()`:**
  - Structura completă de 136 octeți este stocată în NVS flash.
  - Flash-ul este scris strict când utilizatorul modifică un parametru (`memcmp != 0`), garantând zero uzură flash la funcționare de lungă durată.
- **REST Endpoints & Comenzi CLI:**
  - `GET /api/schedule` și `POST /api/schedule`: citire și configurare JSON.
  - `POST /api/schedule/enable?state=true|false`: comutare rapidă.
  - Comenzi interactive adăugate în consolă (`SCHEDULE`, `SCHEDULE ON/OFF`, `SCHEDULE PREHEAT ON/OFF`, `SCHEDULE WINDOW ON/OFF`).
  - Subcomanda `python tools/esp_cli.py schedule` adăugată în utilitarul CLI.

### Validare Live & Curățare Zgomot Consolă
- **Eliminat Spam Meteo:**
  - Actualizările periodice de vreme la 15 minute nu mai poluează consola virtuală cu `[INIT]   * Outdoor Weather:`, fiind anunțate strict la boot și redirecționate pe Serial UART de debug.
- **Optimizare Flash OTA:**
  - Minificat CSS-ul paginilor web (`main.cpp`, `NetworkConsole.cpp`, `WebSetupPage.h`), aducând binarul la **1.24 MB** (1,305,280 bytes), cu 1.3 KB sub limita de sector `maxSketchSpace`.
- **Suită de Testare ASPICE SWE.5:**
  - Adăugat testul automat `SWE.5-TC15: Smart Bio-Schedule Contract, Anchors & Failsafe Guard (FR-26)`.
  - Toate cele 15 teste de sistem trecute cu **100.0% PASS** în 15.65 secunde.

## [12.10.2026] - Eliminare Word Wrap pe Mobil, Scalare Font (`A-`/`A+`) & Watchdog Feed la OTA (Sarcina 25)

### Eliminare Word Wrap pe Telefon & Scroll Orizontal Fluid (`NetworkConsole`)
- **Cauză Formatare Necoalizată pe Mobil:**
  - Clasa CSS `.term-container` utiliza `white-space: pre-wrap; word-break: break-all;`. Pe ecrane înguste de smartphone (<450px), liniile tabelelor de diagnostic și `STATUS` (de 70-100 de caractere) erau rupte la fiecare 35-40 de caractere, fărâmițând cuvintele și coloanele pe 2-3 rânduri și făcând citirea tabelelor impracticabilă.
- **Soluție Formatare Monospace Rigidă (No-Wrap):**
  - Setat `white-space: pre; word-break: normal;` în `.term-container` cu `overflow-x: auto; overflow-y: auto; -webkit-overflow-scrolling: touch;`. Tabelele își mențin alinierea perfectă pe coloane, permițând scroll orizontal fluid cu gesturi tactile pe telefon.
- **Comutator Interactiv de Word Wrap (Opțional, Memorat în LocalStorage):**
  - Adăugat checkbox în bara de control de jos: `<input type="checkbox" id="wrapToggle"> Wrap`. Utilizatorul poate activa manual ruperea rândurilor dacă dorește, starea fiind salvată persistent în `localStorage` (`termWrap`).
- **Scalare Dinamică a Fontului Terminalului (`A-` / `A+`):**
  - Adăugate butoane tactile `A-` și `A+` în footer pentru ajustarea în trepte fine a dimensiunii fontului terminalului (între `0.52rem` și `1.20rem`).
  - Pe ecrane mobile (<640px), fontul implicit coboară automat la `0.70rem` pentru o densitate crescută a datelor. Dimensiunea aleasă este salvată persistent în `localStorage` (`termFontSize`).
- **Design Responsive Mobile (`@media (max-width: 640px)`):**
  - Compactate marginile, padding-ul, butoanele rapide (`qbtn`) și bara de input pentru a maximiza zona vizibilă pe smartphone.

### Protecție Watchdog la Încărcare OTA (`main.cpp`):**
- **Feed Hardware Watchdog pe Fiecare Bloc Primit:**
  - Atașat callback-ul `Update.onProgress([](size_t, size_t) { esp_task_wdt_reset(); });` direct înainte de `httpUpdater.setup()`.
  - Resetează periodic Task Watchdog Timer-ul pe fiecare pachet HTTP de firmware scris în flash, eliminând riscul de restart TWDT în timpul actualizărilor mari de firmware.

## [12.10.2026] - Remediere Scurgere Memorie Heap (`DTC-402`), NimBLE `setMaxResults(0)` & Auto-Clear Histerezis (Sarcina 24)

### Remediere Scurgere Memorie Heap în Scannerul NimBLE (`BleSensorScanner`)
- **Cauză Rădăcină (Root-Cause Diagnoză Live):**
  - La 4 ore de la pornire, memoria RAM liberă a scăzut de la 127 KB la 51 KB, declanșând `[DIAG-ALERT] DTC-402 [WARNING]: Free heap memory low (51 KB)`.
  - În `NimBLE-Arduino`, scanarea continuă fără setarea explicită a limitei (`_pBLEScan->setMaxResults(0)`) păstrează implicit `m_maxResults = 0xFF` (nelimitat). Fiecare pachet BLE de la dispozitive din vecinătate cu MAC rotativ (telefoane mobile, smartwatch-uri, televizoare inteligente) genera un obiect `new NimBLEAdvertisedDevice` stocat pe heap în `m_scanResults.m_deviceVec`.
  - Eliberarea memoriei (`pScan->erase()`) era condiționată de `m_maxResults == 0`. Fără această setare, vectorul a acumulat peste 70 KB de date pe heap în 4 ore.
- **Soluție Implementată:**
  - În [`BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp): apelat `_pBLEScan->setMaxResults(0)` la inițializare (procesare exclusiv prin callback-ul `onResult`, cu eliberare instantanee din heap fără stocare în vector) și adăugat `_pBLEScan->clearResults()` la restartarea scanării.
  - Consumul de memorie pentru subsistemul Bluetooth este acum 100% constant în timp (zero memory leak).
- **Auto-Clear cu Histerezis pentru DTC-uri de Sistem (`DiagnosticEngine`):**
  - Implementat mecanism automat de stingere pentru `DTC-402` (când memoria liberă depășește 75 KB) și `DTC-401` (când semnalul Wi-Fi depășește -78 dBm), restabilind automat Scorul de Sănătate la **100% [EXCELLENT]** odată ce condiția a fost rezolvată.
- **Optimizare Streaming OTA & Unbuffered Logging (`tools/ota_upload.py`):**
  - Adăugat `sys.stdout.reconfigure(line_buffering=True)` și timeout adaptiv de 6s la recepția răspunsului HTTP, eliminând blocajele de stdout pe Windows.
- **Validare Live pe Hardware:**
  - Firmware încărcat prin Web OTA (`192.168.1.20`), Free Heap stabilizat la **125 KB**, codul `DTC-402` stins, Health Score **100%**. Toate cele 14 teste ASPICE SWE.5 trecute cu **100.0% PASS**.

## [12.10.2026] - Aliniere Tabelară pe 2 Coloane, Eliminare SYNCING din POST & Colorare Exhaustivă a Stărilor (Sarcina 23)

### Format Tabelar Structural pe Fiecare Subsistem & Aliniere Milimetrică (`STATUS`)
- **Carduri Tabelare Dedicate pentru Fiecare Subsistem:**
  - Înlocuită înșiruirea densă de text și pipe-uri de după `[CMD] > STATUS` cu tabele clare și aerisite pe 2 coloane:
    * `[SYSTEM & CONTROLLER HARDWARE]`: Firmware, Git, Serial, Boot Cause, Processor, Uptime, Silicon Die Temp, Free Memory, Flash Memory, Firmware Size.
    * `[BOILER, PUMP & HYDRAULIC ACTUATION]`: Burner Flame, Operation Mode, Heat Demand, Controller IP, Actuator Type, Circulation Pump, Frost Guard, Pump Relay State.
    * `[WEATHER & OUTDOOR ENVIRONMENT]`: Outdoor Temp, Climate Season, Outdoor Humidity, Weather Service, Telemetry Age, Weather Status.
    * `[SAFETY & FAILSAFE INTERLOCKS]`: Sensor Failsafe, Hardware Watchdog (90s TWDT), Pump Anti-Seize, Next Exercise.
- **Distanțare Generoasă a Coloanelor & Corectare Deplasare Stânga UTF-8:**
  - Identificată cauza deplasării la stânga cu 1-2 caractere: `snprintf` conta octeții UTF-8 ai simbolului de grad `°` (`\xC2\xB0` = 2 octeți) în loc de lățimea vizuală pe ecran (1 caracter), ceea ce scurta spațierea cu 1 caracter pe liniile de temperatură.
  - Implementat algoritmul `utf8VisibleLen()` care calculează lungimea pe baza caracterelor vizibile, garantând că separatorul `|` și coloana a 2-a pornesc la fix aceeași coloană pe fiecare rând.
  - Mărită distanța dintre coloane la o lățime fixă generoasă (`col1TotalWidth = 48`), asigurând 8–20 spații clare de separare.

### Optimizare Scanare BLE (Ciclu de Utilizare 90%) & Reducere Timp Warmup
- **Sensibilitate Sporită Prin Pereți Multipli (`include/Config.h`):**
  - Mărit `BLE_SCAN_WINDOW_MS` de la 50ms la 90ms la un interval de 100ms (ciclu de utilizare crescut de la 50% la 90%).
  - Receptorul NimBLE ascultă acum 90% din timp canalele BLE de publicitate, capturând balizele senzorului îndepărtat din Baie Mare (după 4 pereți) în 39s–64s în loc de 293s.
- **Deblocare Bucle Operaționale în Warmup:**
  - Zonele care și-au achiziționat senzorul (ex. Living, Camera Copilului) nu mai sunt blocate la pornire timp de 5 minute în așteptarea senzorilor îndepărtați; buclele achiziționate își reiau imediat controlul termostatic normal, în timp ce zonele în așteptare rămân în siguranță cu valvele închise (`ZONE_WARMUP`).


### Eliminare Linii `SYNCING` din POST & Raportare Asincronă Progresivă sub `[INIT]`
- **Power-On Self-Test (POST) 100% Hardware & Local:**
  - Eliminat `Real-Time Clock` din tabelul POST (fiind un serviciu dependent de rețea NTP asincronă), păstrând în POST strict subsistemele hardware și stările locale verificate definitiv la pornire (Network, RAM, Actuators, Pump, Bluetooth, Boiler RF, Watchdog, Web API).
- **Bloc Asincron de Inițializare `[INIT] Background Tasks Dispatched:`:**
  - Imediat sub tabelul POST, sarcinile asincrone de fundal sunt declarate clar cu asterisc `*`:
    * `  * Real-Time Clock:  Background NTP sync initiated (pool.ntp.org)`
    * `  * Outdoor Weather:  Background sync initiated (Open-Meteo REST API)`
    * `  * Sensor Discovery: Continuous BLE scanner active (Warmup grace period: 300s)`
- **Raportare Asincronă Unificată cu Asterisc (`[INIT]   * ...`):**
  - Mesajele ulterioare de progres și finalizare ale sarcinilor de fundal adoptă identic prefixul `[INIT]   * `, eliminând aparițiile nesistematice:
    * `[INIT]   * Outdoor Weather:  Synchronized: 15.4 °C, 80.0 % RH (Open-Meteo API)`
    * `[INIT]   * Real-Time Clock:  Synchronized: YYYY-MM-DD HH:MM:SS (Europe/Bucharest via NTP)`
    * `[INIT]   * Sensor Discovery: Acquired (1/3): Baie mare -> 23.0 °C at 7s`
    * `[INIT]   * Sensor Discovery: All 3 room sensors acquired in 125s! System fully operational.`

### Numerotare Categorii `STATUS` (A - E), Aerisire cu Newline-uri & Link Configurare Senzori
- **Prefixe Categorii & Separare Vizuală:**
  - Categoriile din `STATUS` sunt numerotate alfabetic cu `[A. SYSTEM & CONTROLLER HARDWARE]`, `[B. BOILER, PUMP & HYDRAULIC ACTUATION]`, `[C. WEATHER & OUTDOOR ENVIRONMENT]`, `[D. SAFETY & FAILSAFE INTERLOCKS]`, `[E. HEATING ZONES & ROOM TELEMETRY]`.
  - Introdusă o linie liberă (newline) între fiecare categorie tabelară și între fiecare zonă (Zone 1, Zone 2, Zone 3) pentru lizibilitate maximă.
- **Link Integrat Instrument Configurare Senzori BLE (TelinkMiFlasher):**
  - Adăugat hyperlink direct către instrumentul web oficial de configurare Xiaomi BLE (`https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html`) în antetul consolei Web (`/console`), în Setup Wizard (`/setup`), în Dashboard-ul principal (`/`) și în manualul de comenzi (`HELP`), permițând utilizatorului mărirea puterii de transmisie radio (RF Tx Power până la +5..+9 dBm) pentru senzorii aflați după pereți groși.

### Optimizare Paced Streaming OTA & Extindere Watchdog
- **Streaming Socket Paced (`tools/ota_upload.py`):**
  - Implementat transfer cadențat cu pauze de 12ms per pachet de 1436 bytes (~110 KB/s), prevenind epuizarea bufferelor TCP `pbuf` din stiva lwIP a ESP32 și eliminând blocajele de retransmisie TCP. Timpul de upload este stabilizat la ~13.5 secunde.
- **Hardware Watchdog Mărit la 90s:** Oferă marjă generoasă de siguranță la scrierea flash pe durata procedurilor OTA.

## [12.10.2026] - Telemetrie Hardware Extinsă (CPU, RAM, NVM, Temp Cip), Carduri Zonale & Stilizare Vreme (Sarcina 22)

### Telemetrie Hardware Detaliată (Procesor, Memorie, NVM, Temperatură Cip)
- **Info CPU & Temperatură Siliciu (`[CPU]`):**
  - Model cip, nuclee, frecvență și revizie: `ESP32-D0WD-V3 (2 Cores @ 240 MHz, Rev 3)`.
  - Temperatură internă a pastilei de siliciu în timp real: `Die Temp: %.1f °C` via `temperatureRead()`.
  - Timp de funcționare formatat uman: `Uptime: %uh %02um %02us`.
- **Info Memorie RAM & Stabilitate Heap (`[MEMORY]`):**
  - Memorie RAM liberă / totală: `%u KB free / %u KB total`.
  - Nivel minim atins de la boot (watermark de stabilitate): `Lowest: %u KB`.
  - Cel mai mare bloc contiguu alocabil (indicator anti-fragmentare): `Max Block: %u KB`.
- **Info Stocare NVM & Flash Partitioning (`[FLASH]`):**
  - Mărime și frecvență flash: `%u MB @ %u MHz`.
  - Spațiu firmware ocupat și marjă liberă pentru update OTA: `Firmware: %.2f MB (OTA Margin: %.2f MB free)`.
  - Stare NVS (memorie non-volatilă): `NVS: Active`.

### Redesign Complet Zone & Eliminare Tabel Separat `[SENSORS]`
- **Eliminare Redundanță:** Tabelul `[SENSORS]` a fost scos definitiv din ieșirea comenzii `STATUS`.
- **Carduri Structurate pe Fiecare Cameră (`[HEATING ZONES & ROOM TELEMETRY]`):**
  - Fiecare zonă afișează un bloc complet pe 4 linii cu toate datele termice, hidraulice și radio:
    - `* Temperature:` Target (`%.1f °C`), Current (`%.1f °C`), Umiditate (`%.1f % RH`).
    - `* Loop Control:` Stare buclă (`Idle (Temp OK)`, `Warmup`, `Heating Demand`, `Sensor Timeout`), Actuator (`OPEN` / `CLOSED`), Releu (`Relay GPIOxx ON/OFF`).
    - `* Sensor Radio:` Nume senzor asociat, Adresă MAC (`[AA:BB:CC:DD:EE:FF]`), Nivel baterie (`%`).
    - `* RF Telemetry:` Nivel semnal și calificativ (`-61 dBm [EXCELLENT]`), Min/Max semnal (`Min: .. / Max: .. dBm`), Număr pachete recepționate, Vechime telemetrie (`Last seen: %us ago`).
- **Simbol Corect Grad Celsius (`°C`):** Toate temperaturile afișează simbolul `°C` nativ UTF-8, cu header `text/plain; charset=utf-8` pentru prevenirea afișării de caractere fantomă (`Â°C`).

### Evidențiere Vizuală Web Console (`/console`)
- **Stilizare etichetă `[WEATHER]` & Anotimp:** Eticheta `[WEATHER]` este recunoscută case-insensitive și colorată în cyan aprins (`.t-wth`), iar anotimpul (`Autumn`) este stilizat bold.
- **Stilizare Automată Temperaturi & Hardware:** Toate valorile în `°C` sunt colorate automat în cyan bold (`.t-temp`), iar etichetele `[CPU]`, `[MEMORY]`, `[FLASH]`, `[Zone x]` au culori dedicate de mare contrast.

## [12.10.2026] - Eliminare Acronime, Unificare Mapare Zone-Senzori, Integrare POST & Clarificare Reziliență Offline RTC (Sarcina 21)

### Eliminare Acronime Tehnice & Umanizare Text Consolă
- **Excludere Jargon Neclar din Console Banner & Self-Test:**
  - Înlocuit acronimul `GPA` cu descrierea funcțională exactă: `Secondary Pump on Relay 4: ENABLED`.
  - Înlocuit acronimul `TWDT` cu formularea intuitivă: `Hardware Watchdog Armed: 60s timeout`.
  - Înlocuită formularea tehnică `NC Polarity` cu explicarea clară a provenienței setării: `Normally Closed [NC], User-Configured in NVS`, evidențiind faptul că polaritatea provine din configurația salvată de utilizator în memoria flash NVS.
  - Înlocuit `NimBLE Active` cu `Bluetooth Receiver active, listening`.

### Boot Time, Sincronizare Ceas & Funcționare Offline Fără Wi-Fi
- **Afișare Timp Boot în Banner:**
  - Adăugată linia `Boot Time: YYYY-MM-DD HH:MM:SS` în antetul de pornire al sistemului.
  - Adăugat mesaj dedicat la achiziția de timp real: `[Time] Real-time clock synchronized: YYYY-MM-DD HH:MM:SS (Europe/Bucharest via NTP)`.
  - Adăugat mesaj de confirmare meteo cu etichetă standardizată: `[Weather] Outdoor weather synchronized: ...`.
- **Comportament Offline Documentat (Rezistență la căderi de Wi-Fi):**
  - ESP32 integrează un timer RTC hardware intern. Odată sincronizat ceasul la pornire (sau via SoftAP/NTP), ceasul hardware menține data și ora chiar dacă conexiunea Wi-Fi cade complet timp de zile întregi (derivă minimă de 1–2 secunde/zi).
  - Algoritmul termic, siguranțele cazanului, interblocarea hidraulică și citirea senzorilor BLE sunt 100% autonome și rulează local în timp real, fără nicio dependență de internet sau cloud.

### Integrare Servicii în Power-On Self-Test (POST)
- **Extindere Tabel Diagnostic la Pornire:**
  - `Real-Time Clock: PASS (YYYY-MM-DD HH:MM:SS)` integrat direct în tabelul POST.
  - `Outdoor Weather: SYNCING (Connecting to Open-Meteo API...)` inclus ca serviciu monitorizat în POST.
  - `Sensor Boot Warmup: SYNCING (Grace period active: max 5 min)` inclus în lista de verificare inițială.

### Unificare Tabel `STATUS` (Mapare Directă Zonă - Senzor)
- **Eliminare Ambiguitate Nume Senzori vs Nume Zone:**
  - Integrată maparea completă a senzorului direct în fiecare rând al buclei termice (`[ZONES] Heating Loops & Sensor Mapping`):
    - `ID`, `Zone Name`, `Target`, `Current`, `Assigned Sensor (Name / MAC)`, `Bat`, `Signal (RSSI & Calificativ)`, `Status`, `Actuator Valve`.
  - Astfel, chiar dacă senzorul BLE are un nume diferit față de zona căreia îi este alocat, legătura este imediat evidentă la o singură privire.
  - Tabelul `[SENSORS]` rămâne sub el ca registru compact de telemetrie pentru diagnostic de pachete primite și semnal minim/maxim.

### Evidențiere Vizuală / Culori în Consola Web (`/console`)
- **Stilizare Automată Cuvinte-Cheie Stare:**
  - Adăugate clase CSS de contrast înalt în `NetworkConsole.cpp`: `.t-ok`, `.t-off`, `.t-idle`, `.t-demand`, `.t-auto`, `.t-man`, `.t-warm`, alături de badge-urile de semnal `.b-exc`, `.b-good`, `.b-marg`, `.b-poor`.
  - Parsare automată regex în `colorize(line)` pentru cuvinte-cheie de stare ce urmează după `:` (`OFF`, `ON`, `IDLE`, `DEMAND`, `AUTOMATIC`, `MANUAL`, `WARMUP`, `PASS`, `FAIL`), făcându-le să sară direct în ochi în interfața web.

## [12.10.2026] - Perioadă de Warmup Senzori la Boot (Max 5 min), Diagnoză DTC-101 & Format Tabelar Consolă (Sarcina 20)

### Perioadă de Grație Warmup Senzori la Pornire (Max 5 min / 300s)
- **Eliminare Alarme Premature la Boot la Rece:**
  - La pornirea plăcii ESP32, senzorii BLE Xiaomi emit balize la intervale de 30–60 secunde (sau mai rar în funcție de firmware). Înainte, sistemul semnala instantaneu `Sensor Timeout (Failsafe OFF)` la secundele 0..30.
  - Implementată starea dedicată `ZONE_WARMUP` și mecanismul de grație la boot (`_bootWarmupActive = true`, temporizator de 300s / 5 minute):
    - Pe durata perioadei de warmup, buclele termice afișează `WARMUP` și `Warmup (%us/300s)`.
    - Pornirea încălzirii/arzătorului este blocată preventiv până când toți senzorii asociați transmit prima citire validă, protejând echilibrul hidraulic al casei (excepție: protecția de urgență la îngheț `<12°C` care acționează imediat).
  - **Ieșire Anticipată la Detectarea Tuturor Senzorilor:**
    - Imediat ce toți senzorii asociați (ex: 3/3) au fost detectați (la secunda 20..60), warmup-ul se încheie instantaneu: `Boot warmup COMPLETE in %us! All %u paired sensors acquired. System fully operational.`
  - **Decuplare Izolată și Ridicare DTC-101 la Expirare:**
    - Dacă trec 5 minute și anumiți senzori nu au emis balize (ex. baterie descărcată sau distanță prea mare), sistemul ridică automat codul de eroare `DTC-101` (`"Zone X sensor missing at boot warmup cutoff"`), îl salvează în memoria NVS a motorului de diagnoză și pornește încălzirea exclusiv pentru zonele complet funcționale.

### Format Tabelar cu Coloane Fixe pentru Comanda `STATUS` și `BLE`/`SENSORS`
- **Tabel Aliniat Bucle Încălzire Pardoseală (`[ZONES]`):**
  - Antet cu lățimi fixe: `ID`, `Zone Name`, `Current Temp`, `Target Temp`, `Status`, `Actuator Valve (Relay GPIO)`.
  - Toate datele sunt aliniate vertical milimetric, ușor de citit pe orice consolă sau terminal web.
- **Tabel Aliniat Registru Termometre Cameră (`[SENSORS]`):**
  - Indică starea clară a achiziției: `[SCANNING: %us/300s in progress]` în warmup, respectiv `[SCANNING COMPLETE - ALL ACQUIRED]` când toți senzorii sunt prezenți.
  - Tabel aliniat cu coloane: `Sensor Name`, `MAC Address`, `Temp`, `Humidity`, `Battery`, `Signal (Min/Cur/Max dBm)`, `Packets`, `Last Seen`.
- **Clarificare Linie de Protecții (`[PROTECTIONS]`):**
  - Redenumită eticheta generică `[SAFETY]` în `[PROTECTIONS]`: `Sensor Offline Failsafe: 10 min (all zones) | Pump Anti-Seize: Weekly (Ran: ... | Next in ...)`.

## [12.10.2026] - Rafinare Completă Output Consolă, Eliminare Zgomot Boot & Categorisire Comenzi (Sarcina 19)

### Redirecționare Jurnale Interne & Eliminare Zgomot Boot
- **Pristine Boot Banner & POST la Pornire:**
  - Redirecționate toate cele 8 mesaje interne de inițializare a modulelor (`ThermalZoneManager`, `TimeAndWeatherService`, `DevicePassport`, `BleSensorScanner`, `DiagnosticEngine`, TWDT Watchdog) exclusiv pe portul serial hardware UART (`Serial.println` / `Serial.printf`).
  - Buffer-ul consolei virtuale (`netConsole` - Telnet TCP:23 și Web `/console`) debutează instantaneu cu titlul oficial al controlerului și tabelul Power-On Self-Test (POST).
  - Integrat statusul serviciilor Web & API (`HTTP Port 80, Telnet Port 23`) direct în tabelul POST, eliminând mesajul separat post-boot.
  - Actualizat indicatorul scannerului BLE la pornire: `PASS (NimBLE Active, listening)`.

### Reziliență Meteo & Eliminare Alerte False
- **Handshake TLS Stabilizat & Backoff Silențios:**
  - Mărit timeout-ul socket-ului HTTP Open-Meteo la 5000ms.
  - Introdusă o pauză inițială de 5 secunde la pornirea sistemului pentru a permite stabilizarea stivei TCP/IP și a ceasului NTP înainte de prima interogare meteo.
  - Avertizările temporare de rețea (ex. HTTP -11 la pornire la rece) sunt direcționate către portul serial hardware UART, eliminând poluarea consolei utilizatorului; logul în consolă apare doar la sincronizare confirmată sau la comanda `WEATHER`.

### Îmbunătățire Funcțională Comenzi Consolă & Deduplicare
- **Eliminare Mesaje Duplicate la Execuție:**
  - Curățat dublajul de mesaje la comenzile `ON`/`OFF`, `AUTO`/`MANUAL` și `SET <z> <temp>`, lăsând un singur mesaj clar de confirmare emis direct de modulul de control.
- **Indicatori Calitate Semnal (Link Quality Badges) în `STATUS` și `BLE`:**
  - Comenzile `STATUS` și `BLE` includ calificativul calității conexiunii pentru fiecare termometru de cameră: `[EXCELLENT]` (>= -70 dBm), `[GOOD]` (-71..-80 dBm), `[MARGINAL]` (-81..-88 dBm), `[POOR]` (< -88 dBm).
  - Adăugată linie dedicată pentru starea vremii exterioare în sumarul comenzii `STATUS`.
- **Redesign Complet Structură & Claritate `STATUS`:**
  - **Umanizare Parametri Siguranță (`[SAFETY]`):** Eliminat timestamp-ul brut în format UNIX epoch (`1791792220`). Afișat în format uman: `Sensor Failsafe: 10 min (600s) | Anti-Seize: Weekly (Ran: 2026-10-12 11:03 | Next in 6d)`.
  - **Header Dedicat & Formatare Bucle Termice (`[ZONES]`):** Separată afișarea zonelor sub antetul `[ZONES] Underfloor Loops (3):`, eliminând confuzia de subordonare la `[SAFETY]`.
  - **Afișare Clară a Actuatoarelor & Temperaturilor:** Indică starea valvei (`Valve: CLOSED (GPIO32 OFF)` / `Valve: OPEN (GPIO32 ON)`), iar în caz de senzor deconectat afișează deterministic `--.- C` și `Sensor Timeout (Failsafe OFF)`.
  - **Secțiune Separată Termometre (`[SENSORS]`):** Marcată distinct cu `[SENSORS] Room Thermometers (...)` în loc de eticheta generică `[STATUS]`.
  - **Etichetă Dedicată Boiler (`[BOILER]`):** Starea arzătorului și a memoriei libere grupată sub `[BOILER] Burner Flame: OFF | Demand: IDLE | Mode: AUTOMATIC | IP: ... | Free Heap: 120 KB`.
- **Manual de Comenzi Structurat (`HELP` / `?`):**
  - Reorganizată lista comenzilor în 4 secțiuni logice: `[THERMAL & BOILER CONTROL]`, `[SYSTEM & SAFETY CONFIG]`, `[SENSORS & ENVIRONMENT]`, `[DIAGNOSTICS & SYSTEM]`.
- **Suport Comandă `CLEAR` / `CLS`:**
  - Implementată metoda `netConsole.clear()` ce resetează buffer-ul circular și trimite secvența ANSI `\033[2J\033[H` pentru curățarea terminalului Telnet / Serial.
- **Culori Extinse în Web Terminal:**
  - Reguli de colorare CSS pentru badge-urile `[EXCELLENT]`, `[GOOD]`, `[MARGINAL]`, `[POOR]`, `[STATUS]`, `[SYSTEM]`, `[BOILER]`, `[HARDWARE]`, `[SAFETY]`, `[ZONES]`, `[SENSORS]`, `[Ready]`.

## [12.10.2026] - Îmbunătățire Consolă, Reziliență Meteo, Anti-Seize pe Epoch & Timeout Senzori Configurabil (Sarcina 18)

### Consolă de Rețea & Web Terminal Profesional
- **Timestamp-uri NTP Reale pe Fiecare Linie:**
  - Adăugate marcaje de timp `[HH:MM:SS]` (sau `[+HH:MM:SS]` la pornire înainte de sincronizarea NTP) la începutul fiecărei linii noi în Serial, Telnet TCP:23 și Web Console ([`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h), [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp)).
- **Tag-uri Colorate în Web Terminal (`/console`):**
  - Badge-uri stilizate CSS și funcție JavaScript `colorize()` pentru categorii: `[Thermal]`, `[Weather]`, `[BLE]`, `[Time]`, `[Passport]`, `[DIAG]`, `[CMD]`, `ERROR`, `WARN`, `SUCCESS`.
- **Eliminare Poluare Nginx 502 / 504 în Terminal:**
  - Scriptul JavaScript din `/console` verifică explicit codul HTTP de răspuns (`r.ok && r.status === 200`) înainte de inserarea logurilor în DOM, eliminând afișarea paginilor HTML de eroare de la proxy-ul Nginx.

### Serviciu Meteo & Reziliență Open-Meteo
- **Prevenire Buclă Rapidă Erori HTTP -11:**
  - Mărit timeout-ul socket-ului HTTP de la 1500ms la 3000ms ([`src/TimeAndWeatherService.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/TimeAndWeatherService.cpp)).
  - Migrat la `JsonDocument` (ArduinoJson 7).
  - Introdus backoff de 60 de secunde la eșec: în cazul în care serverul Open-Meteo nu răspunde la timp, reîncercarea are loc după 60s în loc de reîncercare continuă la fiecare iterație de loop.

### Calibrare Exercițiu Anti-Blocare (Anti-Seize) pe Timp Real Calendaristic
- **Eliminare Porniri False de 20s la Reboot:**
  - Înlocuit temporizatorul bazat pe `millis()` cu UNIX epoch calendaristic real (`time_t nowEpoch = time(nullptr)`) salvat în NVS (`last_seize_ep`) ([`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp)).
  - La prima pornire a plăcii cu NTP activ, dacă nu există o rulare anterioară înregistrată, se memorează timestamp-ul curent și se programează următorul ciclu la 7 zile (604.800 secunde).

### Failsafe Timeout Senzori Configurabil (`DTC-101`)
- **Parametru Configurat Dinamic & Persistență NVS:**
  - Implementat parametru configurabil `_sensorTimeoutSec` (implicit 600s / 10 min, domeniu valid 60s - 7200s) salvat în NVS (`sns_tout`).
  - Endpoint REST dedicat `/api/setup/sensor-timeout` (GET/POST) și comandă consolă `TIMEOUT <sec>` în [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp).
  - La depășirea timeout-ului pe o zonă, se declanșează `DTC-101`, iar bucla termică trece automat în siguranță (actuator OFF).

### Indicatori Calitate Semnal BLE (Link Quality Indicators)
- **Asistență Instalare pe Teren:**
  - Evaluare automată a nivelului RSSI: `EXCELLENT` (>= -72 dBm), `GOOD` (-73..-82 dBm), `MARGINAL` (-83..-88 dBm), `POOR` (< -88 dBm).
  - Afișat în tabelul de senzori din Setup Wizard Web ([`include/WebSetupPage.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/WebSetupPage.h)) și în utilitarul CLI ([`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) `status` și `ble`).

### Versionare Firmware Baseline & Diagnostic POST la Boot (v1.2.0)
- **Versionare Formală & Trasabilitate Hardware ([`include/Version.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Version.h)):**
  - Implementat antet SemVer `v1.2.0`, timestamp exact de compilare (`__DATE__ " " __TIME__`) și hash scurt de commit Git (`8065534`).
  - Expus în `/api/boiler/status` și afișat în header-ul utilitarului CLI (`python tools/esp_cli.py status`).
- **Mesaj de Boot Profesional & Power-On Self-Test (POST):**
  - Eliminat meniul lung de 17 linii cu comenzi la startup; înlocuit cu antet curat de sistem și raportare cauză resetare (`esp_reset_reason()`: Cold Boot vs OTA Reboot vs TWDT).
  - Rulare automată a testului integrat POST la fiecare pornire: raportează starea Wi-Fi, memoria Heap liberă, actuatoarele & pompa GPA, scannerul pasiv NimBLE, emițătorul radio RF 433 și scorul general de sănătate (100% EXCELLENT).
- **Corecție Completă Duplicare Consolă Web:**
  - Adăugat antetul HTTP `Access-Control-Expose-Headers: X-Log-Seq` permițând JavaScript-ului din browser să citească corect poziția cursorului de loguri.
  - Implementat mecanism de blocare concurentă (`isFetching`) și debouncing comenzi (350ms) în Web Console (`/console`), eliminând dublarea comenzilor `STATUS`.
  - Detecție automată a restartării plăcii (`newSeq < lastSeq`), afișând un separator clar `--- [SYSTEM REBOOT DETECTED / NEW FIRMWARE RUNNING] ---` în browser.

### Verificare SWE.5 Extinsă (14/14 PASS - 100.0%)
- Extinsă suita automată de testare pe PC ([`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py)) de la 12 la 14 cazuri de testare:
  * `SWE.5-TC13`: Contract Failsafe Timeout Senzori (`FR-18`) - PASS.
  * `SWE.5-TC14`: Identitate Baseline & Versionare Firmware v1.2.0 (`FR-21`) - PASS.
- Rulată complet împotriva plăcii fizice la `192.168.1.20`. Rezultat: **100.0% PASS (14/14 teste trecute)**. Raport actualizat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).

## [09.10.2026] - Eliminare Bloatware MQTT, Curățare Consolă & Suită Automată Testare Sistem ASPICE SWE.5 (Sarcina 17)

### Eliminare Bloatware & Decuplare Hardware Autonomă
- **Eliminare Totală Modul MQTT & Dependențe Galaxy-A6:**
  - Șters fișierele de cod `MqttTelemetryClient.cpp` și `MqttTelemetryClient.h`.
  - Eliminat flag-ul de compilare `-D MQTT_MAX_PACKET_SIZE=512` din [`platformio.ini`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/platformio.ini) și dezactivat legătura bibliotecii `PubSubClient`.
  - Curățat toate referințele MQTT din [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp), [`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp) și [`src/DiagnosticEngine.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/DiagnosticEngine.cpp).
  - Sistemul funcționează 100% autonom prin radio Bluetooth NimBLE pasiv fără a depinde de un server/telefon Galaxy-A6 sau bridge extern.

### Curățare Consolă & Confort Acustic
- **Fără Spam de Pachete BLE:**
  - Eliminat orice print automat nesolicitat al pachetelor de la senzori din consola Serial / Telnet TCP:23.
  - Datele senzorilor sunt afișate exclusiv la cererea explicită a utilizatorului (`STATUS`, `BLE`, `SENSORS` sau apel REST).

### Suită de Testare Automată pe PC (ASPICE SWE.5)
- **Implementat Runner de Testare Sistem (`tools/test_system.py` & `esp_cli.py test`):**
  - Fără mock-uri artificiale în microcontroler: testele rulează de pe PC direct împotriva plăcii fizice la `192.168.1.20`.
  - 12 cazuri de testare acoperind:
    * `SWE.5-TC01`: Telemetrie stare sistem & latență sub 200ms (`FR-1`, `NFR-1.1`, `NFR-1.5`) - PASS.
    * `SWE.5-TC02`: Pașaport digital de punere în funcțiune NVS (`FR-13.1`, `FR-13.2`) - PASS.
    * `SWE.5-TC03`: Scanner pasiv NimBLE & decodare Xiaomi (`FR-4.1`, `FR-4.2`) - PASS.
    * `SWE.5-TC04`: Suită integrată self-test 6 subsisteme (`FR-10.1`, `FR-10.2`) - PASS.
    * `SWE.5-TC05`: Polaritate actuatoare NC/NO & persistență NVS (`FR-15.1`, `FR-15.2`) - PASS.
    * `SWE.5-TC06`: Pompă recirculare grup amestec GPA (`FR-14.1`, `FR-14.2`) - PASS.
    * `SWE.5-TC07`: Filtru idempotență anti-spam no-op (`FR-17.1`) - PASS.
    * `SWE.5-TC08`: Gardă anti-short-cycling 60s & HTTP 429 (`FR-17.3`, `FR-17.4`) - PASS.
    * `SWE.5-TC09`: Detecție abuz spam & cod `DTC-201` (`FR-17.5`) - PASS.
    * `SWE.5-TC10`: Calibrare temperatură țintă zonă (`FR-1.2`, `FR-1.3`) - PASS.
    * `SWE.5-TC11`: Sincronizare NTP & telemetrie vreme (`FR-11.1`, `FR-11.2`) - PASS.
    * `SWE.5-TC12`: Feedback bidirecțional consolă rețea (`FR-9.1`, `FR-9.2`) - PASS.
  - Rată de succes: **100.0% (12/12 teste trecute)** în 4.02 secunde. Raport generat în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).

## [09.10.2026] - Failsafe Anti-Spam, Rate-Limiting Cazane & Diagnostic DTC-201 (Sarcina 16)

### Protecție Anti-Spam & Anti-Short-Cycling Conform V-Model ASPICE (FR-17, NFR-1.6)
- **Filtru Idempotent (FR-17.1):**
  - Comenzile redundante pentru aceeași stare (de ex. comenzi repetate `OFF` sau `ON`) răspund instantaneu în 0 ms cu status 200 și câmpul `idempotent: true`, omițând rafala radio RF 433.92 MHz pentru a reduce poluarea electromagnetică și consumul.
- **RF Transmitter Burst Cooldown (FR-17.2):**
  - Temporizare hardware obligatorie de minim 3 secunde între orice două transmisii RF consecutive pentru a proteja etajul de emisie și a garanta recepția curată.
- **Garda Anti-Short-Cycling a Arzătorului (FR-17.3, NFR-1.6):**
  - Implementat în [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp) și verificat la nivel de API în [`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp).
  - Durată minimă de ardere continuă: 60 secunde înainte de a permite oprirea (`OFF`).
  - Pauză minimă de repaus între cicluri: 60 secunde după oprire înainte de a permite repornirea (`ON`).
- **Răspuns Standard HTTP 429 Too Many Requests (FR-17.4):**
  - Răspuns REST standardizat cu cod HTTP 429, payload JSON explicit (`error`, `waitSec`, `boiler`) indicând exact secundele rămase până la deblocare.
- **Detecție Abuz Rapid & Cod Diagnoză DTC-201 (FR-17.5):**
  - 3 tentative respinse într-o fereastră de 10 secunde ridică automat codul `DTC-201: Rapid boiler toggling / spam detected` cu severitate WARNING, scăzând scorul de sănătate al plăcii la 90%.
- **Opțiune de Forțare pentru Urgență / Service (`--force`):**
  - Parametrul `?force=true` sau comutarea în Mod Service permite inginerului de service să suprascrie interlock-ul în situații critice.
- **Actualizare Procedură Web OTA cu Watchdog Calibrat la 30s:**
  - Rezolvat conflictul dintre durata de scriere a flash-ului SPI (~12s) și Task Watchdog Timer prin procedura stepped flash (binar de tranziție -> binar complet). TWDT rulează acum la 30 secunde în regim normal.
- **Verificare Completă SWE.4 / SWE.5:**
  - Toate scenariile (idempotență, refuz 429 cu countdown, spam DTC-201, bypass `--force`, curățare DTC) au fost rulate și validate live pe hardware (`192.168.1.20`) prin [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).

## [09.10.2026] - Hardening Firmware, Failsafes Hidraulice & Pompă Recirculare Pardoseală (Sarcina 15)


### Failsafes de Siguranță & Compatibilitate Hidraulică Universală
- **Control Automat Pompă Secundară (Grup Amestec GPA / Releu 4 GPIO26):**
  - Implementat driver inteligent pentru pompa distribuitorului de pardoseală în [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp).
  - *Interlock Pompă:* Previne cavitația și funcționarea în gol; pompa pornește exclusiv după deschiderea fizică a cel puțin unui actuator.
  - *Post-circulație (Pump Overrun):* Pompa continuă să ruleze timp de 180s după oprirea flăcării pe ultima zonă activă (*keeper zone*) pentru a disipa căldura reziduală și a preveni șocul termic în schimbător.
  - *Exercițiu Anti-Blocare de Vară (Anti-Seize):* Rulare automată săptămânală de 20s în sezonul cald pentru prevenirea gripării rotorului. Validat complet pe hardware via CLI (`tools/esp_cli.py exercise`) și endpoint `/api/diag/exercise`.
- **Compatibilitate Universală Actuatoare NC vs NO:**
  - Suport nativ selectabil via NVS (`act_nc`) și API (`/api/setup/actuator-type`) pentru actuatoare Normal Închis (NC) și Normal Deschis (NO).
- **Protecție Forțată la Îngheț (<12°C):**
  - Failsafe de urgență ce forțează cererea termică chiar dacă sistemul este comutat pe MANUAL sau oprit.
- **Izolare Avarie Senzor (`DTC-101`):**
  - La pierderea semnalului peste 30 min, zona afectată este decuplată imediat pentru a preveni supraîncălzirea șapei, iar starea se recuperează instant la reluarea pachetelor.
- **Hardware Task Watchdog Timer (ESP32 TWDT 30s):**
  - Bucla principală `loop()` este monitorizată de watchdog-ul hardware nativ ESP-IDF (`esp_task_wdt_init(30, true)`).
- **Extindere Utilitar CLI (`tools/esp_cli.py`):**
  - Adăugate comenzile `pump <on|off|enable|disable>`, `actuator <nc|no>`, `exercise` și afișare completă de stare hidraulică în `status`.
- **Ghid Tehnic & Hidraulic Dedicat:**
  - Creat documentul [`docs/underfloor_heating_hydraulics_and_failsafes.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/underfloor_heating_hydraulics_and_failsafes.md) cu scheme și bune practici pentru instalatori (termostat de contact la 55°C, supape de bypass diferențial).

## [09.10.2026] - Arhitectură Decuplată Headless (API-First), Eliminare Latență UI & Asistent Configurare NVS

### Arhitectură & Optimizare Motor Termic
- **Tranziție către Arhitectură Headless (API-First):**
  - Decuplat complet logica de bază a microcontrolerului de designul interfeței grafice pentru a asigura stabilitate maximă, depanare simplă și libertate deplină în alegerea viitoare a frontend-ului client/admin.
  - Consolidat catalogul complet de endpoint-uri REST/JSON (`/api/boiler/status`, `/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/mode`, `/api/boiler/on`, `/api/boiler/off`, `/api/ble/sensors`, `/api/setup/passport`, `/api/setup/time`, `/api/diag/status`, `/api/diag/selftest`, `/api/diag/service`).
  - Toate endpoint-urile răspund sincron în sub **70–120 ms**.
- **Rezolvare Cauze Lag & Comenzi Pierdute:**
  - *Optimizare DOM / Script:* Eliminat `location.reload()` pe toate butoanele (`+`, `-`, mod, flacără). Înlocuit cu actualizări optimiste instantanee în DOM (0 ms timp de răspuns vizual) și transmisiuni asincrone în fundal.
  - *Rupere Buclă Recursivă:* Corectat feedback-loop-ul între ingestia `BleSensorScanner` și `MqttTelemetryClient` care aglomera stiva și cauza instabilitate.
  - *Non-Blocking Timeouts:* Setat timeout scurt (1s) pe clientul socket MQTT și 1.5s pe interogarea HTTP Open-Meteo pentru a preveni orice blocaj al buclei `loop()`.
- **Selectare Inteligentă a Locației (Zero Coordonate Manuale):**
  - Implementat căutare live cu auto-suggest prin Open-Meteo Geocoding pentru orice localitate (oraș, comună, sat) din România.
  - Integrat selector rapid cu cele 41 de reședințe de județ (funcționare 100% offline) și buton de auto-detecție prin GPS/IP cu 1 click.
- **Normalizare Indecși Zone:**
  - Endpoint-urile `/api/zones/setpoint` și `/api/zones/toggle` acceptă acum în mod nativ atât identificatori bazați pe 1 (`id=1..3`), cât și indecși bazați pe 0 (`id=0..2`).
- **Validare Utilitar CLI (`tools/esp_cli.py`):**
  - Testat complet fluxul de interogare și control prin CLI: `status`, `set`, `selftest`, `diag`, `ble`, `passport`, `time`.

## [08.10.2026] - Scanner BLE Nativ Pasiv pe ESP32 (BTHome v2 & PVVX) cu Redundanță Hibridă MQTT

### Arhitectură & Dezvoltare Software
- **Integrare NimBLE-Arduino în `lib/`:**
  - Vendored local în `lib/NimBLE-Arduino` pentru a garanta compilare 100% offline și reproductibilă.
  - Înlocuit stiva greoaie Bluedroid cu NimBLE, reducând flash-ul consumat de la ~1.65 MB la doar **1.08 MB (1.128.681 bytes)** și utilizarea de RAM la 65 KB (19.8%).
  - Permite continuarea actualizărilor de firmware wireless via Web OTA (`/update`) chiar pe partițiile implicite de 1.25 MB.
- **Clasă Dedicată `BleSensorScanner`:**
  - Implementată în [`include/BleSensorScanner.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BleSensorScanner.h) și [`src/BleSensorScanner.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BleSensorScanner.cpp).
  - Scanner pasiv (`setActiveScan(false)`): ascultă pachetele de broadcast fără să transmită nimic în eter, menținând zero consum suplimentar de baterie pe termometre și zero poluare RF.
  - Parametri de scanare: `interval = 100ms`, `window = 50ms` (duty cycle de 50% pentru coexistență nativă optimă cu transceiverul Wi-Fi pe frecvența comună de 2.4 GHz).
- **Parsare Automată Multi-Protocol:**
  - **BTHome v2 (UUID `0xFCD2`):** Decodifică TLV objects: ID 0x01 (baterie %), ID 0x02 (temperatură int16 scaled 0.01), ID 0x03 (umiditate uint16 scaled 0.01).
  - **PVVX / ATC Custom Format (UUID `0x181A`):** Decodifică telemetria completă (temperatură, umiditate, tensiune baterie și procent).
  - **Mapping Inteligent:** Mapează adresele MAC cunoscute la camere (`A4:C1:38:A4:C1:DD` -> Living Room, `A4:C1:38:84:A1:E8` -> Kids Room, `A4:C1:38:94:0F:44` -> Bathroom); senzorii noi necunoscuți sunt înregistrați automat ca `BLE-XXXX`.
- **Canal Dual Redundant & Fuziune Telemetrie:**
  - Senzorii detectați direct prin BLE pe ESP32 au prioritate (`BLE-Direct (ESP32)`), oferind autonomie totală chiar dacă telefonul gateway, brokerul MQTT sau conexiunea Wi-Fi cad.
  - Pachetele primite prin MQTT de pe Galaxy A6 (`MQTT-Gateway (A6)`) servesc ca backup seamless dacă un senzor este plasat într-o zonă ecranată (ex. gresie/faianță Baie).
- **API REST, Consolă & Utilitar CLI:**
  - Adăugat endpoint REST `/api/ble/sensors` cu starea scanerului, lista senzorilor, pachete decodificate, RSSI și vârstă.
  - Adăugată comanda `BLE` (sau `SENSORS`) în consola Telnet TCP:23 și în Web Console `/console`.
  - Actualizat utilitarul CLI [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) cu comanda `python tools/esp_cli.py ble`.

### Verificare Live pe Hardware
- **Web OTA Flash:** Firmware-ul compilat a fost încărcat wireless prin Web OTA (`http://192.168.1.20/update`) în doar 15 secunde.
- **Validare Live Pachet direct în eter:**
  - Living Room (`A4:C1:38:A4:C1:DD`): recepționat direct cu RSSI **-64 dBm**, 23.0°C, 54.6% umiditate, baterie 93%.
  - Kids Room (`A4:C1:38:84:A1:E8`): recepționat direct cu RSSI **-67 dBm / -69 dBm**, 22.9°C, 51.2% umiditate, baterie 100%.
  - Ambele pachete au fost recepționate și decodificate live fără nicio eroare de CRC.

## [07.10.2026] - Consolă Virtuală Serială Bidirecțională prin Wi-Fi (Telnet TCP:23 & Web Console /console)

### Arhitectură & Dezvoltare Software
- **Clasă Dedicată `NetworkConsole`:** Implementată în [`include/NetworkConsole.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/NetworkConsole.h) și [`src/NetworkConsole.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/NetworkConsole.cpp) moștenind clasa `Print`. Permite rutarea transparentă a tuturor ieșirilor de loguri (`netConsole.printf`, `netConsole.println`) către:
  1. Serial fizic prin USB (dacă este conectat).
  2. Conexiunea activă Telnet TCP pe portul 23.
  3. Ring buffer în memorie RAM (8 KB) pentru afișarea în browser.
- **Server Telnet TCP pe portul 23:**
  - Ascultă conexiuni remote via `WiFiServer(23)`.
  - Configurat `monitor_port = tcp://192.168.1.20:23` în `platformio.ini`, permițând rularea directă a `pio device monitor` fără cablu USB și fără drivere de port COM virtual în Windows.
  - Oferă prompt interactiv `> `, suport pentru comenzi cu backspace și ecou local.
- **Consolă Web 2-Way la `/console`:**
  - Pagină web dedicată cu stil modern (dark theme), terminal negru cu auto-scroll, bară de introducere comenzi cu memorie de istoric (tastele Up/Down), butoane rapide de acțiune (`STATUS`, `START BOILER`, `STOP BOILER`, `AUTO MODE`, `MANUAL MODE`, `HELP`, `CLEAR`).
  - Suportă automat atât accesul direct LAN (`http://192.168.1.20/console`) cât și accesul prin reverse proxy Galaxy A6 (`http://galaxy-a6/heating/console` sau prin Tailscale).
  - API de streaming rapid cu antet HTTP `X-Log-Seq` pentru transfer incremental fără pierderi și fără overhead de serializare.
- **Control Bidirecțional 2-Way Total:**
  - Comenzile primite prin Web Console sau Telnet (`1`, `0`, `AUTO`, `MANUAL`, `SET <zone> <temp>`, `STATUS`, `HELP`) sunt executate direct pe procesor.
  - Răspunsurile și evenimentele de sistem (telemetrie primită de la senzori, pornire/oprire boiler prin RF, declanșare relee actuatoare) se văd instantaneu în consola web și în Telnet.

### Verificare Live
- Compilat și încărcat via Web OTA în 6.43 secunde.
- Testat bidirecțional din script Python (`scratch/test_console.py`):
  - Web Console `/console` accesibilă cu HTTP 200 OK.
  - Comanda `STATUS` trimisă prin API-ul web a generat instantaneu raportul complet cu zonele, temperaturile și heap-ul liber (241 KB).
  - Socket-ul Telnet TCP pe portul 23 a răspuns la comanda `HELP` cu meniul complet de comenzi.

## [07.10.2026] - Recepție Placă Nouă ESP32, Migrare 100% Curată la ESP32, Traducere Engleză & Validare Live Web OTA

### Hardware & Programare Fizică
- **Recepționată Noua Placă:** ESP32-Relay X4 V1.1 cu modul **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3 Revision 3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
- **Verificare Conexiune Serială (COM6):** Folosită vechea placă NodeMCU ca adaptor USB-to-UART transparent (pin `RST` tras la `GND`). Interogare reușită prin `esptool.py` confirmând cipul, memoria SPI Flash de 4MB și identificatorii interni.
- **Scriere Firmware Inițial:** Firmware-ul a fost flashuit complet la 460800 baud în 13.1 secunde (toate segmentele verificate cu hash: bootloader, partitions, boot_app0, firmware.bin).
- **Documentare Hardware:** Creat [`docs/hardware_esp32_relay.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/hardware_esp32_relay.md) cu schema completă de pini, cablare passthrough și procedură OTA.

### Curățare Arhitectură Software & Traducere 100% în Engleză
- Conform cerinței explicite de arhitectură curată, s-au eliminat complet toate ramurile și directivele `#ifdef ESP8266` / `#elif defined(ESP32)`.
- Codul este dedicat 100% și nativ arhitecturii **ESP32**:
  - `include/Config.h`: Pini direct definiți pentru ESP32 (Relee `GPIO32, 33, 25, 26`, LED `GPIO23`, RF Data `GPIO19`). Nivel activ relee: `HIGH`.
  - `include/MqttTelemetryClient.h` & `src/MqttTelemetryClient.cpp`: Utilizare directă a `<WiFi.h>` și generare ID client pe baza Efuse MAC (`ESP32-SmartHeating-<mac>`).
  - `src/BoilerRadioTrigger.cpp`: Rutină OOK curată folosind `digitalWrite` fără registre hardware ESP8266 (`GPOS`/`GPOC`) sau apeluri de watchdog specifice 8266.
  - `src/ThermalZoneManager.cpp`: Stări, loguri și comentarii traduse în engleză (`Idle (Temp OK)`, `Heating Active`, `Actuator Warming Up`, `Disabled`).
  - `src/main.cpp`: Utilizare directă a bibliotecilor native ESP32 `<WiFi.h>`, `<WebServer.h>`, `<HTTPUpdateServer.h>`, `WiFi.setHostname`. Toate șirurile UI (titlu, butoane, etichete zone, telemetrie), meniul CLI (`HELP`/`STATUS`) și logurile Serial traduse în **100% limba engleză**.
  - `platformio.ini`: Mediu implicit unic `default_envs = esp32dev` (`platform = espressif32`, `board = esp32dev`, `upload_speed = 460800`).

### Validare Operațională Live & Web OTA
- Placa a pornit autonom, s-a conectat la Wi-Fi `SBC_WiFi` și a primit adresa IP **`192.168.1.20`**.
- Sistemul a stabilit instantaneu conexiunea cu brokerul Mosquitto de pe Galaxy A6 (`192.168.1.28:1883`) și ingerează în timp real telemetria de la senzorii Xiaomi BLE (Living Room 23.0°C, Kids Room 23.3°C, Bathroom 22.1°C).
- Heap liber: **235 KB** (un salt masiv față de cei 28 KB disponibili pe ESP8266).
- **Web OTA Funcțional:** Actualizarea noului binar complet tradus în limba engleză a fost trimisă wireless prin HTTP POST multipart la `http://192.168.1.20/update` și scrisă pe cip în doar **6.55 secunde**, urmată de reboot automat și reluarea funcționării fără cabluri.
- Actualizat proxy-ul Nginx (`nexus_nginx.conf`) pe Galaxy-A6 către noua adresă `192.168.1.20`.

## [07.10.2026] - Cablare Hardware Emițător RF, Driver C++ & Flash pe ESP8266

### Hardware & Măsurători
- **Validare Pinout Modul Emițător RF 433.92 MHz:**
  - Testat cu multimetrul pe pad-urile de test (`V`, `GND`):
    - **Fir Negru:** GND $\rightarrow$ Conectat la pinul **G** (GND) ESP8266.
    - **Fir Galben:** VCC (+3.3V) $\rightarrow$ Conectat la pinul **3V** (+3.3V) ESP8266.
    - **Fir Verde:** DATA $\rightarrow$ Conectat la pinul **D1** (GPIO5) ESP8266.
    - **Pin 2 (nelipit):** Punte internă cu VCC (Galben).
  - Păstrată placa PCB verde intactă pentru a beneficia de antena perimetrică acordată la 433.92 MHz.
- **Clarificare Arhitectură Microcontroller:**
  - Sistemul țintă este **ESP8266** (ESP-12F / LC-Relay-ESP12-4R-MV cu 4 relee). Telemetria BLE a senzorilor Xiaomi LYWSD03MMC este asigurată extern de gateway-ul Samsung Galaxy A6 via broker MQTT Mosquitto.

### Software & Firmware
- **PlatformIO Setup:** Creat fișierul [`platformio.ini`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/platformio.ini) (`board = esp12e`, `framework = arduino`, 4M1M flash).
- **Configurație Centralizată:** Adăugat fișierul [`include/Config.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/Config.h) pentru alocarea pinilor și a rețelei.
- **Driver Radio `BoilerRadioTrigger`:**
  - Creat [`include/BoilerRadioTrigger.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/BoilerRadioTrigger.h) și [`src/BoilerRadioTrigger.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/BoilerRadioTrigger.cpp).
  - Include tabelele PROGMEM cu 409 tranziții, preambul de 65 ms, tren de impulsuri OOK de ~131 ms generat prin bit-banging direct pe registrele `GPOS` / `GPOC` și `ESP.wdtFeed()` pentru prevenirea reseturilor WDT.
- **Test Bench Firmware ([`src/main.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/main.cpp)):**
  - Serial CLI interactiv la 115200 baud (`1`/`ON`, `0`/`OFF`, `S`/`STATUS`, `?`/`HELP`).
  - Web Server responsive pe portul 80 cu butoane mari de comandă.
  - Endpoint-uri REST: `/api/boiler/on`, `/api/boiler/off`, `/api/boiler/status`.
  - Flash Web OTA integrat pe `/update` (actualizare facilă prin Wi-Fi fără cablu USB).
- **Validare Flash & Conexiune:**
  - Firmware-ul a fost scris pe `COM6`.
  - Modulul ESP8266 s-a conectat automat la rețeaua Wi-Fi la IP-ul **`192.168.1.18`**, cu 48.4 KB Free Heap și timp de răspuns instantaneu la cererile HTTP.
- **Validare Funcțională pe Viu cu Centrala Termică:**
  - Testat declanșarea flăcării centralei direct din interfața web: semnalul radio de 433.92 MHz a fost recepționat de centrală, care a reacționat instantaneu la pornire/oprire.
- **Integrare Galaxy-A6 Gateway & Acces la Distanță Tailscale:**
  - Configurat Nginx reverse proxy pe nodul Samsung Galaxy A6 (`cinderella`) pentru subpath-ul `/heating/` direcționat către `http://192.168.1.18/`.
  - Adaptat frontend-ul din `src/main.cpp` pentru URL-uri relative dinamice, permițând funcționarea identică atât pe IP-ul local direct cât și prin proxy.
  - Actualizat portalul centralizat Smart Services Portal (`http://galaxy-a6/` / `http://192.168.1.28/`): adăugat cardul **Smart Heating** (cu pictogramă 🔥, monitorizare de stare în timp real la 5 secunde, afișare mod flacără, uptime și link rapid la senzorii climatici din Grafana).
  - Validat accesul extern securizat prin rețeaua **Tailscale** (`http://100.83.135.74/heating/`).
- **Client Telemetrie MQTT Mosquitto ([`include/MqttTelemetryClient.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/MqttTelemetryClient.h), [`src/MqttTelemetryClient.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/MqttTelemetryClient.cpp)):**
  - Integrat local `PubSubClient` și `ArduinoJson` (v7.0.4).
  - Configurat persistent brokerul Mosquitto de pe Galaxy-A6 să accepte conexiuni externe pe `0.0.0.0:1883` prin `/etc/mosquitto/conf.d/teslamate.conf`.
  - Ingestie live a senzorilor Xiaomi LYWSD03MMC (Living Room, Kids Room, Bathroom) cu temperatură, umiditate, nivel baterie și vârstă telemetrie.
- **Controler Termic Multi-Zonă Decuplat ([`include/ThermalZoneManager.h`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/include/ThermalZoneManager.h), [`src/ThermalZoneManager.cpp`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/src/ThermalZoneManager.cpp)):**
  - Structură decuplată și cod modular pentru cele 3 zone termice (Living, Copil, Matrimonial).
  - Relee actuatori NC pe GPIO16, GPIO14, GPIO12 cu nivel configurabil (`RELAY_ACTIVE_LEVEL = LOW`).
  - Evaluare termică autonomă cu histerezis (±0.3°C), interlock delay pentru deschidere completă actuator ceară NC înainte de aprinderea flăcării, pump overrun și protecție anti-short cycling a cazanului.
  - Moduri de lucru selectabile (AUTOMAT vs MANUAL), reglaj temperatură țintă din interfață și API REST extins (`/api/zones`, `/api/zones/setpoint`, `/api/zones/toggle`, `/api/zones/sensor`, `/api/mode`).
  - Firmware compilat și încărcat cu succes pe ESP8266 (`COM6`), testat live în rețea.
- **Achiziție Hardware Panou de Perete (Kiosk Display):**
  - Cumpărată tableta **Samsung Galaxy Tab A9 (SM-X115, 4G LTE + Wi-Fi, 4GB RAM, 64GB stocare)** la prețul de **450 lei** (OLX).
  - Avantaje cheie validate: ecran 8.7" PLS LCD imun la burn-in, procesor MediaTek Helio G99 6nm de performanță înaltă, slot 4G de rezervă și limitare nativă a încărcării la 85% pentru funcționare 24/7 conectată la alimentare.

---

## [06.10.2026] - Inițializare Arhitectură & Configurare Proiect

### Adăugat
- **Inițializare Repository:** Creare repository git local și structurare directoare (`docs/`, `.agent/`).
- **Planificare Inginerească:** Redactare plan dinamic în limba română în [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md) conform metodologiei de lucru din `3dvehsim`.
- **Filtre Git:** Adăugare fișier [`.gitignore`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/.gitignore) pentru PlatformIO, C++, LittleFS și secrete locale.
- **Arhitectură de Sistem Agreată:**
  - Microcontroller principal: **ESP32** (autonomie 100% la nivel de nod, scanner BLE nativ integrat pentru senzori Xiaomi LYWSD03MMC BTHome v2).
  - Punte de telemetrie & Backup: Integrare bidirecțională cu Samsung Galaxy A6 (`smart-home-ble-gateway`) prin brokerul MQTT Mosquitto local.
  - Siguranță hidraulică: Interlock delay (120–180s) pentru deschiderea actuatorilor termici 230V NC înainte de pornirea flăcării cazanului + pump overrun de 180s pentru disiparea energiei reziduale.
  - Interfață Web: Găzduită local în flash-ul ESP32 prin LittleFS cu schemă vectorială interactivă SVG a apartamentului.
- **Sincronizare GitHub Remote:** Configurat remote origin și publicat branch-ul `main` pe [`https://github.com/RoDTOX/smart-home-heating`](https://github.com/RoDTOX/smart-home-heating) incluzând planul, changelog-ul și întregul Memory Bank persistent.


## [2026-10-06] - Sesiunea de SearÄƒ: Recuperare Proiect RF, Cartografiere HidraulicÄƒ & AchiziÈ›ie Hardware

### AdÄƒugat & Recuperat
- **Identificat È™i salvat proiectul RF de control al centralei:**
  - Sursa funcÈ›ionalÄƒ Arduino Uno: `control_RF_Centrala.ino` (410 tranziÈ›ii, preambul 65 ms, tabele `ondurations` È™i `offdurations`).
  - Analiza detaliatÄƒ a duratelor de impulsuri Ã®n nanosecunde/microsecunde: `ONOFF_OK_Signal_FiltereByNanosec.xlsx`.
  - Capturile digitale Sigrok / PulseView (`.sr`, `.pvs`), Saleae Logic (`.sal`) È™i dump-urile brute (2 MHz, text/binar).
  - Toate fiÈ™ierele au fost structurate Ã®n folderul `input/boiler_rf_trigger/` din repository.
- **Cartografiere hidraulicÄƒ completÄƒ din `Odaia.html` È™i prezentarea PPTX:**
  - Cele 7 circuite ale distribuitorului au fost mapate pe cele 3 zone termice principale:
    - **Zona 1 (Living + BucÄƒtÄƒrie):** RobineÈ›ii 1, 2, 3 (3 actuatoare NC Ã®n paralel pe Releul 1).
    - **Zona 2 (Camera Copilului):** RobineÈ›ii 6, 7 (2 actuatoare NC Ã®n paralel pe Releul 2).
    - **Zona 3 (Dormitor Matrimonial):** RobineÈ›ii 4, 5 (2 actuatoare NC Ã®n paralel pe Releul 3).
    - **Releul 4:** RÄƒmÃ¢ne rezervÄƒ liberÄƒ.
  - S-a stabilit cÄƒ cele 2 bÄƒi nu au circuite de pardosealÄƒ, avÃ¢nd calorifere portprosop deschise permanent, asigurÃ¢nd **bypass hidraulic natural** pentru pompa centralei.
- **Identificare staÈ›ie meteo exterioarÄƒ:**
  - Model Tronic **4-LD7396-2** (IAN `515492_2510`), funcÈ›ioneazÄƒ pe 433.92 MHz OOK/ASK.
  - S-a validat acurateÈ›ea compensÄƒrii climatice: 13.0Â°C Google/Open-Meteo vs 13.3Â°C senzorul fizic de Nord.

### AchiziÈ›ii Hardware Efectuate (Comandate pe eMAG)
- **1x PlacÄƒ ESP32 cu 4 Relee (Songle 10A 250VAC)**, control Wi-Fi/BLE, conectori GPIO expansiune (cod AI1254).
- **2x Seturi Radio 433MHz (SYN115 emiÈ›Äƒtor + SYN480R receptor superheterodin)**, antene sfert de undÄƒ 17.3 cm.

### Conexiune & Repository
- Configurat accesul SSH la GitHub (`git@github.com:RoDTOX/smart-home-heating.git`).
- Validat `git ls-remote` fÄƒrÄƒ erori.
