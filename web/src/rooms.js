function roomClass(z) {
  if (z.state === "WINDOW" || z.windowPauseActive || z.windowDetected) return "win";
  if (z.state === "FAULT" || z.state === "DISABLED") return "bad";
  if (Z.away) return "away";
  return z.relayEngaged ? "heat" : "";
}

// FR-11.3: the family view shows the zones switched on (and a zone the frost protection heats)
const shownZones = () => Z.zones.map((z, i) => [z, i]).filter(([z]) => z.enabled || z.state === "FROST");

function renderRooms() {
  const hum = {};
  B.forEach(s => hum[s.mac.toUpperCase()] = s);
  const list = shownZones();
  if (!list.length) {
    $("rooms").innerHTML = `<div class="room bad"><div>Nicio zonă pornită</div><div class="s">Punerea în funcțiune se face din Admin.</div></div>`;
    return;
  }
  $("rooms").innerHTML = list.map(([z, i]) => {
    const s = hum[(z.sensorMac || "").toUpperCase()];
    const t = (z.sensorValid && typeof z.currentTemp === "number") ? z.currentTemp.toFixed(1) + "°" : "--";
    const humStr = s ? `💧 ${Math.round(s.hum)}% RH` : "";
    const solarStr = z.solarGain ? "☀️ Sud/Vest" : "";
    const holdStr = z.isBoost ? "Boost manual" : "";
    const subParts = [];
    if (humStr) subParts.push(humStr);
    if (solarStr) subParts.push(solarStr);
    if (holdStr) subParts.push(holdStr);
    const subText = subParts.length ? subParts.join(" · ") : (z.sensorValid ? "Senzor activ" : "Fără senzor");

    const dutyPercent = (z.dutyPct !== undefined) ? z.dutyPct : (z.duty <= 1.0 ? Math.round((z.duty || 0) * 100) : Math.round(z.duty || 0));
    const onMin = (z.onTimeSec !== undefined && z.onTimeSec > 0) ? Math.round(z.onTimeSec / 60) : Math.round(dutyPercent * 0.6);
    let statusText = STATE_RO[z.state] || z.state;
    const isWin = (z.state === "WINDOW" || z.windowPauseActive || z.windowDetected);
    if (isWin) {
      const mLeft = Math.ceil((z.windowPauseRemainingSec || 1800) / 60);
      statusText = (z.windowPauseActive || z.state === "WINDOW")
        ? `🪟 Fereastră deschisă · Pauză ${mLeft}m`
        : "🪟 Fereastră deschisă (căldura continuă)";
    } else if (z.state === "IDLE") {
      statusText = (z.sensorValid && z.currentTemp < (z.effectiveTarget - 0.15))
        ? "Pauză ciclu (menține din șapă)"
        : "Temperatură atinsă";
    } else if (z.state === "HEATING") {
      statusText = (dutyPercent >= 99)
        ? "Încălzire continuă (100%)"
        : `Încălzire TPI · ${onMin} min/h (${dutyPercent}%)`;
    }

    const rtToday = z.runtimeTodaySec || 0;
    const rtYest = z.runtimeYesterdaySec || 0;
    const formatRt = sec => {
      const m = Math.round(sec / 60);
      if (m < 60) return `${m}m`;
      return `${Math.floor(m / 60)}h ${m % 60}m`;
    };
    const rtBadge = `⏱ Încălzit azi: ${formatRt(rtToday)}${rtYest > 0 ? ' (ieri ' + formatRt(rtYest) + ')' : ''}`;
    const winBtn = isWin ? `<div style="margin-top:4px"><button type="button" class="s" style="padding:2px 8px;font-size:11px;background:rgba(56,189,248,0.15);border:1px solid #38bdf8;color:#38bdf8;border-radius:4px;cursor:pointer" onclick="clearWindowPause(${z.id})">Închide pauza ✕</button></div>` : "";

    const zid = (z.id !== undefined) ? z.id : (i + 1);
    const pVal = pendingSetpoints[zid];
    const isPending = (pVal !== undefined && typeof pVal.target === "number");
    const targetVal = isPending ? pVal.target : (typeof z.effectiveTarget === "number" ? z.effectiveTarget : 20.0);
    const isHold = z.isManualHold || isPending;

    return `<div class="room ${roomClass(z)}" data-i="${i}">
      <div class="room-main">
        <div style="font-weight:600">${esc(z.name)}</div>
        <div class="t">${t}</div>
        <div class="s">${esc(subText)}</div>
        <div class="s" style="margin-top:2px">${esc(statusText)}</div>
        ${winBtn}
        <div class="s" style="margin-top:2px;font-size:12px;opacity:0.85">${esc(rtBadge)}</div>
      </div>
      <div class="ctl">
        <button type="button" data-a="up" data-i="${i}" title="Crește ținta">+</button>
        <div class="sp-val" title="Țintă">${(typeof targetVal === "number" ? targetVal.toFixed(1) : "20.0")}°</div>
        <button type="button" data-a="dn" data-i="${i}" title="Scade ținta">−</button>
        <button type="button" class="sp-clr ${isHold ? '' : 'hidden'}" data-a="clr" data-i="${i}" title="Revino la program">↺ prog</button>
      </div>
    </div>`;
  }).join("");
}

