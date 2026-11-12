let gasFormOpen = false;
let gasReadingsSeen = -1;

function fmtReadingTime(e) {
  const d = new Date(e * 1000);
  const p = n => String(n).padStart(2, "0");
  return `${p(d.getDate())}.${p(d.getMonth() + 1)}.${d.getFullYear()} ${p(d.getHours())}:${p(d.getMinutes())}`;
}

// FR-35.1: all stored meter readings, newest first (loaded when the count changes or the form opens)
async function loadGasReadings() {
  const container = $("gasReadingsContainer");
  if (!container) return;
  try {
    const r = await fetch("api/boiler/gas-readings");
    const d = await r.json();
    gasReadingsSeen = d.count;
    if (!d.readings.length) {
      container.innerHTML = `<div class="s" style="color:var(--mut);padding:8px 0">Nicio citire de contor. Introduceți indexul citit de pe contor.</div>`;
      return;
    }
    let h = `<div style="font-weight:600;font-size:12px;margin:12px 0 6px 0">📋 Citiri de contor (${d.count}):</div>
    <div class="scroll"><table style="width:100%;border-collapse:collapse;font-size:11px"><thead>
      <tr style="border-bottom:1px solid var(--line);color:var(--mut);text-align:left"><th style="padding:4px">Dată / Oră</th><th style="padding:4px">Index (m³)</th><th style="padding:4px">Consum de la citirea anterioară</th><th style="padding:4px;text-align:center">Acțiune</th></tr></thead><tbody>`;
    d.readings.forEach((row, k, a) => {
      const prev = k > 0 ? a[k - 1] : null;
      const diff = prev ? `+${(row.index - prev.index).toFixed(3)} m³` : "—";
      h += `<tr style="border-bottom:1px solid var(--line)"><td style="padding:4px"><b>${esc(fmtReadingTime(row.time))}</b></td><td style="padding:4px;font-weight:600">${row.index.toFixed(3)}</td><td style="padding:4px">${diff}</td><td style="padding:4px;text-align:center;white-space:nowrap">
        <button type="button" class="sm" style="padding:1px 6px;font-size:10px;margin-right:4px;background:rgba(56,189,248,0.15);border:1px solid #38bdf8;color:#38bdf8" onclick="editGasReading(${row.time}, ${row.index.toFixed(3)})">✏️ Editează</button>
        <button type="button" class="sm danger" style="padding:1px 6px;font-size:10px" onclick="deleteGasReading(${row.time}, '${row.index.toFixed(3)}')">✕ Șterge</button>
      </td></tr>`;
    });
    container.innerHTML = h + `</tbody></table></div>`;
    container.querySelector("tbody").append(...Array.from(container.querySelectorAll("tbody tr")).reverse());
  } catch (e) {
    container.innerHTML = `<div class="s" style="color:var(--bad)">Citirile nu s-au putut încărca (${esc(e.message)})</div>`;
  }
}

async function editGasReading(epoch, curIdx) {
  const val = prompt(`Modifică valoarea indexului (${curIdx} m³):`, curIdx);
  if (!val || val.trim() === "" || parseFloat(val.replace(',', '.')) === curIdx) return;
  const num = parseFloat(val.replace(',', '.'));
  if (isNaN(num) || num <= 0) { alert("Valoare invalidă!"); return; }
  try {
    const res = await fetch("api/boiler/gas-meter/update", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: `time=${epoch}&index=${encodeURIComponent(num)}`
    });
    if (res.ok) {
      if ($("gasReadMsg")) $("gasReadMsg").textContent = "✓ Index actualizat și consum recalculat";
      loadGasReadings();
      refresh();
    } else {
      const err = await res.json().catch(() => ({}));
      alert("Eroare la actualizare: " + (err.error || res.status));
    }
  } catch (e) {
    alert("Eroare de rețea: " + e.message);
  }
}

async function deleteGasReading(epoch, idxStr) {
  if (!confirm(`Sigur ștergi citirea ${idxStr} m³?`)) return;
  try {
    const res = await fetch("api/boiler/gas-meter/delete", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: `time=${epoch}`
    });
    if (res.ok) {
      if ($("gasReadMsg")) $("gasReadMsg").textContent = "✅ Citire ștearsă și consum recalculat";
      loadGasReadings();
      refresh();
    } else {
      alert("Nu s-a putut șterge citirea.");
    }
  } catch (e) {
    alert("Eroare de rețea: " + e.message);
  }
}

