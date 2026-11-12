function renderChecks() {
  let rows = (D && D.checks || []).map(c =>
    `<tr><td><code>${esc(c.id)}</code></td><td class="${esc(c.result)}">${esc(c.result)}</td><td>${esc(c.value)}</td><td>${esc(CHECK_RO[c.id] || c.rule)}</td></tr>`).join("");

  if (Z) {
    const bOn = Z.boiler === "ON";
    const bBurn = (Z.boilerRuntime && Z.boilerRuntime.currentBurnSec) ? `${Z.boilerRuntime.currentBurnSec} s` : "";
    let bVal = bOn ? `PORNITĂ 🔥${bBurn ? ' (' + bBurn + ')' : ''}` : "OPRITĂ";
    if (!bOn && Z.pendingAction && Z.pendingAction !== "NONE") {
      bVal = `Așteaptă: ${Z.pendingAction} (${Z.pendingWaitSec || 0}s)`;
    } else if (!bOn && Z.boilerCooldownSec > 0) {
      bVal = `Pauză protecție (${Z.boilerCooldownSec}s)`;
    }
    rows += `<tr><td><code>boiler</code></td><td class="OK">OK</td><td>${esc(bVal)}</td><td>Comandă cazan (interlock actuator și protecție anti-ciclare scurtă)</td></tr>`;

    const openZones = (Z.zones || []).filter(z => z.relayEngaged);
    const openCount = openZones.length;
    const totalCount = (Z.zones || []).length;
    const openNames = openZones.map(z => z.name).join(", ");
    const rVal = `${openCount}/${totalCount} deschiși ${openCount > 0 ? '(' + openNames + ')' : '(toate oprite)'}`;
    const formatRtCh = s => {
      const m = Math.round(s / 60);
      if (m < 60) return `${m}m`;
      return `${Math.floor(m / 60)}h ${m % 60}m`;
    };
    const rtBreakdown = (Z.zones || []).map(z => `${z.name} ${formatRtCh(z.runtimeTodaySec || 0)}`).join(", ");
    const rRule = `Comandă actuatoare circuite (robineți NC) · Timp încălzit azi: ${rtBreakdown || "0m"}`;
    rows += `<tr><td><code>relee</code></td><td class="OK">OK</td><td>${esc(rVal)}</td><td>${esc(rRule)}</td></tr>`;
  }

  $("checks").innerHTML = "<tr><th>Verificare</th><th>Rezultat</th><th>Valoare</th><th>Regulă</th></tr>" + rows;

  const dtcActiveBox = $("dtcActiveSection") || $("dtcSection");
  if (dtcActiveBox) {
    const dtcs = (D && D.dtcs) || [];

    if (dtcs.length === 0) {
      dtcActiveBox.innerHTML = `
        <div style="margin-top:10px;padding:8px 12px;background:rgba(46,160,67,0.1);border:1px solid rgba(46,160,67,0.3);border-radius:6px">
          <div style="display:flex;align-items:center;gap:10px">
            <span style="font-size:18px">✅</span>
            <div>
              <div style="font-weight:600;color:var(--ok)">2.2 Niciun cod de eroare activ (DTC)</div>
              <div class="s" style="color:var(--mut)">Toate subsistemele (rețea, senzori BLE, memorie, logică hidraulică) funcționează în parametri normali.</div>
            </div>
          </div>
        </div>`;
    } else {
      const formatAge = s => s < 60 ? `${s}s în urmă` : s < 3600 ? `${Math.floor(s/60)}m în urmă` : `${Math.floor(s/3600)}h în urmă`;
      let tbl = `<div style="margin-top:10px;padding:10px 12px;background:rgba(248,81,73,0.1);border:1px solid rgba(248,81,73,0.3);border-radius:6px">
        <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:8px">
          <div style="font-weight:600;color:var(--err);display:flex;align-items:center;gap:6px">
            <span>⚠️</span> 2.2 Coduri de eroare active (${dtcs.length}):
          </div>
          <button type="button" class="sm danger" id="btnClearDtc" title="Șterge toate codurile active">Curăță codurile de eroare</button>
        </div>
        <div class="scroll"><table style="width:100%">
          <thead><tr><th>Cod</th><th>Severitate</th><th>Descriere &amp; Cauză</th><th>Vechime</th><th>Apariții</th></tr></thead>
          <tbody>`;
      dtcs.forEach(d => {
        const cleanCode = String(d.code).replace("DTC-", "");
        const info = DTC_INFO[cleanCode];
        const causeText = info ? `<div class="s" style="color:var(--mut);margin-top:2px">${esc(info.cause)}</div>` : "";
        const titleText = info ? `<b>${esc(info.title)}:</b> ` : "";
        tbl += `<tr>
          <td><code style="font-weight:bold">${esc(d.code)}</code></td>
          <td class="${d.severity === 'ERROR' ? 'FAIL' : 'INFO'}">${esc(d.severity)}</td>
          <td>${titleText}${esc(d.message)}${causeText}</td>
          <td>${formatAge(d.ageSec || 0)}</td>
          <td>${d.count || 1}x</td>
        </tr>`;
      });
      tbl += `</tbody></table></div>
      </div>`;
      dtcActiveBox.innerHTML = tbl;

      const btnClr = $("btnClearDtc");
      if (btnClr) {
        btnClr.onclick = async () => {
          btnClr.disabled = true;
          btnClr.textContent = "Se curăță...";
          try {
            await api("api/diag/clear", {}, true);
            loadDtcEvents(true);
          } catch(e) {
            console.error("Eroare la curățare DTC:", e);
          } finally {
            refresh();
          }
        };
      }
    }
  }
}


