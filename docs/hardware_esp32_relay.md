# Documentație Hardware: Placă Relee ESP32 (ESP32-Relay X4 V1.1)

Acest document conține toate specificațiile tehnice, pinout-ul, schema de conectare și procedura de programare pentru placa de distribuție a încălzirii, astfel încât proiectul să poată fi replicat sau reconfigurat oricând, pe orice calculator, chiar și după ani de zile.

---

## 1. Specificații Modul & Microcontroler

- **Model Placă:** ESP32-Relay X4 V1.1 (alimentare AC 90-250V sau DC 7-30V / 5V)
- **Modul montat:** DOIT WiFi MODEL ESP32-32E N4
- **Cip procesor:** ESP32-D0WD-V3 (Revision 3)
- **Arhitectură:** Xtensa Dual-Core 32-bit, frecvență 240 MHz
- **Memorie Flash SPI:** 4 MB (4096 KB) - Producător: `0xc4`, Device: `0x6016`
- **Memorie RAM:** 320 KB SRAM intern
- **Adresă MAC unică hardware:** `20:50:0d:1a:72:14`
- **Conectivitate:** Wi-Fi 802.11 b/g/n (2.4 GHz) + Bluetooth 4.2 BR/EDR & BLE (hardware nativ)

---

## 2. Alocare Pini Hardware (Pinout)

| Funcție Hardware | Pin ESP32 | Nivel Logic Activ | Note / Comportament |
|---|---|---|---|
| **Releu 1 (Zona 1: Living + Bucătărie + Hol)** | `GPIO32` | `HIGH` (Activ) | Comandă actuatori electrotermici NC (3 bucle pardoseală) |
| **Releu 2 (Zona 2: Camera Copilului)** | `GPIO33` | `HIGH` (Activ) | Comandă actuator electrotermic NC (1 buclă pardoseală) |
| **Releu 3 (Zona 3: Dormitor Matrimonial)** | `GPIO25` | `HIGH` (Activ) | Comandă actuator electrotermic NC (1 buclă pardoseală) |
| **Releu 4 (Rezervă / Pompă / Bypass)** | `GPIO26` | `HIGH` (Activ) | Contact liber de potențial |
| **LED de Stare (Integrat)** | `GPIO23` | `LOW` (Aprins) | Clipiri rapide la comenzi, stins în repaus când Wi-Fi e conectat |
| **Emițător Radio Centrală 433.92 MHz (DATA)** | `GPIO19` | `HIGH` / `LOW` | Modulare OOK (tren 409 tranziții, 65ms preambul, 131ms date) |
| **UART TX (Transmisie Serial)** | `GPIO1` | — | Conectat la header-ul de programare |
| **UART RX (Recepție Serial)** | `GPIO3` | — | Conectat la header-ul de programare |
| **Buton / Pin Bootloader (Download)** | `GPIO0` | `LOW` (Apasat) | Buton marcat `IO0` pe placă |
| **Buton Reset** | `EN` / `RST` | `LOW` (Apasat) | Buton marcat `RST` pe placă |

---

## 3. Procedura de Programare Inițială prin Cablu (USB-UART Bridge)

Dacă nu ai un programator dedicat USB-TTL (CP2102/FT232), poți folosi orice placă **NodeMCU ESP8266** ca adaptor USB-to-UART transparent.

### A. Cablare între NodeMCU (Bridge) și Placa Relee ESP32:
1. **Pe placa veche NodeMCU:**
   - Pune un fir direct între pinul **`RST`** și **`GND`** (aceasta blochează cipul vechi ESP8266 în reset permanent, lăsând cipul USB CH340 conectat direct la pini).
2. **Legături de semnal:**
   - NodeMCU **`RX`** $\rightarrow$ Placă Relee **`RX`** (pe NodeMCU, pinul RX este de fapt ieșirea TX a cipului USB CH340).
   - NodeMCU **`TX`** $\rightarrow$ Placă Relee **`TX`** (pe NodeMCU, pinul TX este de fapt intrarea RX a cipului USB CH340).
   - NodeMCU **`GND`** $\rightarrow$ Placă Relee **`GND`** (masă comună obligatorie).
3. **Alimentare Placă Relee:**
   - 5V DC conectat la pinii de alimentare ai plăcii de relee.

### B. Intrarea în Modul de Scriere (ROM Bootloader / Download Mode):
1. **Ține apăsat butonul `IO0`** (sau pune un fir între pinul `IO0` și `GND`).
2. În timp ce ții apăsat `IO0`, **apasă o dată scurt pe `RST`** pe placa de relee.
3. Eliberează `IO0`.
4. **Validare vizuală/auditivă:** Releele se opresc complet din orice zgomot (demo-ul de fabrică tace). ESP32 este în mod de scriere.

### C. Comandă de Scriere din Terminal (PlatformIO):
```bash
pio run -e esp32dev --target upload
```

---

## 4. Actualizări Wireless Fără Fir (Web OTA)

Odată ce firmware-ul a fost încărcat o singură dată prin cablu, **NU mai este nevoie niciodată de conexiuni fizice, fire sau butoane**.

### Procedură OTA:
1. Deschide browserul pe orice dispozitiv conectat la rețea:
   ```text
   http://<IP_ESP32>/update
   ```
2. Autentificare:
   - **Utilizator:** `admin`
   - **Parolă:** `admin123`
3. Selectează fișierul compilat:
   ```text
   .pio/build/esp32dev/firmware.bin
   ```
4. Apasă **Update Firmware**. Placa se rescrie prin Wi-Fi și se repornește automat în 15 secunde.

---

## 5. Cerințe de Rețea & Alocare IP

Pentru o funcționare stabilă și acces facil la Web UI și REST API, se recomandă alocarea unei rezervări statice DHCP în routerul local:

| Nume Dispozitiv | Frecvență Wi-Fi | Adresă IP Implicită | Rol / Descriere |
| :--- | :---: | :---: | :--- |
| **`SmartHeating-ESP32`** | 2.4 GHz (802.11 b/g/n) | **`192.168.1.20`** | **Controler Centrală & 4 Relee Pardoseală** |

- **Porturi utilizate:**
  - HTTP `80` (Web UI & REST API)
  - Telnet `23` (Consolă interactivă comenzi)
  - NTP `123` (UDP, sincronizare ceas de rețea)
  - HTTP/HTTPS `80/443` (Interogare API Open-Meteo)

