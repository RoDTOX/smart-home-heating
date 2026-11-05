#include "Commands.h"
#include "App.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "ConfigTransfer.h"
#include "DiagnosticEngine.h"
#include "EventLog.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TelemetryHistoryManager.h"
#include "TimeAndWeatherService.h"
#include "Version.h"
#include "FaultInjector.h"
#include "ZoneController.h"
#include "Maintenance.h"
#include "PlantMonitor.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LittleFS.h>

// FR-8.5 / FR-8.6: the page is a gzip file on LittleFS; an embedded stub answers when it is missing or corrupt
static const char PAGE_FILE[] = "/index.html.gz";
static const char PAGE_TMP[] = "/index.html.tmp";
static const size_t PAGE_MAX_BYTES = 61440;
static const size_t PAGE_MIN_BYTES = 100;
static const size_t FS_FLOOR_BYTES = 24576;  // free LittleFS kept for the gas log, events and history
static const char STUB_HTML[] PROGMEM =
    "<!doctype html><meta charset=utf-8><title>Heating</title><body style=font-family:sans-serif>"
    "<h3>The page is missing</h3><p>The controller keeps heating. Upload the page:</p>"
    "<form method=post action=/api/web/update enctype=multipart/form-data><input type=file name=f><button>Upload</button></form>"
    "<p>Firmware update: <a href=/update>/update</a></p>";

static bool pageFileOk() {
  File f = LittleFS.open(PAGE_FILE, "r");
  if (!f) return false;
  bool ok = f.size() >= PAGE_MIN_BYTES && f.read() == 0x1F && f.read() == 0x8B;
  f.close();
  return ok;
}

static void sendCorsHeaders();

void handleRoot() {
  sendCorsHeaders();
  server.sendHeader("Cache-Control", "no-cache");
  if (pageFileOk()) {
    File f = LittleFS.open(PAGE_FILE, "r");
    server.sendHeader("X-Page-Source", "littlefs");
    server.streamFile(f, "text/html; charset=utf-8");  // the .gz name makes the core add Content-Encoding: gzip
    f.close();
    return;
  }
  server.sendHeader("X-Page-Source", "stub");
  server.send_P(200, "text/html; charset=utf-8", STUB_HTML);
}

static void sendCorsHeaders() {
  if (server.hasHeader("Origin")) {
    String origin = server.header("Origin");
    String host = server.hostHeader();
    if (host.length() > 0 && origin.indexOf(host) >= 0) {
      server.sendHeader("Access-Control-Allow-Origin", origin);
      server.sendHeader("Access-Control-Allow-Credentials", "true");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    } else {
      String localIp = WiFi.localIP().toString();
      if (localIp.length() > 0 && origin.indexOf(localIp) >= 0) {
        server.sendHeader("Access-Control-Allow-Origin", origin);
        server.sendHeader("Access-Control-Allow-Credentials", "true");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
      }
    }
  }
}

static bool webAuthOk() {
  return server.authenticate(commands.getUser(), commands.getPass());
}

static bool requireAuth() {
  if (webAuthOk()) return true;
  sendCorsHeaders();
  server.requestAuthentication();
  return false;
}

static size_t fsFreeBytes() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }

static File g_upFile;
static int g_upErr = 0;      // 0 = fine so far, else the HTTP code of the refusal
static bool g_upSeen = false;
static size_t g_upBytes = 0;
static size_t g_upOldBytes = 0;

static void upAbort(int code) {
  g_upErr = code;
  if (g_upFile) g_upFile.close();
  LittleFS.remove(PAGE_TMP);
}

void handleWebUpdateUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    g_upSeen = true; g_upErr = 0; g_upBytes = 0; g_upOldBytes = 0;
    if (g_upFile) g_upFile.close();
    if (!webAuthOk()) { g_upErr = 401; return; }
    File old = LittleFS.open(PAGE_FILE, "r");
    if (old) { g_upOldBytes = old.size(); old.close(); }
    g_upFile = LittleFS.open(PAGE_TMP, "w");
    if (!g_upFile) g_upErr = 500;
  } else if (u.status == UPLOAD_FILE_WRITE && !g_upErr) {
    if (g_upBytes == 0 && (u.currentSize < 2 || u.buf[0] != 0x1F || u.buf[1] != 0x8B)) upAbort(400);
    else if (g_upBytes + u.currentSize > PAGE_MAX_BYTES) upAbort(413);
    else if (fsFreeBytes() + g_upOldBytes < FS_FLOOR_BYTES + g_upBytes + u.currentSize) upAbort(507);
    else if (g_upFile.write(u.buf, u.currentSize) != u.currentSize) upAbort(507);
    else g_upBytes += u.currentSize;
  } else if (u.status == UPLOAD_FILE_END && !g_upErr) {
    if (g_upFile) g_upFile.close();
    if (g_upBytes < PAGE_MIN_BYTES) upAbort(400);
    else if (!LittleFS.rename(PAGE_TMP, PAGE_FILE)) upAbort(500);  // replaces the old page in one step
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    upAbort(500);
  }
}

void handleWebUpdateDone() {
  if (!webAuthOk()) { server.requestAuthentication(); g_upSeen = false; return; }
  int code = g_upErr ? g_upErr : (g_upSeen ? 200 : 400);
  g_upSeen = false; g_upErr = 0;
  char b[96];
  if (code == 200) snprintf(b, sizeof(b), "{\"status\":\"ok\",\"bytes\":%u,\"fsFree\":%u}", (unsigned)g_upBytes, (unsigned)fsFreeBytes());
  else snprintf(b, sizeof(b), "{\"status\":\"error\",\"code\":%d}", code);
  sendCorsHeaders();
  server.send(code, "application/json", b);
}

void handleWebRemove() {
  if (!webAuthOk()) { server.requestAuthentication(); return; }
  LittleFS.remove(PAGE_FILE);
  sendCorsHeaders();
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiOn() {
  if (!requireAuth()) return;
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  sendCorsHeaders();

  // If a pending OFF was scheduled, user pressing ON cancels the scheduled shutdown
  if (boilerRadio.getPendingAction() == PENDING_OFF) {
    boilerRadio.cancelPendingAction();
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\",\"pendingCancelled\":true,\"message\":\"Oprirea programata a fost anulata. Cazanul ramane ACTIV.\"}"));
    return;
  }

  // 1. Check idempotency (FR-17.1)
  if (boilerRadio.hasTransmittedOnce() && boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\",\"idempotent\":true}"));
    return;
  }

  // 2. Interlock (FR-2): no boiler ON, queued or not, before a zone valve has been energised for interlockDelay
  if (!thermalZones.boilerOnPermitted()) {
    boilerRadio.noteRefusal(RS_INTERLOCK, 0, true);
    boilerRadio.recordSpamAttempt();  // a rejected manual request counts for DTC-201 (FR-17.5)
    server.send(409, "application/json", F("{\"status\":\"interlock\",\"error\":\"No zone valve has been open for interlockDelay\",\"boiler\":\"OFF\"}"));
    return;
  }

  // 3. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  uint8_t guardWhy = RS_NONE;
  if (!boilerRadio.canTransmit(true, &waitSec, &isIdempotent, &guardWhy)) {
    boilerRadio.noteRefusal(guardWhy, waitSec, true);
    boilerRadio.recordSpamAttempt();
    // Schedule deferred startup if not already running
    if (!boilerRadio.isLastCommandOn()) {
      boilerRadio.setPendingAction(PENDING_ON);
      String resp = "{\"status\":\"pending\",\"error\":\"Anti-short-cycling guard active\",\"target\":\"ON\",\"waitSec\":" + String(waitSec) +
                    ",\"pending\":true,\"boiler\":\"OFF\",\"message\":\"Pornire programata automat dupa expirarea pauzei minime (" + String(waitSec) + "s)\"}";
      server.send(429, "application/json", resp);
      return;
    }
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Anti-short-cycling guard active\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
    return;
  }

  blinkLed(1, 40);
  bool success = boilerRadio.sendBoilerOn();
  blinkLed(1, 40);

  if (success) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\"}"));
  } else {
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Transmission throttled\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
  }
}

void handleApiOff() {
  if (!requireAuth()) return;
  // Forcing a stop is always safe; forcing a start is never allowed.
  bool force = server.hasArg("force") && server.arg("force") == "true";
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  sendCorsHeaders();

  // If a pending ON was scheduled, user pressing OFF cancels the scheduled startup
  if (boilerRadio.getPendingAction() == PENDING_ON) {
    boilerRadio.cancelPendingAction();
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\",\"pendingCancelled\":true,\"message\":\"Pornirea programata a fost anulata.\"}"));
    return;
  }

  // 1. Check idempotency (FR-17.1)
  if (!force && boilerRadio.hasTransmittedOnce() && !boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\",\"idempotent\":true}"));
    return;
  }

  // 2. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  uint8_t guardWhy = RS_NONE;
  if (!force && !boilerRadio.canTransmit(false, &waitSec, &isIdempotent, &guardWhy)) {
    boilerRadio.noteRefusal(guardWhy, waitSec, false);
    boilerRadio.recordSpamAttempt();
    // Schedule deferred shutdown: remember command and automatically trigger OFF when cooldown ends!
    if (boilerRadio.isLastCommandOn()) {
      boilerRadio.setPendingAction(PENDING_OFF);
      String resp = "{\"status\":\"pending\",\"error\":\"Anti-short-cycling guard active\",\"target\":\"OFF\",\"waitSec\":" + String(waitSec) +
                    ",\"pending\":true,\"boiler\":\"ON\",\"message\":\"Oprire programata automat dupa expirarea timpului minim de ardere (" + String(waitSec) + "s)\"}";
      server.send(429, "application/json", resp);
      return;
    }
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Anti-short-cycling guard active\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
    return;
  }

  blinkLed(1, 40);
  bool success = boilerRadio.sendBoilerOff(force);
  blinkLed(1, 40);

  if (success) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\"}"));
  } else {
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Transmission throttled\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
  }
}

void handleApiBoilerCancelPending() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  boilerRadio.cancelPendingAction();
  String resp = String("{\"status\":\"ok\",\"pendingCancelled\":true,\"boiler\":\"") + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", resp);
}

