// FR-8.1 (C-47): family view as a panel - icons, themes, room cards with a dial, charts, boiler / gas / state cards
const IC = {
thermo: '<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/>',
flame: '<path d="M8.5 14.5A2.5 2.5 0 0 0 11 12c0-1.38-.5-2-1-3-1.072-2.143-.224-4.054 2-6 .5 2.5 2 4.9 4 6.5 2 1.6 3 3.5 3 5.5a7 7 0 1 1-14 0c0-1.153.433-2.294 1-3a2.5 2.5 0 0 0 2.5 2.5z"/>',
pause: '<circle cx="12" cy="12" r="10"/><line x1="10" x2="10" y1="15" y2="9"/><line x1="14" x2="14" y1="15" y2="9"/>',
clock: '<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>',
down: '<path d="m6 9 6 6 6-6"/>', up: '<path d="m18 15-6-6-6 6"/>',
wind: '<path d="M17.7 7.7a2.5 2.5 0 1 1 1.8 4.3H2"/><path d="M9.6 4.6A2 2 0 1 1 11 8H2"/><path d="M12.6 19.4A2 2 0 1 0 14 16H2"/>',
fuel: '<line x1="3" x2="15" y1="22" y2="22"/><line x1="4" x2="14" y1="9" y2="9"/><path d="M14 22V4a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v18"/><path d="M14 13h2a2 2 0 0 1 2 2v2a2 2 0 0 0 2 2a2 2 0 0 0 2-2V9.83a2 2 0 0 0-.59-1.42L18 5"/>',
activity: '<path d="M22 12h-4l-3 9L9 3l-3 9H2"/>',
check: '<circle cx="12" cy="12" r="10"/><path d="m9 12 2 2 4-4"/>',
alert: '<path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/><path d="M12 9v4"/><path d="M12 17h.01"/>',
snow: '<line x1="2" x2="22" y1="12" y2="12"/><line x1="12" x2="12" y1="2" y2="22"/><path d="m20 16-4-4 4-4"/><path d="m4 8 4 4-4 4"/><path d="m16 4-4 4-4-4"/><path d="m8 20 4-4 4 4"/>',
minus: '<path d="M5 12h14"/>', plus: '<path d="M5 12h14"/><path d="M12 5v14"/>',
home: '<path d="m3 9 9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>',
cal: '<rect width="18" height="18" x="3" y="4" rx="2"/><path d="M16 2v4"/><path d="M8 2v4"/><path d="M3 10h18"/>',
bars: '<path d="M3 3v18h18"/><path d="M8 17v-5"/><path d="M13 17V8"/><path d="M18 17v-3"/>',
cog: '<path d="M4 7h3"/><path d="M12 7h8"/><path d="M4 17h8"/><path d="M17 17h3"/><circle cx="9.5" cy="7" r="2.5"/><circle cx="14.5" cy="17" r="2.5"/>',
term: '<polyline points="4 17 10 11 4 5"/><line x1="12" x2="20" y1="19" y2="19"/>',
lock: '<rect width="18" height="11" x="3" y="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>'
};
const ic = (n, c = "", st = "") => `<svg class="ic ${c}"${st ? ` style="${st}"` : ""} viewBox="0 0 24 24" aria-hidden="true">${IC[n]}</svg>`;

