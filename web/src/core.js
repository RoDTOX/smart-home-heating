const MIN_T = 15, MAX_T = 28, STEP = 0.5;  // C-36: ceiling 28 C, boost included
const RELAYS = [[32, "Releu 1 (GPIO32)"], [33, "Releu 2 (GPIO33)"], [25, "Releu 3 (GPIO25)"], [26, "Releu 4 (GPIO26)"]];
const DOC = JSON.parse(document.getElementById("doc").textContent);
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"]/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"}[c]));
const isAdmin = () => document.body.classList.contains("admin");
let Z = null, S = null, B = [], ST = null, D = null, openRoom = -1, editing = false, dtcHistory = [];
let histData24h = null, histData5d = null, histLoading = false, lastHistFetch = 0;
let selectedHistRange = "24h";
let again = false, lastFound = [];
let RAW = {}, prevFlat = {}, changed = new Set(), trace = [], openSig = null;
const dirtyRows = new Set();
const pendingSetpoints = {}; // zoneId -> { target: number, holdUntil: number, timer: number }

// A refusal from the controller (for example a boost above the limit) is a toast fixed to the viewport, visible 8 s
// or until tapped (FR-8.1): the next refresh must not wipe it
let errUntil = 0;
function showErr(m) { $("toast").textContent = m; errUntil = Date.now() + 8000; }
$("toast").addEventListener("click", () => { $("toast").textContent = ""; errUntil = 0; });

async function api(path, params, isJson = false) {
  let opt = {};
  const headers = {};
  if (params) {
    if (isJson) {
      headers["Content-Type"] = "application/json";
      opt = {method: "POST", headers: headers, body: JSON.stringify(params)};
    } else {
      opt = {method: "POST", headers: headers, body: new URLSearchParams(params)};
    }
  } else {
    opt = {headers: headers};
  }
  const auth = sessionStorage.getItem("sh_admin_auth");
  if (auth) {
    headers["Authorization"] = auth;
  }
  const r = await fetch(path, opt);
  if (!r.ok) {
    let msg = path + " HTTP " + r.status;
    try { const j = await r.json(); if (j.message) msg = j.message; else if (j.error) msg = j.error; } catch (e) {}
    throw new Error(msg);
  }
  return r.json();
}

function goToAdminCreds() {
  if (!isAdmin()) {
    openAdminAuthModal(() => {
      const grp = $("grpSetup");
      if (grp) grp.open = true;
      const passInp = $("cfgAdminPass");
      if (passInp) {
        passInp.scrollIntoView({ behavior: "smooth", block: "center" });
        passInp.focus();
      }
    });
    return;
  }
  const grp = $("grpSetup");
  if (grp) grp.open = true;
  const passInp = $("cfgAdminPass");
  if (passInp) {
    passInp.scrollIntoView({ behavior: "smooth", block: "center" });
    passInp.focus();
  }
}

let inFlight = false;
async function refresh() {
  // One refresh at a time, endpoints one after another: the board serves one client request per loop pass
  if (inFlight) { again = true; return; }  // a change made meanwhile (add / delete a zone) is shown by one more pass
  inFlight = true;
  try {
    const admin = isAdmin();
    const debugOpen = admin && ($("grpDebug") && $("grpDebug").open);
    const eps = debugOpen
      ? DOC.endpoints
      : (admin ? ["api/zones", "api/ble/sensors", "api/boiler/status", "api/diag/status", "api/schedule"]
               : ["api/zones", "api/ble/sensors", "api/boiler/status", "api/diag/status"]);
    for (const ep of eps) RAW[ep] = ep === "api/setup/discovered-sensors" ? await api("api/setup/discovered-sensors") : await api(ep);
    Z = RAW["api/zones"]; B = RAW["api/ble/sensors"].sensors; ST = RAW["api/boiler/status"]; D = RAW["api/diag/status"];
    S = RAW["api/schedule"] || S;
    $("devBanner").style.display = ST && ST.profile === "dev-open" ? "inline-block" : "none";
    if (ST && $("fwInfo")) {
      $("fwInfo").textContent = `· FW v${ST.firmwareVersion || "1.11.05.24"} (${(ST.gitCommit || "").split("-")[0]}) · UI v1.11.07.01 (LittleFS)`;
    }
    updateAdminBtn();
    const now = Date.now();
    for (const zid in pendingSetpoints) {
      if (!pendingSetpoints[zid].timer && now > pendingSetpoints[zid].holdUntil) {
        delete pendingSetpoints[zid];
      }
    }
    renderHeader(); renderRooms(); renderAlerts();
    if (admin) {
      renderChecks(); renderSetup(); renderSensors();
      if (!isSchedBusy()) renderSchedule();
      if ($("grpHistory") && $("grpHistory").open) {
        renderTpiInspector();
        renderGasMeter();
        if (!histData24h || (Date.now() - lastHistFetch > 60000)) loadHistoryChart();
      }
      if (debugOpen) { renderPins(); renderMem(); updateWatch(); }
      if (!busy("loc") && RAW["api/setup/location"]) $("loc").value = RAW["api/setup/location"].coords;
    }
    if (Date.now() >= errUntil) $("toast").textContent = "";
  } catch (e) { showErr("Conexiune pierdută (" + e.message + ")"); }
  inFlight = false;
  if (again) { again = false; refresh(); }
}

