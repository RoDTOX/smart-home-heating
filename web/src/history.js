// FR-34: LittleFS Telemetry Timeline, Temperature Curves & TPI Algorithm Inspector

function buildRunBlocks(ordered, pred) {
  const blocks = [];
  let start = -1, end = -1;
  for (let k = 0; k < ordered.length; k++) {
    const item = ordered[k];
    if (pred(item)) {
      if (start === -1) {
        start = item.slotsAgo;
        end = item.slotsAgo;
      } else if (end - item.slotsAgo === 1) {
        end = item.slotsAgo;
      } else {
        blocks.push({start, end});
        start = item.slotsAgo;
        end = item.slotsAgo;
      }
    } else if (start !== -1) {
      blocks.push({start, end});
      start = -1;
      end = -1;
    }
  }
  if (start !== -1) blocks.push({start, end});
  return blocks;
}

function buildCurveSegments(pointsList, maxGapSlots, maxBridgeGap) {
  const segments = [];
  let curSeg = [];
  pointsList.forEach(pt => {
    if (curSeg.length > 0 && curSeg[curSeg.length - 1].slotsAgo - pt.slotsAgo > maxGapSlots) {
      segments.push(curSeg);
      curSeg = [];
    }
    curSeg.push(pt);
  });
  if (curSeg.length > 0) segments.push(curSeg);

  const bridges = [];
  for (let si = 0; si < segments.length - 1; si++) {
    const ptA = segments[si][segments[si].length - 1];
    const ptB = segments[si + 1][0];
    if (Math.abs(ptA.slotsAgo - ptB.slotsAgo) <= maxBridgeGap) bridges.push([ptA, ptB]);
  }
  return {segments, bridges};
}