static const char* getGasMeterJson() {
  static char j[512];
  float measuredFlow = 0.0f;
  bool haveMeasured = telemetryHistory.gasFlowMeasured(measuredFlow);
  float flameRate = haveMeasured ? measuredFlow : paramStore.getf(Param::GasFlameRate);
  uint32_t lastEpoch = paramStore.state(StateKey::GasMeterBaseEpoch);  // 0 = no reading typed yet (FR-35.1)
  bool haveReading = lastEpoch != 0;
  float lastIndex = (float)paramStore.state(StateKey::GasMeterBaseIndex) / 1000.0f;
  uint32_t baseSec = paramStore.state(StateKey::GasMeterBaseSec);
  uint32_t totalSec = boilerRadio.getRuntimeTotalSec();
  uint32_t elapsedSec = (totalSec >= baseSec) ? (totalSec - baseSec) : 0;
  float currentEstimatedIndex = lastIndex + (elapsedSec / 3600.0f) * flameRate;
  float gasToday = ((float)boilerRadio.getRuntimeTodaySec() / 3600.0f) * flameRate;
  float gasYesterday = ((float)boilerRadio.getRuntimeYesterdaySec() / 3600.0f) * flameRate;
  float gasThisMonth = ((float)boilerRadio.getRuntimeThisMonthSec() / 3600.0f) * flameRate;
  time_t nowT = time(nullptr);
  if (nowT >= 1700000000) {
    float fromReadings = 0.0f;
    if (telemetryHistory.gasMonthFromReadings((uint32_t)nowT, fromReadings)) gasThisMonth = fromReadings;
  }

  char lastIdxStr[16], lastTimeStr[16], estIdxStr[16], measFlowStr[16];
  if (haveReading) {
    snprintf(lastIdxStr, sizeof(lastIdxStr), "%.3f", lastIndex);
    snprintf(lastTimeStr, sizeof(lastTimeStr), "%u", (unsigned)lastEpoch);
    snprintf(estIdxStr, sizeof(estIdxStr), "%.3f", currentEstimatedIndex);
  } else {
    strcpy(lastIdxStr, "null");
    strcpy(lastTimeStr, "null");
    strcpy(estIdxStr, "null");
  }
  if (haveMeasured) {
    snprintf(measFlowStr, sizeof(measFlowStr), "%.3f", measuredFlow);
  } else {
    strcpy(measFlowStr, "null");
  }

  snprintf(j, sizeof(j),
           "\"gasMeter\":{\"lastReadingIndex\":%s,\"lastReadingTime\":%s,\"estimatedIndex\":%s,"
           "\"readingCount\":%u,\"gasTodayM3\":%.2f,\"gasYesterdayM3\":%.2f,\"gasThisMonthM3\":%.2f,"
           "\"gasFlowMeasured\":%s,\"gasFlameRate\":%.2f,\"boilerSetTemp\":%u,\"boilerFlowObserved\":%u,"
           "\"boilerPowerKw\":%u,\"gasPrice\":%.2f},",
           lastIdxStr, lastTimeStr, estIdxStr,
           (unsigned)telemetryHistory.gasReadingCount(),
           gasToday, gasYesterday, gasThisMonth, measFlowStr,
           paramStore.getf(Param::GasFlameRate),
           (unsigned)paramStore.get(Param::BoilerSetTemp),
           (unsigned)paramStore.get(Param::BoilerFlowObserved),
           (unsigned)paramStore.get(Param::BoilerPowerKw),
           paramStore.getf(Param::GasPrice));
  return j;
}

void handleApiBoilerResetRuntime() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  boilerRadio.resetRuntime();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"message\":\"Boiler runtime counters reset to 0\"}"));
}

void handleApiStatus();

// FR-35.1: the only call that writes a reading. dryRun=1 validates and answers, stores nothing (test mode).
void handleApiBoilerGasMeter() {
  sendCorsHeaders();
  String idxArg = server.hasArg("index") ? server.arg("index") : String("");
  char* end = nullptr;
  double idx = strtod(idxArg.c_str(), &end);
  if (idxArg.length() == 0 || !end || *end != '\0' || !(idx >= 0.0 && idx <= 999999.9995)) {
    server.send(400, "application/json",
                F("{\"status\":\"error\",\"error\":\"index missing or outside 0 to 999999 (FR-35.1)\"}"));
    return;
  }
  uint32_t now = (uint32_t)time(nullptr);
  uint32_t epoch = now;
  if (server.hasArg("time")) {
    String tArg = server.arg("time");
    char* tEnd = nullptr;
    unsigned long tv = strtoul(tArg.c_str(), &tEnd, 10);
    if (tArg.length() == 0 || !tEnd || *tEnd != '\0' || tv < 1700000000UL || tv > (unsigned long)now) {
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"error\":\"time must be an epoch in the past (FR-35.1)\"}"));
      return;
    }
    epoch = (uint32_t)tv;
  }
  if (epoch < 1700000000UL) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"clock not set, give time (FR-35.1)\"}"));
    return;
  }
  uint32_t idxMilli = (uint32_t)lround(idx * 1000.0);
  if (server.hasArg("dryRun") && server.arg("dryRun") != "1" && server.arg("dryRun") != "0") {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"dryRun must be 0 or 1\"}"));
    return;
  }
  bool dry = server.hasArg("dryRun") && server.arg("dryRun") == "1";
  bool stored = false;
  if (!dry) {
    // the burner runtime is known only for a reading typed now, not for a back-dated one
    uint32_t runtimeAtReading = (epoch + 120UL >= now) ? boilerRadio.getRuntimeTotalSec() : TelemetryHistoryManager::GAS_NO_RUNTIME;
    if (!telemetryHistory.gasReadingAdd(epoch, idxMilli, runtimeAtReading)) {
      server.send(500, "application/json", F("{\"status\":\"error\",\"error\":\"reading not stored\"}"));
      return;
    }
    stored = true;
    // the last reading (by time) is the base of the estimate; an older reading only joins the log
    if (epoch >= paramStore.state(StateKey::GasMeterBaseEpoch)) {
      paramStore.saveState(StateKey::GasMeterBaseIndex, idxMilli);
      paramStore.saveState(StateKey::GasMeterBaseSec, boilerRadio.getRuntimeTotalSec());
      paramStore.saveState(StateKey::GasMeterBaseEpoch, epoch);
    }
  }
  char resp[768];
  const char* gm = getGasMeterJson();
  size_t gmLen = strlen(gm);
  char gmClean[512];
  if (gmLen > 0 && gm[gmLen - 1] == ',') {
    strncpy(gmClean, gm, gmLen - 1);
    gmClean[gmLen - 1] = '\0';
  } else {
    strncpy(gmClean, gm, sizeof(gmClean) - 1);
    gmClean[sizeof(gmClean) - 1] = '\0';
  }
  snprintf(resp, sizeof(resp),
           "{\"status\":\"ok\",\"stored\":%s,\"reading\":{\"time\":%u,\"index\":%.3f},%s}",
           stored ? "true" : "false", (unsigned)epoch, (double)idxMilli / 1000.0, gmClean);
  server.send(200, "application/json", resp);
}

struct GasListCtx {
  String chunk;
  bool first;
};

// FR-35.1: all stored readings, oldest first, streamed (up to 1000 entries)
void handleApiGasReadings() {
  sendCorsHeaders();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent(String("{\"count\":") + telemetryHistory.gasReadingCount() + ",\"replaced\":" +
                     telemetryHistory.gasReadingsReplaced() + ",\"readings\":[");
  GasListCtx ctx;
  ctx.first = true;
  telemetryHistory.gasReadingsForEach(
      [](uint32_t e, uint32_t idx, uint32_t rt, void* p) {
        GasListCtx* c = (GasListCtx*)p;
        c->chunk += String(c->first ? "" : ",") + "{\"time\":" + String(e) + ",\"index\":" + String((double)idx / 1000.0, 3) +
                    ",\"runtimeSec\":" + (rt == TelemetryHistoryManager::GAS_NO_RUNTIME ? String("null") : String(rt)) + "}";
        c->first = false;
        if (c->chunk.length() > 400) {
          server.sendContent(c->chunk);
          c->chunk = "";
        }
      },
      &ctx);
  ctx.chunk += "]}";
  server.sendContent(ctx.chunk);
  server.sendContent("");
}

// NFR-2.2: a JSON answer is written in chunks from one fixed buffer, never built as a growing String
struct JsonOut {
  char b[384];
  size_t n = 0;
  JsonOut() {
    sendCorsHeaders();
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
  }
  void flush() {
    if (n) { server.sendContent(b, n); n = 0; }
  }
  void put(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(b + n, sizeof(b) - n, fmt, ap);
    va_end(ap);
    if (w >= (int)(sizeof(b) - n)) {  // did not fit: send what is there, write again into the empty buffer
      flush();
      va_start(ap, fmt);
      w = vsnprintf(b, sizeof(b), fmt, ap);
      va_end(ap);
    }
    if (w > 0) n += ((size_t)w < sizeof(b) ? (size_t)w : sizeof(b) - 1);
  }
  void text(const String &t) { flush(); server.sendContent(t); }
  void end() { flush(); server.sendContent(""); }
};
static const char *tf(bool v) { return v ? "true" : "false"; }