function renderHeader() {
  $("clock").textContent = ST ? ST.time.slice(0, 5) : "--:--";
  $("outdoor").textContent = ST && ST.outdoorTemp > -100 ? "afară " + ST.outdoorTemp.toFixed(1) + "°" : "afară --";
  $("boiler").textContent = "centrala " + (Z.boiler === "ON" ? "pornită 🔥" : "oprită");
  $("level").textContent = scheduleText();
  $("away").classList.toggle("on", Z.away);
}

function renderAlerts() {
  const a = [];
  Z.zones.forEach(z => {
    // Until FR-51 is built (migration step 7) the zone does not heat on sensor loss: say what the valve does
    if (z.state === "FAULT") a.push(`${z.name}: senzor pierdut – ` + (z.relayEngaged ? "robinet deschis" : "zona nu încălzește"));
    if (z.state === "FROST") a.push(`${z.name}: protecție la îngheț activă`);
    if (z.state === "WINDOW") a.push(`${z.name}: fereastră deschisă – pauză ${Math.ceil(z.windowPauseRemainingSec / 60)} min`);
  });
  B.forEach(s => { if (s.bat > 0 && s.bat < 15) a.push(`Baterie slabă: ${s.name || s.mac} (${s.bat}%)`); });
  (D && D.dtcs || []).forEach(d => {
    if (d.code === "DTC-403") {
      a.push(`<span style="display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap">` +
             `<span>⚠️ <b>DTC-403</b>: Credențiale implicite active (schimbă parola)</span>` +
             `<button type="button" class="s" style="padding:2px 8px;font-size:11px;background:#f59e0b;color:#000;border:none;border-radius:4px;cursor:pointer;font-weight:600" onclick="goToAdminCreds()">Schimbă Parola 🔐</button>` +
             `</span>`);
    } else {
      a.push(esc(`${d.code}: ${d.message}`));
    }
  });
  $("alerts").innerHTML = a.map(x => x.startsWith("<span") ? `<li>${x}</li>` : `<li>${x}</li>`).join("");
}

const busy = id => $(id).contains(document.activeElement);

$("rooms").addEventListener("click", async e => {
  const b = e.target.closest("button");
  if (b && b.dataset.a) {
    const i = +b.dataset.i, z = Z && Z.zones && Z.zones[i];
    if (!z) return;
    const zid = (z.id !== undefined) ? z.id : (i + 1);

    if (b.dataset.a === "clr") {
      delete pendingSetpoints[zid];
      try {
        await api("api/zones/hold/clear", {id: zid});
        refresh();
      } catch (err) { showErr(err.message); }
      return;
    }

    // 1. Calculate new target optimistically from persistent pendingSetpoints or effectiveTarget
    const step = b.dataset.a === "up" ? STEP : -STEP;
    const pVal = pendingSetpoints[zid];
    const curTarget = (pVal !== undefined && typeof pVal.target === "number")
      ? pVal.target
      : (typeof z.effectiveTarget === "number" ? z.effectiveTarget : 20.0);
    const nextTarget = Math.min(MAX_T, Math.max(MIN_T, +(curTarget + step).toFixed(1)));

    // Cancel in-flight debounce timer if user is clicking rapidly
    if (pVal && pVal.timer) clearTimeout(pVal.timer);

    pendingSetpoints[zid] = {
      target: nextTarget,
      holdUntil: Date.now() + 2500,
      timer: setTimeout(async () => {
        try {
          await api("api/zones/setpoint", {id: zid, target: nextTarget.toFixed(1)});
        } catch (err) {
          showErr(err.message);
        } finally {
          setTimeout(() => {
            if (pendingSetpoints[zid] && pendingSetpoints[zid].target === nextTarget) {
              delete pendingSetpoints[zid];
            }
          }, 1200);
          refresh();
        }
      }, 350)
    };

    // 2. Instant DOM update with zero flicker
    const card = b.closest(".room");
    if (card) {
      const spVal = card.querySelector(".sp-val");
      if (spVal) spVal.textContent = nextTarget.toFixed(1) + "°";
      const clrBtn = card.querySelector(".sp-clr");
      if (clrBtn) clrBtn.classList.remove("hidden");
    }
    return;
  }
  if (e.target.closest(".ctl")) return;
});

$("away").onclick = async () => {
  const nextAway = !Z.away;
  Z.away = nextAway;
  $("away").classList.toggle("on", nextAway);
  try {
    await api("api/schedule/enable", {away: nextAway ? 1 : 0});
  } catch (err) {
    showErr(err.message);
  } finally {
    refresh();
  }
};

window.clearWindowPause = async function(id) {
  try {
    await fetch(`/api/zones/window/clear?id=${encodeURIComponent(id)}`, { method: "POST" });
    refresh();
  } catch (e) {
    console.error("clearWindowPause error:", e);
  }
};