let dtcEventsLoaded = false;
async function loadDtcEvents(force = false) {
  const box = $("persistentDtcHistory");
  if (!box) return;
  if (dtcEventsLoaded && !force) return;
  box.innerHTML = `<div class="s" style="color:var(--mut)">Se încarcă istoricul din memoria flash...</div>`;
  try {
    const d = await (await fetch("api/diag/events")).json();
    if (!d || !d.events || d.events.length === 0) {
      box.innerHTML = `<div class="s" style="color:var(--mut);padding:4px 0">Niciun eveniment diagnostic înregistrat în memoria flash.</div>`;
      dtcEventsLoaded = true;
      return;
    }
    const typeMap = {
      "dtc-raised": '<span style="color:var(--bad);font-weight:600">🔴 Ridicat</span>',
      "dtc-cleared": '<span style="color:var(--ok);font-weight:600">🟢 Curățat</span>',
      "restart": '<span style="color:#f59e0b;font-weight:600">⚡ Restart</span>'
    };
    box.innerHTML = `<div class="scroll" style="margin-top:6px;max-height:260px;overflow-y:auto"><table style="font-size:11px;width:100%;border-collapse:collapse">
      <thead><tr>
        <th style="padding:4px 6px">Dată / Oră</th>
        <th style="padding:4px 6px">Stare</th>
        <th style="padding:4px 6px">Cod</th>
        <th style="padding:4px 6px">Descriere &amp; Cauză</th>
        <th style="padding:4px 6px">Zonă / Țintă</th>
      </tr></thead>
      <tbody>` +
      d.events.map(e => {
        const timeStr = (e.time > 1700000000) ? new Date(e.time * 1000).toLocaleString('ro-RO') : `+${e.time}s uptime`;
        let codeStr = '—';
        let descStr = '';
        let targetStr = 'Global';

        if (e.type === 'restart') {
          const rInfo = RESET_REASONS[e.value] || {title: "Repornire sistem (motiv " + e.value + ")", desc: "Reset placă ESP32"};
          codeStr = '<code style="color:#f59e0b;font-weight:bold">SYS-REBOOT</code>';
          descStr = `<b>${esc(rInfo.title)}</b><div class="s" style="color:var(--mut);margin-top:2px">${esc(rInfo.desc)}</div>`;
          targetStr = 'Sistem (Restaurat)';
        } else {
          const cleanCode = String(e.code || '').replace("DTC-", "");
          const info = DTC_INFO[cleanCode] || {title: "Cod eroare diagnostic", cause: "Eveniment înregistrat de DiagnosticEngine."};
          codeStr = e.code ? `<code><b>${esc(e.code)}</b></code>` : '—';
          descStr = `<b>${esc(info.title)}</b><div class="s" style="color:var(--mut);margin-top:2px">${esc(info.cause)}</div>`;
          targetStr = e.zone ? `Zona ${e.zone}` : 'Global';
        }

        return `<tr>
          <td style="white-space:nowrap;padding:5px 6px"><b>${esc(timeStr)}</b></td>
          <td style="white-space:nowrap;padding:5px 6px">${typeMap[e.type] || esc(e.type)}</td>
          <td style="white-space:nowrap;padding:5px 6px">${codeStr}</td>
          <td style="padding:5px 6px">${descStr}</td>
          <td style="white-space:nowrap;padding:5px 6px">${esc(targetStr)}</td>
        </tr>`;
      }).join("") +
      `</tbody></table></div>`;
    dtcEventsLoaded = true;
  } catch(err) {
    box.innerHTML = `<div class="s" style="color:var(--err)">Eroare la încărcarea istoricului: ${esc(err.message)}</div>`;
  }
}