void handleApiZones() {
  JsonOut o;
  uint32_t boilerWaitSec = 0;
  boilerRadio.canTransmit(!boilerRadio.isLastCommandOn(), &boilerWaitSec);
  o.put("{\"autoMode\":%s,\"boiler\":\"%s\",\"boilerCooldownSec\":%lu,\"pendingAction\":\"%s\",\"pendingWaitSec\":%lu,",
        tf(thermalZones.isAutoMode()), boilerRadio.isLastCommandOn() ? "ON" : "OFF", (unsigned long)boilerWaitSec,
        boilerRadio.getPendingActionStr(), (unsigned long)boilerWaitSec);
  o.put("\"boilerDemand\":%s,\"actuatorType\":\"%s\",\"pumpEnabled\":%s,\"pumpActive\":%s,\"frostGuardActive\":%s,",
        tf(thermalZones.isBoilerDemanded()), thermalZones.isActuatorNormallyClosed() ? "NC" : "NO",
        tf(thermalZones.isPumpControlEnabled()), tf(thermalZones.isPumpActive()), tf(thermalZones.isFrostProtectionActive()));
  o.put("\"sensorTimeoutSec\":%lu,\"lastAntiSeizeEpoch\":%lu,\"scheduleEnabled\":%s,\"away\":%s,",
        (unsigned long)thermalZones.getSensorTimeoutSec(), (unsigned long)thermalZones.getLastAntiSeizeEpoch(),
        tf(thermalZones.isScheduleEnabled()), tf(thermalZones.isAway()));
  o.put("\"activeLevel\":\"%s\",\"nextLevel\":\"%s\",\"minsToNextSlot\":%ld,\"preHeatActive\":%s,",
        ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()), ThermalZoneManager::getLevelName(thermalZones.getNextLevel()),
        (long)thermalZones.getMinutesToNextSlot(), tf(thermalZones.isPreHeatActive()));
  o.put("\"outdoorTemp\":%.1f,\"outdoorBand\":%u,", (double)timeAndWeather.getOutdoorTemperature(), (unsigned)ZoneController::getOutdoorBand(timeAndWeather.getOutdoorTemperature()));
  o.put("\"boilerRuntime\":{\"totalSec\":%lu,\"todaySec\":%lu,\"yesterdaySec\":%lu,\"thisMonthSec\":%lu,\"lastMonthSec\":%lu,\"currentBurnSec\":%lu},\"zones\":[",
        (unsigned long)boilerRadio.getRuntimeTotalSec(), (unsigned long)boilerRadio.getRuntimeTodaySec(),
        (unsigned long)boilerRadio.getRuntimeYesterdaySec(), (unsigned long)boilerRadio.getRuntimeThisMonthSec(),
        (unsigned long)boilerRadio.getRuntimeLastMonthSec(), (unsigned long)boilerRadio.getCurrentFlameDurationSec());

  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    const ThermalZoneConfig &cfg = thermalZones.getConfig(i);
    const ThermalZoneStatus &st = thermalZones.getStatus(i);
    uint32_t openSec = 0;
    if (st.relayEngaged) {
      if (st.stateChangedMs > 0 && millis() >= st.stateChangedMs) {
        openSec = (millis() - st.stateChangedMs) / 1000;
        if (openSec > 120) openSec = 120;
      } else {
        openSec = 120;
      }
    } else {
      // Linear cooling contraction: 120s -> 0s
      if (st.stateChangedMs > 0 && millis() >= st.stateChangedMs) {
        uint32_t offElapsed = (millis() - st.stateChangedMs) / 1000;
        openSec = offElapsed < 120 ? 120 - offElapsed : 0;
      }
    }
    o.put("%s{\"id\":%d,\"name\":\"%s\",\"sensor\":\"%s\",\"sensorMac\":\"%s\",\"relayPin\":%d,",
          i > 0 ? "," : "", (int)cfg.id, cfg.name, cfg.sensorName, cfg.sensorMac, (int)cfg.relayPin);
    o.put("\"targetTemp\":%.1f,\"effectiveTarget\":%.1f,\"scheduleTarget\":%.1f,\"isBoost\":%s,\"hysteresis\":%.2f,\"enabled\":%s,",
          (double)cfg.targetTemp, (double)thermalZones.getEffectiveTargetTemp(i), (double)thermalZones.getScheduleTargetTemp(i),
          tf(thermalZones.isBoost(i)), (double)thermalZones.getHysteresis(), tf(cfg.enabled));
    o.put("\"currentTemp\":%.1f,\"sensorValid\":%s,\"sensorFault\":%s,\"frostActive\":%s,\"windowPauseActive\":%s,\"windowDetected\":%s,",
          (double)st.currentTemp, tf(st.sensorValid), tf(st.sensorFault), tf(st.frostActive), tf(thermalZones.isWindowPauseActive(i)), tf(thermalZones.isWindowDetected(i)));
    o.put("\"windowPauseRemainingSec\":%lu,\"windowDropThreshold\":%.1f,\"lastWindowEpoch\":%lu,\"lastWindowDurationSec\":%lu,",
          (unsigned long)thermalZones.getWindowPauseRemainingSec(i), (double)thermalZones.getWindowThreshold(),
          (unsigned long)thermalZones.getWindowLastDetectionEpoch(i), (unsigned long)thermalZones.getWindowLastDetectionDurationSec(i));
    o.put("\"demand\":%s,\"relayEngaged\":%s,\"openSec\":%lu,\"isManualHold\":%s,\"duty\":%.2f,\"dutyPct\":%d,\"onTimeSec\":%lu,\"learnedDuty\":%.1f,\"tempOffset\":%.1f,",
          tf(st.demand), tf(st.relayEngaged), (unsigned long)openSec, tf(st.isManualHold), (double)thermalZones.getDuty(i), (int)(thermalZones.getDuty(i) * 100),
          (unsigned long)(thermalZones.getZoneOnMs(i) / 1000), (double)thermalZones.getLearnedDutyPct(i), (double)cfg.tempOffset);
    o.put("\"runtimeTodaySec\":%lu,\"runtimeYesterdaySec\":%lu,\"state\":\"%s\"}",
          (unsigned long)thermalZones.getZoneRuntimeTodaySec(i), (unsigned long)thermalZones.getZoneRuntimeYesterdaySec(i),
          thermalZones.getStateCode(i));
  }
  o.put("]}");
  o.end();
}

void handleApiZoneSetpoint() {
  if (!server.hasArg("id") || !server.hasArg("target")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameters (id and target)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx >= thermalZones.getZoneCount()) {
    server.send(400, "application/json",
                F("{\"error\":\"Invalid zone index\"}"));
    return;
  }
  float target = server.arg("target").toFloat();
  sendCorsHeaders();
  if (!thermalZones.setTargetTemperatureChecked(idx, target)) {  // FR-1.2: boost limits for every source
    server.send(400, "application/json", F("{\"error\":\"A boost is 0.5 to 2.0 C above the schedule level in 0.5 C steps\"}"));
    return;
  }
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiZoneHoldClear() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    thermalZones.clearManualHold(idx);
    sendCorsHeaders();
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiZoneWindowClear() {
  sendCorsHeaders();
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  String idStr = server.arg("id");
  if (idStr.equalsIgnoreCase("all")) {
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
      thermalZones.clearWindowPause(i);
    }
    server.send(200, "application/json", F("{\"status\":\"ok\",\"cleared\":\"all\"}"));
    return;
  }
  int idVal = idStr.toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    bool cleared = thermalZones.clearWindowPause(idx);
    server.send(200, "application/json",
                cleared ? F("{\"status\":\"ok\"}")
                        : F("{\"status\":\"noop\",\"message\":\"Window pause was not active\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiZoneWindowThreshold() {
  sendCorsHeaders();
  if (!server.hasArg("threshold")) {
    server.send(400, "application/json", F("{\"error\":\"Missing parameter threshold\"}"));
    return;
  }
  // One global `windowThreshold` for all zones (FR-40); a zone id, if sent, is ignored.
  if (!thermalZones.setWindowThreshold(server.arg("threshold").toFloat())) {
    server.send(400, "application/json",
                F("{\"error\":\"windowThreshold out of range (requirements \u00a76: 0.8 to 2.0 C)\"}"));
    return;
  }
  server.send(200, "application/json",
              String("{\"status\":\"ok\",\"windowThreshold\":") + String(thermalZones.getWindowThreshold(), 1) + "}");
}

static void sendSetupError(const String &message);

void handleApiZoneToggle() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    const ThermalZoneConfig &cfg = thermalZones.getConfig(idx);
    bool current = cfg.enabled;
    // FR-11.3: on only with a sensor; FR-3: not off while the valve is energised (no overrun hold before step 4)
    if (!current && strlen(cfg.sensorMac) == 0 && strlen(cfg.sensorName) == 0) {
      sendSetupError("A zone is switched on only with a sensor");
      return;
    }
    if (current && thermalZones.getStatus(idx).relayEngaged) {
      sendSetupError("Valve is open; switch the zone off once the valve is closed");
      return;
    }
    thermalZones.setZoneEnabled(idx, !current);
    sendCorsHeaders();
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiSeason() {
  String m = server.arg("mode");
  m.toLowerCase();
  sendCorsHeaders();
  if (m != "auto" && m != "on" && m != "off") {
    server.send(400, "application/json", F("{\"error\":\"mode must be auto, on or off\"}"));
    return;
  }
  thermalZones.setSeasonMode(m == "auto" ? 0 : m == "on" ? 1 : 2);
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiMode() {
  if (!server.hasArg("mode")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (mode)\"}"));
    return;
  }
  String m = server.arg("mode");
  m.toLowerCase();
  if (m == "auto") {
    thermalZones.setAutoMode(true);
  } else if (m == "manual") {
    if (!requireAuth()) return;
    thermalZones.setAutoMode(false);
  }
  sendCorsHeaders();
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiZoneSensor() {
  if (!server.hasArg("id") || !server.hasArg("sensor")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameters (id and sensor)\"}"));
    return;
  }
  size_t id = server.arg("id").toInt();
  String sensorName = server.arg("sensor");
  if (id < thermalZones.getZoneCount()) {
    thermalZones.setSensorName(id, sensorName.c_str());
    sendCorsHeaders();
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiBleSensors() {
  if (!bleScanner.isSearching()) bleScanner.prune();
  JsonOut o;
  o.put("{\"scanning\":%s,\"count\":%u,\"sensors\":[", tf(bleScanner.isScanning()), (unsigned)bleScanner.getSensorCount());
  for (size_t i = 0; i < bleScanner.getSensorCount(); i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s)
      continue;
    o.put("%s{\"name\":\"%s\",\"mac\":\"%s\",\"temp\":%.2f,\"hum\":%.1f,\"bat\":%d,\"rssi\":%d,\"packets\":%lu,\"source\":\"%s\",\"ageSec\":%lu}",
          i > 0 ? "," : "", s->name, s->mac, (double)s->temperature, (double)s->humidity, (int)s->battery, (int)s->rssi,
          (unsigned long)s->packetCount, bleScanner.getSourceStr(s->source), (unsigned long)((millis() - s->lastSeenMs) / 1000));
  }
  o.put("]}");
  o.end();
}

void handleApiSetupLocation() {
  sendCorsHeaders();
  if (server.method() == HTTP_POST) {
    if (!timeAndWeather.setLocation(server.arg("coords").c_str())) {
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"message\":\"Use coords=lat,lon (-90..90, -180..180)\"}"));
      return;
    }
    timeAndWeather.forceWeatherUpdate();
  }
  server.send(200, "application/json",
              String("{\"coords\":\"") + timeAndWeather.getLocation() + "\"}");
}

void handleApiSetupDiscoveredSensors() {
  if (!bleScanner.isSearching()) bleScanner.prune();  // FR-11.6: right after a zone was deleted or re-mapped
  String json;
  json.reserve(1536);
  json = "{\"active\":" + String(bleScanner.isSearching() ? "true" : "false") +
         ",\"remainingSec\":" + String(bleScanner.searchRemainingSec()) + ",\"sensors\":[";
  size_t count = bleScanner.getSensorCount();
  size_t written = 0;
  for (size_t i = 0; i < count; i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s || !s->valid)
      continue;
    if (written > 0)
      json += ",";
    json += "{";
    json += "\"mac\":\"" + String(s->mac) + "\",";
    json += "\"name\":\"" + String(s->name) + "\",";
    json += "\"temp\":" + String(s->temperature, 1) + ",";
    json += "\"hum\":" + String(s->humidity, 1) + ",";
    json += "\"bat\":" + String(s->battery) + ",";
    json += "\"rssi\":" + String(s->rssi) + ",";
    json += "\"ageSec\":" + String((millis() - s->lastSeenMs) / 1000) + ",";
    int zoneNumber = 0;  // 1-based number of the zone that uses this sensor, 0 = none (FR-11.5)
    for (size_t z = 0; z < thermalZones.getZoneCount(); z++) {
      const ThermalZoneConfig &zc = thermalZones.getConfig(z);
      if ((zc.sensorMac[0] && strcasecmp(zc.sensorMac, s->mac) == 0) ||
          (zc.sensorName[0] && strcasecmp(zc.sensorName, s->name) == 0)) {
        zoneNumber = (int)z + 1;
        break;
      }
    }
    json += "\"zone\":" + String(zoneNumber);
    json += "}";
    written++;
  }
  json += "]}";
  sendCorsHeaders();
  server.send(200, "application/json", json);
}