// theme: label, mode, then the values of THEME_KEYS
const THEME_KEYS = ["page", "card", "elev", "line", "fg", "mut", "heat", "glow", "idle", "ok", "warn", "bad", "frost", "radius", "blur"];
const THEMES = {
  "ha-charcoal": ["HA Charcoal", "dark", "#16171b", "#1f2025", "#2a2b32", "#2f3038", "#ececf1", "#9a9ba6", "#ffa63d", "rgba(255,179,92,.35)", "#6b8cae", "#3ec27a", "#f2c230", "#ef5350", "#5fd0e8", "16px", "0px"],
  "glass-glow": ["Glass Glow", "dark", "linear-gradient(135deg,#0f1b2d 0%,#2a1738 100%)", "rgba(255,255,255,.08)", "rgba(255,255,255,.14)", "rgba(255,255,255,.22)", "#ffffff", "rgba(255,255,255,.65)", "#ff9a4d", "rgba(255,176,102,.45)", "#7fb2ff", "#5ee0a0", "#ffd75e", "#ff6b6b", "#7de3ff", "28px", "24px"],
  "smartiva-dark": ["Smartiva Dark", "dark", "#000000", "#0b0b0d", "#15151a", "#26262c", "#fafafa", "#8a8a94", "#ff8a1f", "rgba(255,138,31,.5)", "#5d7a99", "#2fd37a", "#ffc61a", "#ff4040", "#4cc9e6", "20px", "0px"],
  "nordic-light": ["Nordic Light", "light", "#f6f3ee", "#ffffff", "#efe9e1", "#e3dbd0", "#2b2622", "#8b8178", "#f08a3c", "rgba(245,178,122,.35)", "#7e9bb5", "#3f9f6f", "#d9a21b", "#d94a46", "#4fb8d0", "20px", "0px"],
  "smartiva-glass": ["Smartiva Glass", "dark", "radial-gradient(circle at 78% 36%,rgba(255,145,36,.22) 0%,transparent 44%),radial-gradient(circle at 18% 72%,rgba(62,125,178,.22) 0%,transparent 48%),linear-gradient(135deg,#1a2028 0%,#282e35 50%,#191d24 100%)", "rgba(255,255,255,.085)", "rgba(255,255,255,.14)", "rgba(255,255,255,.18)", "#f5f7fa", "rgba(232,238,245,.66)", "#e8f117", "rgba(232,241,23,.35)", "#78a9d6", "#4ade80", "#ffb72b", "#ff5a5f", "#59d2ff", "24px", "28px"]
,
  "meteo-dinamic": ["Meteo Dinamic", "dark"]  // values computed by meteoTheme()
};
// FR-8.1 (C-49): theme Meteo Dinamic. The background follows only what the controller reports: its clock and date
// (night, dawn, day, dusk) and the outdoor temperature (frost, cold, mild, warm). Nothing the controller does not report.
const SUN_H = [[8, 17], [7, 18], [6, 18], [6, 20], [5, 21], [5, 21], [5, 21], [6, 20], [7, 19], [7, 18], [7, 17], [8, 16]];  // rough sunrise / sunset hour per month
let meteoKey = "";
function meteoState() {
  if (!ST || !ST.time) return ["night", "150,170,200"];
  const h = +ST.time.slice(0, 2) + ST.time.slice(3, 5) / 60, sun = SUN_H[(+(ST.date || "").slice(5, 7) || 1) - 1];
  const phase = h < sun[0] - 1 || h >= sun[1] + 1 ? "night" : h < sun[0] + 1 ? "dawn" : h >= sun[1] - 1 ? "dusk" : "day";
  const t = ST.outdoorTemp > -100 ? ST.outdoorTemp : null;
  return [phase, t === null ? "150,170,200" : t <= 0 ? "89,210,255" : t < 10 ? "96,150,220" : t < 20 ? "94,200,170" : "255,170,70"];
}
function meteoTheme() {
  const [phase, sky] = meteoState();
  meteoKey = phase + sky;
  if (phase === "day") return ["Meteo Dinamic", "light",
    `radial-gradient(circle at 82% 12%,rgba(255,214,120,.6) 0%,transparent 38%),radial-gradient(circle at 14% 82%,rgba(${sky},.4) 0%,transparent 50%),linear-gradient(160deg,#dce8f4 0%,#c6d9ea 55%,#e9eef3 100%)`,
    "rgba(255,255,255,.6)", "rgba(255,255,255,.75)", "rgba(30,50,80,.16)", "#1c2733", "rgba(28,39,51,.66)",
    "#f08a3c", "rgba(240,138,60,.4)", "#4f7fae", "#2f8f5f", "#b8860b", "#d94a46", "#2a9bb5", "24px", "24px"];
  const low = phase === "dawn" ? "18% 88%" : "82% 88%";
  const page = phase === "night"
    ? `radial-gradient(circle at 80% 18%,rgba(${sky},.2) 0%,transparent 45%),linear-gradient(160deg,#0b1020 0%,#141a2e 55%,#0a0d18 100%)`
    : `radial-gradient(circle at ${low},rgba(255,140,80,.38) 0%,transparent 52%),radial-gradient(circle at 80% 14%,rgba(${sky},.22) 0%,transparent 45%),linear-gradient(160deg,#1a2140 0%,#33294a 60%,#4d3340 100%)`;
  return ["Meteo Dinamic", "dark", page, "rgba(255,255,255,.085)", "rgba(255,255,255,.14)", "rgba(255,255,255,.18)", "#f5f7fa", "rgba(232,238,245,.68)",
    "#ffa63d", "rgba(255,166,61,.4)", "#7fb2ff", "#5ee0a0", "#ffd75e", "#ff6b6b", "#7de3ff", "24px", "24px"];
}
let themeId = "smartiva-glass";  // first visit; the choice is kept in the browser
try { const t = localStorage.getItem("sh_theme"); if (THEMES[t]) themeId = t; } catch (e) {}
function applyTheme(id) {
  themeId = id;
  const t = id === "meteo-dinamic" ? meteoTheme() : THEMES[id], r = document.documentElement, dark = t[1] === "dark";
  THEME_KEYS.forEach((k, i) => r.style.setProperty("--" + k, t[i + 2]));
  r.style.setProperty("--bg", t[4]);
  r.style.setProperty("--away", t[7]);
  r.style.setProperty("--inv", dark ? "#16171b" : "#ffffff");
  r.style.setProperty("--chg", dark ? "#854d0e" : "#fde68a");
  r.style.colorScheme = t[1];
  r.dataset.theme = id;
  try { localStorage.setItem("sh_theme", id); } catch (e) {}
  $("themeBtns").querySelectorAll("button").forEach(b => b.setAttribute("aria-pressed", b.dataset.t === id));
  $("themeSel").value = id;
}
$("themeBtns").innerHTML = Object.keys(THEMES).map(id => `<button type="button" data-t="${id}">${THEMES[id][0]}</button>`).join("");
$("themeSel").innerHTML = Object.keys(THEMES).map(id => `<option value="${id}">${THEMES[id][0]}</option>`).join("");
$("themeBtns").addEventListener("click", e => { const b = e.target.closest("button"); if (b) applyTheme(b.dataset.t); });
$("themeSel").addEventListener("change", e => applyTheme(e.target.value));
applyTheme(themeId);
$("logo").innerHTML = ic("thermo");
$("pIcClock").innerHTML = ic("clock", "i14");
$("pIcGas").innerHTML = ic("fuel");
$("pIcChart").innerHTML = ic("activity");

