# Changelog: Sistem Inteligent de Încălzire (Smart Heating) - Octombrie 2026

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan_oct2026.md).

---

## [06.10.2026] - Inițializare Arhitectură & Configurare Proiect

### Adăugat
- **Inițializare Repository:** Creare repository git local și structurare directoare (`docs/`, `.agent/`).
- **Planificare Inginerească:** Redactare plan dinamic în limba română în [`docs/plan_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan_oct2026.md) conform metodologiei de lucru din `3dvehsim`.
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
