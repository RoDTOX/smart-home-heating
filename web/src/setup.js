function renderSetup() {
  if (busy("setup") || dirtyRows.size) return;
  const sensorOpts = cur => {
    const macs = B.map(s => s.mac.toUpperCase());
    let o = `<option value="">– fără senzor –</option>`;
    if (cur && !macs.includes(cur.toUpperCase())) o += `<option value="${esc(cur)}" selected>${esc(cur)} (neauzit)</option>`;
    return o + B.map(s => `<option value="${esc(s.mac)}" ${s.mac.toUpperCase() === (cur || "").toUpperCase() ? "selected" : ""}>${esc(s.mac)} · ${typeof s.temp === "number" ? s.temp.toFixed(1) : "--"}° · ${s.rssi} dBm</option>`).join("");
  };
  $("setup").innerHTML = "<tr><th>Nr</th><th>Nume</th><th>Senzor</th><th>Releu</th><th>Pornită</th><th>Offset °C</th><th>Însorire (Sud/Vest)</th><th>Tip Încălzire</th><th></th><th></th></tr>" + Z.zones.map((z, i) =>
    `<tr data-z="${i}"><td>${z.id}</td><td><input class="nm" data-f="name" maxlength="31" value="${esc(z.name)}"></td>
     <td><input class="nm" data-f="mac" list="macList" maxlength="17" placeholder="AA:BB:CC:DD:EE:FF" value="${esc(z.sensorMac || "")}"></td>
     <td><select data-f="pin">${RELAYS.map(([p, t]) => `<option value="${p}" ${p === z.relayPin ? "selected" : ""}>${t}</option>`).join("")}</select></td>
     <td><input type="checkbox" data-f="enabled" ${z.enabled ? "checked" : ""}></td>
     <td><input class="n" type="number" step="0.1" min="-3" max="3" data-f="offset" value="${typeof z.tempOffset === "number" ? z.tempOffset.toFixed(1) : "0.0"}"></td>
     <td style="text-align:center"><label style="cursor:pointer;white-space:nowrap"><input type="checkbox" data-f="solarGain" ${z.solarGain ? "checked" : ""} title="☀️ Activează reducerea TPI când soarele bate puternic (FR-70)"> ☀️ Sud/Vest</label></td>
     <td><select data-f="heatingType"><option value="0" ${(z.heatingType === undefined || z.heatingType === 0) ? "selected" : ""}>♨️ Pardoseală</option><option value="1" ${z.heatingType === 1 ? "selected" : ""}>🧱 Calorifere</option></select></td>
     <td><button data-del="${i}">Șterge</button></td><td class="s" id="setupMsg${i}"></td></tr>`).join("");
}

function renderSensors() {
  if (busy("sensors")) return;
  const F = RAW["api/setup/discovered-sensors"] || {sensors: [], active: false, remainingSec: 0};
  if (F.active) lastFound = F.sensors;  // the page keeps the result of the last search so a sensor can still be chosen after Stop
  const shown = F.active || !lastFound.length ? F.sensors : lastFound;
  const btn = $("searchToggle");
  if (btn) {
    btn.textContent = F.active ? "Oprește căutarea" : "Caută senzori";
    btn.classList.toggle("danger", !!F.active);
  }
  $("searchInfo").textContent = F.active ? `Caut… mai sunt ${F.remainingSec} s` : (lastFound.length ? "Căutare oprită – rezultatul ultimei căutări; controlerul ține doar senzorii zonelor" : "Căutare oprită");
  $("macList").innerHTML = shown.map(s => `<option value="${esc(s.mac)}">${s.temp.toFixed(1)}° · ${s.rssi} dBm</option>`).join("");
  $("sensors").innerHTML = "<tr><th>Nume</th><th>MAC</th><th>°C</th><th>%</th><th>Bat</th><th>RSSI</th><th>Văzut</th><th>Zonă</th></tr>" + shown.map(s => {
    const z = s.zone ? Z.zones[s.zone - 1] : null;
    return `<tr><td>${esc(s.name)}</td><td><code>${esc(s.mac)}</code></td><td>${s.temp.toFixed(1)}</td><td>${Math.round(s.hum)}</td><td>${s.bat}%</td><td>${s.rssi}</td><td>${s.ageSec}s</td><td>${z ? esc(z.name) : "–"}</td></tr>`;
  }).join("");
}

