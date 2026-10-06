# Sumar Sesiune de Lucru - 06 Octombrie 2026

## 1. Ce am realizat astÄƒzi

### A. Recuperare date RF & AnalizÄƒ semnal centralÄƒ
* Am localizat È™i recuperat proiectul funcÈ›ional Arduino: `control_RF_Centrala.ino`.
* Am recuperat fiÈ™ierele de analizÄƒ È™i filtrare timpi impulsuri: `ONOFF_OK_Signal_FiltereByNanosec.xlsx`.
* Am salvat capturile originale de analizor logic (PulseView/Sigrok `.sr`, `.pvs`, Saleae `.sal`, exporturi text la 2 MHz).
* Tot setul de date se aflÄƒ Ã®n siguranÈ›Äƒ Ã®n repository la `input/boiler_rf_trigger/`.

### B. AnalizÄƒ HidraulicÄƒ & Cartografiere Apartament
* Din `Odaia.html` È™i prezentarea PPTX (`Schema circuite incalzire pardoseala.pptx`), am extras È™i mapat cele 7 circuite ale distribuitorului pe cele 3 zone termice:
  * **Zona 1 (Living + BucÄƒtÄƒrie + Dining + Vestibul):** RobineÈ›ii 1, 2, 3 -> **Releul 1** (3 actuatoare NC legate Ã®n paralel).
  * **Zona 2 (Camera Copilului / Dormitor mijloc):** RobineÈ›ii 6, 7 -> **Releul 2** (2 actuatoare NC legate Ã®n paralel).
  * **Zona 3 (Dormitor Matrimonial):** RobineÈ›ii 4, 5 -> **Releul 3** (2 actuatoare NC legate Ã®n paralel).
  * **Releul 4:** RÄƒmÃ¢ne rezervÄƒ liberÄƒ.
* Cele 2 bÄƒi au calorifere portprosop pe perete (fÄƒrÄƒ pardosealÄƒ), lÄƒsate deschise hidraulic, asigurÃ¢nd **bypass hidraulic natural** pentru pompa centralei.

### C. Compensare ClimaticÄƒ (TemperaturÄƒ ExterioarÄƒ)
* Am identificat staÈ›ia meteo existentÄƒ: **Tronic 4-LD7396-2** (IAN `515492_2510`), funcÈ›ionÃ¢nd pe 433.92 MHz.
* Am validat cÄƒ citirea Open-Meteo REST API (13.0Â°C) este la doar 0.3Â°C de senzorul fizic de pe peretele de Nord (13.3Â°C). Sistemul va folosi arhitecturÄƒ hibridÄƒ: radio 433MHz primar + fallback Open-Meteo Wi-Fi.

### D. Hardware Comandat (eMAG)
1. **1x PlacÄƒ ESP32 cu 4 Relee (Songle 10A 250VAC)** (cod AI1254).
2. **2x Seturi Radio 433.92 MHz (SYN115 emiÈ›Äƒtor PLL + SYN480R receptor superheterodin)**.
   * Antene de sfert de undÄƒ din fir de cupru de 17.3 cm lipite pe pad-urile `ANT`.

---

## 2. Punct de reluare pentru mÃ¢ine (pe celÄƒlalt PC)

1. `git pull` pe celÄƒlalt PC pentru a avea totul sincronizat.
2. Implementarea driverului software `BoilerRadioTrigger` pentru ESP32 (folosind perifericul hardware RMT sau timer pe 64-biÈ›i).
3. Construirea motorului termic `ThermalZoneManager` (histerezis, interlock delay 120-180s, pump overrun).
4. Extragerea contururilor SVG ale celor 3 zone din `input/Odaia.html` pentru dashboard-ul Web LittleFS.
