// ---- Engineering view: every API value with its name, explanation and requirement (FR-8.1)
function flatten(v, path, out) {
  if (v !== null && typeof v === "object") {
    const arr = Array.isArray(v);
    for (const k of Object.keys(v)) flatten(v[k], arr ? `${path}[${k}]` : (path ? `${path}.${k}` : k), out);
  } else out[path] = v;
  return out;
}
const docKey = path => path.replace(/\[\d+\]/g, "[]");
const docOf = path => DOC.doc[docKey(path)] || ["(fără explicație)", ""];

function showValue(path, v) {
  const k = docKey(path);
  if (k === "zones[].state") return `${v} · ${STATE_RO[v] || "stare necunoscută – defect (FR-8.4)"}`;
  if (k === "zones[].relayPin" || k === "pumpRelayPin") { const r = RELAYS.find(([p]) => p === v); return r ? `${v} · ${r[1].split(" (")[0]}` : String(v); }
  if (/Epoch$/.test(k) && v > 0) return `${v} · ${new Date(v * 1000).toLocaleString("ro-RO")}`;
  return typeof v === "string" ? `"${v}"` : String(v);
}

function updateWatch() {
  const now = new Date().toLocaleTimeString("ro-RO");
  const flat = {};
  for (const ep of DOC.endpoints) {
    if (RAW[ep]) for (const [p, v] of Object.entries(flatten(RAW[ep], "", {}))) flat[`${ep} › ${p}`] = v;
  }
  changed = new Set();
  const hadPrev = Object.keys(prevFlat).length > 0;
  for (const [sig, v] of Object.entries(flat)) {
    if (hadPrev && prevFlat[sig] !== v) {
      changed.add(sig);
      const vol = (docOf(sig.split(" › ")[1])[2] === "v");
      if (!vol || $("traceAll").checked) trace.push([now, sig, sig in prevFlat ? prevFlat[sig] : "(nou)", v]);
    }
  }
  if (trace.length > 5000) trace.splice(0, trace.length - 5000);
  prevFlat = flat;
  renderWatch();
  renderTrace();
}

function renderWatch() {
  const f = $("filter").value.trim().toLowerCase();
  let html = "", n = 0, group = "";
  for (const [sig, v] of Object.entries(prevFlat)) {
    const [ep, path] = sig.split(" › ");
    const d = docOf(path);
    if (f && !(sig.toLowerCase().includes(f) || d[0].toLowerCase().includes(f))) continue;
    if (ep !== group) { group = ep; html += `<tr class="grp"><td colspan="3">${esc(ep)}</td></tr>`; }
    html += `<tr data-sig="${esc(sig)}" class="${changed.has(sig) ? "chg" : ""}"><td>${esc(path)}</td><td class="v">${esc(showValue(path, v))}</td><td>${esc(d[0])}</td></tr>`;
    if (openSig === sig) html += `<tr class="det"><td colspan="3">Cerința: ${esc(d[1] || "–")} · sursa: GET ${esc(ep)} · cheie: ${esc(docKey(path))}</td></tr>`;
    n++;
  }
  $("watch").innerHTML = "<tr><th>Semnal (nume API)</th><th>Valoare</th><th>Explicație</th></tr>" + html;
  $("watchInfo").textContent = `${n} valori`;
}

function renderTrace() {
  $("traceInfo").textContent = `${trace.length} schimbări`;
  $("traceTable").innerHTML = "<tr><th>Ora</th><th>Semnal</th><th>Înainte</th><th>Acum</th></tr>" + trace.slice(-300).reverse().map(t =>
    `<tr><td>${t[0]}</td><td>${esc(t[1])}</td><td>${esc(String(t[2]))}</td><td>${esc(String(t[3]))}</td></tr>`).join("");
}

$("watch").addEventListener("click", e => {
  const tr = e.target.closest("tr[data-sig]");
  if (!tr) return;
  openSig = openSig === tr.dataset.sig ? null : tr.dataset.sig;
  renderWatch();
});
$("filter").addEventListener("input", renderWatch);
$("clearTrace").onclick = () => { trace = []; renderTrace(); };
$("csv").onclick = () => {
  const q = s => `"${String(s).replace(/"/g, '""')}"`;
  const csv = "time,signal,before,now\n" + trace.map(t => t.map(q).join(",")).join("\n");
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([csv], {type: "text/csv"}));
  a.download = "trace-" + new Date().toISOString().slice(0, 19).replace(/:/g, "") + ".csv";
  a.click();
  URL.revokeObjectURL(a.href);
};