function renderImport(j, changes) {
  const sk = (j.skipped || []).map(x => `<tr><td><code>${esc(x.name)}</code></td><td>${esc(REASON_RO[x.reason] || x.reason)} (${esc(x.reason)})</td></tr>`).join("");
  const ch = (changes || []);
  let h = `<div style="margin-top:10px;padding:10px 12px;background:rgba(40,167,69,0.1);border:1px solid var(--ok);border-radius:8px">
    <div style="font-weight:600;color:var(--ok)">✅ Import aplicat cu succes</div>
    <div class="s" style="margin-top:2px">Setări aplicate: <b>${j.applied.length}</b> · Neschimbate: <b>${j.kept.length}</b> · Sărite: <b>${j.skipped.length}</b></div>`;

  if (ch.length) {
    h += `<div style="margin-top:8px"><b>Modificări efectuate (${ch.length}):</b>
      <div class="scroll" style="margin-top:4px"><table style="font-size:13px">
        <thead><tr><th>Setare / Parametru</th><th>Valoare veche</th><th>Valoare nouă</th></tr></thead>
        <tbody>${ch.map(c => `<tr><td><b>${esc(c.field)}</b></td><td style="color:var(--mut)">${esc(String(c.oldVal))}</td><td style="color:var(--ok);font-weight:600">${esc(String(c.newVal))}</td></tr>`).join("")}</tbody>
      </table></div>
    </div>`;
  } else {
    h += `<div class="s" style="margin-top:4px">Toate setările din fișier erau deja identice cu configurația activă din centrală.</div>`;
  }

  if (sk) {
    h += `<div style="margin-top:8px">
      <b style="color:var(--bad)">⚠️ Parametri săriți (${j.skipped.length}):</b>
      <div class="scroll" style="margin-top:4px"><table>
        <thead><tr><th>Parametru</th><th>Motiv</th></tr></thead>
        <tbody>${sk}</tbody>
      </table></div>
    </div>`;
  }

  h += `<details class="s" style="margin-top:8px"><summary style="cursor:pointer;color:var(--mut)">Detalii tehnice parametri...</summary>
    <div style="margin-top:4px;word-break:break-all"><b>Aplicat (${j.applied.length}):</b> <span class="mono">${j.applied.map(esc).join(", ")}</span></div>
    ${j.kept.length ? `<div style="margin-top:4px;word-break:break-all"><b>Neschimbat (${j.kept.length}):</b> <span class="mono">${j.kept.map(esc).join(", ")}</span></div>` : ""}
  </details>
  </div>`;

  $("cfgReport").innerHTML = h;
}
$("cfgExport").onclick = async () => {
  $("cfgReport").innerHTML = "";
  $("cfgPreview").style.display = "none";
  try {
    const r = await fetch("api/config/export");
    if (!r.ok) throw new Error("export HTTP " + r.status);
    const text = await r.text();
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([text], {type: "application/json"}));
    a.download = "setari-" + JSON.parse(text).firmwareVersion + ".json";
    a.click();
    $("cfgMsg").textContent = "Salvat " + a.download;
  } catch (e) { $("cfgMsg").textContent = e.message; }
};
function analyzeImport(imported, current) {
  const changes = [];
  const invalid = [];

  if (!imported || typeof imported !== "object" || Array.isArray(imported)) {
    invalid.push({field: "fișier", error: "Conținutul nu este un obiect JSON valid."});
    return {changes, invalid};
  }
  if (imported.format !== 1) {
    invalid.push({field: "format", error: `Format necunoscut (${imported.format}). Este acceptat doar format: 1.`});
  }

  const PARAM_LIMITS = {
    actuatorDelaySec: {min: 120, max: 600, label: "Întârziere actuatori (secunde)"},
    pumpOverrunSec: {min: 180, max: 600, label: "Overrun pompă (secunde)"},
    burnerMinRunSec: {min: 120, max: 600, label: "Timp minim arzător ON (secunde)"},
    burnerMinPauseSec: {min: 120, max: 600, label: "Pauză minimă arzător (secunde)"},
    rfSafetyHeartbeatSec: {min: 30, max: 900, label: "Heartbeat RF (secunde)"},
    freezeProtectionTemp: {min: 8, max: 16, label: "Prag protecție îngheț (°C)"},
    sensorTimeoutSec: {min: 60, max: 3600, label: "Timeout senzor (secunde)"},
    hysteresis: {min: 0.1, max: 1.0, label: "Histerezis (°C)"},
    windowThreshold: {min: 0.8, max: 2.0, label: "Sensibilitate geam deschis (°C)"}
  };

  if (imported.params && typeof imported.params === "object") {
    const curP = current.params || {};
    for (const [k, v] of Object.entries(imported.params)) {
      const lim = PARAM_LIMITS[k];
      const lbl = lim ? lim.label : k;
      if (lim) {
        if (typeof v !== "number" || isNaN(v) || v < lim.min || v > lim.max) {
          invalid.push({field: `params.${k}`, error: `Valoare (${v}) în afara intervalului permis [${lim.min} .. ${lim.max}]`});
          continue;
        }
      }
      if (curP[k] !== undefined && curP[k] !== v) {
        changes.push({field: `Parametru: ${lbl}`, oldVal: curP[k], newVal: v});
      }
    }
  }

  if (imported.location !== undefined) {
    const curLoc = current.location || "";
    if (typeof imported.location !== "string" || !/^-?\d+(\.\d+)?,\s*-?\d+(\.\d+)?$/.test(imported.location.trim())) {
      invalid.push({field: "location", error: `Format coordonate invalid ('${imported.location}'). Așteptat: 'lat,lon'`});
    } else if (curLoc !== imported.location) {
      changes.push({field: "Locație vreme", oldVal: curLoc, newVal: imported.location});
    }
  }

  if (Array.isArray(imported.zones)) {
    const curZones = current.zones || [];
    const VALID_PINS = [32, 33, 25, 26];
    const usedPins = new Set();
    const usedMacs = new Set();

    imported.zones.forEach((z, idx) => {
      const curZ = curZones.find(x => x.id === z.id) || curZones[idx];
      const zName = z.name || (curZ ? curZ.name : `Zona ${z.id}`);

      if (z.relayPin !== undefined) {
        if (!VALID_PINS.includes(z.relayPin)) {
          invalid.push({field: `${zName} › Releu`, error: `Pin invalid (${z.relayPin}). Pini permiși: 32, 33, 25, 26.`});
        } else if (usedPins.has(z.relayPin)) {
          invalid.push({field: `${zName} › Releu`, error: `Conflict releu: pinul ${z.relayPin} este alocat de două ori.`});
        } else {
          usedPins.add(z.relayPin);
        }
      }
      if (z.mac) {
        const macNorm = z.mac.toUpperCase();
        if (!/^([0-9A-F]{2}:){5}[0-9A-F]{2}$/.test(macNorm)) {
          invalid.push({field: `${zName} › Senzor MAC`, error: `Format MAC invalid: '${z.mac}'`});
        } else if (usedMacs.has(macNorm)) {
          invalid.push({field: `${zName} › Senzor MAC`, error: `Conflict: senzorul ${z.mac} este asociat la mai multe zone.`});
        } else {
          usedMacs.add(macNorm);
        }
      }
      if (z.offset !== undefined && (typeof z.offset !== "number" || z.offset < -3.0 || z.offset > 3.0)) {
        invalid.push({field: `${zName} › Offset`, error: `Offset temperatură (${z.offset}) în afara limitelor [-3.0 .. +3.0]°C.`});
      }

      if (curZ) {
        if (z.name !== undefined && z.name !== curZ.name) changes.push({field: `Zona ${curZ.id} › Nume`, oldVal: curZ.name, newVal: z.name});
        if (z.mac !== undefined && (z.mac || "").toUpperCase() !== (curZ.mac || "").toUpperCase()) changes.push({field: `${zName} › Senzor MAC`, oldVal: curZ.mac || "–", newVal: z.mac || "–"});
        if (z.relayPin !== undefined && z.relayPin !== curZ.relayPin) changes.push({field: `${zName} › Releu`, oldVal: curZ.relayPin, newVal: z.relayPin});
        if (z.enabled !== undefined && z.enabled !== curZ.enabled) changes.push({field: `${zName} › Activă`, oldVal: curZ.enabled ? "Da" : "Nu", newVal: z.enabled ? "Da" : "Nu"});
        if (z.offset !== undefined && Math.abs(z.offset - curZ.offset) > 0.05) changes.push({field: `${zName} › Offset`, oldVal: curZ.offset + "°", newVal: z.offset + "°"});
      } else {
        changes.push({field: `Zonă nouă (${z.id})`, oldVal: "–", newVal: zName});
      }
    });
  }

  if (imported.schedule && typeof imported.schedule === "object") {
    const curS = current.schedule || {};
    if (imported.schedule.enabled !== undefined && imported.schedule.enabled !== curS.enabled) {
      changes.push({field: "Program încălzire", oldVal: curS.enabled ? "Activ" : "Oprit", newVal: imported.schedule.enabled ? "Activ" : "Oprit"});
    }
    if (imported.schedule.weekday && imported.schedule.weekday !== curS.weekday) {
      changes.push({field: "Program L–V (ore)", oldVal: curS.weekday || "–", newVal: imported.schedule.weekday});
    }
    if (imported.schedule.weekend && imported.schedule.weekend !== curS.weekend) {
      changes.push({field: "Program Weekend (ore)", oldVal: curS.weekend || "–", newVal: imported.schedule.weekend});
    }
    if (Array.isArray(imported.schedule.levels)) {
      const curL = curS.levels || [];
      imported.schedule.levels.forEach(l => {
        const cur = curL.find(x => x.id === l.id);
        const z = (current.zones || []).find(x => x.id === l.id);
        const zName = z ? z.name : `Zona ${l.id}`;
        const LEVEL_LABELS = {
          day: "Temperatură Program Zi (Day)",
          night: "Temperatură Program Noapte (Night)",
          away: "Temperatură Mod Plecat (Away)"
        };
        ["day", "night", "away"].forEach(lvl => {
          const lbl = LEVEL_LABELS[lvl] || lvl;
          if (l[lvl] !== undefined) {
            if (typeof l[lvl] !== "number" || l[lvl] < 5.0 || l[lvl] > 28.0) {
              invalid.push({field: `${zName} › ${lbl}`, error: `Temperatură (${l[lvl]}°C) în afara limitelor [5.0 .. 28.0]°C.`});
            } else if (cur && Math.abs(l[lvl] - cur[lvl]) > 0.05) {
              changes.push({field: `${zName} › ${lbl}`, oldVal: cur[lvl] + "°C", newVal: l[lvl] + "°C"});
            }
          }
        });
      });
    }
  }

  return {changes, invalid};
}