const MONTHS_RO = ["ianuarie", "februarie", "martie", "aprilie", "mai", "iunie", "iulie", "august", "septembrie", "octombrie", "noiembrie", "decembrie"];
const p2 = n => String(n).padStart(2, "0");
const mmss = s => p2(Math.floor(s / 60)) + ":" + p2(s % 60);
const hmin = s => { const m = Math.round((s || 0) / 60); return m < 60 ? m + " min" : Math.floor(m / 60) + " h " + p2(m % 60) + " min"; };

function roomClass(z) {
  if (z.state === "WINDOW" || z.windowPauseActive || z.windowDetected) return "win";
  if (z.state === "FAULT" || z.state === "DISABLED") return "bad";
  if (z.state === "FROST") return "frost";
  if (Z.away) return "away";
  return z.relayEngaged ? "heat" : "";
}
const ROOM_COL = {win: "warn", bad: "bad", frost: "frost", away: "mut", heat: "heat", "": "idle"};
const ROOM_IC = {win: "wind", bad: "alert", frost: "snow", away: "home", heat: "flame", "": "check"};

// FR-11.3: the family view shows the zones switched on (and a zone the frost protection heats)
const shownZones = () => Z.zones.map((z, i) => [z, i]).filter(([z]) => z.enabled || z.state === "FROST");
const zoneId = (z, i) => (z.id !== undefined) ? z.id : (i + 1);
function zoneTarget(z, i) {
  const pVal = pendingSetpoints[zoneId(z, i)];
  return (pVal !== undefined && typeof pVal.target === "number") ? pVal.target : (typeof z.effectiveTarget === "number" ? z.effectiveTarget : 20.0);
}
const isWindow = z => z.state === "WINDOW" || z.windowPauseActive || z.windowDetected;
function zoneStatus(z) {
  const dutyPercent = (z.dutyPct !== undefined) ? z.dutyPct : (z.duty <= 1.0 ? Math.round((z.duty || 0) * 100) : Math.round(z.duty || 0));
  const onMin = (z.onTimeSec !== undefined && z.onTimeSec > 0) ? Math.round(z.onTimeSec / 60) : Math.round(dutyPercent * 0.6);
  if (isWindow(z)) {
    const mLeft = Math.ceil((z.windowPauseRemainingSec || 1800) / 60);
    return (z.windowPauseActive || z.state === "WINDOW") ? `Fereastră deschisă · Pauză ${mLeft}m` : "Fereastră deschisă (căldura continuă)";
  }
  if (z.state === "IDLE") return (z.sensorValid && z.currentTemp < (z.effectiveTarget - 0.15)) ? "Pauză ciclu (menține din șapă)" : "Temperatură atinsă";
  if (z.state === "HEATING") return (dutyPercent >= 99) ? "Încălzire continuă (100%)" : `Încălzire TPI · ${onMin} min/h (${dutyPercent}%)`;
  return STATE_RO[z.state] || z.state;
}

// dial geometry: 270 degrees, from MIN_T to MAX_T (FR-1.2)
const polar = (r, a) => { const t = (a - 90) * Math.PI / 180; return [144 + r * Math.cos(t), 144 + r * Math.sin(t)]; };
function arcPath(a0, a1) {
  const s = polar(116, a1), e = polar(116, a0);
  return `M ${s[0].toFixed(2)} ${s[1].toFixed(2)} A 116 116 0 ${a1 - a0 <= 180 ? 0 : 1} 0 ${e[0].toFixed(2)} ${e[1].toFixed(2)}`;
}
const tRatio = t => Math.max(0, Math.min(1, (t - MIN_T) / (MAX_T - MIN_T)));

