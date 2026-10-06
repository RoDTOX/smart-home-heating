# Boiler RF Trigger - Captures, Analiza & Cod de Referinta

Acest director contine capturile realizate cu analizorul logic / digital sniffer, fisierele de analiza si codul functional de referinta pentru comanda radio (RF 433MHz) a centralei termice (ON/OFF).

---

## Directoare si Fisiere

```text
boiler_rf_trigger/
|-- README.md
|-- arduino_reference/
|   `-- control_RF_Centrala.ino         # Codul functional C++ (Arduino Uno / ATmega328P)
|-- analysis/
|   `-- ONOFF_OK_Signal_FiltereByNanosec.xlsx  # Calculul si filtrarea duratelor impulsurilor (ns / us)
`-- captures/
    |-- ON_OFF_200MSamples_200kHz_best.sr      # Captura Sigrok / PulseView optima (200 kS/s)
    |-- ON_CLEAN.sr                            # Captura curata semnal ON
    |-- ON_thermo.sr                           # Captura semnal emis de termostatul original
    |-- ONiGuess.sr / ONNNNIHope.sr            # Capturi intermediare de calibrare
    |-- ON_OFF.pvs                             # Sesiune PulseView (configuratie vizualizare)
    |-- Session 0.sal                          # Sesiune Saleae Logic v2
    |-- 01logicbitsonoff.txt                   # Export text al starilor logice (libsigrok, 2 MHz)
    |-- hexdigitslogicdata.txt                 # Export hexazecimal al semnalului
    `-- rawbinaryonoff                         # Dump binar brut al capturii
```

---

## Detalii Protocol & Semnal RF

* **Modulatie:** OOK / ASK (On-Off Keying / Amplitude Shift Keying) la 433 MHz.
* **Numar tranzitii:** 410 pasi logici (`NUM_STEPS 410`).
* **Preambul:** Puls lung de sincronizare (~65.000 us = 65 ms).
* **Durate impulsuri de date:** Variaza intre ~150 us si ~700 us (frecvent ~200 us, ~450 us, ~500 us, ~650 us, ~700 us).
* **Implementare referinta (`control_RF_Centrala.ino`):**
  * Salveaza tabelele de tranzitie direct in memoria Flash (`PROGMEM`) pentru a economisi memoria RAM:
    * `offstates[410]` - succesiunea starilor HIGH (1) si LOW (0).
    * `ondurations[410]` - duratele in microsecunde pentru comanda de pornire (ON).
    * `offdurations[410]` - duratele in microsecunde pentru comanda de oprire (OFF).
  * Comanda pinul digital prin registre (`PORTB |= (1 << PORTB0)`) cu intreruperile dezactivate (`noInterrupts()`) si `delayMicroseconds()`.

---

## Portare pe ESP32 (`BoilerRadioTrigger`)

Pentru integrarea in firmware-ul ESP32 al proiectului `smart-home-heating`:
1. **Periferic RMT (Recomandat):** ESP32 dispune de perifericul dedicat **RMT** (Remote Control), conceput exact pentru generarea hardware de semnale pre-modulate fara a bloca nucleul CPU sau task-urile FreeRTOS.
2. **Alternativa Timer Hardware:** Utilizarea `esp_timer` sau a unui timer hardware pe 64-biti.
3. **GPIO Output:** Semnalul logic este trimis direct pe pinul DATA al unui modul de transmisie RF 433MHz (ex. FS1000A, STX882 sau similar).