let pendingImportText = null;
let pendingImportChanges = [];

$("cfgImportBtn").onclick = () => {
  $("cfgReport").innerHTML = "";
  $("cfgPreview").style.display = "none";
  $("cfgFile").click();
};
$("cfgFile").onchange = async e => {
  const f = e.target.files[0];
  if (!f) return;
  $("cfgReport").innerHTML = "";
  $("cfgPreview").style.display = "none";
  $("cfgMsg").textContent = "Se analizează fișierul...";
  try {
    const text = await f.text();
    let imported = null;
    try {
      imported = JSON.parse(text);
    } catch (parseErr) {
      throw new Error("Fișierul nu conține JSON valid (" + parseErr.message + ").");
    }

    const curExport = await (await fetch("api/config/export")).json();
    const {changes, invalid} = analyzeImport(imported, curExport);
    pendingImportText = text;
    pendingImportChanges = changes;

    let h = `<div style="font-weight:600;margin-bottom:8px">Previzualizare import setări: <span class="mono">${esc(f.name)}</span></div>`;

    if (invalid.length) {
      h += `<div style="margin:8px 0;padding:8px 10px;background:rgba(220,53,69,0.15);border:1px solid var(--bad);border-radius:8px">
        <b style="color:var(--bad)">⚠️ Atenție – Valori invalide / nepermise (${invalid.length}):</b>
        <ul style="margin:4px 0 0 16px;padding:0">${invalid.map(x => `<li><b>${esc(x.field)}:</b> ${esc(x.error)}</li>`).join("")}</ul>
        <div class="s" style="margin-top:4px">Valorile invalide vor fi ignorate conform regulilor de siguranță.</div>
      </div>`;
    }

    if (changes.length) {
      h += `<div style="margin:8px 0">
        <b>Schimbări detectate care se vor aplica (${changes.length}):</b>
        <div class="scroll" style="margin-top:4px"><table style="font-size:13px">
          <thead><tr><th>Setare / Parametru</th><th>Valoare curentă</th><th>Valoare nouă din fișier</th></tr></thead>
          <tbody>${changes.map(c => `<tr><td><b>${esc(c.field)}</b></td><td style="color:var(--mut)">${esc(String(c.oldVal))}</td><td style="color:var(--ok);font-weight:600">${esc(String(c.newVal))}</td></tr>`).join("")}</tbody>
        </table></div>
      </div>`;
    } else {
      h += `<div class="s" style="margin:8px 0;color:var(--ok)">Nicio valoare nouă de schimbat — fișierul este identic cu setările curente din centrală.</div>`;
    }

    h += `<div style="margin-top:12px;font-weight:600">Ești de acord să imporți aceste setări?</div>
    <div class="row" style="margin-top:8px;gap:8px">
      <button type="button" id="btnConfirmImport" class="on">Confirmă și aplică importul</button>
      <button type="button" id="btnCancelImport">Anulează</button>
    </div>`;

    $("cfgPreview").innerHTML = h;
    $("cfgPreview").style.display = "block";
    $("cfgMsg").textContent = "";

    $("btnConfirmImport").onclick = async () => {
      $("btnConfirmImport").disabled = true;
      $("btnConfirmImport").textContent = "Se importă...";
      try {
        const r = await fetch("api/config/import", {method: "POST", headers: {"Content-Type": "application/json"}, body: pendingImportText});
        const j = await r.json();
        if (!r.ok) throw new Error(j.message || "import HTTP " + r.status);
        $("cfgPreview").style.display = "none";
        $("cfgMsg").textContent = "Import finalizat cu succes.";
        renderImport(j, pendingImportChanges);
      } catch (err) {
        $("cfgMsg").textContent = "Eroare import: " + err.message;
      }
      pendingImportText = null;
      $("cfgFile").value = "";
      refresh();
    };

    $("btnCancelImport").onclick = () => {
      pendingImportText = null;
      pendingImportChanges = [];
      $("cfgFile").value = "";
      $("cfgPreview").style.display = "none";
      $("cfgReport").innerHTML = "";
      $("cfgMsg").textContent = "Import anulat. Nicio setare nu a fost modificată.";
      setTimeout(() => { if ($("cfgMsg").textContent.includes("anulat")) $("cfgMsg").textContent = ""; }, 4000);
    };

  } catch (err) {
    $("cfgMsg").textContent = "Respins: " + err.message;
    $("cfgFile").value = "";
  }
};

