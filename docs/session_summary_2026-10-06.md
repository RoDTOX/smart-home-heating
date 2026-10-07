# Sumar Sesiune de Lucru - 06 Octombrie 2026

## 1. Ce am realizat astăzi

### A. Recuperare date RF & Analiză semnal centrală
* Am localizat și recuperat proiectul funcțional Arduino: `control_RF_Centrala.ino`.
* Am recuperat fișierele de analiză și filtrare timpi impulsuri: `ONOFF_OK_Signal_FiltereByNanosec.xlsx`.
* Am salvat capturile originale de analizor logic (PulseView/Sigrok `.sr`, `.pvs`, Saleae `.sal`, exporturi text la 2 MHz).
* Tot setul de date se află în siguranță în repository la `input/boiler_rf_trigger/`.

### B. Analiză Hidraulică & Cartografiere Apartament
* Din `Odaia.html` și prezentarea PPTX (`Schema circuite incalzire pardoseala.pptx`), am extras și mapat cele 7 circuite ale distribuitorului pe cele 3 zone termice:
  * **Zona 1 (Living + Bucătărie + Dining + Vestibul):** Robineții 1, 2, 3 -> **Releul 1** (3 actuatoare NC legate în paralel).
  * **Zona 2 (Camera Copilului / Dormitor mijloc):** Robineții 6, 7 -> **Releul 2** (2 actuatoare NC legate în paralel).
  * **Zona 3 (Dormitor Matrimonial):** Robineții 4, 5 -> **Releul 3** (2 actuatoare NC legate în paralel).
  * **Releul 4:** Rămâne rezervă liberă.
* Cele 2 băi au calorifere portprosop pe perete (fără pardoseală), lăsate deschise hidraulic, asigurând **bypass hidraulic natural** pentru pompa centralei.

### C. Compensare Climatică (Temperatură Exterioară)
* Am identificat stația meteo existentă: **Tronic 4-LD7396-2** (IAN `515492_2510`), funcționând pe 433.92 MHz.
* Am validat că citirea Open-Meteo REST API (13.0°C) este la doar 0.3°C de senzorul fizic de pe peretele de Nord (13.3°C). Sistemul va folosi arhitectură hibridă: radio 433MHz primar + fallback Open-Meteo Wi-Fi.

### D. Hardware Comandat (eMAG)
1. **1x Placă ESP32 cu 4 Relee (Songle 10A 250VAC)** (cod AI1254).
2. **2x Seturi Radio 433.92 MHz (SYN115 emițător PLL + SYN480R receptor superheterodin)**.
   * Antene de sfert de undă din fir de cupru de 17.3 cm lipite pe pad-urile `ANT`.

---

## 2. Punct de reluare (Sarcini Imediate)

1. Implementarea driverului software `BoilerRadioTrigger` pentru ESP32 (folosind perifericul hardware RMT sau timer pe 64-biți).
2. Construirea motorului termic `ThermalZoneManager` (histerezis, interlock delay 120-180s, pump overrun).
3. Extragerea contururilor SVG ale celor 3 zone din `input/Odaia.html` pentru dashboard-ul Web LittleFS.