let openZ = null, roomsSig = "";
function renderRooms() {
  const list = shownZones();
  if (!list.length) {
    roomsSig = "";
    $("rooms").innerHTML = `<div class="card"><div style="font-weight:700">Nicio zonă pornită</div><div class="s">Punerea în funcțiune se face din Admin.</div></div>`;
    return;
  }
  if (openZ === null) openZ = list[0][1];
  const sig = list.map(([z, i]) => i + ":" + z.name).join("|") + "#" + openZ;
  if (sig !== roomsSig) {
    roomsSig = sig;
    $("rooms").innerHTML = list.map(([z, i]) => {
      const ex = i === openZ;
      let ticks = "";
      if (ex) for (let k = 0; k < 46; k++) {
        const a = 225 + k / 45 * 270, p = polar(96, a), q = polar(102, a);
        ticks += `<line class="tick" x1="${p[0].toFixed(1)}" y1="${p[1].toFixed(1)}" x2="${q[0].toFixed(1)}" y2="${q[1].toFixed(1)}"/>`;
      }
      const pMin = polar(132, 225), pMax = polar(132, 495);
      return `<div class="card zone" data-i="${i}">
      <div class="wave" aria-hidden="true"><svg viewBox="0 0 800 80" preserveAspectRatio="none"><defs><linearGradient id="wv${i}" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" style="stop-color:var(--heat);stop-opacity:0"/><stop offset="1" style="stop-color:var(--heat);stop-opacity:.48"/></linearGradient></defs>
        <path d="M0,36 C100,10 200,62 300,36 C400,10 500,62 600,36 C700,10 750,50 800,36 L800,80 L0,80 Z" fill="url(#wv${i})"/></svg></div>
      <button type="button" class="zhead" data-x="${i}" aria-expanded="${ex}">
        <div class="l"><svg class="ring" width="52" height="52" viewBox="0 0 52 52" aria-hidden="true"><circle class="rb" cx="26" cy="26" r="23.75"/>
          <circle class="rf" data-k="ring" cx="26" cy="26" r="23.75" stroke-dasharray="149.23"/></svg>
          <div style="min-width:0"><div class="zname">${esc(z.name)}</div><div class="zmeta" data-k="meta"></div><div class="zstat" data-k="stat"></div></div></div>
        <div class="r"><div class="ztemp mn"><span data-k="temp"></span><span class="deg">°</span></div><span class="chev" aria-hidden="true">${ic(ex ? "up" : "down", "i20")}</span></div>
      </button>
      ${ex ? `<div class="zbody">
        <div class="winbar" data-k="win" role="alert" hidden><div class="l"><div class="radar"><i></i><i></i>${ic("wind", "i20")}</div>
          <div><div class="t">Fereastră deschisă</div><div class="u" data-k="wintxt"></div></div></div>
          <button type="button" onclick="clearWindowPause(${zoneId(z, i)})">Reia încălzirea</button></div>
        <div class="dialwrap"><div class="dial">
          <svg data-k="dial" viewBox="0 0 288 288" role="slider" tabindex="0" aria-label="Termostat ${esc(z.name)}" aria-valuemin="${MIN_T}" aria-valuemax="${MAX_T}">
            <defs><linearGradient id="dg${i}" x1="0" y1="1" x2="1" y2="0"><stop offset="0" style="stop-color:var(--frost)"/>
              <stop offset=".45" style="stop-color:var(--idle)"/><stop offset="1" style="stop-color:var(--acc)"/></linearGradient></defs>
            <g data-k="ticks">${ticks}</g><path class="track" d="${arcPath(225, 495)}"/><path class="arc" data-k="arc" stroke="url(#dg${i})"/>
            <text x="${pMin[0].toFixed(1)}" y="${(pMin[1] + 12).toFixed(1)}" text-anchor="middle">${MIN_T}°</text>
            <text x="${pMax[0].toFixed(1)}" y="${(pMax[1] + 12).toFixed(1)}" text-anchor="middle">${MAX_T}°</text>
            <circle class="cur" data-k="cur" r="4.5"/><circle class="thumb" data-k="thumb" r="11"/></svg>
          <div class="readout"><span class="a">Actual</span><div class="big mn"><span data-k="temp2"></span><span class="deg">°</span></div>
            <div class="tg"><span class="s">Țintă</span><b class="mn" data-k="rtarget"></b></div><div class="eta" data-k="eta"></div></div></div>
          <div class="stepper"><button type="button" data-a="dn" data-i="${i}" title="Scade ținta" aria-label="Scade ținta">${ic("minus", "i20")}</button>
            <div class="mid"><span class="tiny">Reglaj fin</span><b class="mn" data-k="fine"></b></div>
            <button type="button" data-a="up" data-i="${i}" title="Crește ținta" aria-label="Crește ținta">${ic("plus", "i20")}</button></div></div>
        <div class="zacts"><button type="button" data-a="clr" data-i="${i}" data-k="clr" title="Revino la program" hidden>Înapoi la program</button>
          <span class="tiny" data-k="rt"></span></div>
        <div class="chartbox"><div class="pr" style="margin-bottom:8px"><span class="lbl">Ultimele 24 h · ${esc(z.name)}</span>
          <div class="legend"><span><i style="width:12px;height:2px;background:var(--fg)"></i>Cameră</span>
          <span><i style="width:12px;height:2px;background:var(--idle)"></i>Afară</span>
          <span><i style="width:8px;height:4px;border-radius:2px;background:var(--heat)"></i>Vană deschisă</span></div></div>
          <div class="chart" id="zChart"></div></div>
      </div>` : ""}
      </div>`;
    }).join("");
    paintCharts();
  }
  list.forEach(([z, i]) => paintRoom(z, i));
}