function prepareHistoryChartData(range, data, zonesData, nowMs = Date.now()) {
  if (!data || !data.points || !data.points.length) {
    return {ready: false, emptyHtml: `<div class="s" style="padding:20px;text-align:center">Nicio înregistrare disponibilă pentru intervalul selectat (${esc(range)}).</div>`};
  }

  const isMultiDay = (range === "3d" || range === "5d");
  const cur = (data.currentSlot !== undefined) ? data.currentSlot : 0;
  const slotCapacity = isMultiDay ? 240 : 288;
  const totalWindowSlots = (range === "1h") ? 12
    : (range === "12h") ? 144
    : (range === "24h") ? 288
    : (range === "3d") ? 144 : 240;

  const slotMap = {};
  (data.points || []).forEach(p => { slotMap[p[0]] = p; });

  const ordered = [];
  for (let i = 1; i <= slotCapacity; i++) {
    const s = (cur + i) % slotCapacity;
    if (slotMap[s]) {
      const slotsAgo = slotCapacity - i;
      if (slotsAgo < totalWindowSlots) ordered.push({p: slotMap[s], slotsAgo});
    }
  }

  if (!ordered.length) {
    return {ready: false, emptyHtml: `<div class="s" style="padding:20px;text-align:center">Date insuficiente pentru ${esc(range)}.</div>`};
  }

  const zones = (zonesData && zonesData.zones && zonesData.zones.length) ? zonesData.zones : [
    {id: 1, name: "Zona 1"}, {id: 2, name: "Zona 2"}, {id: 3, name: "Zona 3"}
  ];
  const zoneColors = ["#38bdf8", "#a78bfa", "#f472b6", "#34d399"];
  const rowDefs = [
    {label: "⚡ Evenimente", isEvent: true, color: "#f59e0b"},
    {label: "🔥 Cazan ON", bit: 1, color: "var(--heat)"}
  ];
  zones.forEach((z, idx) => {
    rowDefs.push({
      label: esc(z.name || ("Zona " + (idx + 1))),
      bit: 1 << (idx + 2),
      color: zoneColors[idx % zoneColors.length],
      zoneIdx: idx
    });
  });

  const W = 880, x0 = 100, x1 = 835, chartW = x1 - x0;
  const slotW = chartW / totalWindowSlots;

  const validTemps = [], validOutTemps = [];
  ordered.forEach(({p}) => {
    for (let k = 1; k <= 3; k++) {
      if (p[k] !== null && p[k] !== undefined && p[k] > 5 && p[k] < 35) validTemps.push(p[k]);
    }
    if (p[4] !== null && p[4] !== undefined && p[4] > -40 && p[4] < 55) validOutTemps.push(p[4]);
  });

  let minT = validTemps.length ? Math.floor(Math.min(...validTemps)) - 1 : 18;
  let maxT = validTemps.length ? Math.ceil(Math.max(...validTemps)) + 1 : 26;
  if (maxT - minT < 4) maxT = minT + 4;

  let minOutT = validOutTemps.length ? Math.floor(Math.min(...validOutTemps)) - 1 : 0;
  let maxOutT = validOutTemps.length ? Math.ceil(Math.max(...validOutTemps)) + 1 : 15;
  if (maxOutT - minOutT < 4) maxOutT = minOutT + 4;

  const topActY = 20, actRowH = 14, actGap = 5;
  const tempHeaderY = topActY + rowDefs.length * (actRowH + actGap) + 18;
  const tempTop = tempHeaderY + 12, tempH = 115, tempBot = tempTop + tempH, totalH = tempBot + 35;
  const tempToY = t => tempBot - ((t - minT) / (maxT - minT)) * tempH;
  const outTempToY = t => tempBot - ((t - minOutT) / (maxOutT - minOutT)) * tempH;

  const timeStepMarks = (range === "5d") ? 5 : 6;
  const totalWindowHours = (range === "1h") ? 1.0 : (range === "12h") ? 12 : (range === "24h") ? 24 : (range === "3d") ? 72 : 120;
  const todayDate = new Date(nowMs).getDate();
  const timeMarks = [];
  for (let m = 0; m <= timeStepMarks; m++) {
    const frac = m / timeStepMarks;
    const gx = x0 + frac * chartW;
    const markDate = new Date(nowMs - (1 - frac) * totalWindowHours * 3600 * 1000);
    const hh = String(markDate.getHours()).padStart(2, "0");
    const mm = String(markDate.getMinutes()).padStart(2, "0");
    let label = `${hh}:${mm}`;
    if (m === timeStepMarks) label = `Acum (${hh}:${mm})`;
    else if (isMultiDay) label = `${DAYS_SHORT[markDate.getDay()]} ${hh}:${mm}`;
    else if (range !== "1h" && markDate.getDate() !== todayDate) label = `Ieri ${hh}:${mm}`;
    timeMarks.push({gx, label});
  }

  const curEpoch = (data.currentEpoch && data.currentEpoch > 1700000000) ? data.currentEpoch : Math.floor(nowMs / 1000);
  const visibleEvents = [];
  (data.reboots || []).forEach(rb => {
    const slotSec = isMultiDay ? 1800 : 300;
    const rSlot = isMultiDay ? rb.slot5d : rb.slot24h;
    const slotsAgo = (cur >= rSlot) ? (cur - rSlot) : (cur + slotCapacity - rSlot);
    const tEnd = rb.epoch ? rb.epoch : (curEpoch - slotsAgo * slotSec);
    const tStart = (rb.offEpoch && rb.offEpoch > 1700000000 && rb.offEpoch < tEnd) ? rb.offEpoch : (tEnd - 120);
    const durSec = Math.max(10, tEnd - tStart);
    const durText = (durSec < 90) ? `${durSec}s` : (durSec < 3600 ? `~${Math.round(durSec / 60)}m` : `~${(durSec / 3600).toFixed(1)}h`);
    const dStart = new Date(tStart * 1000), dEnd = new Date(tEnd * 1000);
    const startStr = `${String(dStart.getHours()).padStart(2, "0")}:${String(dStart.getMinutes()).padStart(2, "0")}`;
    const endStr = `${String(dEnd.getHours()).padStart(2, "0")}:${String(dEnd.getMinutes()).padStart(2, "0")}`;
    const hoursAgoEnd = Math.max(0, (nowMs / 1000 - tEnd) / 3600);
    const hoursAgoStart = Math.max(0, (nowMs / 1000 - tStart) / 3600);
    if (hoursAgoEnd <= totalWindowHours) {
      const fracEnd = Math.max(0, Math.min(1, 1 - (hoursAgoEnd / totalWindowHours)));
      const fracStart = Math.max(0, Math.min(1, 1 - (hoursAgoStart / totalWindowHours)));
      const xEnd = x0 + fracEnd * chartW, xStart = x0 + fracStart * chartW;
      const minW = Math.max(10, slotW * 0.45);
      const bx = Math.max(x0, Math.min(x1 - minW, Math.min(xStart, xEnd)));
      const bw = Math.max(minW, Math.min(x1 - bx, Math.max(Math.abs(xEnd - xStart), minW)));
      const rawReason = rb.reason || "Restart sistem";
      let shortReason = "Restart";
      if (/firmware|software/i.test(rawReason)) shortReason = "Restart FW";
      else if (/power|curent|alimentare/i.test(rawReason)) shortReason = "Pană curent";
      else if (/watchdog|wdt/i.test(rawReason)) shortReason = "Watchdog";
      else if (/panic|crash/i.test(rawReason)) shortReason = "Crash";
      else if (/brownout/i.test(rawReason)) shortReason = "Brownout";
      visibleEvents.push({
        bx, bw, rx: (xStart + xEnd) / 2, tStart, tEnd, rb,
        reason: rawReason, shortReason,
        intervalLabel: `${startStr} → ${endStr} (${durText})`,
        startStr, endStr, durText
      });
    }
  });

  const tracks = rowDefs.map((r, ri) => {
    const ry = topActY + ri * (actRowH + actGap);
    if (r.isEvent) return {r, ry, blocks: [], winBlocks: []};
    const blocks = buildRunBlocks(ordered, item => (item.p[5] & r.bit) !== 0);
    const winBlocks = (r.zoneIdx !== undefined)
      ? buildRunBlocks(ordered, item => (((item.p[5] >> 5) & 0x03) === (r.zoneIdx + 1)))
      : [];
    return {r, ry, blocks, winBlocks};
  });

  const maxGapSlots = (range === "1h") ? 2 : (isMultiDay ? 3 : 4);
  const outPoints = [];
  ordered.forEach(item => {
    const v = item.p[4];
    if (v !== null && v !== undefined && v > -40 && v < 55) {
      outPoints.push({px: x1 - item.slotsAgo * slotW, py: outTempToY(v), slotsAgo: item.slotsAgo});
    }
  });
  const outCurve = buildCurveSegments(outPoints, maxGapSlots, maxGapSlots);

  const zoneCurves = zones.map((z, idx) => {
    const k = idx + 1, color = zoneColors[idx % zoneColors.length], pts = [];
    ordered.forEach(item => {
      const v = item.p[k];
      if (v !== null && v !== undefined && v > 0) {
        pts.push({px: x1 - item.slotsAgo * slotW, py: tempToY(v), slotsAgo: item.slotsAgo});
      }
    });
    return {color, ...buildCurveSegments(pts, maxGapSlots, maxGapSlots)};
  });

  return {
    ready: true, range, isMultiDay, ordered, latestItem: ordered[ordered.length - 1],
    zones, zoneColors, visibleEvents, tracks, timeMarks,
    W, totalH, x0, x1, chartW, slotW, totalWindowSlots,
    topActY, actRowH, tempHeaderY, tempTop, tempH, tempBot,
    minT, maxT, tStep: (maxT - minT > 8) ? 3 : 2,
    minOutT, maxOutT, outStep: (maxOutT - minOutT > 10) ? 4 : ((maxOutT - minOutT > 6) ? 3 : 2),
    tempToY, outTempToY, outCurve, zoneCurves
  };
}

