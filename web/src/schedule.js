// FR-8.1: the schedule in words, with the time of the next change
function scheduleText() {
  if (Z.away) return "Mod plecat: toate camerele pe treapta plecat";
  if (!Z.scheduleEnabled) return "Program oprit: camerele țin ținta setată";
  if (S && S.scheduleMode === "constant") return "Program: Temperatură constantă 24/7";
  let at = "";
  const m = ST && /^(\d\d):(\d\d)/.exec(ST.time || "");
  if (m) {
    const t = (+m[1] * 60 + +m[2] + Z.minsToNextSlot) % 1440;
    at = " până la " + String(Math.floor(t / 60)).padStart(2, "0") + ":" + String(t % 60).padStart(2, "0");
  }
  const pre = Z.preHeatActive ? " · preîncălzire" : "";
  const curMom = S && S.activeMoment ? `P${S.activeMoment}` : LEVEL_RO[Z.activeLevel];
  const nxtMom = S && S.nextMoment ? `P${S.nextMoment}` : LEVEL_RO[Z.nextLevel];
  return `Program: ${curMom}${at}, apoi ${nxtMom}${pre}`;
}


let schedDirty = false;
let schedState = null;
let activeDayIndex = 1; // 1 = Monday

function isSchedBusy() {
  const sf = $("saveSched") ? $("saveSched").closest("details, section") : null;
  return schedDirty || editing || (sf && sf.contains(document.activeElement));
}



function initSchedStateFromS() {
  if (!S) return;
  const zones = S.zones || [];
  const defaultNames = ["Trezire", "Plecat", "Seară", "Noapte"];
  schedState = {
    mode: S.scheduleMode || "5+2",
    enabled: !!S.enabled,
    momentNames: (S.momentNames && S.momentNames.length === 4) ? [...S.momentNames] : defaultNames,
    awayTemps: {},
    constantTemps: {},
    days: []
  };

  zones.forEach(z => {
    schedState.awayTemps[z.id] = (S.awayTemps && S.awayTemps[z.id] !== undefined) ? S.awayTemps[z.id] : (z.away !== undefined ? z.away : 18.0);
    schedState.constantTemps[z.id] = z.day !== undefined ? z.day : 22.0;
  });

  const defaultTimes = ["06:30", "08:30", "17:00", "22:30"];
  const defaultWeTimes = ["07:30", "09:30", "18:00", "23:00"];

  for (let d = 0; d < 7; d++) {
    const isWk = (d === 0 || d === 6);
    const dayMoments = [];
    const srcMoments = (S.days && S.days[d]) ? S.days[d] : (isWk ? S.weekend_moments : S.weekday_moments) || [];
    
    for (let m = 0; m < 4; m++) {
      const srcM = srcMoments[m] || {};
      const t = srcM.time || (isWk ? defaultWeTimes[m] : defaultTimes[m]);
      const temps = {};
      zones.forEach(z => {
        if (srcM.temps && srcM.temps[z.id] !== undefined) {
          temps[z.id] = srcM.temps[z.id];
        } else {
          temps[z.id] = m === 3 ? (z.night || 21.0) : (z.day || 23.0);
        }
      });
      dayMoments.push({moment: m + 1, time: t, temps: temps});
    }
    schedState.days.push(dayMoments);
  }

  if (schedState.days[1] && schedState.days[1][0] && schedState.days[1][0].temps) {
    zones.forEach(z => {
      if (schedState.days[1][0].temps[z.id] !== undefined) {
        schedState.constantTemps[z.id] = schedState.days[1][0].temps[z.id];
      }
    });
  }
}

