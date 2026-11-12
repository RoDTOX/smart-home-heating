const LEVELS = ["day", "night", "away"], LEVEL_RO = {day: "zi", night: "noapte", away: "plecat"};
// FR-8.4: zone state code -> page text
const STATE_RO = {FROST: "Protecție îngheț", DISABLED: "Zonă oprită", FAULT: "Senzor pierdut – încălzește de rezervă",
  ACQUIRING: "Așteaptă senzorul", WINDOW: "Fereastră deschisă", HEATING: "Încălzește", IDLE: "Temperatură atinsă"};
// FR-10.4: check id -> rule in words
const CHECK_RO = {
  wifi: "Condiție: semnal Wi-Fi optim (prag minim ≥ -82 dBm)",
  heap: "Condiție: memorie RAM liberă suficientă (prag minim ≥ 60 KB)",
  ble: "Condiție: scanare pasivă Bluetooth activă pentru senzori",
  sensors: "Condiție: fiecare zonă activă primește citiri în sensorTimeout",
  time: "Condiție: ceas de sistem sincronizat via NTP",
  weather: "Condiție: prognoză meteo actuală (< 6 ore)",
  radio: "Canal simplex 433 MHz (fără canal de retur din cazan)"
};
// FR-10.2: DTC code -> friendly title & cause
const DTC_INFO = {
  "101": {title: "Senzor pierdut", cause: "Nicio citire în sensorTimeout. Zona a trecut pe încălzire de siguranță (fallback duty)."},
  "102": {title: "Baterie descărcată", cause: "Nivel baterie senzor sub 20%. Înlocuiți bateria CR2032."},
  "103": {title: "Citire coruptă", cause: "Temperatura sau umiditatea citită depășește limitele plauzibile."},
  "104": {title: "Senzor suspect", cause: "3 citiri aberante consecutive eliminate în decurs de 1 oră."},
  "105": {title: "Semnal BLE slab", cause: "Semnalul senzorului BLE a scăzut sub -90 dBm timp de peste 1 oră."},
  "201": {title: "Ciclare rapidă cazan", cause: "Comenzi de comutare prea frecvente. A intervenit protecția anti-short-cycling."},
  "202": {title: "Zonă fără efect termic", cause: "Cerere termică activă dar temperatura nu crește (posibil vană blocată sau circuit închis)."},
  "203": {title: "Supraîncălzire zonă", cause: "Temperatura zonei crește peste țintă deși vana a fost comandată închisă."},
  "204": {title: "Cazanul nu încălzește", cause: "Nicio zonă și niciun senzor de tur nu reacționează la comanda de pornire a cazanului."},
  "205": {title: "Debit peste limită", cause: "Debitul agentului termic depășește limita maximă de siguranță timp de > 2 min."},
  "206": {title: "Defecțiune debitmetru", cause: "Senzor de debit sau retur lipsă sau cu valori implauzibile."},
  "207": {title: "Link cazan fără fail-safe", cause: "Semnalul de heartbeat către cazan este dezactivat."},
  "208": {title: "Eroare modul extensie", cause: "Modulul de extensie ieșiri relee nu răspunde comenzilor I2C."},
  "301": {title: "Duty învățat resetat", cause: "Valoarea istorică de încălzire a depășit limitele și a fost reinițializată."},
  "302": {title: "Ora nesincronizată", cause: "Ora de rețea (NTP) nu a putut fi sincronizată."},
  "303": {title: "Rollback OTA eșuat", cause: "Noua imagine de firmware nu a trecut verificările de siguranță și s-a revenit la versiunea anterioară."},
  "304": {title: "Autotestare eșuată", cause: "Unul din pașii de autotestare la pornire a raportat o eroare hardware."},
  "401": {title: "Semnal Wi-Fi slab", cause: "Semnalul Wi-Fi a scăzut sub -82 dBm sau conexiunea a fost pierdută temporar."},
  "402": {title: "Memorie liberă redusă", cause: "Memoria RAM liberă a scăzut temporar sub 60 KB."},
  "403": {title: "Credențiale implicite active", cause: "Dispozitivul rulează cu utilizatorul sau parola implicită din fabrică. Schimbați credențialele în configurare."}
};

const RESET_REASONS = {
  1: {title: "Pornire la rece (Power-on Reset)", desc: "Alimentare deconectată și reconectată la rețea / panou."},
  3: {title: "Repornire Software / OTA (Restaurare Sistem)", desc: "Sistem restaurat cu succes după finalizarea actualizării firmware OTA sau comandă software de restart (downtime anterior ~1-2 min)."},
  4: {title: "Panică Nucleu (Crash / Exception)", desc: "Eroare critică în execuția firmware-ului ESP32."},
  5: {title: "Watchdog Întrerupere (IWDT)", desc: "Buclă blocată sau rutină de întrerupere fără cedare."},
  6: {title: "Task Watchdog (TWDT)", desc: "Task-ul de control sau bucla principală nu a răspuns în limita de 90 secunde."},
  7: {title: "Watchdog Software", desc: "Reset declanșat de cronometrul intern de veghe."},
  8: {title: "Ieșire din Deep Sleep", desc: "Trezire din mod de consum redus."},
  9: {title: "Cădere tensiune (Brownout)", desc: "Tensiunea pe linia de alimentare de 3.3V/5V a scăzut sub pragul sigur."},
  10: {title: "Reset extern SDIO", desc: "Semnal extern de reset pe pinii de comunicație."}
};
const MOMENT_LABELS = [
  "P1: Trezire",
  "P2: Plecat / Zi",
  "P3: Seară",
  "P4: Somn / Noapte"
];
const DAY_TABS = [
  {idx: 1, key: "mon", label: "Luni"},
  {idx: 2, key: "tue", label: "Marți"},
  {idx: 3, key: "wed", label: "Miercuri"},
  {idx: 4, key: "thu", label: "Joi"},
  {idx: 5, key: "fri", label: "Vineri"},
  {idx: 6, key: "sat", label: "Sâmbătă"},
  {idx: 0, key: "sun", label: "Duminică"}
];
// FR-65.1 / FR-65.6: save the settings file; send a chosen file unchanged and show what the controller did with it
const REASON_RO = {unknown: "nume necunoscut", invalid: "valoare nepermisă", "no-zone": "zonă inexistentă",
  "relay-taken": "releu folosit de altă zonă", "pump-relay": "releul pompei", "valve-energised": "robinet deschis", duplicate: "id repetat"};
// FR-11.3: one Save for all zones; the controller checks them together on the configuration that results
const SAVE_WHY = {"sensor-taken": "senzorul e la altă zonă", "relay-taken": "releul e la altă zonă", "pump-relay": "releul e al pompei",
  "valve-energised": "supapa e deschisă", invalid: "valoare nepermisă (o zonă pornită are nevoie de senzor)", duplicate: "zonă dublă"};
const DAYS_SHORT = ["Dum", "Lun", "Mar", "Mie", "Joi", "Vin", "Sâm"];
const DAYS_FULL = ["Duminică", "Luni", "Marți", "Miercuri", "Joi", "Vineri", "Sâmbătă"];
