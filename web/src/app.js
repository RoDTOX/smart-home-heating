// FR-8.1: the admin sections look and work like the cards of the panel - an icon tile, one section open at a time,
// and a row of buttons that jumps to a section
const ADM = [["grpSched", "cal", "Program"], ["grpDiag", "activity", "Diagnoză"], ["grpHistory", "bars", "Istoric"],
  ["grpSetup", "cog", "Configurare"], ["grpDebug", "term", "Inginerie"]];
$("icAdm").innerHTML = ic("cog");
$("icLock").innerHTML = ic("lock");
$("admNav").innerHTML = ADM.map(a => `<button type="button" data-g="${a[0]}">${a[2]}</button>`).join("");
function paintAdmNav() { $("admNav").querySelectorAll("button").forEach(b => b.setAttribute("aria-pressed", $(b.dataset.g).open)); }
ADM.forEach(a => {
  const d = $(a[0]);
  d.querySelector("summary").insertAdjacentHTML("afterbegin", `<span class="sico">${ic(a[1], "i20")}</span>`);
  d.addEventListener("toggle", () => {
    if (d.open) ADM.forEach(o => { if (o[0] !== a[0]) $(o[0]).open = false; });
    paintAdmNav();
  });
});
$("admNav").addEventListener("click", e => {
  const b = e.target.closest("button");
  if (!b) return;
  const d = $(b.dataset.g);
  d.open = true;
  d.scrollIntoView({behavior: "smooth", block: "start"});
  refresh();
});
paintAdmNav();
startTimer();
refresh();