function renderHistoryInfoBar(item, timeStr, isCurrent, nearReboot, zones, zoneColors) {
  const bar = $("histInfoBar");
  if (!bar) return;

  let row1 = `<div style="display:flex;align-items:center;justify-content:space-between;gap:8px;height:24px;overflow-x:auto">` +
    `<div style="display:flex;align-items:center;gap:8px;flex-shrink:0">` +
    `<div style="display:flex;align-items:center;gap:5px;font-weight:600;color:var(--fg);white-space:nowrap"><span>${isCurrent ? '⏱️' : '📅'}</span> <span>${esc(timeStr)}</span></div>`;

  if (item) {
    const p = item.p, bOn = (p[5] & 1) !== 0;
    row1 += `<span style="padding:2px 8px;border-radius:4px;font-weight:600;font-size:11px;white-space:nowrap;background:${bOn ? 'rgba(239,68,68,0.15);color:#ef4444;border:1px solid rgba(239,68,68,0.4)' : 'rgba(100,116,139,0.15);color:var(--mut);border:1px solid var(--line)'}">${bOn ? '🔥 Cazan PORNIT' : '⚪ Cazan Oprit'}</span>`;
    if (p[4] !== null && p[4] !== undefined) {
      row1 += `<span style="padding:2px 8px;border-radius:4px;font-size:11px;background:rgba(148,163,184,0.1);border:1px solid #64748b;color:#94a3b8;white-space:nowrap">🌡️ Afară: ${p[4].toFixed(1)}°C</span>`;
    }
  } else {
    row1 += `<span class="s" style="color:var(--mut);white-space:nowrap">Fără date temperatură în acest interval</span>`;
  }
  row1 += `</div><div style="display:flex;align-items:center;gap:6px;flex-shrink:0">`;
  if (nearReboot) {
    const rawReason = String(nearReboot.reason || ''), shortReason = nearReboot.shortReason || 'Restart';
    const labelText = nearReboot.startStr ? `${shortReason}: ${nearReboot.startStr} → ${nearReboot.endStr} (${nearReboot.durText})` : shortReason;
    row1 += `<span style="padding:2px 8px;border-radius:4px;font-weight:600;font-size:11px;background:rgba(245,158,11,0.15);border:1px solid #f59e0b;color:#f59e0b;white-space:nowrap" title="${esc(rawReason)} · ${esc(nearReboot.intervalLabel || '')}">⚡ ${esc(labelText)}</span>`;
  }
  row1 += `</div></div>`;

  let row2 = `<div style="display:flex;align-items:center;gap:8px;height:24px;overflow-x:auto">`;
  zones.forEach((z, idx) => {
    const color = zoneColors[idx % zoneColors.length];
    if (item) {
      const p = item.p, vOn = (p[5] & (1 << (idx + 2))) !== 0;
      const winActive = (((p[5] >> 5) & 0x03) === (idx + 1));
      const temp = (p[idx + 1] !== null && p[idx + 1] !== undefined) ? p[idx + 1].toFixed(1) + '°C' : '--';
      const stateLabel = winActive ? (vOn ? '🪟 geam deschis' : '🪟 pauză geam') : (vOn ? 'vană deschisă' : 'închis');
      const stateColor = winActive ? color : (vOn ? 'var(--ok)' : 'var(--mut)');
      row2 += `<span style="padding:2px 8px;border-radius:4px;font-size:11px;background:rgba(255,255,255,0.03);border:1px solid var(--line);color:var(--fg);white-space:nowrap"><b style="color:${color}">${esc(z.name)}</b>: ${temp} <span style="color:${stateColor};font-weight:500">(${stateLabel})</span></span>`;
    } else {
      row2 += `<span style="padding:2px 8px;border-radius:4px;font-size:11px;background:rgba(255,255,255,0.03);border:1px solid var(--line);color:var(--mut);white-space:nowrap"><b style="color:${color}">${esc(z.name)}</b>: --</span>`;
    }
  });
  bar.innerHTML = row1 + row2 + `</div>`;
}