function paintRoom(z, i) {
  const el = $("rooms").querySelector(`.zone[data-i="${i}"]`);
  if (!el) return;
  const q = k => el.querySelector(`[data-k="${k}"]`);
  const cls = roomClass(z), target = zoneTarget(z, i), has = z.sensorValid && typeof z.currentTemp === "number";
  const s = B.find(x => x.mac.toUpperCase() === (z.sensorMac || "").toUpperCase());
  el.className = "card zone " + cls;
  el.style.setProperty("--acc", `var(--${ROOM_COL[cls]})`);
  q("ring").style.strokeDashoffset = 149.23 * (1 - Math.max(.04, has ? tRatio(z.currentTemp) : 0));
  const meta = [];
  if (s) meta.push(`Umiditate ${Math.round(s.hum)}%`);
  meta.push(`Țintă ${target.toFixed(1)}°C`);
  if (z.solarGain) meta.push("☀️ Sud/Vest");
  if (z.isBoost) meta.push("Boost manual");
  if (!has) meta.push("Fără senzor");
  q("meta").innerHTML = meta.map(esc).join(' <span aria-hidden="true">·</span> ');
  const status = zoneStatus(z);
  q("stat").innerHTML = ic(status.startsWith("Pauză") ? "pause" : ROOM_IC[cls], "i14") + `<span>${esc(status)}</span>`;
  q("temp").textContent = has ? z.currentTemp.toFixed(1) : "--";
  if (i !== openZ) return;
  const tr = tRatio(target), ta = 225 + tr * 270, tp = polar(116, ta), cp = polar(116, 225 + tRatio(has ? z.currentTemp : MIN_T) * 270);
  q("arc").setAttribute("d", tr > .005 ? arcPath(225, ta) : "");
  q("thumb").setAttribute("cx", tp[0]); q("thumb").setAttribute("cy", tp[1]);
  q("cur").setAttribute("cx", cp[0]); q("cur").setAttribute("cy", cp[1]);
  q("ticks").querySelectorAll("line").forEach((l, k) => l.classList.toggle("on", k / 45 <= tr));
  const d = q("dial");
  d.setAttribute("aria-valuenow", target); d.setAttribute("aria-valuetext", target.toFixed(1) + " grade Celsius");
  q("temp2").textContent = has ? z.currentTemp.toFixed(1) : "--";
  q("rtarget").textContent = target.toFixed(1) + "°C";
  q("fine").textContent = target.toFixed(1) + " °C";
  q("eta").textContent = STATE_RO[z.state] || z.state;
  q("win").hidden = !isWindow(z);
  q("wintxt").textContent = (z.windowPauseActive || z.state === "WINDOW")
    ? `Încălzirea camerei e în pauză încă ${Math.ceil((z.windowPauseRemainingSec || 0) / 60)} min.` : "Căldura continuă.";
  q("clr").hidden = !(z.isManualHold || z.isBoost || pendingSetpoints[zoneId(z, i)] !== undefined);
  q("rt").textContent = `Încălzit azi: ${hmin(z.runtimeTodaySec)}` + (z.runtimeYesterdaySec > 0 ? ` (ieri ${hmin(z.runtimeYesterdaySec)})` : "");
}

// Optimistic target: shown at once, sent after 350 ms without another change (steppers, dial, keys)
function setTarget(i, t) {
  const z = Z && Z.zones && Z.zones[i];
  if (!z) return;
  const zid = zoneId(z, i), pVal = pendingSetpoints[zid];
  const nextTarget = Math.min(MAX_T, Math.max(MIN_T, +(Math.round(t / STEP) * STEP).toFixed(1)));
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
          if (pendingSetpoints[zid] && pendingSetpoints[zid].target === nextTarget) delete pendingSetpoints[zid];
        }, 1200);
        refresh();
      }
    }, 350)
  };
  paintRoom(z, i);
}