function renderGasMeter() {
  const box = $("gasMeterBox");
  if (!box) return;
  const gm = (RAW && RAW["api/boiler/status"] && RAW["api/boiler/status"].gasMeter) ? RAW["api/boiler/status"].gasMeter : null;
  if (!gm) {
    box.style.display = "none";
    return;
  }
  box.style.display = "block";

  const has = gm.lastReadingTime !== null && gm.lastReadingTime !== undefined;
  const estIdx = (has && gm.estimatedIndex !== null) ? Number(gm.estimatedIndex).toFixed(3) : "—";
  const lastStr = has ? `Ultima citire: ${Number(gm.lastReadingIndex).toFixed(3)} m³ · ${fmtReadingTime(gm.lastReadingTime)}` : "Nicio citire de contor încă";
  const gasMeasured = (gm.gasFlowMeasured !== null && gm.gasFlowMeasured !== undefined);
  const NOTE_GAS = "se calculează după două citiri de contor";
  const todayM3 = (gm.gasTodayM3 !== null && gm.gasTodayM3 !== undefined) ? Number(gm.gasTodayM3 || 0).toFixed(2) : "—";
  const todayHeatM3 = (gm.gasTodayHeatM3 !== null && gm.gasTodayHeatM3 !== undefined) ? Number(gm.gasTodayHeatM3 || 0).toFixed(2) : todayM3;
  const todayDhwM3 = (gm.gasTodayDhwM3 !== null && gm.gasTodayDhwM3 !== undefined) ? Number(gm.gasTodayDhwM3 || 0).toFixed(2) : "0.00";
  const yestM3 = (gm.gasYesterdayM3 !== null && gm.gasYesterdayM3 !== undefined) ? Number(gm.gasYesterdayM3 || 0).toFixed(2) : "—";
  const monthM3 = (gm.gasThisMonthM3 !== null && gm.gasThisMonthM3 !== undefined) ? Number(gm.gasThisMonthM3 || 0).toFixed(2) : "—";
  const setT = gm.boilerSetTemp, flowT = gm.boilerFlowObserved;
  const measured = (gm.gasFlowMeasured !== null && gm.gasFlowMeasured !== undefined);
  const flameRate = Number(measured ? gm.gasFlowMeasured : gm.gasFlameRate).toFixed(2);
  const flowLabel = measured ? "Debit măsurat (calibrat noaptea)" : "Debit estimat (implicit, se corectează singur)";
  const boilerKw = gm.boilerPowerKw;
  const price = Number(gm.gasPrice).toFixed(2);
  const todayLei = todayM3 !== "—" ? (parseFloat(todayM3) * parseFloat(price)).toFixed(2) : "—";
  const yestLei = yestM3 !== "—" ? (parseFloat(yestM3) * parseFloat(price)).toFixed(2) : "—";
  const monthLei = monthM3 !== "—" ? (parseFloat(monthM3) * parseFloat(price)).toFixed(2) : "—";
  const bst = (RAW && RAW["api/boiler/status"]) ? RAW["api/boiler/status"] : null;
  const burnTodaySec = (bst && bst.boilerRuntime && bst.boilerRuntime.todaySec) ? bst.boilerRuntime.todaySec : 0;  // boilerRuntime.todaySec
  const burnTodayH = (burnTodaySec / 3600).toFixed(1);

  if ($("gasEstIdx")) {
    $("gasEstIdx").textContent = estIdx;
    $("gasLastReading").textContent = lastStr;
    $("gasTodayM3").textContent = todayM3;
    $("gasTodayLei").textContent = `~ ${todayLei} Lei · Ieri: ${yestM3} m³ (${yestLei} Lei)`;
    $("gasTodayBreakdown").textContent = `♨️ Încălzire: ${todayHeatM3} m³ · 🚿 ACM: ${todayDhwM3} m³`;
    $("gasMonthM3").textContent = monthM3;
    $("gasMonthLei").textContent = `~ ${monthLei} Lei (la ${price} Lei/m³)`;
    $("gasSetT").textContent = `${setT}°C `;
    $("gasFlowT").textContent = `(livrează ~${flowT}°C)`;
    $("gasSubRate").textContent = `${flowLabel}: ${flameRate} m³/h`;
    if ($("gasCalibRate")) $("gasCalibRate").textContent = flameRate;
    if ($("gasCalibType")) {
      $("gasCalibType").textContent = measured ? '✓ Calibrat nocturn (fără ACM)' : '⚠️ Necalibrat (estimare implicită)';
      $("gasCalibType").style.color = measured ? 'var(--ok, #22c55e)' : 'var(--warn, #f59e0b)';
    }
    $("gasBurnTimeStr").textContent = `Timp ardere azi: ${burnTodayH}h (centrală pornită)`;
    if (!gasFormOpen) {
      if ($("cfgGasSetTemp")) $("cfgGasSetTemp").value = setT;
      if ($("cfgGasFlowObs")) $("cfgGasFlowObs").value = flowT;
      if ($("cfgGasPrice")) $("cfgGasPrice").value = price;
    }
    if (gm.readingCount !== gasReadingsSeen) loadGasReadings();
    return;
  }

  box.innerHTML = `<div style="display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:10px">
    <div style="font-weight:600;font-size:14px">🔥 3.3 Contor gaz &amp; centrală (FR-35.1)</div>
    <button type="button" class="sm" id="btnToggleGasForm">⚙️ Citire contor / Setări</button>
  </div>
  <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:12px;margin-bottom:10px">
    <div style="background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px">
      <div class="s" style="color:var(--mut)">Index contor estimat</div>
      <div style="font-size:22px;font-weight:700;color:var(--fg);margin:2px 0"><span id="gasEstIdx">${estIdx}</span> <span style="font-size:14px;font-weight:400">m³</span></div>
      <div class="s" id="gasLastReading" style="color:var(--mut)">${lastStr}</div>
      <div class="s" id="gasBurnTimeStr" style="color:var(--mut)">Timp ardere azi: ${burnTodayH}h</div>
    </div>
    <div style="background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px">
      <div class="s" style="color:var(--mut)">Consum gaz azi</div>
      <div style="font-size:22px;font-weight:700;color:var(--heat);margin:2px 0"><span id="gasTodayM3">${todayM3}</span> <span style="font-size:14px;font-weight:400">m³</span></div>
      <div class="s" id="gasTodayLei" style="color:var(--mut)">${gasMeasured ? `~ ${todayLei} Lei · Ieri: ${yestM3} m³ (${yestLei} Lei)` : NOTE_GAS}</div>
      <div class="s" id="gasTodayBreakdown" style="color:var(--heat);font-size:11px;margin-top:2px">♨️ Încălzire: ${todayHeatM3} m³ · 🚿 ACM: ${todayDhwM3} m³</div>
    </div>
    <div style="background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px">
      <div class="s" style="color:var(--mut)">Consum luna aceasta</div>
      <div style="font-size:22px;font-weight:700;color:var(--fg);margin:2px 0"><span id="gasMonthM3">${monthM3}</span> <span style="font-size:14px;font-weight:400">m³</span></div>
      <div class="s" id="gasMonthLei" style="color:var(--mut)">${gasMeasured ? `~ ${monthLei} Lei (la ${price} Lei/m³)` : NOTE_GAS}</div>
    </div>
    <div style="background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px">
      <div class="s" style="color:var(--mut)">Temperatură tur centrală</div>
      <div style="font-size:22px;font-weight:700;color:var(--fg);margin:2px 0"><span id="gasSetT">${setT}°C </span><span id="gasFlowT" style="font-size:13px;font-weight:400;color:var(--heat)">(livrează ~${flowT}°C)</span></div>
      <div class="s" id="gasSubRate" style="color:var(--mut)">${flowLabel}: ${flameRate} m³/h</div>
    </div>
    <div style="background:var(--card);border:1px solid var(--line);border-radius:8px;padding:10px">
      <div class="s" style="color:var(--mut)">Factor consum (calibrare)</div>
      <div style="font-size:22px;font-weight:700;color:var(--fg);margin:2px 0"><span id="gasCalibRate">${flameRate}</span> <span style="font-size:14px;font-weight:400">m³/h</span></div>
      <div class="s" id="gasCalibType" style="color:${measured ? 'var(--ok, #22c55e)' : 'var(--warn, #f59e0b)'}">${measured ? '✓ Calibrat nocturn (fără ACM)' : '⚠️ Necalibrat (estimare implicită)'}</div>
    </div>
  </div>
  <div id="gasReadForm" style="display:none;background:var(--card);border:1px solid var(--line);border-radius:8px;padding:12px;margin-top:10px">
    <div style="font-weight:600;font-size:13px;margin-bottom:8px">Citire contor (indexul de pe contorul fizic)</div>
    <div class="row" style="gap:10px;align-items:flex-end;flex-wrap:wrap">
      <div><label class="s" style="display:block;margin-bottom:3px">Index citit (m³)</label><input type="text" inputmode="decimal" id="cfgGasIndex" placeholder="ex. 2265.304" style="width:130px" /></div>
      <div><label class="s" style="display:block;margin-bottom:3px">Ora citirii (gol = acum)</label><input type="datetime-local" id="cfgGasTime" style="width:190px" /></div>
      <button type="button" id="btnSaveGasReading" class="ok">💾 Salvează citirea</button>
      <span class="s" id="gasReadMsg" style="color:var(--mut)"></span>
    </div>
    <div style="font-weight:600;font-size:13px;margin:14px 0 8px 0">Setări centrală & gaz</div>
    <div class="row" style="gap:10px;align-items:flex-end;flex-wrap:wrap">
      <div><label class="s" style="display:block;margin-bottom:3px">Temp. setată centrală (°C)</label><input type="number" step="1" id="cfgGasSetTemp" value="${setT}" style="width:80px" /></div>
      <div><label class="s" style="display:block;margin-bottom:3px">Temp. observată tur (°C)</label><input type="number" step="1" id="cfgGasFlowObs" value="${flowT}" style="width:80px" /></div>
      <div><label class="s" style="display:block;margin-bottom:3px">Preț gaz (Lei/m³)</label><input type="number" step="0.01" id="cfgGasPrice" value="${price}" style="width:80px" /></div>
      <button type="button" id="btnSaveGasSettings" class="ok">💾 Salvează setările</button>
    </div>
    <div class="s" style="color:var(--mut);margin-top:8px">O citire tastată se păstrează cu data și ora ei și nu se șterge. Setările centralei nu scriu citiri.</div>
    <div id="gasReadingsContainer" style="margin-top:10px"></div>
  </div>`;

  $("btnToggleGasForm").onclick = () => {
    gasFormOpen = !gasFormOpen;
    $("gasReadForm").style.display = gasFormOpen ? "block" : "none";
    if (gasFormOpen) loadGasReadings();
  };
  $("btnSaveGasReading").onclick = async () => {
    const idx = $("cfgGasIndex").value.trim().replace(',', '.');
    const t = $("cfgGasTime").value;
    const msg = $("gasReadMsg");
    if (!idx) { msg.textContent = "Introdu indexul contorului (ex. 2266.123)"; return; }
    let body = `index=${encodeURIComponent(idx)}`;
    if (t) body += `&time=${Math.floor(new Date(t).getTime() / 1000)}`;
    try {
      const res = await fetch("api/boiler/gas-meter", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body });
      if (res.ok) { msg.textContent = "✅ Citire salvată"; $("cfgGasIndex").value = ""; $("cfgGasTime").value = ""; loadGasReadings(); refresh(); }
      else { const e = await res.json().catch(() => ({})); msg.textContent = "Respinsă: " + (e.error || ("HTTP " + res.status)); }
    } catch (err) { msg.textContent = "Eroare de rețea: " + err; }
  };
  $("btnSaveGasSettings").onclick = async () => {
    const body = `boilerSetTemp=${encodeURIComponent($("cfgGasSetTemp").value)}&boilerFlowObserved=${encodeURIComponent($("cfgGasFlowObs").value)}&gasPrice=${encodeURIComponent($("cfgGasPrice").value)}`;
    try {
      const res = await fetch("api/config/system", { method: "POST", headers: { "Content-Type": "application/x-www-form-urlencoded" }, body });
      $("gasReadMsg").textContent = res.ok ? "✅ Setări salvate" : "Respinse: valoare în afara limitelor sau autentificare necesară (HTTP " + res.status + ")";
      refresh();
    } catch (err) { $("gasReadMsg").textContent = "Eroare de rețea: " + err; }
  };
  loadGasReadings();
}