static void sendSetupError(const String &message) {
  sendCorsHeaders();
  server.send(400, "application/json",
              "{\"status\":\"error\",\"message\":\"" + message + "; nothing changed\"}");
}

// Printable text that fits the record and cannot break the JSON it is written into
static bool validLabel(const String &s, size_t capacity, bool allowEmpty) {
  if (s.length() >= capacity || (!allowEmpty && s.length() == 0)) return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((uint8_t)c < 0x20 || c == '"' || c == '\\') return false;
  }
  return true;
}

static bool validMac(const String &s) {
  if (s.length() == 0) return true;
  if (s.length() != 17) return false;
  for (size_t i = 0; i < 17; i++) {
    char c = s[i];
    if (i % 3 == 2 ? c != ':' : !isxdigit((unsigned char)c)) return false;
  }
  return true;
}

// FR-73: add a zone on the lowest free relay output
void handleApiSetupZoneAdd() {
  String name = server.hasArg("name") ? server.arg("name") : String();
  if (name.length() > 0 && !validLabel(name, 32, false)) {
    sendSetupError("Name must be 1-31 characters, no quotes or backslash");
    return;
  }
  size_t index = 0;
  sendCorsHeaders();
  if (thermalZones.addZone(name.c_str(), index) != ThermalZoneManager::ZoneResult::Ok) {
    server.send(409, "application/json", F("{\"status\":\"error\",\"message\":\"No relay output is free; nothing changed\"}"));
    return;
  }
  server.send(200, "application/json",
              "{\"status\":\"ok\",\"zone\":" + String((unsigned)(index + 1)) + ",\"relayPin\":" +
                  String(thermalZones.getConfig(index).relayPin) + "}");
}

// FR-74: delete a zone (1-based number); later zones move up
void handleApiSetupZoneDelete() {
  int number = server.hasArg("zone") ? server.arg("zone").toInt() : 0;
  if (number < 1 || number > (int)thermalZones.getZoneCount()) {
    sendSetupError("Unknown zone number");
    return;
  }
  sendCorsHeaders();
  if (thermalZones.deleteZone((size_t)number - 1) != ThermalZoneManager::ZoneResult::Ok) {
    server.send(409, "application/json",
                F("{\"status\":\"error\",\"message\":\"Valve is open; delete the zone once the valve is closed; nothing changed\"}"));
    return;
  }
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

// FR-11.5: start or stop the sensor search
void handleApiSetupSensorSearch() {
  String action = server.arg("action");
  if (action == "start") bleScanner.startSearch();
  else if (action == "stop") bleScanner.stopSearch();
  else {
    sendSetupError("action must be start or stop");
    return;
  }
  handleApiSetupDiscoveredSensors();
}

// FR-11.3: every field is checked before anything changes; a rejected setting returns 400 and changes nothing.
void handleApiSetupMapZone() {
  if (!server.hasArg("zone") || !isDigit(server.arg("zone")[0])) {
    sendSetupError("Missing or invalid zone index");
    return;
  }
  size_t z = (size_t)server.arg("zone").toInt();
  if (z >= thermalZones.getZoneCount()) {
    sendSetupError("Invalid zone index");
    return;
  }
  const ThermalZoneConfig &cur = thermalZones.getConfig(z);
  String name = server.hasArg("name") ? server.arg("name") : String(cur.name);
  String mac = server.hasArg("mac") ? server.arg("mac") : String(cur.sensorMac);
  mac.toUpperCase();
  String sname =
      server.hasArg("sname") ? server.arg("sname") : String(cur.sensorName);
  uint8_t pin =
      server.hasArg("pin") ? (uint8_t)server.arg("pin").toInt() : cur.relayPin;
  float target =
      server.hasArg("target") ? server.arg("target").toFloat() : cur.targetTemp;
  float offset =
      server.hasArg("offset") ? server.arg("offset").toFloat() : cur.tempOffset;
  String enArg = server.hasArg("enabled") ? server.arg("enabled") : String(cur.enabled ? "1" : "0");
  bool en = (enArg == "true" || enArg == "1");

  if (!(en || enArg == "false" || enArg == "0")) {
    sendSetupError("enabled must be 1/0 or true/false");
    return;
  }
  if (!validLabel(name, sizeof(cur.name), false)) {
    sendSetupError("Name must be 1-31 characters, no quotes or backslash");
    return;
  }
  if (!validMac(mac)) {
    sendSetupError("Sensor MAC must be AA:BB:CC:DD:EE:FF or empty");
    return;
  }
  if (!validLabel(sname, sizeof(cur.sensorName), true)) {
    sendSetupError("Sensor alias must be up to 31 characters, no quotes or backslash");
    return;
  }
  // FR-3: no overrun hold before migration step 4, so an energised valve is neither moved nor switched off
  if (thermalZones.getStatus(z).relayEngaged && (pin != cur.relayPin || (cur.enabled && !en))) {
    sendSetupError("Valve is open; change the relay or switch the zone off once the valve is closed");
    return;
  }
  if (!ParamStore::validRelayPin(pin)) {
    sendSetupError("Invalid relay pin. Allowed: 32, 33, 25, 26");
    return;
  }
  int other = thermalZones.zoneUsingRelay(pin, z);
  if (other >= 0) {
    sendSetupError("Relay already used by zone " + String(other + 1));
    return;
  }
  if (mac.length() > 0) {  // FR-11.3: a sensor MAC serves one zone
    for (size_t k = 0; k < thermalZones.getZoneCount(); k++) {
      if (k != z && strcasecmp(thermalZones.getConfig(k).sensorMac, mac.c_str()) == 0) {
        sendSetupError("Sensor already used by zone " + String((unsigned)(k + 1)));
        return;
      }
    }
  }
  if (thermalZones.isPumpControlEnabled() && pin == thermalZones.getPumpRelayPin()) {
    sendSetupError("Relay is the pump output");
    return;
  }
  if (en && mac.length() == 0 && sname.length() == 0) {
    sendSetupError("A zone is switched on only with a sensor");
    return;
  }
  if (server.hasArg("target") && (target < 15.0f || target > TARGET_MAX_C)) {
    sendSetupError("Target out of range (15 to 28 C)");
    return;
  }
  if (offset < -3.0f || offset > 3.0f) {
    sendSetupError("Offset out of range (-3 to +3 C)");
    return;
  }
  // `hysteresis` is one global value (requirements §6); "hyst" sets it for all zones.
  if (server.hasArg("hyst") &&
      !paramStore.inRange(Param::Hysteresis,
                          ParamStore::toRaw(Param::Hysteresis, server.arg("hyst").toFloat()))) {
    sendSetupError("hysteresis out of range (0.1 to 1.0 C)");
    return;
  }

  if (server.hasArg("hyst")) thermalZones.setHysteresis(server.arg("hyst").toFloat());
  thermalZones.mapZone(z, name.c_str(), mac.c_str(), sname.c_str(), pin, target, en);
  thermalZones.setTempOffset(z, offset);
  sendCorsHeaders();
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"Zone configuration updated "
                "and saved to NVS\"}"));
}

void handleApiSetupTime() {
  if (server.hasArg("refreshWeather")) {
    timeAndWeather.forceWeatherUpdate();
  }
  sendCorsHeaders();
  server.send(200, "application/json", timeAndWeather.toJson());
}

void handleApiDiagStatus() {
  JsonOut o;
  o.put("{\"checks\":");
  o.text(diagEngine.checksJson());
  o.put(",\"dtcCount\":%u,\"dtcs\":[", (unsigned)diagEngine.getActiveDtcCount());
  size_t written = 0;
  for (size_t i = 0; i < diagEngine.getDtcCount(); i++) {
    const DtcRecord *d = diagEngine.getDtc(i);
    if (!d || !d->active)
      continue;
    o.put("%s{\"code\":\"%s\",\"severity\":\"%s\",\"message\":\"%s\",\"count\":%lu,\"ageSec\":%lu}", written > 0 ? "," : "",
          d->code, diagEngine.getSeverityStr(d->severity), d->message, (unsigned long)d->occurrenceCount,
          (unsigned long)((millis() - d->lastRaisedMs) / 1000));
    written++;
  }
  o.put("],\"pins\":[");
  // FR-10.5: live pad level of every board pin; digitalRead never changes the pin
  static const struct { const char *name; uint8_t gpio; } pinList[] = {
    {"Relay 1", PIN_RELAY_ZONE_1}, {"Relay 2", PIN_RELAY_ZONE_2},
    {"Relay 3", PIN_RELAY_ZONE_3}, {"Relay 4", PIN_RELAY_SPARE},
    {"433 MHz TX", PIN_RF_DATA}, {"Status LED", PIN_STATUS_LED}};
  for (size_t p = 0; p < sizeof(pinList) / sizeof(pinList[0]); p++)
    o.put("%s{\"name\":\"%s\",\"gpio\":%d,\"level\":%d}", p > 0 ? "," : "", pinList[p].name, (int)pinList[p].gpio,
          digitalRead(pinList[p].gpio) ? 1 : 0);
  o.put("]}");
  o.end();
}

void handleApiPlantStatus() {
  sendCorsHeaders();
  server.send(200, "application/json", plantMonitor.getStatusJson());
}

void handleApiDiagInjectGet() {
  sendCorsHeaders();
  server.send(200, "application/json", faultInjector.getInjectionsJsonString());
}

void handleApiDiagInjectPost() {
  if (!requireAuth()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }
  const char* pt = doc["point"] | "";
  uint8_t zone = doc["zone"] | 0;
  if (!pt || strlen(pt) == 0) {
    server.send(400, "application/json", "{\"error\":\"missing point\"}");
    return;
  }
  if (faultInjector.inject(String(pt), zone)) {
    sendCorsHeaders();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"injection refused or unsupported\"}");
  }
}