function renderMomentsTableHtml(moments, groupTag, extraActions = "") {
  const zones = S.zones || [];
  let h = `<div class="scroll" style="margin-top:6px"><table class="sched-moment-tbl">`;
  h += `<thead><tr><th>Moment</th><th>Oră (24h)</th>`;
  zones.forEach(z => { h += `<th>${esc(z.name)}</th>`; });
  h += `</tr></thead><tbody>`;

  const curDow = (S && typeof S.dayOfWeek === "number") ? S.dayOfWeek : new Date().getDay();
  const isWknd = (S && typeof S.isWeekend === "boolean") ? S.isWeekend : (curDow === 0 || curDow === 6);
  let groupIsActive = false;
  if (S && S.enabled && (!schedState || schedState.mode !== "constant")) {
    if (groupTag === "unified") groupIsActive = true;
    else if (groupTag === "wd") groupIsActive = !isWknd;
    else if (groupTag === "we") groupIsActive = isWknd;
    else if (groupTag && groupTag.startsWith("d")) {
      const tagDow = parseInt(groupTag.substring(1), 10);
      groupIsActive = (tagDow === curDow);
    }
  }

  moments.forEach((m, idx) => {
    const isAct = groupIsActive && (S && S.activeMoment === (idx + 1));
    const actMark = isAct
      ? `<span style="color:var(--ok);font-size:13px;width:14px;display:inline-block;text-align:center" title="Moment activ acum">●</span>`
      : `<span style="color:var(--mut);opacity:0.4;font-size:13px;width:14px;display:inline-block;text-align:center" title="Moment inactiv">○</span>`;
    h += `<tr class="${isAct ? 'chg' : ''}">`;
    const mName = (schedState && schedState.momentNames && schedState.momentNames[idx]) ? schedState.momentNames[idx] : MOMENT_LABELS[idx];
    h += `<td><div style="display:flex;align-items:center;gap:6px">
      ${actMark}<span class="s" style="font-weight:bold">P${idx + 1}:</span>
      <input type="text" class="mom-name" data-m="${idx}" value="${esc(mName)}" maxlength="15" title="Redenumește momentul" style="width:105px;font-weight:600;padding:2px 4px;font-size:13px">
    </div></td>`;
    h += `<td><input type="text" class="time24" data-group="${groupTag}" data-m="${idx}" value="${esc(m.time)}" placeholder="06:30" maxlength="5"></td>`;
    zones.forEach(z => {
      const val = (m.temps && m.temps[z.id] !== undefined) ? Number(m.temps[z.id]).toFixed(1) : "22.0";
      h += `<td><input type="number" step="0.5" min="5" max="28" class="n sched-t" data-group="${groupTag}" data-m="${idx}" data-z="${z.id}" value="${val}"></td>`;
    });
    h += `</tr>`;
  });

  h += `</tbody></table></div>`;
  if (extraActions) {
    h += `<div class="row" style="margin-top:8px;gap:6px">${extraActions}</div>`;
  }
  return h;
}