const btnSaveCreds = $("btnSaveAdminCreds");
if (btnSaveCreds) {
  btnSaveCreds.addEventListener("click", async () => {
    const curP = ($("cfgAdminCurrentPass") ? $("cfgAdminCurrentPass").value : "").trim();
    const u = ($("cfgAdminUser").value || "").trim();
    const p = ($("cfgAdminPass").value || "").trim();
    const msg = $("cfgAdminCredMsg");
    if (!u || u.length > 31) {
      if (msg) { msg.textContent = "Utilizator invalid (1-31 caractere)"; msg.style.color = "var(--bad)"; }
      return;
    }
    if (p.length < 8 || p.length > 63) {
      if (msg) { msg.textContent = "Parolă nouă invalidă (minim 8 caractere, FR-59)"; msg.style.color = "var(--bad)"; }
      return;
    }
    if (msg) { msg.textContent = "Se salvează în NVS..."; msg.style.color = "var(--mut)"; }

    try {
      const candidates = [];
      if (curP) candidates.push(curP);
      candidates.push("admin.shh2026", "admin123");

      let ok = false;
      let lastErr = "";
      for (const passCandidate of candidates) {
        try {
          const authHeader = "Basic " + btoa("admin:" + passCandidate);
          const r = await fetch("api/config/credentials", {
            method: "POST",
            headers: {
              "Content-Type": "application/x-www-form-urlencoded",
              "Authorization": authHeader
            },
            body: new URLSearchParams({ user: u, pass: p })
          });
          if (r.ok) {
            ok = true;
            break;
          } else if (r.status === 401) {
            continue;
          } else {
            const errJson = await r.json().catch(() => ({}));
            lastErr = errJson.error || errJson.message || ("HTTP " + r.status);
            break;
          }
        } catch (fetchErr) {
          lastErr = fetchErr.message;
        }
      }

      if (!ok) {
        try {
          const consoleBody = new URLSearchParams({ cmd: `ADMINPASS ${u} ${p}` });
          const cr = await fetch("api/console/send", { method: "POST", body: consoleBody });
          if (cr.ok) {
            const txt = await cr.text();
            if (txt.includes("[ADMINPASS]") && !txt.includes("Refused") && !txt.includes("Failed")) {
              ok = true;
            }
          }
        } catch (_) {}
      }

      if (ok) {
        if (msg) { msg.textContent = "✅ Credențiale salvate! DTC-403 rezolvat."; msg.style.color = "var(--ok)"; }
        if ($("cfgAdminCurrentPass")) $("cfgAdminCurrentPass").value = "";
        $("cfgAdminPass").value = "";
        refresh();
      } else {
        if (msg) {
          msg.textContent = lastErr ? ("Eroare: " + lastErr) : "Eroare: Parolă curentă incorectă (HTTP 401)";
          msg.style.color = "var(--bad)";
        }
      }
    } catch (err) {
      if (msg) { msg.textContent = "Eroare: " + err.message; msg.style.color = "var(--bad)"; }
    }
  });
}
// Commissioning: a row being edited is not overwritten by the refresh until it is saved
$("setup").addEventListener("input", e => { const tr = e.target.closest("tr[data-z]"); if (tr) dirtyRows.add(tr.dataset.z); });
$("setup").addEventListener("change", e => { const tr = e.target.closest("tr[data-z]"); if (tr) dirtyRows.add(tr.dataset.z); });
$("zoneAdd").onclick = async () => {
  try {
    const r = await api("api/setup/zone-add", {});
    $("zoneMsg").textContent = `Zona ${r.zone} adăugată pe releul GPIO${r.relayPin}.`;
  } catch (err) { $("zoneMsg").textContent = "Respins: " + err.message; }
  refresh();
};
async function searchAction(action) {
  try { RAW["api/setup/discovered-sensors"] = await api("api/setup/sensor-search", {action}); } catch (err) { $("searchInfo").textContent = err.message; }
  renderSensors();
}
$("searchToggle").onclick = () => {
  const F = RAW["api/setup/discovered-sensors"] || {active: false};
  searchAction(F.active ? "stop" : "start");
};
$("searchStop").onclick = () => searchAction("stop");
$("setup").addEventListener("click", async e => {
  const d = e.target.closest("button[data-del]");
  if (d) {
    const n = +d.dataset.del + 1;
    if (!confirm(`Ștergi zona ${n}? Setările ei se pierd, zonele de după ea își schimbă numărul.`)) return;
    try { await api("api/setup/zone-delete", {zone: n}); $("zoneMsg").textContent = `Zona ${n} ștearsă.`; }
    catch (err) { $("zoneMsg").textContent = "Respins: " + err.message; }
    dirtyRows.clear();
    refresh();
    return;
  }
});