void handleApiDiagInjectClear() {
  if (!requireAuth()) return;
  faultInjector.clear();
  sendCorsHeaders();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleApiDiagEvents() {
  sendCorsHeaders();
  server.send(200, "application/json", diagEngine.eventsJson());
}

// FR-62.1: the event log, newest first, streamed in chunks from one fixed buffer
struct LogOut {
  JsonOut *j;
  bool first;
};

static void logEntryOut(const EventEntry &e, void *arg) {
  LogOut *o = (LogOut *)arg;
  JsonOut &j = *o->j;
  // FR-62.1: local time as yymmddhhmmss, uptime seconds until the clock is set
  char when[16];
  if (e.flags & 1) {
    time_t t = (time_t)e.time;
    struct tm ti;
    localtime_r(&t, &ti);
    strftime(when, sizeof(when), "%y%m%d%H%M%S", &ti);
  } else {
    snprintf(when, sizeof(when), "%lu", (unsigned long)e.time);
  }
  j.put("%s{\"time\":\"%s\",\"timeValid\":%s,\"type\":\"%s\",\"subject\":%u,\"zone\":%u,\"reason\":\"%s\",\"old\":%u,\"new\":%u,"
        "\"boiler\":%s,\"overrun\":%s,\"pending\":\"%s\"",
        o->first ? "" : ",", when, tf(e.flags & 1), EventLog::typeName(e.type), e.subject, e.zone,
        EventLog::reasonName(e.reason), e.oldV, e.newV, tf(e.ctx & EVCTX_BOILER_ON), tf(e.ctx & EVCTX_OVERRUN),
        (e.ctx & EVCTX_PENDING_OFF) ? "off" : ((e.ctx & EVCTX_PENDING_ON) ? "on" : "none"));
  if (e.type == EV_DTC) {
    const DtcRecord *d = diagEngine.getDtc(e.subject);
    j.put(",\"code\":\"%s\"", d ? d->code : "");
  }
  j.put("}");
  o->first = false;
}

void handleApiDiagLog() {
  long v = server.hasArg("n") ? server.arg("n").toInt() : 200;
  size_t n = v < 1 ? 1 : (v > 4096 ? 4096 : (size_t)v);
  size_t have = eventLog.queued() + eventLog.stored();
  if (n > have) n = have;
  JsonOut j;
  j.put("{\"count\":%u,\"stored\":%u,\"queued\":%u,\"events\":[", (unsigned)n, (unsigned)eventLog.stored(),
        (unsigned)eventLog.queued());
  LogOut o = {&j, true};
  eventLog.forEachNewest(n, logEntryOut, &o);
  j.put("]}");
  j.end();
}

extern volatile uint32_t g_wxStackFreeMin;  // TimeAndWeatherService.cpp

// FR-10.6: read-only memory probe, one fixed buffer, no String
void handleApiDiagMem() {
  multi_heap_info_t i8;
  heap_caps_get_info(&i8, MALLOC_CAP_8BIT);
  char b[900];
  int n = snprintf(b, sizeof(b),
    "{\"free\":%u,\"minFreeEver\":%u,\"largestBlock\":%u,\"allocated\":%u,\"allocBlocks\":%u,\"freeBlocks\":%u,"
    "\"internalFree\":%u,\"internalMin\":%u,\"internalLargest\":%u,\"heapSize\":%u,\"freeIram32\":%u,\"uptime\":%lu,"
    "\"fsTotal\":%u,\"fsUsed\":%u,\"controlStackFree\":%u,\"loopStackFree\":%u,\"tasks\":[",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), (unsigned)i8.total_allocated_bytes,
    (unsigned)i8.allocated_blocks, (unsigned)i8.free_blocks,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)ESP.getHeapSize(),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT), (unsigned long)(millis() / 1000),
    (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes(),
    (unsigned)App::getControlStackFree(), (unsigned)App::getLoopStackFree());
  static const char *const names[] = {"loopTask", "ctl", "wifi", "tiT", "btController", "nimble_host", "wx"};
  bool first = true;
  for (size_t k = 0; k < sizeof(names) / sizeof(names[0]) && n > 0 && n < (int)sizeof(b) - 64; k++) {
    TaskHandle_t h = xTaskGetHandle(names[k]);
    unsigned hw;
    if (h) hw = (unsigned)uxTaskGetStackHighWaterMark(h);
    else if (k == 6 && g_wxStackFreeMin) hw = g_wxStackFreeMin;  // "wx" lives only while it fetches: report the last figure
    else continue;
    n += snprintf(b + n, sizeof(b) - n, "%s{\"name\":\"%s\",\"stackFree\":%u,\"stackFreeMin\":%u}", first ? "" : ",", names[k], hw, hw);
    first = false;
  }
  snprintf(b + n, sizeof(b) - n, "]}");
  sendCorsHeaders();
  server.send(200, "application/json", b);
}

void handleApiDiagSelfTest() {
  String report = diagEngine.runSelfTest();
  sendCorsHeaders();
  server.send(200, "text/plain", report);
}

void handleApiDiagClear() {
  if (!requireAuth()) return;
  diagEngine.clearAllDtcs();
  sendCorsHeaders();
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"All DTC codes cleared\"}"));
}

void handleApiSetupActuatorType() {
  if (server.method() == HTTP_POST) {
    if (!requireAuth()) return;
    if (server.hasArg("type")) {
      String t = server.arg("type");
      t.toLowerCase();
      thermalZones.setActuatorNormallyClosed(t == "nc");
    } else if (server.hasArg("nc")) {
      String v = server.arg("nc");
      v.toLowerCase();
      thermalZones.setActuatorNormallyClosed(v == "true" || v == "1");
    }
  }
  String json = "{\"status\":\"ok\",\"actuatorType\":\"" +
                String(thermalZones.isActuatorNormallyClosed() ? "NC" : "NO") +
                "\"}";
  sendCorsHeaders();
  server.send(200, "application/json", json);
}

void handleApiSetupPump() {
  if (server.method() == HTTP_POST) {
    if (!requireAuth()) return;
    if (server.hasArg("enabled")) {
      String v = server.arg("enabled");
      v.toLowerCase();
      if (!thermalZones.setPumpControlEnabled(v == "true" || v == "1")) {
        sendSetupError("Relay 4 is a zone output, the pump cannot use it");
        return;
      }
    }
  }
  String json = "{\"status\":\"ok\",\"pumpEnabled\":" +
                String(thermalZones.isPumpControlEnabled() ? "true" : "false") +
                ",\"pumpActive\":" +
                String(thermalZones.isPumpActive() ? "true" : "false") + "}";
  sendCorsHeaders();
  server.send(200, "application/json", json);
}

void handleApiDiagExercise() {
  thermalZones.triggerAntiSeizeExercise();
  sendCorsHeaders();
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"Anti-seize exercise cycle "
                "started (20s)\"}"));
}

void handleApiSetupSensorTimeout() {
  if (server.method() == HTTP_POST) {
    if (!requireAuth()) return;
  }
  if (server.method() == HTTP_POST && (server.hasArg("timeoutSec") || server.hasArg("sec"))) {
    String v = server.hasArg("timeoutSec") ? server.arg("timeoutSec") : server.arg("sec");
    if (!thermalZones.setSensorTimeoutSec((uint32_t)v.toInt())) {
      sendCorsHeaders();
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"message\":\"sensorTimeout out of range (60 to 3600 s)\"}"));
      return;
    }
  }
  String json = "{\"status\":\"ok\",\"sensorTimeoutSec\":" +
                String(thermalZones.getSensorTimeoutSec()) + "}";
  sendCorsHeaders();
  server.send(200, "application/json", json);
}

void handleApiStatus() {
  JsonOut o;
  uint32_t bWaitSec = 0;
  boilerRadio.canTransmit(!boilerRadio.isLastCommandOn(), &bWaitSec);
  ThermalZoneManager::EffMode em = thermalZones.effectiveMode();
  uint32_t sm = paramStore.get(Param::SeasonMode);
  o.put("{\"profile\":\"%s\",\"firmwareVersion\":\"%s\",\"buildDate\":\"%s\",\"gitCommit\":\"%s\",\"resetReason\":\"%s\",",
#ifdef SH_DEV_OPEN
        "dev-open",
#else
        "release",
#endif
        APP_VERSION, APP_BUILD_DATE " " APP_BUILD_TIME, APP_GIT_COMMIT, getResetReasonStr(esp_reset_reason()));
  o.put("\"boiler\":\"%s\",\"boilerCooldownSec\":%lu,\"pendingAction\":\"%s\",\"pendingWaitSec\":%lu,\"lastTxMs\":%lu,",
        boilerRadio.isLastCommandOn() ? "ON" : "OFF", (unsigned long)bWaitSec, boilerRadio.getPendingActionStr(),
        (unsigned long)bWaitSec, (unsigned long)boilerRadio.getLastTransmissionTime());
  o.put("\"freeHeap\":%lu,\"loopStackFree\":%u,\"controlStackFree\":%u,\"longestTickDelayMs\":%lu,\"longestLoopPassMs\":%lu,\"rssi\":%d,\"uptimeSec\":%lu,\"wifiWatchdog\":\"active\",",  // FR-23: stack watermarks & timing
        (unsigned long)ESP.getFreeHeap(), (unsigned)App::getLoopStackFree(), (unsigned)App::getControlStackFree(),
        (unsigned long)App::getLongestTickDelayMs(), (unsigned long)App::getLongestLoopPassMs(),
        (int)WiFi.RSSI(), (unsigned long)(millis() / 1000));
  o.put("\"time\":\"%s\",\"date\":\"%s\",\"season\":\"%s\",", timeAndWeather.getFormattedTime().c_str(),
        timeAndWeather.getFormattedDate().c_str(), timeAndWeather.getSeason().c_str());
  o.put("\"isHeatingSeason\":%s,\"seasonMode\":\"%s\",\"seasonState\":\"%s\",\"effectiveMode\":\"%s\",\"manualRemainingSec\":%lu,",
        tf(thermalZones.isSeasonHeating()), sm == 1 ? "ON" : sm == 2 ? "OFF" : "AUTO",
        thermalZones.isSeasonHeating() ? "HEATING" : "SUMMER",
        em == ThermalZoneManager::EffMode::Manual ? "MANUAL" : em == ThermalZoneManager::EffMode::Off ? "OFF" : "AUTO",
        (unsigned long)thermalZones.manualRemainingSec());
  o.put("\"outdoorTemp\":%.1f,\"bleScanning\":%s,\"autoMode\":%s,\"boilerDemand\":%s,\"actuatorType\":\"%s\",",
        (double)timeAndWeather.getOutdoorTemperature(), tf(bleScanner.isScanning()), tf(thermalZones.isAutoMode()),
        tf(thermalZones.isBoilerDemanded()), thermalZones.isActuatorNormallyClosed() ? "NC" : "NO");
  o.put("\"pumpEnabled\":%s,\"pumpActive\":%s,\"frostGuardActive\":%s,\"sensorTimeoutSec\":%lu,\"lastAntiSeizeEpoch\":%lu,",
        tf(thermalZones.isPumpControlEnabled()), tf(thermalZones.isPumpActive()), tf(thermalZones.isFrostProtectionActive()),
        (unsigned long)thermalZones.getSensorTimeoutSec(), (unsigned long)thermalZones.getLastAntiSeizeEpoch());
  o.put("\"scheduleEnabled\":%s,\"away\":%s,\"activeLevel\":\"%s\",\"minsToNextSlot\":%ld,\"preHeatActive\":%s,",
        tf(thermalZones.isScheduleEnabled()), tf(thermalZones.isAway()),
        ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()), (long)thermalZones.getMinutesToNextSlot(),
        tf(thermalZones.isPreHeatActive()));
  o.put("\"boilerRuntime\":{\"totalSec\":%lu,\"todaySec\":%lu,\"yesterdaySec\":%lu,\"thisMonthSec\":%lu,\"lastMonthSec\":%lu,\"currentBurnSec\":%lu},",
        (unsigned long)boilerRadio.getRuntimeTotalSec(), (unsigned long)boilerRadio.getRuntimeTodaySec(),
        (unsigned long)boilerRadio.getRuntimeYesterdaySec(), (unsigned long)boilerRadio.getRuntimeThisMonthSec(),
        (unsigned long)boilerRadio.getRuntimeLastMonthSec(), (unsigned long)boilerRadio.getCurrentFlameDurationSec());
  o.text(getGasMeterJson());
  o.put("\"sensors\":[");
  size_t count = bleScanner.getSensorCount();
  size_t written = 0;
  for (size_t i = 0; i < count; i++) {
    const BleSensorData *sd = bleScanner.getSensor(i);
    if (!sd || !sd->valid)
      continue;
    o.put("%s{\"name\":\"%s\",\"mac\":\"%s\",\"temp\":%.2f,\"hum\":%.1f,\"bat\":%d,\"rssi\":%d,\"ageSec\":%lu}",
          written > 0 ? "," : "", sd->name, sd->mac, (double)sd->temperature, (double)sd->humidity, (int)sd->battery, (int)sd->rssi,
          (unsigned long)((millis() - sd->lastSeenMs) / 1000));
    written++;
  }
  o.put("]}");
  o.end();
}