function renderSchedule() {
  if (!S) return;
  if (!schedDirty && !editing) {
    initSchedStateFromS();
  }
  if (!schedState) return;

  $("schedOn").checked = schedState.enabled;

  if ($("schedActiveBadge")) {
    const isConst = (schedState && schedState.mode === "constant") || (S && S.scheduleMode === "constant");
    const isAway = (S && S.away) || (Z && Z.away);
    let badgeText = "";
    if (!S || !S.enabled) {
      badgeText = `<span class="INFO">○ Oprit</span>`;
    } else if (isAway) {
      badgeText = `<span class="INFO">● Activ (Mod Plecat)</span>`;
    } else if (isConst) {
      badgeText = `<span class="OK">● Activ (Constant 24/7)</span>`;
    } else {
      const actM = S.activeMoment ? `P${S.activeMoment}` : "";
      badgeText = `<span class="OK">● Activ</span> ${actM ? `(${actM})` : ''}`;
    }
    $("schedActiveBadge").innerHTML = badgeText;
  }

  document.querySelectorAll(".sched-mode-btn").forEach(btn => {
    btn.classList.toggle("on", btn.dataset.mode === schedState.mode);
  });

  const descEl = $("schedModeDesc");
  if (descEl) {
    const descs = {
      "5+2": "Program diferențiat: 4 momente zilnice pentru Luni–Vineri și 4 momente pentru Weekend.",
      "unified": "Program unificat: același set de 4 momente aplicat în fiecare zi a săptămânii (Luni–Duminică).",
      "7day": "Program pe 7 zile: câte 4 momente configurabile independent pentru fiecare zi a săptămânii.",
      "constant": "Temperatură constantă: o singură valoare țintă 24/7 menținută continuu pentru fiecare cameră."
    };
    descEl.textContent = descs[schedState.mode] || "";
  }

  const c = $("schedContent");
  if (c) {
    const zones = S.zones || [];
    if (schedState.mode === "constant") {
      let h = `<div class="scroll"><table style="max-width:480px"><thead><tr><th>Cameră</th><th>Temperatură țintă (°C)</th></tr></thead><tbody>`;
      zones.forEach(z => {
        const val = Number(schedState.constantTemps[z.id] !== undefined ? schedState.constantTemps[z.id] : 22.0).toFixed(1);
        h += `<tr><td><b>${esc(z.name)}</b></td><td><input type="number" step="0.5" min="5" max="28" class="n const-t" data-z="${z.id}" value="${val}"></td></tr>`;
      });
      h += `</tbody></table></div>`;
      c.innerHTML = h;
    } else if (schedState.mode === "unified") {
      c.innerHTML = renderMomentsTableHtml(schedState.days[1], "unified");
    } else if (schedState.mode === "5+2") {
      let h = `<div style="font-weight:600;margin-top:4px">Momente Luni – Vineri:</div>`;
      h += renderMomentsTableHtml(schedState.days[1], "wd");
      h += `<div style="font-weight:600;margin-top:14px">Momente Weekend (Sâmbătă – Duminică):</div>`;
      h += renderMomentsTableHtml(schedState.days[0], "we", `
        <button type="button" class="sm" id="btnCopyWdToWe">Copiază L–V în Weekend</button>
        <button type="button" class="sm" id="btnCopyWeToWd">Copiază Weekend în L–V</button>
      `);
      c.innerHTML = h;
    } else if (schedState.mode === "7day") {
      let h = `<div class="row" id="dayTabs" style="gap:4px;margin-bottom:8px">`;
      DAY_TABS.forEach(d => {
        h += `<button type="button" class="sm day-tab-btn ${d.idx === activeDayIndex ? 'on' : ''}" data-day="${d.idx}">${d.label}</button>`;
      });
      h += `</div>`;
      const curTab = DAY_TABS.find(d => d.idx === activeDayIndex) || DAY_TABS[0];
      const copyActions = `
        <button type="button" class="sm" id="btnCopyDayToAll">Copiază ${curTab.label} la toate zilele</button>
        <button type="button" class="sm" id="btnCopyDayToWd">Copiază la L–V</button>
        <button type="button" class="sm" id="btnCopyDayToWe">Copiază la Weekend</button>
      `;
      h += renderMomentsTableHtml(schedState.days[activeDayIndex], `day_${activeDayIndex}`, copyActions);
      c.innerHTML = h;
    }
  }

  const awayCont = $("awayInputs");
  if (awayCont) {
    const zones = S.zones || [];
    awayCont.innerHTML = zones.map(z => {
      const val = Number(schedState.awayTemps[z.id] !== undefined ? schedState.awayTemps[z.id] : 18.0).toFixed(1);
      return `<label class="slot-chip" style="display:inline-flex;gap:6px;align-items:center">
        <span>${esc(z.name)}:</span>
        <input type="number" step="0.5" min="5" max="28" class="n away-t" data-z="${z.id}" value="${val}">
        <span class="s">°C</span>
      </label>`;
    }).join("");
  }


}