function drawCurveSvg(svgParts, curve, color, isDashed, dotRadius) {
  curve.bridges.forEach(([ptA, ptB]) => {
    svgParts.push(`<line x1="${ptA.px.toFixed(1)}" y1="${ptA.py.toFixed(1)}" x2="${ptB.px.toFixed(1)}" y2="${ptB.py.toFixed(1)}" stroke="${color}" stroke-width="1" stroke-dasharray="2,3" opacity="0.3" />`);
  });
  curve.segments.forEach(seg => {
    if (seg.length > 1) {
      const ptsStr = seg.map(p => `${p.px.toFixed(1)},${p.py.toFixed(1)}`).join(" ");
      const dashAttr = isDashed ? ' stroke-dasharray="3,3"' : '';
      svgParts.push(`<polyline fill="none" stroke="${color}" stroke-width="1.3" stroke-linejoin="round" stroke-linecap="round"${dashAttr} points="${ptsStr}" />`);
    }
    seg.forEach(p => {
      svgParts.push(`<circle cx="${p.px.toFixed(1)}" cy="${p.py.toFixed(1)}" r="${dotRadius}" fill="${color}" />`);
    });
  });
}

function drawHistoryChart(container, m) {
  if (!m.ready) {
    container.innerHTML = m.emptyHtml;
    return;
  }

  const svgParts = [`<svg id="histSvg" viewBox="0 0 ${m.W} ${m.totalH}" style="width:100%;height:auto;display:block;font-family:system-ui,-apple-system,sans-serif">`];
  m.timeMarks.forEach(tm => {
    svgParts.push(`<line x1="${tm.gx.toFixed(1)}" y1="${m.topActY}" x2="${tm.gx.toFixed(1)}" y2="${m.tempBot}" stroke="var(--line)" stroke-width="1" stroke-dasharray="2,2" />`);
    svgParts.push(`<text x="${tm.gx.toFixed(1)}" y="${m.totalH - 12}" font-size="11" fill="var(--mut)" text-anchor="middle">${tm.label}</text>`);
  });

  m.tracks.forEach(({r, ry, blocks, winBlocks}) => {
    svgParts.push(`<text x="${m.x0 - 8}" y="${(ry + 11).toFixed(1)}" font-size="11" font-weight="600" fill="var(--fg)" text-anchor="end">${r.label}</text>`);
    svgParts.push(`<rect x="${m.x0}" y="${ry}" width="${m.chartW}" height="${m.actRowH}" rx="3" fill="var(--bg)" stroke="var(--line)" />`);
    if (r.isEvent) {
      m.visibleEvents.forEach(ve => {
        svgParts.push(`<rect x="${ve.bx.toFixed(1)}" y="${ry}" width="${ve.bw.toFixed(1)}" height="${m.actRowH}" rx="3" fill="rgba(245,158,11,0.12)" stroke="#f59e0b" stroke-width="1.2" />`);
        if (ve.bw >= 68) {
          svgParts.push(`<text x="${(ve.bx + 8).toFixed(1)}" y="${(ry + 10.5).toFixed(1)}" font-size="9" fill="#f59e0b" font-weight="bold">⚡</text>`);
          svgParts.push(`<text x="${(ve.bx + 20).toFixed(1)}" y="${(ry + 10.5).toFixed(1)}" font-size="8.5" fill="#f59e0b" font-weight="600">${esc(ve.shortReason)} (${esc(ve.durText)})</text>`);
        } else if (ve.bw >= 38) {
          svgParts.push(`<text x="${(ve.bx + 6).toFixed(1)}" y="${(ry + 10.5).toFixed(1)}" font-size="9" fill="#f59e0b" font-weight="bold">⚡</text>`);
          svgParts.push(`<text x="${(ve.bx + 17).toFixed(1)}" y="${(ry + 10.5).toFixed(1)}" font-size="8.5" fill="#f59e0b" font-weight="600">${esc(ve.shortReason)}</text>`);
        } else {
          svgParts.push(`<text x="${(ve.bx + ve.bw / 2).toFixed(1)}" y="${(ry + 10.5).toFixed(1)}" font-size="9" text-anchor="middle" fill="#f59e0b" font-weight="bold">⚡</text>`);
        }
      });
      return;
    }
    blocks.forEach(b => {
      const segX = Math.max(m.x0, m.x1 - (b.start + 0.5) * m.slotW);
      const segW = Math.max(2, Math.min(m.x1, m.x1 - (b.end - 0.5) * m.slotW) - segX);
      const rx = Math.min(2, Math.max(1, Math.floor(segW / 2)));
      svgParts.push(`<rect x="${segX.toFixed(1)}" y="${ry}" width="${segW.toFixed(1)}" height="${m.actRowH}" rx="${rx}" fill="${r.color}" />`);
    });
    winBlocks.forEach(wb => {
      const segX = Math.max(m.x0, m.x1 - (wb.start + 0.5) * m.slotW);
      const segW = Math.max(3, Math.min(m.x1, m.x1 - (wb.end - 0.5) * m.slotW) - segX);
      svgParts.push(`<rect x="${segX.toFixed(1)}" y="${ry}" width="${segW.toFixed(1)}" height="${m.actRowH}" rx="2" fill="${r.color}33" stroke="${r.color}" stroke-width="1.2" />`);
      if (segW >= 14) svgParts.push(`<text x="${(segX + segW / 2).toFixed(1)}" y="${(ry + 10).toFixed(1)}" font-size="8.5" text-anchor="middle" fill="${r.color}">🪟</text>`);
    });
  });

  svgParts.push(`<text x="${m.x0}" y="${m.tempHeaderY - 2}" font-size="11" font-weight="600" fill="var(--fg)">Temperaturi Zone (°C)</text>`);
  const tempLegStr = m.zones.map((z, idx) => `<tspan fill="${m.zoneColors[idx % m.zoneColors.length]}">— ${esc(z.name)}</tspan>`).join("   ") +
    `   <tspan fill="#94a3b8">-- Afară (axa dr., °C)</tspan>`;
  svgParts.push(`<text x="${m.x1}" y="${m.tempHeaderY - 2}" font-size="11" text-anchor="end">${tempLegStr}</text>`);
  svgParts.push(`<rect x="${m.x0}" y="${m.tempTop}" width="${m.chartW}" height="${m.tempH}" rx="4" fill="var(--bg)" stroke="var(--line)" />`);

  for (let t = m.minT; t <= m.maxT; t += m.tStep) {
    const gy = m.tempToY(t);
    svgParts.push(`<line x1="${m.x0}" y1="${gy.toFixed(1)}" x2="${m.x1}" y2="${gy.toFixed(1)}" stroke="var(--line)" stroke-width="1" />`);
    svgParts.push(`<text x="${m.x0 - 8}" y="${(gy + 4).toFixed(1)}" font-size="10" fill="var(--mut)" text-anchor="end">${t}°</text>`);
  }
  for (let t = m.minOutT; t <= m.maxOutT; t += m.outStep) {
    const gy = m.outTempToY(t);
    svgParts.push(`<line x1="${(m.x1 - 3).toFixed(1)}" y1="${gy.toFixed(1)}" x2="${(m.x1 + 3).toFixed(1)}" y2="${gy.toFixed(1)}" stroke="#94a3b8" stroke-width="1" opacity="0.6" />`);
    svgParts.push(`<text x="${(m.x1 + 8).toFixed(1)}" y="${(gy + 3.5).toFixed(1)}" font-size="10" fill="#94a3b8" text-anchor="start">${t}°</text>`);
  }

  const dotRadius = m.range === "1h" ? 2.5 : 1.5;
  drawCurveSvg(svgParts, m.outCurve, "#94a3b8", true, dotRadius);
  m.zoneCurves.forEach(zc => drawCurveSvg(svgParts, zc, zc.color, false, dotRadius));

  svgParts.push(`<line id="histCursorLine" x1="0" y1="${m.topActY}" x2="0" y2="${m.tempBot}" stroke="#facc15" stroke-width="1.5" stroke-dasharray="3,2" style="display:none;pointer-events:none" />`);
  svgParts.push(`<rect id="histOverlay" x="${m.x0}" y="${m.topActY}" width="${m.chartW}" height="${m.tempBot - m.topActY}" fill="transparent" style="cursor:crosshair" /></svg>`);
  container.innerHTML = svgParts.join("");

  const overlay = $("histOverlay"), curLine = $("histCursorLine");
  const nowH = String(new Date().getHours()).padStart(2, "0"), nowM = String(new Date().getMinutes()).padStart(2, "0");
  renderHistoryInfoBar(m.latestItem, `Acum (${nowH}:${nowM})`, true, null, m.zones, m.zoneColors);

  if (overlay) {
    overlay.onmousemove = e => {
      const rect = overlay.getBoundingClientRect();
      const frac = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
      const cursorSlotsAgo = (1 - frac) * m.totalWindowSlots;
      const xPos = m.x0 + frac * m.chartW;
      if (curLine) {
        curLine.setAttribute("x1", xPos.toFixed(1));
        curLine.setAttribute("x2", xPos.toFixed(1));
        curLine.style.display = "block";
      }
      const hoursAgo = m.isMultiDay ? (cursorSlotsAgo * 0.5) : (cursorSlotsAgo * (5 / 60));
      const curDate = new Date(Date.now() - hoursAgo * 3600 * 1000);
      const ch = String(curDate.getHours()).padStart(2, "0"), cm = String(curDate.getMinutes()).padStart(2, "0");
      const agoText = hoursAgo < 0.05 ? "" : (hoursAgo < 1 ? ` (-${Math.round(hoursAgo * 60)}m)` : ` (-${hoursAgo.toFixed(1)}h)`);
      const timeStr = `${DAYS_FULL[curDate.getDay()]} ${ch}:${cm}${agoText}`;

      let closest = null, minDiff = 99999;
      m.ordered.forEach(item => {
        const diff = Math.abs(item.slotsAgo - cursorSlotsAgo);
        if (diff < minDiff) { minDiff = diff; closest = item; }
      });
      let nearReboot = null;
      m.visibleEvents.forEach(ve => {
        if (xPos >= ve.bx - 4 && xPos <= ve.bx + ve.bw + 4) nearReboot = ve;
      });
      const maxTipDist = (m.range === "1h") ? 1.5 : (m.isMultiDay ? 3 : 5);
      renderHistoryInfoBar((closest && minDiff <= maxTipDist) ? closest : null, timeStr, false, nearReboot, m.zones, m.zoneColors);
    };
    overlay.onmouseleave = () => {
      if (curLine) curLine.style.display = "none";
      renderHistoryInfoBar(m.latestItem, `Acum (${nowH}:${nowM})`, true, null, m.zones, m.zoneColors);
    };
  }
}