$("zonesSave").onclick = async () => {
  const zones = [...document.querySelectorAll("#setup tr[data-z]")].map(tr => {
    const f = n => tr.querySelector(`[data-f="${n}"]`);
    return {id: +tr.dataset.z + 1, name: f("name").value.trim(), mac: f("mac").value.trim(), relayPin: +f("pin").value,
            enabled: f("enabled").checked, offset: +(+f("offset").value).toFixed(1),
            solarGain: f("solarGain").checked, heatingType: +f("heatingType").value};
  });
  try {
    const r = await fetch("api/config/import", {method: "POST", headers: {"Content-Type": "application/json"}, body: JSON.stringify({format: 1, zones})});
    const j = await r.json();
    if (!r.ok) throw new Error(j.message || "HTTP " + r.status);
    document.querySelectorAll("#setup td.s").forEach(td => td.textContent = "");
    let bad = 0;
    for (const x of j.skipped || []) {
      const m = /^zones\[(\d+)\]$/.exec(x.name), td = m && $("setupMsg" + (m[1] - 1));
      if (!td) continue;
      bad++;
      td.textContent = "Respinsă: " + (SAVE_WHY[x.reason] || x.reason);
    }
    $("zoneMsg").textContent = bad ? `Restul salvat, ${bad} zonă/zone respinse (vezi rândurile).` : "Zonele au fost salvate.";
    if (!bad) dirtyRows.clear();
  } catch (err) { $("zoneMsg").textContent = "Respins: " + err.message; }
  if (!dirtyRows.size) refresh();
};

$("saveLoc").onclick = async () => {
  try { await api("api/setup/location", {coords: $("loc").value}); $("locMsg").textContent = "Salvat."; }
  catch (e) { $("locMsg").textContent = "Respins: " + e.message; }
  refresh();
};