// While the schedule form is being edited, refreshes must not overwrite the inputs (PB-66, C-14)
const schedForm = $("saveSched") ? $("saveSched").closest("details, section") : document;
function markSchedDirty() {
  schedDirty = true;
  $("undoSched").style.display = "";
}
function syncSchedInput(t) {
  markSchedDirty();
  editing = true;
  if (!schedState) return;

  if (t.id === "schedOn") {
    schedState.enabled = t.checked;
  } else if (t.classList.contains("mom-name")) {
    const m = +t.dataset.m, val = t.value.trim();
    if (schedState.momentNames && val) {
      schedState.momentNames[m] = val;
      // Mirror name in other moment name inputs if visible
      document.querySelectorAll(`.mom-name[data-m="${m}"]`).forEach(inp => {
        if (inp !== t) inp.value = val;
      });
    }
  } else if (t.classList.contains("time24")) {
    const grp = t.dataset.group, m = +t.dataset.m, val = t.value.trim();
    if (grp === "unified") {
      for (let d = 0; d < 7; d++) schedState.days[d][m].time = val;
    } else if (grp === "wd") {
      for (let d = 1; d <= 5; d++) schedState.days[d][m].time = val;
    } else if (grp === "we") {
      schedState.days[0][m].time = val; schedState.days[6][m].time = val;
    } else if (grp && grp.startsWith("day_")) {
      const d = +grp.split("_")[1];
      schedState.days[d][m].time = val;
    }
  } else if (t.classList.contains("sched-t")) {
    const grp = t.dataset.group, m = +t.dataset.m, z = t.dataset.z, val = parseFloat(t.value);
    if (!isNaN(val)) {
      if (grp === "unified") {
        for (let d = 0; d < 7; d++) schedState.days[d][m].temps[z] = val;
      } else if (grp === "wd") {
        for (let d = 1; d <= 5; d++) schedState.days[d][m].temps[z] = val;
      } else if (grp === "we") {
        schedState.days[0][m].temps[z] = val; schedState.days[6][m].temps[z] = val;
      } else if (grp && grp.startsWith("day_")) {
        const d = +grp.split("_")[1];
        schedState.days[d][m].temps[z] = val;
      }
    }
  } else if (t.classList.contains("const-t")) {
    const z = t.dataset.z, val = parseFloat(t.value);
    if (!isNaN(val)) {
      schedState.constantTemps[z] = val;
      for (let d = 0; d < 7; d++) schedState.days[d][0].temps[z] = val;
    }
  } else if (t.classList.contains("away-t")) {
    const z = t.dataset.z, val = parseFloat(t.value);
    if (!isNaN(val)) {
      schedState.awayTemps[z] = val;
    }
  }
}

schedForm.addEventListener("input", e => syncSchedInput(e.target));
schedForm.addEventListener("change", e => syncSchedInput(e.target));
schedForm.addEventListener("focusin", () => editing = true);
schedForm.addEventListener("focusout", () => {
  setTimeout(() => {
    if (schedForm && !schedForm.contains(document.activeElement) && !schedDirty) editing = false;
  }, 50);
});

// Interactive clicks for Mode, Day Tabs, Copy, Quick Away, Legacy Slots
schedForm.addEventListener("click", e => {
  // Mode selection button
  const modeBtn = e.target.closest(".sched-mode-btn");
  if (modeBtn && schedState) {
    schedState.mode = modeBtn.dataset.mode;
    markSchedDirty();
    renderSchedule();
    return;
  }

  // 7-Day Day tab button
  const dayBtn = e.target.closest(".day-tab-btn");
  if (dayBtn) {
    activeDayIndex = +dayBtn.dataset.day;
    renderSchedule();
    return;
  }



  // Copy helpers for 5+2
  if (e.target.id === "btnCopyWdToWe" && schedState) {
    for (let m = 0; m < 4; m++) {
      schedState.days[0][m] = JSON.parse(JSON.stringify(schedState.days[1][m]));
      schedState.days[6][m] = JSON.parse(JSON.stringify(schedState.days[1][m]));
    }
    markSchedDirty();
    renderSchedule();
    return;
  }
  if (e.target.id === "btnCopyWeToWd" && schedState) {
    for (let d = 1; d <= 5; d++) {
      for (let m = 0; m < 4; m++) {
        schedState.days[d][m] = JSON.parse(JSON.stringify(schedState.days[0][m]));
      }
    }
    markSchedDirty();
    renderSchedule();
    return;
  }

  // Copy helpers for 7day
  if (e.target.id === "btnCopyDayToAll" && schedState) {
    const src = schedState.days[activeDayIndex];
    for (let d = 0; d < 7; d++) {
      if (d !== activeDayIndex) {
        schedState.days[d] = JSON.parse(JSON.stringify(src));
      }
    }
    markSchedDirty();
    renderSchedule();
    return;
  }
  if (e.target.id === "btnCopyDayToWd" && schedState) {
    const src = schedState.days[activeDayIndex];
    for (let d = 1; d <= 5; d++) {
      schedState.days[d] = JSON.parse(JSON.stringify(src));
    }
    markSchedDirty();
    renderSchedule();
    return;
  }
  if (e.target.id === "btnCopyDayToWe" && schedState) {
    const src = schedState.days[activeDayIndex];
    schedState.days[0] = JSON.parse(JSON.stringify(src));
    schedState.days[6] = JSON.parse(JSON.stringify(src));
    markSchedDirty();
    renderSchedule();
    return;
  }


});

