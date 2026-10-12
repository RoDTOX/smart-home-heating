#include "NetworkConsole.h"
#include <time.h>

NetworkConsole netConsole;

NetworkConsole::NetworkConsole()
    : _telnetServer(TELNET_PORT),
      _webServer(nullptr),
      _commandHandler(nullptr),
      _head(0),
      _count(0),
      _totalBytes(0),
      _atLineStart(true) {
    memset(_logRing, 0, sizeof(_logRing));
}

void NetworkConsole::begin(WebServer* server) {
    _webServer = server;
    _telnetServer.begin();
    _telnetServer.setNoDelay(true);

    if (_webServer) {
        setupWebRoutes();
    }
}

void NetworkConsole::setCommandHandler(CommandCallback cb) {
    _commandHandler = cb;
}

void NetworkConsole::appendToRing(char c) {
    _logRing[_head] = c;
    _head = (_head + 1) % LOG_BUFFER_SIZE;
    if (_count < LOG_BUFFER_SIZE) {
        _count++;
    }
    _totalBytes++;
}

void NetworkConsole::printTimestamp() {
    time_t now = time(nullptr);
    char buf[16];
    if (now > 1700000000) {
        struct tm ti;
        localtime_r(&now, &ti);
        snprintf(buf, sizeof(buf), "[%02d:%02d:%02d] ", ti.tm_hour, ti.tm_min, ti.tm_sec);
    } else {
        uint32_t s = millis() / 1000;
        snprintf(buf, sizeof(buf), "[+%02u:%02u:%02u] ", (s / 3600) % 24, (s / 60) % 60, s % 60);
    }
    rawWrite((const uint8_t*)buf, strlen(buf));
}

void NetworkConsole::rawWrite(const uint8_t *buffer, size_t size) {
    Serial.write(buffer, size);
    for (size_t i = 0; i < size; i++) {
        appendToRing((char)buffer[i]);
    }
    if (_telnetClient && _telnetClient.connected()) {
        _telnetClient.write(buffer, size);
    }
}

size_t NetworkConsole::write(uint8_t c) {
    if (_atLineStart && c != '\r' && c != '\n') {
        _atLineStart = false;
        printTimestamp();
    }
    if (c == '\n') {
        _atLineStart = true;
    }
    rawWrite(&c, 1);
    return 1;
}

size_t NetworkConsole::write(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        write(buffer[i]);
    }
    return size;
}

void NetworkConsole::broadcast(const char* str) {
    if (str) {
        write((const uint8_t*)str, strlen(str));
    }
}

bool NetworkConsole::hasTelnetClient() {
    return (bool)_telnetClient && (_telnetClient.connected() != 0);
}

void NetworkConsole::executeCommand(const String& cmd) {
    String cleanCmd = cmd;
    cleanCmd.trim();
    if (cleanCmd.length() == 0) return;

    this->printf("[CMD] > %s\n", cleanCmd.c_str());

    if (_commandHandler) {
        _commandHandler(cleanCmd);
    }
}

void NetworkConsole::clear() {
    _head = 0;
    _count = 0;
    _atLineStart = true;
    Serial.print("\033[2J\033[H");
    if (_telnetClient && _telnetClient.connected()) {
        _telnetClient.print("\033[2J\033[H");
    }
}