// FR-10.5: live pin levels read from the chip
function renderPins() {
  const pins = (D && D.pins) || [];
  $("pinsTable").innerHTML = "<tr><th>I/O Funcție</th><th>GPIO</th><th>Stare Fizică</th></tr>" +
    pins.map(p => `<tr><td><b>${p.name}</b></td><td><code>GPIO${p.gpio}</code></td><td><span style="color:${p.level ? 'var(--heat, #f59e0b)' : 'var(--mut, #888)'};font-weight:600">${p.level ? "● 1 (ACTIV / HIGH)" : "○ 0 (INACTIV / LOW)"}</span></td></tr>`).join("");
}

// FR-10.6: Hardware & memory internals
function renderMem() {
  const m = RAW["api/diag/mem"];
  if (!m) return;
  const kb = v => (v / 1024).toFixed(1) + " KB";
  const mb = v => (v / (1024 * 1024)).toFixed(1) + " MB";

  if ($("chipInfoLine")) {
    const tempStr = (typeof m.chipTemp === "number" && m.chipTemp > 0) ? `${m.chipTemp.toFixed(1)}°C` : "N/A";
    $("chipInfoLine").innerHTML = `<b>ESP32-WROOM-32E</b> (D0WD-V3, rev ${m.chipRev !== undefined ? m.chipRev : 3}, ${m.chipCores || 2} nuclee)<br>` +
      `⏱️ Ceas CPU: <b>${m.cpuFreqMHz || 240} MHz</b><br>` +
      `🌡️ Temp. internă cip: <b>${tempStr}</b>`;
  }
  if ($("memInfoLine")) {
    $("memInfoLine").innerHTML = `Liber RAM: <b>${kb(m.free)}</b> / Total: ${kb(m.heapSize)}<br>` +
      `📉 Minim istoric: <b>${kb(m.minFreeEver)}</b><br>` +
      `🧱 Bloc maxim continuu: <b>${kb(m.largestBlock)}</b>`;
  }
  if ($("partInfoLine")) {
    const fsKb = m.fsTotal ? `${kb(m.fsUsed)} / ${kb(m.fsTotal)} (${Math.round((m.fsUsed / m.fsTotal) * 100)}% plin)` : "—";
    $("partInfoLine").innerHTML = `Flash fizic: <b>${m.flashSize ? mb(m.flashSize) : "4.0 MB"}</b> (${m.flashSpeed ? (m.flashSpeed / 1000000) : 40} MHz)<br>` +
      `🚀 Slot OTA activ: <b><code>${m.partRunning || "ota_0"}</code></b> (pasiv: <code>${m.partNext || "ota_1"}</code>)<br>` +
      `📁 LittleFS: <b>${fsKb}</b>`;
  }
}

// Firmware Live Console & Log Stream
let consoleSeq = 0, consolePaused = false, consolePollTimer = null, lastSecEventsFetch = 0;

async function pollConsole() {
  if (consolePaused || document.hidden) return;
  const grpDbg = $("grpDebug");
  if (!grpDbg || !grpDbg.open) return;

  try {
    const res = await fetch(`api/console/logs?since=${consoleSeq}`);
    if (res.ok) {
      const seqHdr = res.headers.get("X-Log-Seq");
      if (seqHdr) consoleSeq = parseInt(seqHdr, 10);
      const text = await res.text();
      if (text) {
        const term = $("consoleTerminal");
        if (term) {
          term.textContent += text;
          if (term.textContent.length > 30000) {
            term.textContent = term.textContent.slice(-20000);
          }
          if ($("consoleAutoScroll") && $("consoleAutoScroll").checked) {
            term.scrollTop = term.scrollHeight;
          }
        }
      }
      if ($("consoleStatus")) $("consoleStatus").textContent = "● Conectat la stream";
    }
    if (Date.now() - lastSecEventsFetch > 12000) {
      lastSecEventsFetch = Date.now();
      loadSecEvents();
    }
  } catch (err) {
    if ($("consoleStatus")) $("consoleStatus").textContent = "○ Rețea offline / reconectare...";
  }
}

let consolePw = null;  // FR-75.5: the password stays in memory, never in the page storage