function renderHistoryChart() {
  const container = $("histSvgContainer");
  if (!container) return;
  const isMultiDay = (selectedHistRange === "3d" || selectedHistRange === "5d");
  const data = isMultiDay ? histData5d : histData24h;
  const model = prepareHistoryChartData(selectedHistRange, data, Z);
  drawHistoryChart(container, model);
}

// FR-47: Live TPI Algorithm Inspector & Calculation Breakdown

function renderTpiInspector() {
  const box = $("tpiInspectorBox");
  if (!box || !Z || !Z.zones) return;

  const outT = (ST && ST.outdoorTemp !== undefined) ? ST.outdoorTemp : (RAW["api/boiler/status"] ? RAW["api/boiler/status"].outdoorTemp : null);
  let outCompPct = 0;
  if (outT !== null && outT < 15.0) {
    outCompPct = Math.min(30, Math.max(0, Math.round((15.0 - outT) * 1.5)));
  }

  let html = `<div style="font-weight:600;font-size:14px;margin-bottom:8px">🧠 3.2 Diagnoză Algoritm TPI – Calcul în timp real per zonă (FR-47)</div>
  <div class="scroll"><table style="font-size:12px;width:100%">
    <thead>
      <tr>
        <th>Zonă</th>
        <th>Curent / Țintă</th>
        <th>Deficit (P)</th>
        <th>Compensare Meteo (${outT !== null ? outT.toFixed(1) + '°C' : '--'})</th>
        <th>Inerție NVS (I)</th>
        <th>Duty Calculat</th>
        <th>Timp Ciclu (1h)</th>
        <th>Stare Vană</th>
      </tr>
    </thead>
    <tbody>`;

  Z.zones.forEach((z, i) => {
    const cur = (z.sensorValid && typeof z.currentTemp === "number") ? z.currentTemp.toFixed(1) + '°C' : '--';
    const tgt = (typeof z.effectiveTarget === "number") ? z.effectiveTarget.toFixed(1) + '°C' : '--';
    const err = z.sensorValid ? (z.effectiveTarget - z.currentTemp) : 0;
    const isDeficit = err >= 0.39;
    const pPct = isDeficit ? 100 : Math.max(0, Math.round((err / 1.5) * 100));
    const learned = Math.round(z.learnedDuty || 0);
    const duty = (z.dutyPct !== undefined) ? z.dutyPct : (z.duty <= 1.0 ? Math.round((z.duty || 0) * 100) : Math.round(z.duty || 0));
    const onMin = (z.onTimeSec !== undefined && z.onTimeSec > 0) ? Math.round(z.onTimeSec / 60) : Math.round(duty * 0.6);
    const pauseMin = Math.max(0, 60 - onMin);

    let pCol = isDeficit ? `<b style="color:var(--heat)">Deficit ≥0.4° (+100%)</b>` : `+${pPct}%`;
    let meteoCol = isDeficit ? `<span class="s">–</span>` : (outCompPct > 0 ? `+${outCompPct}%` : `0%`);
    let learnedCol = isDeficit ? `<span class="s">–</span>` : `+${learned}%`;
    let dutyCol = (duty >= 99) ? `<b style="color:var(--heat)">100% continuu</b>` : `<b>${duty}%</b>`;
    let timeCol = (duty >= 99) ? `60 min deschis` : `${onMin} min deschis / ${pauseMin} min pauză`;
    
    let stCol = z.relayEngaged
      ? `<span style="color:var(--ok);font-weight:600">● Vana DESCHISĂ</span>`
      : (z.demand ? `<span style="color:var(--heat)">⏳ Interlock deschidere</span>` : `<span class="s">○ Vana închisă (pauză)</span>`);

    html += `<tr>
      <td><b>${esc(z.name)}</b></td>
      <td>${cur} / ${tgt}</td>
      <td>${pCol}</td>
      <td>${meteoCol}</td>
      <td>${learnedCol}</td>
      <td>${dutyCol}</td>
      <td>${timeCol}</td>
      <td>${stCol}</td>
    </tr>`;
  });

  const wasDetailsOpen = box.querySelector("details")?.open;
  html += `</tbody></table></div>
  <details class="s" style="margin-top:8px;font-size:12px;line-height:1.4;color:var(--mut)" ${wasDetailsOpen ? 'open' : ''}>
    <summary style="cursor:pointer;font-weight:600">ℹ️ Cum funcționează algoritmul TPI la încălzirea în pardoseală (clic pentru detalii)</summary>
    <div style="margin-top:4px;padding:6px 10px;background:var(--bg);border-radius:6px;border:1px solid var(--line)">
      Ciclul de bază este de 60 min. Când diferența față de țintă este ≥ 0.4°C (deficit termic), vana stă deschisă 100% continuu pentru a încălzi șapa masivă de beton. Când ajunge la sub 0.4°C de țintă, TPI modulează fin: <code>Duty = Deficit / 1.5°C + Compensație Meteo + Inerție NVS</code>. Vana se deschide doar minutele calculate per oră, apoi intră în pauză permițând șapei să radieze căldura lin, fără oscilații sau supraîncălzire.
    </div>
  </details>`;

  box.innerHTML = html;
}