void NetworkConsole::update() {
    // 1. Manage Telnet incoming connections
    if (_telnetServer.hasClient()) {
        if (!_telnetClient || !_telnetClient.connected()) {
            if (_telnetClient) _telnetClient.stop();
            _telnetClient = _telnetServer.available();
            _telnetClient.print("\r\n==================================================\r\n");
            _telnetClient.print("  Smart Heating ESP32 - Virtual Serial Console\r\n");
            _telnetClient.print("==================================================\r\n");
            _telnetClient.print("Type 'HELP' or '?' for available commands.\r\n\r\n> ");
            _telnetRxBuffer = "";
        } else {
            WiFiClient rejected = _telnetServer.available();
            rejected.println("Console busy. Connection closed.");
            rejected.stop();
        }
    }

    // 2. Read incoming characters from Telnet client
    if (_telnetClient && _telnetClient.connected()) {
        while (_telnetClient.available()) {
            char c = (char)_telnetClient.read();
            if (c == '\r' || c == '\n') {
                if (_telnetRxBuffer.length() > 0) {
                    _telnetClient.print("\r\n");
                    String cmd = _telnetRxBuffer;
                    _telnetRxBuffer = "";
                    executeCommand(cmd);
                    _telnetClient.print("\r\n> ");
                }
            } else if (c == 0x08 || c == 0x7F) { // Backspace
                if (_telnetRxBuffer.length() > 0) {
                    _telnetRxBuffer.remove(_telnetRxBuffer.length() - 1);
                    _telnetClient.print("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                _telnetRxBuffer += c;
                _telnetClient.print(c); // Local echo
            }
        }
    }
}

void NetworkConsole::setupWebRoutes() {
    _webServer->on("/console", HTTP_GET, std::bind(&NetworkConsole::handleWebConsolePage, this));
    _webServer->on("/api/console/logs", HTTP_GET, std::bind(&NetworkConsole::handleApiConsoleLogs, this));
    _webServer->on("/api/console/send", HTTP_POST, std::bind(&NetworkConsole::handleApiConsoleSend, this));
}

void NetworkConsole::handleApiConsoleLogs() {
    uint32_t since = _webServer->hasArg("since") ? (uint32_t)_webServer->arg("since").toInt() : 0;
    String output = "";

    // If since == 0, OR if the board rebooted (since > _totalBytes), OR ring wrapped:
    if (since == 0 || since > _totalBytes || (_totalBytes >= since && (_totalBytes - since) >= _count)) {
        // Output all buffered content in chronological order
        output.reserve(_count);
        if (_count < LOG_BUFFER_SIZE) {
            for (size_t i = 0; i < _count; i++) {
                output += _logRing[i];
            }
        } else {
            // Buffer wrapped: start from _head to end, then 0 to _head
            for (size_t i = _head; i < LOG_BUFFER_SIZE; i++) {
                output += _logRing[i];
            }
            for (size_t i = 0; i < _head; i++) {
                output += _logRing[i];
            }
        }
    } else if (since < _totalBytes) {
        size_t delta = _totalBytes - since;
        if (delta > _count) delta = _count;
        output.reserve(delta);
        size_t startPos = (_head + LOG_BUFFER_SIZE - delta) % LOG_BUFFER_SIZE;
        for (size_t i = 0; i < delta; i++) {
            output += _logRing[(startPos + i) % LOG_BUFFER_SIZE];
        }
    }

    _webServer->sendHeader("X-Log-Seq", String(_totalBytes));
    _webServer->sendHeader("Access-Control-Expose-Headers", "X-Log-Seq");
    _webServer->sendHeader("Access-Control-Allow-Origin", "*");
    _webServer->send(200, "text/plain; charset=utf-8", output);
}

void NetworkConsole::handleApiConsoleSend() {
    String cmd = "";
    if (_webServer->hasArg("cmd")) {
        cmd = _webServer->arg("cmd");
    } else if (_webServer->hasArg("plain")) {
        cmd = _webServer->arg("plain");
    }

    executeCommand(cmd);

    _webServer->sendHeader("Access-Control-Allow-Origin", "*");
    _webServer->send(200, "application/json", "{\"status\":\"ok\"}");
}

void NetworkConsole::handleWebConsolePage() {
    String html = F(
        "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Smart Heating - Live Wi-Fi Console</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0;}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,monospace;"
        "background:#090d16;color:#f1f5f9;display:flex;flex-direction:column;height:100vh;padding:12px;}"
        ".header{display:flex;justify-content:space-between;align-items:center;padding:8px 12px;"
        "background:#111827;border-radius:10px;border:1px solid #1f2937;margin-bottom:10px;}"
        ".title{font-size:1.1rem;font-weight:700;color:#38bdf8;display:flex;align-items:center;gap:8px;}"
        ".badge{background:#065f46;color:#34d399;font-size:0.75rem;padding:3px 8px;border-radius:6px;font-weight:600;}"
        ".nav-links{display:flex;gap:12px;font-size:0.85rem;}"
        ".nav-links a{color:#94a3b8;text-decoration:none;transition:color 0.2s;}"
        ".nav-links a:hover{color:#38bdf8;}"
        ".quick-bar{display:flex;gap:6px;overflow-x:auto;padding-bottom:6px;margin-bottom:8px;}"
        ".qbtn{background:#1f2937;color:#e2e8f0;border:1px solid #374151;border-radius:6px;padding:6px 12px;"
        "font-size:0.8rem;font-weight:600;cursor:pointer;white-space:nowrap;transition:all 0.15s;}"
        ".qbtn:hover{background:#374151;color:#ffffff;border-color:#60a5fa;}"
        ".qbtn.on{background:#065f46;border-color:#059669;color:#a7f3d0;}"
        ".qbtn.off{background:#7f1d1d;border-color:#dc2626;color:#fecaca;}"
        ".term-container{flex:1;background:#030712;border:1px solid #1f2937;border-radius:10px;"
        "padding:12px;overflow-y:auto;font-family:'Courier New',Courier,monospace;font-size:0.85rem;"
        "line-height:1.45;color:#e2e8f0;white-space:pre-wrap;word-break:break-all;}"
        ".log-line{padding:1px 0;}"
        ".t-time{color:#64748b;}"
        ".t-thm{color:#fb923c;font-weight:600;}"
        ".t-wth{color:#38bdf8;font-weight:700;}"
        ".t-ble{color:#c084fc;font-weight:600;}"
        ".t-tim{color:#facc15;font-weight:600;}"
        ".t-pas{color:#34d399;font-weight:600;}"
        ".t-dia{color:#f472b6;font-weight:600;}"
        ".t-cmd{color:#22d3ee;font-weight:700;}"
        ".t-cpu{color:#c084fc;font-weight:700;}"
        ".t-mem{color:#f472b6;font-weight:700;}"
        ".t-fls{color:#fb923c;font-weight:700;}"
        ".t-temp{color:#38bdf8;font-weight:700;}"
        ".t-err{color:#f87171;font-weight:700;}"
        ".t-wrn{color:#fbbf24;font-weight:600;}"
        ".t-ok{color:#4ade80;font-weight:700;}"
        ".t-off{color:#f87171;font-weight:700;}"
        ".t-idle{color:#38bdf8;font-weight:700;}"
        ".t-demand{color:#fb923c;font-weight:700;}"
        ".t-auto{color:#22d3ee;font-weight:700;}"
        ".t-man{color:#e879f9;font-weight:700;}"
        ".t-warm{color:#facc15;font-weight:700;}"
        ".b-exc{color:#34d399;font-weight:700;background:rgba(52,211,153,0.15);padding:1px 5px;border-radius:4px;}"
        ".b-good{color:#60a5fa;font-weight:700;background:rgba(96,165,250,0.15);padding:1px 5px;border-radius:4px;}"
        ".b-marg{color:#f59e0b;font-weight:700;background:rgba(245,158,11,0.15);padding:1px 5px;border-radius:4px;}"
        ".b-poor{color:#ef4444;font-weight:700;background:rgba(239,68,68,0.15);padding:1px 5px;border-radius:4px;}"
        ".input-bar{display:flex;gap:8px;margin-top:10px;align-items:center;}"
        ".cmd-input{flex:1;background:#111827;border:1px solid #374151;color:#f9fafb;padding:10px 14px;"
        "border-radius:8px;font-family:'Courier New',Courier,monospace;font-size:0.95rem;outline:none;}"
        ".cmd-input:focus{border-color:#38bdf8;box-shadow:0 0 0 2px rgba(56,189,248,0.2);}"
        ".send-btn{background:#0284c7;color:#ffffff;border:none;border-radius:8px;padding:10px 20px;"
        "font-weight:700;cursor:pointer;font-size:0.9rem;transition:background 0.2s;}"
        ".send-btn:hover{background:#0369a1;}"
        ".controls{display:flex;justify-content:space-between;align-items:center;font-size:0.75rem;color:#64748b;margin-top:6px;padding:0 4px;}"
        "</style></head><body>"
        "<div class='header'>"
        "<div class='title'><span>&gt;_</span> Smart Heating 2-Way Console <span class='badge' id='statusBadge'>LIVE</span></div>"
        "<div class='nav-links'>"
        "<a id='navDash' href='/'>&larr; Dashboard</a>"
        "<a id='navOta' href='/update'>OTA Firmware</a>"
        "<a href='https://pvvx.github.io/ATC_MiThermometer/TelinkMiFlasher.html' target='_blank' rel='noopener' title='Configure Xiaomi BLE sensor RF transmit power and settings'>BLE Sensor Config</a>"
        "</div>"
        "</div>"
        "<div class='quick-bar'>"
        "<button class='qbtn' onclick=\"sendCmd('STATUS')\">STATUS</button>"
        "<button class='qbtn on' onclick=\"sendCmd('ON')\">START BOILER</button>"
        "<button class='qbtn off' onclick=\"sendCmd('OFF')\">STOP BOILER</button>"
        "<button class='qbtn' onclick=\"sendCmd('AUTO')\">AUTO MODE</button>"
        "<button class='qbtn' onclick=\"sendCmd('MANUAL')\">MANUAL MODE</button>"
        "<button class='qbtn' onclick=\"sendCmd('HELP')\">HELP (?)</button>"
        "<button class='qbtn' onclick=\"clearTerm()\">CLEAR</button>"
        "</div>"
        "<div class='term-container' id='terminal'></div>"
        "<form class='input-bar' onsubmit=\"handleSubmit(event)\">"
        "<input type='text' id='cmdInput' class='cmd-input' placeholder=\"Enter command (e.g. STATUS, ON, OFF, SET 1 22.5, HELP)...\" autocomplete='off' autofocus>"
        "<button type='submit' class='send-btn'>Send</button>"
        "</form>"
        "<div class='controls'>"
        "<span>Telnet / Remote Monitor: <b>192.168.1.20:23</b></span>"
        "<label><input type='checkbox' id='autoScroll' checked> Auto-scroll</label>"
        "</div>"
        "<script>"
        "const base = window.location.pathname.replace(/\\/console\\/?$/, '');"
        "const navDash = document.getElementById('navDash');"
        "const navOta = document.getElementById('navOta');"
        "if (navDash) navDash.href = (base || '') + '/';"
        "if (navOta) navOta.href = (base || '') + '/update';"
        "let lastSeq = 0;"
        "let isFetching = false;"
        "let isSending = false;"
        "const term = document.getElementById('terminal');"
        "const cmdInput = document.getElementById('cmdInput');"
        "const autoScroll = document.getElementById('autoScroll');"
        "const history = [];"
        "let histIdx = -1;"
        "function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
        "function colorize(line){"
        "  let s = escHtml(line);"
        "  s = s.replace(/^(\\[\\d{2}:\\d{2}:\\d{2}\\]|\\[\\+\\d{2}:\\d{2}:\\d{2}\\])/, '<span class=\"t-time\">$1</span>');"
        "  s = s.replace(/(\\[Thermal\\]|\\[Warmup\\])/gi, '<span class=\"t-thm\">$1</span>');"
        "  s = s.replace(/(\\[Weather\\]|\\[WEATHER\\])/gi, '<span class=\"t-wth\">$1</span>');"
        "  s = s.replace(/(\\[BLE\\])/gi, '<span class=\"t-ble\">$1</span>');"
        "  s = s.replace(/(\\[Time\\])/gi, '<span class=\"t-tim\">$1</span>');"
        "  s = s.replace(/(\\[Passport\\])/gi, '<span class=\"t-pas\">$1</span>');"
        "  s = s.replace(/(\\[CPU\\])/gi, '<span class=\"t-cpu\">$1</span>');"
        "  s = s.replace(/(\\[MEMORY\\])/gi, '<span class=\"t-mem\">$1</span>');"
        "  s = s.replace(/(\\[FLASH\\])/gi, '<span class=\"t-fls\">$1</span>');"
        "  s = s.replace(/(\\[INIT\\])/gi, '<span class=\"t-tim\">$1</span>');"
        "  s = s.replace(/(\\[DIAG\\]|\\[DTC-[^\\]]+\\]|\\[POST\\])/gi, '<span class=\"t-dia\">$1</span>');"
        "  s = s.replace(/(\\[(?:[A-Z]\\.\\s*)?(?:CMD|Ready|STATUS|SYSTEM|BOILER|HARDWARE|PROTECTIONS|SAFETY|WEATHER|ZONES|Zone\\s*\\d+|HEATING|SENSORS)[^\\]]*\\])/gi, '<span class=\"t-cmd\">$1</span>');"
        "  s = s.replace(/(\\s\\|\\s\\*)/g, ' <span class=\"t-time\">|</span> *');"
        "  s = s.replace(/(\\[EXCELLENT\\]|\\[EXC\\])/g, '<span class=\"b-exc\">$1</span>');"
        "  s = s.replace(/(\\[GOOD\\])/g, '<span class=\"b-good\">$1</span>');"
        "  s = s.replace(/(\\[MARGINAL\\])/g, '<span class=\"b-marg\">$1</span>');"
        "  s = s.replace(/(\\[POOR\\]|\\[DEGRADED\\])/g, '<span class=\"b-poor\">$1</span>');"
        "  s = s.replace(/(\\b(?:ERROR|FAIL|FAILED|CRITICAL)\\b)/gi, '<span class=\"t-err\">$1</span>');"
        "  s = s.replace(/(\\b(?:Warning|WARN)\\b)/gi, '<span class=\"t-wrn\">$1</span>');"
        "  s = s.replace(/(\\b(?:PASS|ONLINE|CONNECTED|ENABLED|RUNNING|STARTED|ACTIVE)\\b)/gi, '<span class=\"t-ok\">$1</span>');"
        "  s = s.replace(/(\\b(?:OPEN|ON)\\b)(?!-)/gi, '<span class=\"t-ok\">$1</span>');"
        "  s = s.replace(/(\\b(?:FAIL|FAILED|OFFLINE|DISCONNECTED|DISABLED|STOPPED|CLOSED|OFF|TIMEOUT)\\b)/gi, '<span class=\"t-off\">$1</span>');"
        "  s = s.replace(/(\\b(?:IDLE|STANDBY)\\b)/gi, '<span class=\"t-idle\">$1</span>');"
        "  s = s.replace(/(\\b(?:DEMAND|HEATING)\\b)/gi, '<span class=\"t-demand\">$1</span>');"
        "  s = s.replace(/(\\b(?:AUTOMATIC|AUTO)\\b)/gi, '<span class=\"t-auto\">$1</span>');"
        "  s = s.replace(/(\\b(?:MANUAL)\\b)/gi, '<span class=\"t-man\">$1</span>');"
        "  s = s.replace(/(\\b(?:WARMUP|SYNCING)\\b|Scanning\\.\\.\\.)/gi, '<span class=\"t-warm\">$1</span>');"
        "  s = s.replace(/(\\b(?:Autumn|Toamna|Winter|Iarna|Spring|Primavara|Summer|Vara)\\b)/gi, '<span class=\"t-auto\">$1</span>');"
        "  s = s.replace(/(-?\\d+\\.?\\d*\\s*°C)/g, '<span class=\"t-temp\">$1</span>');"
        "  return s;"
        "}"
        "async function fetchLogs() {"
        "  if (isFetching) return;"
        "  isFetching = true;"
        "  try {"
        "    const r = await fetch(base + '/api/console/logs?since=' + lastSeq);"
        "    if (!r.ok || r.status !== 200) {"
        "      throw new Error('HTTP ' + r.status);"
        "    }"
        "    const seqHdr = r.headers.get('X-Log-Seq');"
        "    const newSeq = seqHdr ? parseInt(seqHdr, 10) : 0;"
        "    if (lastSeq > 0 && newSeq > 0 && newSeq < lastSeq) {"
        "      term.insertAdjacentHTML('beforeend', '<div class=\"log-line\" style=\"color:#38bdf8;font-weight:bold;border-top:1px dashed #374151;margin:8px 0;padding-top:4px;\">--- [SYSTEM REBOOT DETECTED / NEW FIRMWARE RUNNING] ---</div>');"
        "    }"
        "    if (newSeq > 0) {"
        "      lastSeq = newSeq;"
        "    }"
        "    const txt = await r.text();"
        "    if (txt && txt.length > 0) {"
        "      const lines = txt.split('\\n');"
        "      let html = '';"
        "      for (let i = 0; i < lines.length; i++) {"
        "        if (i === lines.length - 1 && lines[i] === '') continue;"
        "        html += '<div class=\"log-line\">' + colorize(lines[i]) + '</div>';"
        "      }"
        "      term.insertAdjacentHTML('beforeend', html);"
        "      while (term.children.length > 2000) term.removeChild(term.firstChild);"
        "      if (autoScroll.checked) term.scrollTop = term.scrollHeight;"
        "    }"
        "    document.getElementById('statusBadge').textContent = 'CONNECTED';"
        "    document.getElementById('statusBadge').style.background = '#065f46';"
        "  } catch(e) {"
        "    document.getElementById('statusBadge').textContent = 'DISCONNECTED';"
        "    document.getElementById('statusBadge').style.background = '#7f1d1d';"
        "  } finally {"
        "    isFetching = false;"
        "  }"
        "}"
        "async function sendCmd(c) {"
        "  if (!c || isSending) return;"
        "  isSending = true;"
        "  history.unshift(c);"
        "  histIdx = -1;"
        "  try {"
        "    await fetch(base + '/api/console/send?cmd=' + encodeURIComponent(c), {method:'POST'});"
        "    setTimeout(fetchLogs, 150);"
        "  } catch(e) { console.error(e); }"
        "  finally {"
        "    setTimeout(() => { isSending = false; }, 350);"
        "  }"
        "}"
        "function handleSubmit(e) {"
        "  e.preventDefault();"
        "  const val = cmdInput.value.trim();"
        "  if (val) {"
        "    sendCmd(val);"
        "    cmdInput.value = '';"
        "  }"
        "}"
        "function clearTerm() {"
        "  term.innerHTML = '';"
        "}"
        "cmdInput.addEventListener('keydown', (e) => {"
        "  if (e.key === 'ArrowUp') {"
        "    if (histIdx + 1 < history.length) {"
        "      histIdx++;"
        "      cmdInput.value = history[histIdx];"
        "    }"
        "    e.preventDefault();"
        "  } else if (e.key === 'ArrowDown') {"
        "    if (histIdx > 0) {"
        "      histIdx--;"
        "      cmdInput.value = history[histIdx];"
        "    } else {"
        "      histIdx = -1;"
        "      cmdInput.value = '';"
        "    }"
        "    e.preventDefault();"
        "  }"
        "});"
        "setInterval(fetchLogs, 800);"
        "fetchLogs();"
        "</script>"
        "</body></html>"
    );

    _webServer->send(200, "text/html", html);
}