void handleApiScheduleGet() {
  sendCorsHeaders();
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleEnable() {
  sendCorsHeaders();
  if (server.hasArg("enabled")) {
    thermalZones.setScheduleEnabled(server.arg("enabled") == "true" || server.arg("enabled") == "1");
  }
  if (server.hasArg("preheat")) {
    bool pre = (server.arg("preheat") == "true" || server.arg("preheat") == "1");
    thermalZones.setPreHeatEnabled(pre);
  }
  if (server.hasArg("window")) {
    bool win = (server.arg("window") == "true" || server.arg("window") == "1");
    thermalZones.setWindowDetectionEnabled(win);
  }
  if (server.hasArg("away")) {
    thermalZones.setAway(server.arg("away") == "true" || server.arg("away") == "1");
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleLevel() {
  sendCorsHeaders();
  int zone = server.arg("zone").toInt();
  int level = ThermalZoneManager::parseLevel(server.arg("level").c_str());
  if (zone < 1 || level < 0 ||
      !thermalZones.setLevelTemp(zone - 1, (ComfortLevel)level, server.arg("temp").toFloat())) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use zone=1..3, level=day|night|away, temp=5..28\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleSlots() {
  sendCorsHeaders();
  bool weekend = server.arg("weekend") == "true" || server.arg("weekend") == "1";
  if (!thermalZones.setSlots(weekend, server.arg("slots").c_str())) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use slots=HH:MM=day|night|away,... (max 8)\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleReset() {
  sendCorsHeaders();
  thermalZones.resetScheduleToDefault();
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleMode() {
  sendCorsHeaders();
  String m = server.hasArg("mode") ? server.arg("mode") : "";
  if (m.length() == 0 && server.hasArg("plain")) {
    JsonDocument doc;
    if (!deserializeJson(doc, server.arg("plain")) && doc.containsKey("mode")) {
      m = doc["mode"].as<String>();
    }
  }
  if (m.length() > 0) {
    if (!thermalZones.setScheduleModeByName(m.c_str())) {
      server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use mode=5+2|constant|unified|7day\"}"));
      return;
    }
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleMoment() {
  sendCorsHeaders();
  String day = server.arg("day");
  int moment = server.arg("moment").toInt();
  String timeStr = server.arg("time");

  if (server.hasArg("plain")) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (!err) {
      if (doc.containsKey("day")) day = doc["day"].as<String>();
      if (doc.containsKey("moment")) moment = doc["moment"].as<int>();
      if (doc.containsKey("time")) timeStr = doc["time"].as<String>();
      if (doc.containsKey("temps")) {
        JsonObject tempsObj = doc["temps"].as<JsonObject>();
        float tArr[MAX_THERMAL_ZONES];
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) tArr[i] = -1.0f;
        for (JsonPair p : tempsObj) {
          int zId = atoi(p.key().c_str());
          if (zId >= 1 && zId <= (int)thermalZones.getZoneCount()) {
            tArr[zId - 1] = p.value().as<float>();
          }
        }
        int h = 0, m = 0;
        if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2 && moment >= 1 && moment <= 4 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
          if (thermalZones.setMomentForGroup(day.c_str(), (uint8_t)(moment - 1), (uint8_t)h, (uint8_t)m, tArr)) {
            server.send(200, "application/json", thermalZones.getScheduleJson());
            return;
          }
        }
      }
    }
  }

  if (moment >= 1 && moment <= 4) {
    int h = 0, m = 0;
    if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
      float tArr[MAX_THERMAL_ZONES];
      for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String key = "temp_" + String(i + 1);
        tArr[i] = server.hasArg(key) ? server.arg(key).toFloat() : -1.0f;
      }
      if (thermalZones.setMomentForGroup(day.c_str(), (uint8_t)(moment - 1), (uint8_t)h, (uint8_t)m, tArr)) {
        server.send(200, "application/json", thermalZones.getScheduleJson());
        return;
      }
    }
  }

  server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Invalid moment parameters\"}"));
}

void handleApiScheduleNames() {
  sendCorsHeaders();
  if (server.hasArg("plain")) {
    JsonDocument doc;
    if (!deserializeJson(doc, server.arg("plain"))) {
      if (doc.containsKey("names") && doc["names"].is<JsonArray>()) {
        JsonArray arr = doc["names"].as<JsonArray>();
        for (size_t i = 0; i < arr.size() && i < MAX_SCHEDULE_MOMENTS; i++) {
          thermalZones.setMomentName((uint8_t)i, arr[i].as<const char*>());
        }
        server.send(200, "application/json", thermalZones.getScheduleJson());
        return;
      } else if (doc.containsKey("moment") && doc.containsKey("name")) {
        int m = doc["moment"].as<int>();
        if (m >= 1 && m <= MAX_SCHEDULE_MOMENTS) {
          thermalZones.setMomentName((uint8_t)(m - 1), doc["name"].as<const char*>());
          server.send(200, "application/json", thermalZones.getScheduleJson());
          return;
        }
      }
    }
  }
  for (size_t i = 0; i < MAX_SCHEDULE_MOMENTS; i++) {
    String pKey = "p" + String(i + 1);
    if (server.hasArg(pKey)) {
      thermalZones.setMomentName((uint8_t)i, server.arg(pKey).c_str());
    }
  }
  if (server.hasArg("moment") && server.hasArg("name")) {
    int m = server.arg("moment").toInt();
    if (m >= 1 && m <= MAX_SCHEDULE_MOMENTS) {
      thermalZones.setMomentName((uint8_t)(m - 1), server.arg("name").c_str());
    }
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiHistory() {
  sendCorsHeaders();
  if (server.hasArg("time")) {
    uint32_t clientTime = (uint32_t)server.arg("time").toInt();
    if (clientTime > 1700000000UL) {
      telemetryHistory.setLocalTimeFromClient(clientTime);
    }
  }
  String range = server.hasArg("range") ? server.arg("range") : "24h";
  telemetryHistory.streamHistoryJson(server, range);
}

void handleApiHistoryClear() {
  sendCorsHeaders();
  telemetryHistory.clearHistory();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"cleared\":true}"));
}

void handleApiConfigSystemGet() {
  JsonOut o;
  auto pi = [](Param q) { return (long)paramStore.get(q); };
  auto pf = [](Param q) { return (double)paramStore.getf(q); };
  o.put("{\"status\":\"ok\",\"zonesCount\":%u,\"openLoopBypass\":%s,\"actuatorDelaySec\":%lu,\"pumpOverrunSec\":%lu,",
        (unsigned)thermalZones.getZoneCount(), tf(thermalZones.isOpenLoopBypass()),
        (unsigned long)(thermalZones.getInterlockDelayMs() / 1000), (unsigned long)(thermalZones.getPumpOverrunMs() / 1000));
  o.put("\"burnerMinRunSec\":%lu,\"burnerMinPauseSec\":%lu,\"rfMinCooldownSec\":%lu,\"rfSafetyHeartbeatSec\":%lu,\"rfSafetyHeartbeatEnabled\":%s,",
        (unsigned long)(boilerRadio.getMinRunTimeMs() / 1000), (unsigned long)(boilerRadio.getMinPauseTimeMs() / 1000),
        (unsigned long)(boilerRadio.getRfMinCooldownMs() / 1000), (unsigned long)(boilerRadio.getSafetyHeartbeatMs() / 1000),
        tf(boilerRadio.isSafetyHeartbeatEnabled()));
  o.put("\"pumpControlEnabled\":%s,\"pumpRelayPin\":%d,\"actuatorNormallyClosed\":%s,\"freezeProtectionTemp\":%.1f,\"sensorTimeoutSec\":%lu,",
        tf(thermalZones.isPumpControlEnabled()), (int)thermalZones.getPumpRelayPin(), tf(thermalZones.isActuatorNormallyClosed()),
        (double)thermalZones.getFreezeProtectionTemp(), (unsigned long)thermalZones.getSensorTimeoutSec());
  o.put("\"hysteresis\":%.2f,\"windowThreshold\":%.1f,\"boilerSetTemp\":%ld,\"boilerFlowObserved\":%ld,\"gasFlameRate\":%.2f,\"boilerPowerKw\":%ld,",
        (double)thermalZones.getHysteresis(), (double)thermalZones.getWindowThreshold(), pi(Param::BoilerSetTemp),
        pi(Param::BoilerFlowObserved), pf(Param::GasFlameRate), pi(Param::BoilerPowerKw));
  o.put("\"gasPrice\":%.2f,\"validMin\":%.1f,\"validMax\":%.1f,\"maxJump\":%.1f,\"manualTimeoutSec\":%ld,\"boostDurationSec\":%ld,",
        pf(Param::GasPrice), pf(Param::ValidMin), pf(Param::ValidMax), pf(Param::MaxJump), pi(Param::ManualTimeout), pi(Param::BoostDuration));
  o.put("\"heatLimit\":%ld,\"seasonOnDays\":%ld,\"seasonOffDays\":%ld,\"heatingType\":%ld,\"boilerLinkType\":%ld,\"faultDuty\":%ld,\"faultFrostOutdoor\":%ld,\"adaptiveWindow\":%s,\"exerciseIdleDays\":%ld,\"selfTestBurn\":%ld,\"overheatMargin\":%.1f,\"overheatTime\":%ld,\"noEffectDuty\":%ld,\"noEffectTime\":%ld,\"noEffectRise\":%.1f,\"zones\":[", pi(Param::HeatLimit), pi(Param::SeasonOnDays),
        pi(Param::SeasonOffDays), pi(Param::HeatingType), pi(Param::BoilerLinkType), pi(Param::FaultDuty), pi(Param::FaultFrostOutdoor), tf(thermalZones.getHeatingType() == HEATING_TYPE_FLOOR), pi(Param::ExerciseIdleDays), pi(Param::SelfTestBurn), pf(Param::OverheatMargin), pi(Param::OverheatTime), pi(Param::NoEffectDuty), pi(Param::NoEffectTime), pf(Param::NoEffectRise));
  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    const auto& z = thermalZones.getConfig(i);
    o.put("%s{\"id\":%d,\"name\":\"%s\",\"mac\":\"%s\",\"relayPin\":%d,\"enabled\":%s}", i > 0 ? "," : "", (int)z.id, z.name,
          z.sensorMac, (int)z.relayPin, tf(z.enabled));
  }
  o.put("]}");
  o.end();
}

