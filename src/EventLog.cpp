#include "EventLog.h"
#include "NetworkConsole.h"
#include <LittleFS.h>

EventLog eventLog;

// FR-62.1: two append-only files; when the one in use is full the other is erased and used
static const char *const EV_FILES[2] = {"/evlog_a.bin", "/evlog_b.bin"};
static const uint16_t EV_MAGIC = 0x4C45;  // "EL"
static const size_t EV_HEADER = 16;       // magic, sequence, rest unused
static const uint16_t EV_SEG_ENTRIES = 2048;
static const uint32_t EV_FS_FLOOR = 24576;  // free LittleFS kept for the gas log and the page
static const uint8_t EV_BATCH = 8;
static const uint32_t EV_BATCH_MS = 30000UL;
static const uint32_t EV_DEDUPE_MS = 10000UL;

static const char *const TYPE_NAMES[EV_TYPE_COUNT] = {
    "", "out", "rf", "refused", "pending", "interlock", "overrun", "frost", "sensor",
    "mode", "boot", "warmup", "time-sync", "dtc", "dropped"};

static const char *const REASON_NAMES[RS_COUNT] = {
    "none", "boot", "demand", "no-demand", "manual", "frost", "sensor-loss", "overrun-end", "exercise",
    "config", "safety", "user", "queue", "heartbeat", "guard-run", "guard-pause", "guard-cooldown",
    "interlock", "demand-lost", "pump-guard", "wifi"};

const char *EventLog::typeName(uint8_t type) { return type < EV_TYPE_COUNT ? TYPE_NAMES[type] : ""; }
const char *EventLog::reasonName(uint8_t reason) { return reason < RS_COUNT ? REASON_NAMES[reason] : "none"; }

bool EventLog::readHeader(uint8_t seg, uint32_t &seq, uint16_t &count) {
  File f = LittleFS.open(EV_FILES[seg], "r");
  if (!f)
    return false;
  uint8_t h[EV_HEADER];
  size_t size = f.size();
  bool ok = size >= EV_HEADER && f.read(h, EV_HEADER) == (int)EV_HEADER &&
            (uint16_t)(h[0] | (h[1] << 8)) == EV_MAGIC;
  f.close();
  if (!ok)
    return false;
  seq = h[2] | (h[3] << 8) | ((uint32_t)h[4] << 16) | ((uint32_t)h[5] << 24);
  size_t n = (size - EV_HEADER) / sizeof(EventEntry);
  count = n > EV_SEG_ENTRIES ? EV_SEG_ENTRIES : (uint16_t)n;
  if ((size - EV_HEADER) % sizeof(EventEntry))
    _needNew = true;  // a torn last entry: appending would misalign, so the next write starts the other file
  return true;
}

void EventLog::begin() {
  _mounted = LittleFS.begin(true);
  if (!_mounted) {
    netConsole.println(F("[EVLOG][WARN] LittleFS not mounted, the event log stays in RAM"));
    return;
  }
  uint32_t s[2] = {0, 0};
  uint16_t c[2] = {0, 0};
  bool tornA = false;
  bool v0 = readHeader(0, s[0], c[0]);
  tornA = _needNew;
  _needNew = false;
  bool v1 = readHeader(1, s[1], c[1]);
  bool tornB = _needNew;
  _needNew = false;
  if (!v0 && !v1) {
    _seg = 0;
    _seq = 0;
    _segCount = _otherCount = 0;
    _needNew = true;
  } else if (v0 && (!v1 || s[0] > s[1])) {
    _seg = 0;
    _seq = s[0];
    _segCount = c[0];
    _otherCount = v1 ? c[1] : 0;
    _needNew = tornA;
  } else {
    _seg = 1;
    _seq = s[1];
    _segCount = c[1];
    _otherCount = v0 ? c[0] : 0;
    _needNew = tornB;
  }
}

// The file in use becomes the old one; the older file is erased and gets the next sequence number
bool EventLog::rotate() {
  uint8_t other = 1 - _seg;
  LittleFS.remove(EV_FILES[other]);
  File f = LittleFS.open(EV_FILES[other], "w");
  if (!f)
    return false;
  uint8_t h[EV_HEADER] = {0};
  uint32_t seq = _seq + 1;
  h[0] = EV_MAGIC & 0xFF;
  h[1] = EV_MAGIC >> 8;
  h[2] = seq & 0xFF;
  h[3] = (seq >> 8) & 0xFF;
  h[4] = (seq >> 16) & 0xFF;
  h[5] = (seq >> 24) & 0xFF;
  bool ok = f.write(h, EV_HEADER) == EV_HEADER;
  f.close();
  if (!ok)
    return false;
  _otherCount = _segCount;
  _seg = other;
  _seq = seq;
  _segCount = 0;
  _needNew = false;
  return true;
}