function renderHeader() {
  const on = Z.boiler === "ON";
  $("clock").textContent = ST ? ST.time.slice(0, 5) : "--:--";
  $("outdoor").textContent = ST && ST.outdoorTemp > -100 ? "afară " + ST.outdoorTemp.toFixed(1) + "°" : "afară --";
  $("boiler").innerHTML = on ? ic("flame", "flick") + "centrala pornită" : ic("pause") + "centrala oprită";
  $("boiler").classList.toggle("on", on);
  $("level").textContent = scheduleText();
  $("away").classList.toggle("on", Z.away);
  if (themeId === "meteo-dinamic" && meteoState().join("") !== meteoKey) applyTheme(themeId);
  renderSide();
}

// Left column: clock of the controller, boiler as commanded (no flame signal), gas figures of FR-35.1
let clk = null;
function paintClock() {
  if (!clk) return;
  const s = (clk.base + Math.floor((Date.now() - clk.at) / 1000)) % 86400;
  $("pHm").textContent = p2(Math.floor(s / 3600)) + ":" + p2(Math.floor(s / 60) % 60);
  $("pSs").textContent = p2(s % 60);
}
setInterval(paintClock, 1000);

function renderSide() {
  const on = Z.boiler === "ON", rt = Z.boilerRuntime || {};
  const m = ST && /^(\d\d):(\d\d):(\d\d)/.exec(ST.time || "");
  if (m) clk = {base: +m[1] * 3600 + +m[2] * 60 + +m[3], at: Date.now()};
  paintClock();
  const dm = ST && /^(\d{4})-(\d\d)-(\d\d)/.exec(ST.date || "");
  if (dm) $("pDate").textContent = `${DAYS_FULL[new Date(+dm[1], +dm[2] - 1, +dm[3]).getDay()]}, ${+dm[3]} ${MONTHS_RO[+dm[2] - 1]} ${dm[1]}`;
  $("pOut").textContent = ST && ST.outdoorTemp > -100 ? ST.outdoorTemp.toFixed(1) + "°C" : "--";
  $("pBoiler").classList.toggle("on", on);
  $("pBIcon").innerHTML = on ? ic("flame", "i24 flick") : ic("pause", "i24");
  $("pBState").textContent = on ? "Pornită" : "Oprită";
  $("pBPause").textContent = Z.boilerCooldownSec > 0 ? mmss(Z.boilerCooldownSec) : "Pregătit";
  $("pBPause").classList.toggle("wait", Z.boilerCooldownSec > 0);
  $("pBToday").textContent = hmin(Z.boilerRuntime.todaySec);
  $("pBYest").textContent = hmin(rt.yesterdaySec);
  $("pBBurn").textContent = on ? hmin(rt.currentBurnSec) : "—";
  const g = ST && ST.gasMeter;
  if (!g) { $("pGas").innerHTML = ""; return; }
  const measured = typeof g.gasFlowMeasured === "number", price = g.gasPrice || 0;
  $("pGPrice").textContent = price.toFixed(2) + " Lei/m³";
  const cell = (k, m3, dec) => `<div class="kv"><div class="k">${k}</div><div class="v big mn">${measured ? (m3 * price).toFixed(dec) + " Lei" : "—"}</div>` +
    `<div class="tiny mn">${measured ? m3.toFixed(2) + " m³" : ""}</div></div>`;
  $("pGas").innerHTML = `<div class="pr gasrate"><div class="lbl">${ic("flame")}<span>Debit mediu măsurat</span></div>` +
    `<span class="v mn">${measured ? g.gasFlowMeasured.toFixed(2) + " m³/h" : "—"}</span></div>` +
    `<div class="g3" style="margin-top:12px">${cell("Astăzi", g.gasTodayM3 || 0, 2)}${cell("Ieri", g.gasYesterdayM3 || 0, 2)}${cell("Luna curentă", g.gasThisMonthM3 || 0, 0)}</div>` +
    (measured ? "" : `<div class="tiny" style="margin-top:8px">Consumul apare după două citiri de contor (Admin).</div>`);
}