// FR-42: numeric settings of requirements §6, by their API name.
struct ConfigArg {
  const char* arg;
  Param param;
};
static const ConfigArg CONFIG_ARGS[] = {
    {"actuatorDelaySec", Param::InterlockDelay},
    {"pumpOverrunSec", Param::Overrun},
    {"burnerMinRunSec", Param::BurnerMinRun},
    {"burnerMinPauseSec", Param::BurnerMinPause},
    {"rfSafetyHeartbeatSec", Param::HbInterval},
    {"freezeProtectionTemp", Param::FrostTemp},
    {"sensorTimeoutSec", Param::SensorTimeout},
    {"hysteresis", Param::Hysteresis},
    {"windowThreshold", Param::WindowThreshold},
    {"boilerSetTemp", Param::BoilerSetTemp},
    {"boilerFlowObserved", Param::BoilerFlowObserved},
    {"gasPrice", Param::GasPrice},
    {"validMin", Param::ValidMin},
    {"validMax", Param::ValidMax},
    {"maxJump", Param::MaxJump},
    {"manualTimeoutSec", Param::ManualTimeout},
    {"boostDurationSec", Param::BoostDuration},
    {"heatLimit", Param::HeatLimit},
    {"seasonOnDays", Param::SeasonOnDays},
    {"seasonOffDays", Param::SeasonOffDays},
    {"heatingType", Param::HeatingType},
    {"boilerLinkType", Param::BoilerLinkType},
    {"faultDuty", Param::FaultDuty},
    {"faultFrostOutdoor", Param::FaultFrostOutdoor},
    {"hbInterval", Param::HbInterval},
    {"boilerLinkHeartbeat", Param::BoilerLinkHeartbeat},
    {"exerciseIdleDays", Param::ExerciseIdleDays},
    {"selfTestBurn", Param::SelfTestBurn},
    {"overheatMargin", Param::OverheatMargin},
    {"overheatTime", Param::OverheatTime},
    {"noEffectDuty", Param::NoEffectDuty},
    {"noEffectTime", Param::NoEffectTime},
    {"noEffectRise", Param::NoEffectRise},
};
static const char* CONFIG_FLAGS[] = {"openLoopBypass", "rfSafetyHeartbeatEnabled", "pumpControlEnabled",
                                     "actuatorNormallyClosed", "boilerLinkHeartbeat"};

static bool isNumber(const String& v) {
  if (v.length() == 0) return false;
  char* end = nullptr;
  strtod(v.c_str(), &end);
  return end && *end == '\0';
}

// A value must lie exactly on the parameter's step (1 s, 0.1 C, 0.01 C) and inside its §6 range; nothing is
// rounded into range (FR-42).
static bool validParamValue(Param p, const String& v) {
  if (!isNumber(v)) return false;
  double scaled = strtod(v.c_str(), nullptr) * ParamStore::def(p).scale;
  int32_t raw = (int32_t)lround(scaled);
  return fabs(scaled - raw) < 1e-6 && paramStore.inRange(p, raw);
}

static bool isFlag(const String& v) {
  return v == "true" || v == "false" || v == "1" || v == "0";
}

static bool flagOn(const String& v) {
  return v == "true" || v == "1";
}

// Every value is checked first; one value outside requirements §6 rejects the whole request (HTTP 400) and
// nothing is changed (FR-42).
void handleApiConfigSystemPost() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  String bad;
  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    if (name == "plain") continue;
    String v = server.arg(i);
    bool known = false, ok = false;
    for (const auto& c : CONFIG_ARGS) {
      if (name == c.arg) {
        known = true;
        ok = validParamValue(c.param, v);
      }
    }
    for (const char* f : CONFIG_FLAGS) {
      if (name == f) {
        known = true;
        ok = isFlag(v);
      }
    }
    // FR-11.3: the pump relay must not be a zone output
    if (name == "pumpControlEnabled" && ok && flagOn(v) && !thermalZones.isPumpControlEnabled() &&
        thermalZones.zoneUsingRelay(thermalZones.getPumpRelayPin(), SIZE_MAX) >= 0)
      ok = false;
    if (!known || !ok) bad += (bad.length() ? ",\"" : "\"") + name + "\"";
  }
  if (!bad.length() && (server.hasArg("validMin") || server.hasArg("freezeProtectionTemp"))) {  // FR-53: validMin < frostTemp
    float vmin = server.hasArg("validMin") ? server.arg("validMin").toFloat() : paramStore.getf(Param::ValidMin);
    float frost = server.hasArg("freezeProtectionTemp") ? server.arg("freezeProtectionTemp").toFloat() : thermalZones.getFreezeProtectionTemp();
    if (vmin > frost - 0.05f) bad = server.hasArg("validMin") ? "\"validMin\"" : "\"freezeProtectionTemp\"";
  }
  if (bad.length()) {
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Unknown, read-only or out of range (requirements \u00a76); "
                "nothing changed\",\"fields\":[" + bad + "]}");
    return;
  }

  if (server.hasArg("actuatorDelaySec")) thermalZones.setInterlockDelaySec(server.arg("actuatorDelaySec").toInt());
  if (server.hasArg("pumpOverrunSec")) thermalZones.setOverrunSec(server.arg("pumpOverrunSec").toInt());
  if (server.hasArg("burnerMinRunSec")) boilerRadio.setMinRunSec(server.arg("burnerMinRunSec").toInt());
  if (server.hasArg("burnerMinPauseSec")) boilerRadio.setMinPauseSec(server.arg("burnerMinPauseSec").toInt());
  if (server.hasArg("rfSafetyHeartbeatSec")) boilerRadio.setHeartbeatSec(server.arg("rfSafetyHeartbeatSec").toInt());
  if (server.hasArg("validMin") && server.arg("validMin").toFloat() < paramStore.getf(Param::ValidMin))
    paramStore.setf(Param::ValidMin, server.arg("validMin").toFloat());  // lowered first, so a lower frostTemp fits
  if (server.hasArg("freezeProtectionTemp")) thermalZones.setFreezeProtectionTemp(server.arg("freezeProtectionTemp").toFloat());
  if (server.hasArg("sensorTimeoutSec")) thermalZones.setSensorTimeoutSec(server.arg("sensorTimeoutSec").toInt());
  if (server.hasArg("hysteresis")) thermalZones.setHysteresis(server.arg("hysteresis").toFloat());
  if (server.hasArg("windowThreshold")) thermalZones.setWindowThreshold(server.arg("windowThreshold").toFloat());
  if (server.hasArg("boilerSetTemp")) paramStore.set(Param::BoilerSetTemp, server.arg("boilerSetTemp").toInt());
  if (server.hasArg("boilerFlowObserved")) paramStore.set(Param::BoilerFlowObserved, server.arg("boilerFlowObserved").toInt());
  if (server.hasArg("gasPrice")) paramStore.setf(Param::GasPrice, server.arg("gasPrice").toFloat());
  for (size_t k = 0; k < sizeof(CONFIG_ARGS) / sizeof(CONFIG_ARGS[0]); k++)  // plain parameters (steps 6, 8): the store checks the range
    if (CONFIG_ARGS[k].param >= Param::ValidMin && server.hasArg(CONFIG_ARGS[k].arg))
      paramStore.setf(CONFIG_ARGS[k].param, server.arg(CONFIG_ARGS[k].arg).toFloat());
  if (server.hasArg("openLoopBypass")) thermalZones.setOpenLoopBypass(flagOn(server.arg("openLoopBypass")));
  if (server.hasArg("rfSafetyHeartbeatEnabled")) boilerRadio.setSafetyHeartbeatEnabled(flagOn(server.arg("rfSafetyHeartbeatEnabled")));
  if (server.hasArg("boilerLinkHeartbeat")) boilerRadio.setSafetyHeartbeatEnabled(flagOn(server.arg("boilerLinkHeartbeat")));
  if (server.hasArg("rfSafetyHeartbeatSec")) boilerRadio.setHeartbeatSec(server.arg("rfSafetyHeartbeatSec").toInt());
  if (server.hasArg("hbInterval")) boilerRadio.setHeartbeatSec(server.arg("hbInterval").toInt());
  if (server.hasArg("boilerLinkType")) boilerRadio.setLinkType((BoilerLinkType)server.arg("boilerLinkType").toInt());
  if (server.hasArg("pumpControlEnabled")) thermalZones.setPumpControlEnabled(flagOn(server.arg("pumpControlEnabled")));
  if (server.hasArg("actuatorNormallyClosed")) thermalZones.setActuatorNormallyClosed(flagOn(server.arg("actuatorNormallyClosed")));
  if (server.hasArg("heatingType")) thermalZones.setHeatingType(server.arg("heatingType").toInt());
  handleApiConfigSystemGet();
}

// FR-65.1: the settings file. No Access-Control-Allow-Origin header (FR-65.7).
void handleApiConfigExport() {
  server.send(200, "application/json", configExportJson());
}

// FR-65.7: checks in this order, the first that fails answers and nothing is changed: media type (415), size (413),
// JSON object with "format": 1 (400, inside configImport). Authentication (401, FR-58) comes with migration step 9.
void handleApiConfigImport() {
  if (!requireAuth()) return;
  String type = server.header("Content-Type");
  type.toLowerCase();
  type.trim();
  bool json = type.startsWith("application/json") && (type.length() == 16 || type[16] == ';' || type[16] == ' ');
  if (!json) {
    server.send(415, "application/json", F("{\"status\":\"error\",\"message\":\"Content-Type must be application/json; nothing changed\"}"));
    return;
  }
  const String& body = server.arg("plain");
  if (body.length() > CONFIG_IMPORT_MAX_BYTES) {
    server.send(413, "application/json", F("{\"status\":\"error\",\"message\":\"Body over 16384 bytes; nothing changed\"}"));
    return;
  }
  String answer;
  int code = configImport(body, answer);
  server.send(code, "application/json", answer);
}

// FR-59: change credentials via API without re-flashing
void handleApiConfigCredentials() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"missing user or pass (FR-59)\"}"));
    return;
  }
  String user = server.arg("user");
  String pass = server.arg("pass");
  user.trim();
  pass.trim();
  if (user.length() == 0 || user.length() > 31 || pass.length() < 8 || pass.length() > 63) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"user must be 1-31 chars and pass 8-63 chars (FR-59)\"}"));
    return;
  }
  if (commands.setCredentials(user.c_str(), pass.c_str(), true)) {
    server.send(200, "application/json", String("{\"status\":\"ok\",\"user\":\"") + user + "\"}");
  } else {
    server.send(500, "application/json", F("{\"status\":\"error\",\"error\":\"failed saving credentials\"}"));
  }
}