$("undoSched").onclick = () => {
  schedDirty = false;
  editing = false;
  $("undoSched").style.display = "none";
  $("schedMsg").textContent = "Modificări anulate.";
  initSchedStateFromS();
  renderSchedule();
};

$("saveSched").onclick = async () => {
  if (!schedState) return;
  try {
    $("schedMsg").textContent = "Se salvează...";
    // 1. Enable / disable
    await api("api/schedule/enable", {enabled: $("schedOn").checked ? 1 : 0});

    // 1b. Customizable moment names
    if (schedState.momentNames && schedState.momentNames.length === 4) {
      await api("api/schedule/names", {names: schedState.momentNames}, true);
    }

    // 2. Mode
    const mode = schedState.mode;
    await api("api/schedule/mode", {mode: mode});

    // 3. Save moments according to mode
    if (mode === "constant") {
      const temps = {};
      (S.zones || []).forEach(z => {
        const v = parseFloat(schedState.constantTemps[z.id] || 22.0);
        temps[z.id] = v;
      });
      await api("api/schedule/moment", {day: "all", moment: 1, time: "00:00", temps: temps}, true);
      for (const z of (S.zones || [])) {
        await api("api/schedule/level", {zone: z.id, level: "day", temp: temps[z.id]});
      }
    } else if (mode === "unified") {
      for (let m = 0; m < 4; m++) {
        const mom = schedState.days[1][m];
        await api("api/schedule/moment", {day: "all", moment: m + 1, time: mom.time, temps: mom.temps}, true);
      }
    } else if (mode === "5+2") {
      for (let m = 0; m < 4; m++) {
        const momWd = schedState.days[1][m];
        await api("api/schedule/moment", {day: "weekday", moment: m + 1, time: momWd.time, temps: momWd.temps}, true);
        const momWe = schedState.days[0][m];
        await api("api/schedule/moment", {day: "weekend", moment: m + 1, time: momWe.time, temps: momWe.temps}, true);
      }
    } else if (mode === "7day") {
      const dayKeys = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"];
      for (let d = 0; d < 7; d++) {
        for (let m = 0; m < 4; m++) {
          const mom = schedState.days[d][m];
          await api("api/schedule/moment", {day: dayKeys[d], moment: m + 1, time: mom.time, temps: mom.temps}, true);
        }
      }
    }

    // 4. Save Away temps
    for (const z of (S.zones || [])) {
      const aw = parseFloat(schedState.awayTemps[z.id] || 18.0);
      await api("api/schedule/level", {zone: z.id, level: "away", temp: aw});
    }

    schedDirty = false;
    editing = false;
    $("undoSched").style.display = "none";
    $("schedMsg").textContent = "Program salvat cu succes.";
    setTimeout(() => { if ($("schedMsg").textContent === "Program salvat cu succes.") $("schedMsg").textContent = ""; }, 4000);
    refresh();
  } catch (e) {
    $("schedMsg").textContent = "Eroare: " + e.message;
  }
};