// 24 h history (FR-34): points [slot, z1, z2, z3, outdoor, flags]; flags bit 7 recorded, bit 0 boiler, bits 2-4 valves
let PH = null, phAt = 0;
const panelHistDue = () => Date.now() - phAt > 300000;
function histOrdered() {
  if (!PH || !PH.points) return [];
  const cur = PH.currentSlot || 0, map = {}, out = [];
  PH.points.forEach(p => { map[p[0]] = p; });
  for (let k = 1; k <= 288; k++) {
    const s = (cur + k) % 288, p = map[s];
    if (p && (p[5] & 0x80)) out.push({k, s, p});
  }
  return out;
}
const CH_COL = ["heat", "ok", "frost"];
function drawChart(el, o) {
  const pts = o.pts;
  if (!pts.length) { el.innerHTML = `<div class="tiny" style="padding:12px 0">Istoricul apare după primele înregistrări.</div>`; return; }
  const W = el.clientWidth || 300, H = o.h, L = 38, R = 38, T = 6, Bm = 18, pw = W - L - R, ph = H - T - Bm - 6;
  const ok = v => typeof v === "number" && v > -50 && v < 80;
  const range = idxs => {
    let lo = 99, hi = -99;
    pts.forEach(d => idxs.forEach(c => { const v = d.p[c]; if (ok(v)) { lo = Math.min(lo, v); hi = Math.max(hi, v); } }));
    if (lo > hi) return [0, 1];
    lo = Math.floor(lo * 2 - .5) / 2; hi = Math.ceil(hi * 2 + .5) / 2;
    return [lo, hi];
  };
  const x = k => L + (k - 1) / 287 * pw;
  const rl = range(o.lines.map(l => l.c)), rr = range([4]);
  const y = (v, r) => T + ph - (v - r[0]) / (r[1] - r[0]) * ph;
  let s = `<svg width="${W}" height="${H}" viewBox="0 0 ${W} ${H}" role="img" aria-label="${esc(o.label)}">`;
  [0, .5, 1].forEach(f => {
    const yy = T + ph - f * ph;
    s += `<line class="gl" x1="${L}" x2="${W - R}" y1="${yy}" y2="${yy}"/>` +
      `<text class="ax" x="${L - 6}" y="${yy + 3.5}" text-anchor="end">${(rl[0] + f * (rl[1] - rl[0])).toFixed(1)}°</text>` +
      `<text class="ax" x="${W - R + 6}" y="${yy + 3.5}" style="fill:var(--idle)">${(rr[0] + f * (rr[1] - rr[0])).toFixed(1)}°</text>`;
  });
  pts.forEach(d => { if (d.s % 72 === 0) s += `<text class="ax" x="${x(d.k).toFixed(1)}" y="${H - 4}" text-anchor="middle">${p2(d.s / 12)}:00</text>`; });
  const path = (c, r) => {
    let dd = "", prev = -9;
    pts.forEach(d => { const v = d.p[c]; if (!ok(v)) return; dd += (d.k - prev > 2 ? "M" : "L") + x(d.k).toFixed(1) + " " + y(v, r).toFixed(1); prev = d.k; });
    return dd;
  };
  s += `<path d="${path(4, rr)}" fill="none" style="stroke:var(--idle);stroke-width:1.5;opacity:.8" stroke-dasharray="3 3"/>`;
  o.lines.forEach(l => { s += `<path d="${path(l.c, rl)}" fill="none" style="stroke:var(--${l.col});stroke-width:${l.w}"/>`; });
  const bw = Math.max(1, pw / 287);
  pts.forEach(d => { if (d.p[5] & o.bit) s += `<rect x="${(x(d.k) - bw / 2).toFixed(1)}" y="${T + ph + 3}" width="${(bw + .3).toFixed(1)}" height="4" style="fill:var(--heat);opacity:.8"/>`; });
  el.innerHTML = s + "</svg>";
}
function paintCharts() {
  if (!Z) return;
  const pts = histOrdered(), list = shownZones().filter(([z, i]) => i < 3);
  $("pLegend").innerHTML = list.map(([z, i]) => `<span><i style="width:12px;height:6px;border-radius:2px;background:var(--${CH_COL[i]})"></i>${esc(z.name)}</span>`).join("") +
    `<span><i style="width:12px;height:2px;background:var(--idle)"></i>Afară (dreapta)</span>` +
    `<span><i style="width:8px;height:4px;border-radius:2px;background:var(--heat)"></i>Centrală pornită</span>`;
  drawChart($("pChart"), {h: 168, pts, bit: 1, label: "Temperaturi în ultimele 24 de ore, toate camerele",
    lines: list.map(([z, i]) => ({c: i + 1, col: CH_COL[i], w: 1.8}))});
  const zc = $("zChart");
  if (zc && openZ !== null && openZ >= 0 && openZ < 3) drawChart(zc, {h: 130, pts, bit: 1 << (openZ + 2), label: "Temperatura camerei în ultimele 24 de ore",
    lines: [{c: openZ + 1, col: "fg", w: 2}]});
  else if (zc) zc.innerHTML = "";
}
let rzT = 0;
addEventListener("resize", () => { cancelAnimationFrame(rzT); rzT = requestAnimationFrame(paintCharts); });