function postConsole(cmd, pw) {
  const h = { "Content-Type": "application/x-www-form-urlencoded" };
  if (pw) h["Authorization"] = "Basic " + btoa("console:" + pw);
  return fetch("api/console/send", { method: "POST", headers: h, body: `cmd=${encodeURIComponent(cmd)}&since=${consoleSeq}` });
}

async function loadSecEvents() {
  const box = $("secEventsBox");
  if (!box) return;
  try {
    const d = await (await fetch("api/security/events")).json();
    const names = {"login-fail": "autentificare eșuată", "login-ok": "autentificare reușită", "lockout": "blocare", "command": "comandă", "password-change": "parolă schimbată", "factory-reset": "resetare"};
    box.innerHTML = `<div style="font-weight:600;font-size:12px;margin:8px 0 4px 0">Evenimente de securitate (${d.count}):</div>` +
      d.events.slice().reverse().slice(0, 20).map(e => `<div class="s" style="color:var(--mut)">${esc(e.time > 1700000000 ? new Date(e.time * 1000).toLocaleString() : "+" + e.time + " s")} · ${esc(names[e.type] || e.type)} · ${esc(e.source)}${e.client ? " · " + esc(e.client) : ""}</div>`).join("");
  } catch (e) { box.innerHTML = ""; }
}

async function sendConsoleCmd() {
  const inp = $("consoleInput");
  if (!inp || !inp.value.trim()) return;
  const cmd = inp.value.trim();
  inp.value = "";
  const term = $("consoleTerminal");
  if (term) {
    term.textContent += `\n> ${cmd}\n`;
    term.scrollTop = term.scrollHeight;
  }
  try {
    // FR-75.1: HELP and STATUS need no login; any other command asks for the password once (kept in memory only)
    let res = await postConsole(cmd, null);
    if (res.status === 401 || res.status === 429) {
      const msg = res.status === 429 ? "Blocat temporar după încercări greșite." : ((await res.json().catch(() => ({}))).error || "Autentificare necesară");
      if (res.status === 429 || /no console password/.test(msg)) { if (term) term.textContent += `[${msg}]\n`; return; }
      const pw = consolePw || window.prompt("Parola consolei (utilizator: console):");
      if (!pw) { if (term) term.textContent += "[Comandă anulată: fără parolă]\n"; return; }
      res = await postConsole(cmd, pw);
      if (res.status === 401) { consolePw = null; if (term) term.textContent += "[Parolă greșită]\n"; return; }
      consolePw = pw;
    }
    if (res.status === 403) {
      const e = await res.json().catch(() => ({}));
      if (term) term.textContent += `[${e.error || "refuzat"}: PASSWD <veche> <nouă>]\n`;
      return;
    }
    if (res.ok) {
      const seqHdr = res.headers.get("X-Log-Seq");
      if (seqHdr) consoleSeq = parseInt(seqHdr, 10);
      const text = await res.text();
      if (text && term) {
        term.textContent += text;
        term.scrollTop = term.scrollHeight;
      }
    }
  } catch (err) {
    if (term) term.textContent += `[Eroare transmitere comandă: ${err}]\n`;
  }
}

const clearCBtn = $("clearConsoleBtn");
if (clearCBtn) clearCBtn.onclick = () => { if ($("consoleTerminal")) $("consoleTerminal").textContent = ""; };

const pauseCBtn = $("pauseConsoleBtn");
if (pauseCBtn) pauseCBtn.onclick = () => {
  consolePaused = !consolePaused;
  pauseCBtn.textContent = consolePaused ? "Reia stream" : "Pauză stream";
  if ($("consoleStatus")) $("consoleStatus").textContent = consolePaused ? "⏸ Pauzat" : "● Conectat la stream";
  if (!consolePaused) pollConsole();
};

const sendCBtn = $("consoleSendBtn");
if (sendCBtn) sendCBtn.onclick = sendConsoleCmd;

const inpC = $("consoleInput");
if (inpC) inpC.onkeydown = e => { if (e.key === "Enter") { e.preventDefault(); sendConsoleCmd(); } };

const grpDbg = $("grpDebug");
if (grpDbg) {
  grpDbg.addEventListener("toggle", () => {
    if (grpDbg.open) {
      pollConsole();
      if (!consolePollTimer) consolePollTimer = setInterval(pollConsole, 1500);
    } else if (consolePollTimer) {
      clearInterval(consolePollTimer);
      consolePollTimer = null;
    }
  });
}