// FR-58: Lightweight authentication verification endpoint
void handleApiAuthVerify() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"authenticated\":true}"));
}

// FR-43: only in MANUAL mode, only an output of a zone or the pump, respecting NC / NO (FR-15.1)
void handleApiRelayTest() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  if (!server.hasArg("pin")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Missing 'pin' parameter.\"}"));
    return;
  }
  long pin = server.arg("pin").toInt();
  if (pin < 0 || pin > 255 || !ParamStore::validRelayPin((uint8_t)pin)) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Invalid relay pin. Allowed: 32, 33, 25, 26.\"}"));
    return;
  }
  int zone = thermalZones.zoneUsingRelay((uint8_t)pin, SIZE_MAX);
  bool isPump = zone < 0 && thermalZones.isPumpControlEnabled() && (uint8_t)pin == thermalZones.getPumpRelayPin();
  if (thermalZones.isAutoMode() || (zone < 0 && !isPump)) {
    server.send(409, "application/json",
                F("{\"status\":\"error\",\"message\":\"Relay test needs MANUAL mode and an output of a zone or the pump\"}"));
    return;
  }
  bool state = (server.arg("state") == "1" || server.arg("state") == "true" || server.arg("state") == "on");
  if (zone >= 0) thermalZones.setRelayManual((size_t)zone, state);
  else thermalZones.setPumpManual(state);
  netConsole.printf("[RELAY-TEST] Relay test: pin %ld -> %s\n", pin, state ? "ON" : "OFF");
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

#ifdef SH_DEV_OPEN
// Development build only (FR-75.6): readings and weather states for the system tests, which cannot reach the board's
// sensors or the Internet otherwise. Not compiled into the release profile.
void handleApiDevSensor() {
  if (!server.hasArg("mac") || !server.hasArg("temp")) {
    server.send(400, "application/json", F("{\"error\":\"mac and temp needed\"}"));
    return;
  }
  String mac = server.arg("mac");
  mac.toUpperCase();
  bleScanner.ingestExternalReading(mac.c_str(), "", server.arg("temp").toFloat(),
                                   server.hasArg("hum") ? server.arg("hum").toFloat() : 50.0f,
                                   (uint8_t)server.arg("bat").toInt(), (int8_t)server.arg("rssi").toInt());
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiDevSeasonDay() {
  if (server.arg("mean") != "none") thermalZones.seasonNewDay(server.arg("mean").toFloat());  // no data: no change
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiDevWeather() {
  if (server.hasArg("failed")) timeAndWeather.devSetPollFailed((uint32_t)server.arg("ageSec").toInt());
  if (server.hasArg("refresh")) timeAndWeather.forceWeatherUpdate();
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}
#endif

void handleApiMaintStatus() {
  sendCorsHeaders();
  const char* gateReason = nullptr;
  bool canOta = maintenance.canStartOta(&gateReason);
  char resp[384];
  snprintf(resp, sizeof(resp),
           "{\"otaQuietGate\":{\"canStartOta\":%s,\"boilerOff\":%s,\"reason\":\"%s\"},"
           "\"ota\":{\"rollbackSupported\":%s,\"appValid\":%s},"
           "\"exercise\":{\"running\":%s,\"remainingSec\":%u,\"lastEpoch\":%u}}",
           canOta ? "true" : "false",
           !boilerRadio.isLastCommandOn() ? "true" : "false",
           gateReason ? gateReason : "OK",
           maintenance.isRollbackSupported() ? "true" : "false",
           maintenance.isAppValid() ? "true" : "false",
           maintenance.isExerciseRunning() ? "true" : "false",
           (unsigned)maintenance.getExerciseRemainingSec(),
           (unsigned)maintenance.getLastExerciseEpoch());
  server.send(200, "application/json", resp);
}

void handleApiMaintSelfTest() {
  sendCorsHeaders();
  bool includeBurn = server.hasArg("burn") && server.arg("burn") == "1";
  const auto& rep = maintenance.runSelfTest(includeBurn);
  char resp[768];
  int n = snprintf(resp, sizeof(resp), "{\"timestamp\":%u,\"allPassed\":%s,\"checks\":[",
                   (unsigned)rep.timestamp, rep.allPassed ? "true" : "false");
  for (uint8_t i = 0; i < rep.checkCount; i++) {
    n += snprintf(resp + n, sizeof(resp) - n, "%s{\"name\":\"%s\",\"ok\":%s,\"detail\":\"%s\"}",
                  i > 0 ? "," : "", rep.checks[i].name, rep.checks[i].ok ? "true" : "false", rep.checks[i].detail);
  }
  snprintf(resp + n, sizeof(resp) - n, "]}");
  server.send(200, "application/json", resp);
}

void handleApiMaintExercise() {
  if (!requireAuth()) return;
  sendCorsHeaders();
  maintenance.triggerManualExercise();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"message\":\"Valve exercise triggered (5 min)\"}"));
}

void registerWebRoutes() {
  static const char* headerKeys[] = {"Content-Type", "Authorization", "Origin"};  // FR-58, FR-65.7, FR-75.1
  server.collectHeaders(headerKeys, 2);
  // Web Server routes
#ifdef SH_DEV_OPEN
  server.on("/api/dev/sensor", HTTP_POST, handleApiDevSensor);
  server.on("/api/dev/weather", HTTP_POST, handleApiDevWeather);
  server.on("/api/dev/season-day", HTTP_POST, handleApiDevSeasonDay);
#endif
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/setup/location", handleApiSetupLocation);
  server.on("/api/setup/discovered-sensors", HTTP_GET,
            handleApiSetupDiscoveredSensors);
  server.on("/api/setup/map-zone", HTTP_POST, handleApiSetupMapZone);
  server.on("/api/setup/zone-add", HTTP_POST, handleApiSetupZoneAdd);
  server.on("/api/setup/zone-delete", HTTP_POST, handleApiSetupZoneDelete);
  server.on("/api/setup/sensor-search", HTTP_POST, handleApiSetupSensorSearch);
  server.on("/api/setup/time", HTTP_GET, handleApiSetupTime);
  server.on("/api/boiler/on", HTTP_POST, handleApiOn);
  server.on("/api/boiler/off", HTTP_POST, handleApiOff);
  server.on("/api/boiler/cancel", HTTP_POST, handleApiBoilerCancelPending);
  server.on("/api/boiler/runtime/reset", HTTP_POST, handleApiBoilerResetRuntime);
  server.on("/api/boiler/gas-meter", HTTP_POST, handleApiBoilerGasMeter);
  server.on("/api/boiler/gas-readings", HTTP_GET, handleApiGasReadings);
  server.on("/api/boiler/status", HTTP_GET, handleApiStatus);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/zones", HTTP_GET, handleApiZones);
  server.on("/api/zones/setpoint", HTTP_POST, handleApiZoneSetpoint);
  server.on("/api/zones/toggle", HTTP_POST, handleApiZoneToggle);
  server.on("/api/zones/hold/clear", HTTP_POST, handleApiZoneHoldClear);
  server.on("/api/zones/window/clear", HTTP_POST, handleApiZoneWindowClear);
  server.on("/api/zones/window/threshold", HTTP_POST, handleApiZoneWindowThreshold);
  server.on("/api/zones/sensor", HTTP_POST, handleApiZoneSensor);
  server.on("/api/mode", HTTP_POST, handleApiMode);
  server.on("/api/season", HTTP_POST, handleApiSeason);
  server.on("/api/ble/sensors", HTTP_GET, handleApiBleSensors);
  server.on("/api/diag/status", HTTP_GET, handleApiDiagStatus);
  server.on("/api/diag/inject", HTTP_GET, handleApiDiagInjectGet);
  server.on("/api/diag/inject", HTTP_POST, handleApiDiagInjectPost);
  server.on("/api/diag/inject/clear", HTTP_POST, handleApiDiagInjectClear);
  server.on("/api/diag/events", HTTP_GET, handleApiDiagEvents);
  server.on("/api/diag/log", HTTP_GET, handleApiDiagLog);
  server.on("/api/diag/mem", HTTP_GET, handleApiDiagMem);
  server.on("/api/web/update", HTTP_POST, handleWebUpdateDone, handleWebUpdateUpload);
  server.on("/api/web/remove", HTTP_POST, handleWebRemove);
  server.on("/api/diag/selftest", HTTP_GET, handleApiDiagSelfTest);
  server.on("/api/diag/clear", HTTP_POST, handleApiDiagClear);
  server.on("/api/diag/exercise", HTTP_POST, handleApiDiagExercise);
  server.on("/api/setup/actuator-type", handleApiSetupActuatorType);
  server.on("/api/setup/pump", handleApiSetupPump);
  server.on("/api/setup/sensor-timeout", handleApiSetupSensorTimeout);
  server.on("/api/schedule", HTTP_GET, handleApiScheduleGet);
  server.on("/api/schedule/enable", HTTP_POST, handleApiScheduleEnable);
  server.on("/api/schedule/level", HTTP_POST, handleApiScheduleLevel);
  server.on("/api/schedule/slots", HTTP_POST, handleApiScheduleSlots);
  server.on("/api/schedule/reset", HTTP_POST, handleApiScheduleReset);
  server.on("/api/schedule/mode", HTTP_POST, handleApiScheduleMode);
  server.on("/api/schedule/moment", HTTP_POST, handleApiScheduleMoment);
  server.on("/api/schedule/names", HTTP_POST, handleApiScheduleNames);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/history/clear", HTTP_POST, handleApiHistoryClear);
  server.on("/api/config/system", HTTP_GET, handleApiConfigSystemGet);
  server.on("/api/config/system", HTTP_POST, handleApiConfigSystemPost);
  server.on("/api/relay/test", HTTP_POST, handleApiRelayTest);
  server.on("/api/config/export", HTTP_GET, handleApiConfigExport);
  server.on("/api/config/import", HTTP_POST, handleApiConfigImport);
  server.on("/api/config/credentials", HTTP_POST, handleApiConfigCredentials);
  server.on("/api/auth/verify", HTTP_POST, handleApiAuthVerify);
  server.on("/api/auth/verify", HTTP_GET, handleApiAuthVerify);
  server.on("/api/maint/status", HTTP_GET, handleApiMaintStatus);
  server.on("/api/maint/selftest", HTTP_GET, handleApiMaintSelfTest);
  server.on("/api/maint/selftest", HTTP_POST, handleApiMaintSelfTest);
  server.on("/api/maint/exercise", HTTP_POST, handleApiMaintExercise);
  server.on("/api/plant/status", HTTP_GET, handleApiPlantStatus);
}