void EventLog::add(uint8_t type, uint8_t subject, uint8_t reason, uint8_t zone, uint16_t oldV, uint16_t newV,
                   bool dedupe) {
  uint32_t nowMs = millis();
  if (dedupe) {
    uint32_t sig = ((uint32_t)type << 24) | ((uint32_t)subject << 16) | ((uint32_t)reason << 8) | (newV & 0xFF);
    if (_hasSig && sig == _lastSig && (uint32_t)(nowMs - _lastSigMs) < EV_DEDUPE_MS)
      return;
    _lastSig = sig;
    _lastSigMs = nowMs;
    _hasSig = true;
  }
  EventEntry &e = _q[_qHead];
  memset(&e, 0, sizeof(e));
  time_t epoch = time(nullptr);
  bool valid = epoch > 1700000000;
  e.time = valid ? (uint32_t)epoch : nowMs / 1000;
  e.flags = valid ? 1 : 0;
  e.type = type;
  e.subject = subject;
  e.reason = reason;
  e.zone = zone;
  e.ctx = _ctxFn ? _ctxFn() : 0;
  e.oldV = oldV;
  e.newV = newV;
  if (_qCount == QUEUE_SIZE) {
    if (_dropped < 0xFFFF)
      _dropped++;  // the slot of the oldest entry is overwritten
  } else {
    if (_qCount == 0)
      _firstQueuedMs = nowMs;
    _qCount++;
  }
  _qHead = (_qHead + 1) % QUEUE_SIZE;
}

// One append per batch, from loop(); a failed write keeps the entries and tries again 30 s later
void EventLog::flush() {
  if (!_mounted || !_qCount)
    return;
  uint32_t nowMs = millis();
  if (_qCount < EV_BATCH && (uint32_t)(nowMs - _firstQueuedMs) < EV_BATCH_MS)
    return;
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < EV_FS_FLOOR) {
    _dropped = (uint32_t)_dropped + _qCount > 0xFFFF ? 0xFFFF : _dropped + _qCount;
    _qCount = 0;
    return;
  }
  uint8_t tail = (_qHead + QUEUE_SIZE - _qCount) % QUEUE_SIZE;
  uint8_t done = 0;
  while (done < _qCount) {
    if (_needNew || _segCount >= EV_SEG_ENTRIES) {
      if (!rotate())
        break;
    }
    uint8_t n = (uint8_t)min((size_t)(EV_SEG_ENTRIES - _segCount), (size_t)(_qCount - done));
    File f = LittleFS.open(EV_FILES[_seg], "a");
    if (!f)
      break;
    uint8_t written = 0;
    while (written < n &&
           f.write((const uint8_t *)&_q[(tail + done + written) % QUEUE_SIZE], sizeof(EventEntry)) ==
               sizeof(EventEntry))
      written++;
    f.close();
    _segCount += written;
    done += written;
    if (written < n)
      break;
  }
  _qCount -= done;
  if (_qCount)
    _firstQueuedMs = nowMs;
}

void EventLog::update() {
  if (_dropped && _qCount < QUEUE_SIZE - 1) {
    uint16_t d = _dropped;
    _dropped = 0;
    add(EV_DROPPED, 0, RS_NONE, 0, 0, d);
  }
  flush();
}

size_t EventLog::forEachNewest(size_t n, EventVisitFn fn, void *arg) {
  size_t done = 0;
  for (uint8_t i = 0; i < _qCount && done < n; i++) {
    fn(_q[(_qHead + QUEUE_SIZE - 1 - i) % QUEUE_SIZE], arg);
    done++;
  }
  if (!_mounted)
    return done;
  for (uint8_t pass = 0; pass < 2 && done < n; pass++) {
    File f = LittleFS.open(EV_FILES[pass == 0 ? _seg : 1 - _seg], "r");
    if (!f)
      continue;
    size_t size = f.size();
    size_t cnt = size > EV_HEADER ? (size - EV_HEADER) / sizeof(EventEntry) : 0;
    EventEntry buf[8];
    while (cnt > 0 && done < n) {
      size_t blk = cnt < 8 ? cnt : 8;
      size_t first = cnt - blk;
      f.seek(EV_HEADER + first * sizeof(EventEntry));
      if (f.read((uint8_t *)buf, blk * sizeof(EventEntry)) != (int)(blk * sizeof(EventEntry)))
        break;
      for (size_t k = blk; k > 0 && done < n; k--) {
        fn(buf[k - 1], arg);
        done++;
      }
      cnt = first;
    }
    f.close();
  }
  return done;
}