// Card `Stare sistem`: the active alarms, or that there is none
function renderAlerts() {
  const a = [];
  Z.zones.forEach(z => {
    if (z.state === "FAULT") a.push(esc(`${z.name}: senzor pierdut – ` + (z.relayEngaged ? "robinet deschis" : "zona nu încălzește")));
    if (z.state === "FROST") a.push(esc(`${z.name}: protecție la îngheț activă`));
    if (z.state === "WINDOW") a.push(esc(`${z.name}: fereastră deschisă – pauză ${Math.ceil(z.windowPauseRemainingSec / 60)} min`));
  });
  B.forEach(s => { if (s.bat > 0 && s.bat < 15) a.push(esc(`Baterie slabă: ${s.name || s.mac} (${s.bat}%)`)); });
  (D && D.dtcs || []).forEach(d => {
    if (d.code === "DTC-403") {
      a.push(`<span style="display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap">` +
             `<span><b>DTC-403</b>: Credențiale implicite active (schimbă parola)</span>` +
             `<button type="button" class="sm" onclick="goToAdminCreds()">Schimbă parola</button></span>`);
    } else {
      const info = DTC_INFO[String(d.code).replace("DTC-", "")];
      a.push(`<b class="mn">${esc(d.code)}</b> ${esc(info ? info.title : d.message)}` + (info ? `<p>${esc(info.cause)}</p>` : ""));
    }
  });
  $("pIcState").innerHTML = a.length ? ic("alert", "", "color:var(--bad)") : ic("check", "", "color:var(--ok)");
  $("alerts").innerHTML = a.length ? a.map(x => `<li>${x}</li>`).join("")
    : `<li class="none">Fără alarme active<p>Toate zonele și senzorii răspund.</p></li>`;
}

const busy = id => $(id).contains(document.activeElement);

$("rooms").addEventListener("click", async e => {
  const b = e.target.closest("button");
  if (b && b.dataset.a) {
    const i = +b.dataset.i, z = Z && Z.zones && Z.zones[i];
    if (!z) return;
    const zid = zoneId(z, i);
    if (b.dataset.a === "clr") {
      if (pendingSetpoints[zid] && pendingSetpoints[zid].timer) clearTimeout(pendingSetpoints[zid].timer);
      delete pendingSetpoints[zid];
      try {
        await api("api/zones/hold/clear", {id: zid});
        refresh();
      } catch (err) { showErr(err.message); }
      return;
    }
    setTarget(i, zoneTarget(z, i) + (b.dataset.a === "up" ? STEP : -STEP));
    return;
  }
  // a tap on the head of a card expands it and collapses the other one
  if (b && b.dataset.x !== undefined) { openZ = (openZ === +b.dataset.x) ? -1 : +b.dataset.x; renderRooms(); }
});

// Dial: drag or arrow keys set the target in steps of STEP
let dialDrag = null;
function dialFromPointer(e) {
  const r = dialDrag.rect, px = e.clientX - (r.left + r.width / 2), py = e.clientY - (r.top + r.height / 2);
  let a = Math.atan2(px, -py) * 180 / Math.PI;
  if (a < 0) a += 360;
  let rel = a - 225;
  if (rel < 0) rel += 360;
  if (rel > 270) rel = rel > 315 ? 0 : 270;
  setTarget(dialDrag.i, MIN_T + rel / 270 * (MAX_T - MIN_T));
}
$("rooms").addEventListener("pointerdown", e => {
  const d = e.target.closest('[data-k="dial"]');
  if (!d) return;
  dialDrag = {i: +d.closest(".zone").dataset.i, rect: d.getBoundingClientRect()};
  d.setPointerCapture(e.pointerId);
  dialFromPointer(e);
});
$("rooms").addEventListener("pointermove", e => { if (dialDrag) dialFromPointer(e); });
["pointerup", "pointercancel"].forEach(t => $("rooms").addEventListener(t, () => { dialDrag = null; }));
$("rooms").addEventListener("keydown", e => {
  const d = e.target.closest('[data-k="dial"]');
  if (!d) return;
  const i = +d.closest(".zone").dataset.i, z = Z && Z.zones && Z.zones[i];
  const step = (e.key === "ArrowUp" || e.key === "ArrowRight") ? STEP : (e.key === "ArrowDown" || e.key === "ArrowLeft") ? -STEP : 0;
  if (z && step) { e.preventDefault(); setTarget(i, zoneTarget(z, i) + step); }
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
    await api(`api/zones/window/clear?id=${encodeURIComponent(id)}`, { method: "POST" });
    refresh();
  } catch (e) {
    console.error("clearWindowPause error:", e);
  }
};