async function loadHistoryChart(force = false) {
  if (histLoading) return;
  histLoading = true;
  lastHistFetch = Date.now();
  const summaryEl = $("histSummary");
  if (summaryEl) summaryEl.textContent = "Se actualizează...";

  const isMultiDay = (selectedHistRange === "3d" || selectedHistRange === "5d");
  const ep = isMultiDay ? "api/history?range=5d" : "api/history?range=24h";

  try {
    const r = await fetch(ep);
    if (!r.ok) throw new Error("HTTP " + r.status);
    const j = await r.json();
    if (isMultiDay) histData5d = j;
    else histData24h = j;

    renderHistoryChart();
    renderTpiInspector();
    renderGasMeter();

    if (summaryEl) {
      const nowStr = (ST && ST.time) ? ST.time.slice(0, 5) : new Date().toLocaleTimeString().slice(0, 5);
      summaryEl.textContent = `Actualizat ${nowStr}`;
    }
  } catch(e) {
    if (summaryEl) summaryEl.textContent = "Eroare la încărcare: " + e.message;
  } finally {
    histLoading = false;
  }
}

const grpHist = $("grpHistory");
if (grpHist) {
  grpHist.addEventListener("toggle", () => {
    if (grpHist.open && (!histData24h || !histData5d)) loadHistoryChart();
  });
}
const btnRefHist = $("btnRefreshHist");
if (btnRefHist) {
  btnRefHist.onclick = () => loadHistoryChart(true);
}
const rangeTabs = $("histRangeTabs");
if (rangeTabs) {
  rangeTabs.addEventListener("click", e => {
    const btn = e.target.closest("button.hist-range-btn");
    if (!btn) return;
    rangeTabs.querySelectorAll(".hist-range-btn").forEach(b => b.classList.remove("on"));
    btn.classList.add("on");
    selectedHistRange = btn.dataset.range;
    renderHistoryChart();
    loadHistoryChart();
  });
}