let timer = null;
function startTimer() { clearInterval(timer); timer = setInterval(() => { if (!document.hidden) refresh(); }, isAdmin() ? 3000 : 5000); }

let pendingAdminCallback = null;

function updateAdminBtn() {
  const btn = $("toggleAdmin");
  if (btn) {
    if (isAdmin()) {
      btn.textContent = "✕ Ieși Admin";
      btn.style.borderColor = "var(--line)";
      btn.style.color = "var(--mut)";
      btn.title = "Ieși din panoul de administrare";
    } else {
      btn.textContent = "⚙️ Admin";
      btn.style.borderColor = "var(--line)";
      btn.style.color = "var(--fg)";
      btn.title = "Deschide panoul de administrare";
    }
  }
}

function openAdminAuthModal(cb) {
  pendingAdminCallback = cb || null;
  const modal = $("adminAuthModal");
  if (modal) {
    modal.style.display = "flex";
    $("adminAuthErr").textContent = "";
    $("adminAuthPass").value = "";
    setTimeout(() => $("adminAuthPass").focus(), 50);
  }
}

function closeAdminAuthModal() {
  const modal = $("adminAuthModal");
  if (modal) modal.style.display = "none";
  pendingAdminCallback = null;
}

async function submitAdminAuth() {
  const pass = ($("adminAuthPass").value || "").trim();
  const errBox = $("adminAuthErr");
  if (!pass) {
    errBox.textContent = "Introduceți parola administrator.";
    return;
  }
  errBox.textContent = "Se verifică...";
  errBox.style.color = "var(--mut)";

  const userCandidates = ["bogdan", "admin"];
  let ok = false;
  let authHeader = "";

  for (const u of userCandidates) {
    try {
      const candidateAuth = "Basic " + btoa(u + ":" + pass);
      const r = await fetch("api/auth/verify", {
        method: "POST",
        headers: { "Authorization": candidateAuth }
      });
      if (r.ok) {
        ok = true;
        authHeader = candidateAuth;
        sessionStorage.setItem("sh_admin_user", u);
        break;
      }
    } catch (_) {}
  }

  if (!ok) {
    for (const u of userCandidates) {
      try {
        const candidateAuth = "Basic " + btoa(u + ":" + pass);
        const r = await fetch("api/boiler/off", {
          method: "POST",
          headers: { "Authorization": candidateAuth }
        });
        if (r.status !== 401) {
          ok = true;
          authHeader = candidateAuth;
          sessionStorage.setItem("sh_admin_user", u);
          break;
        }
      } catch (_) {}
    }
  }

  if (ok) {
    sessionStorage.setItem("sh_admin_auth", authHeader);
    sessionStorage.setItem("sh_admin_pass", pass);
    document.body.classList.add("admin");
    location.hash = "admin";
    updateAdminBtn();
    closeAdminAuthModal();
    startTimer();
    refresh();
    if (pendingAdminCallback) {
      const cb = pendingAdminCallback;
      pendingAdminCallback = null;
      cb();
    }
  } else {
    errBox.textContent = "Parolă incorectă!";
    errBox.style.color = "var(--bad)";
    $("adminAuthPass").focus();
  }
}

$("toggleAdmin").onclick = e => {
  e.preventDefault();
  if (isAdmin()) {
    sessionStorage.removeItem("sh_admin_auth");
    sessionStorage.removeItem("sh_admin_pass");
    sessionStorage.removeItem("sh_admin_user");
    document.body.classList.remove("admin");
    location.hash = "";
    updateAdminBtn();
    startTimer();
    refresh();
  } else {
    const cachedAuth = sessionStorage.getItem("sh_admin_auth");
    if (cachedAuth) {
      document.body.classList.add("admin");
      location.hash = "admin";
      updateAdminBtn();
      startTimer();
      refresh();
    } else {
      openAdminAuthModal();
    }
  }
};

const savedAuth = sessionStorage.getItem("sh_admin_auth");
if (savedAuth && location.hash === "#admin") {
  document.body.classList.add("admin");
} else {
  if (location.hash === "#admin") location.hash = "";
}
updateAdminBtn();

