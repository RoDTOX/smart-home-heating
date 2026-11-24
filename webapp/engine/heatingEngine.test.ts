import test from 'node:test';
import assert from 'node:assert/strict';
import {
  applyConfigImport,
  buildConfigExport,
  computeHeatUpEta,
  computePreHeatLeadMinutes,
  computeTpiDuty,
  createInitialSystemState,
  evaluateFrostProtection,
  executeConsoleCommand,
  recordGasMeterReading,
  setZoneTarget,
  tickSystemState,
  toggleAwayMode,
  validateNumericParam,
} from './heatingEngine';

test('TPI deficit rule (FR-47) demands 100% duty when error >= 0.4 C', () => {
  const res = computeTpiDuty(22.0, 22.5, 5.0, true, 12, 0.3);
  assert.equal(res.duty, 1.0);
  assert.equal(res.onMinutesPerHour, 60);
  assert.equal(res.demand, true);
});

test('TPI overshoot cut shuts off heating above target + hysteresis', () => {
  const res = computeTpiDuty(23.4, 23.0, 5.0, true, 15, 0.3);
  assert.equal(res.duty, 0);
  assert.equal(res.demand, false);
});

test('Frost protection (FR-16.1) activates below frostTemp and requires +1.5 C hysteresis to release', () => {
  assert.equal(evaluateFrostProtection(false, true, 11.9, 12.0), true);
  assert.equal(evaluateFrostProtection(true, true, 12.8, 12.0), true);
  assert.equal(evaluateFrostProtection(true, true, 13.5, 12.0), false);
});

test('HeatUpModel pre-heat lead time matches FR-48 bounds and weather-lost fallback', () => {
  assert.equal(computePreHeatLeadMinutes(5.0, true), 120);
  assert.equal(computePreHeatLeadMinutes(-5.0, true), 210);
  assert.equal(computePreHeatLeadMinutes(-20.0, true), 240);
  assert.equal(computePreHeatLeadMinutes(20.0, true), 45);
  assert.equal(computePreHeatLeadMinutes(0.0, false), 90);
});

test('HeatUpModel ETA returns La temperatură when at target', () => {
  const eta = computeHeatUpEta(
    { currentTemp: 23.1, targetTemp: 23.0, heatRateDegPerHour: 0.6, heatRateLearned: true },
    5.0,
  );
  assert.equal(eta.needed, false);
  assert.equal(eta.label, 'La temperatură');
});

test('ParamStore §6 range validation rejects out-of-range safety parameters', () => {
  assert.equal(validateNumericParam('interlockDelay', 180).ok, true);
  assert.equal(validateNumericParam('interlockDelay', 60).ok, false);
  assert.equal(validateNumericParam('overrun', 120).ok, false);
  assert.equal(validateNumericParam('frostTemp', 7.5).ok, false);
  assert.equal(validateNumericParam('frostTemp', 12.0).ok, true);
  assert.equal(validateNumericParam('windowThreshold', 2.5).ok, false);
});

test('Away mode lowers all zone targets and can be reversed', () => {
  const s0 = createInitialSystemState();
  const sAway = toggleAwayMode(s0, true);
  assert.equal(sAway.schedule.away, true);
  assert.ok(sAway.zones.every((z) => z.targetTemp === 16.0));

  const sBack = toggleAwayMode(sAway, false);
  assert.equal(sBack.schedule.away, false);
  assert.equal(sBack.zones[0].targetTemp, 23.0);
});

test('SafetySupervisor holds overrun valve open after boiler turns OFF', () => {
  let s = createInitialSystemState();
  // Boiler is currently ON with zone 3 open and burnSec = 420 (>= burnerMinRun 180)
  // Drop zone 3 target well below current temp so demand disappears
  s = setZoneTarget(s, 3, 20.0);
  s = tickSystemState(s, 5);
  assert.equal(s.boiler.on, false);
  assert.ok(s.boiler.overrunRemainingSec > 0);
  // Zone 3 valve must stay open during overrun heat dissipation (FR-3)
  assert.equal(s.zones[2].valveOpen, true);
});

test('Gas meter reading validates range and updates estimated index', () => {
  const s0 = createInitialSystemState();
  const bad = recordGasMeterReading(s0, -5);
  assert.equal(bad.ok, false);

  const ok = recordGasMeterReading(s0, 4820.125);
  assert.equal(ok.ok, true);
  assert.equal(ok.state.gas.lastReadingIndex, 4820.125);
  assert.equal(ok.state.gas.estimatedIndex, 4820.125);
  assert.equal(ok.state.gas.readings.length, s0.gas.readings.length + 1);
});

test('Config export/import round-trip applies valid fields and skips unknown/invalid', () => {
  const s0 = createInitialSystemState();
  const exported = buildConfigExport(s0);
  const modified = {
    ...exported,
    params: {
      ...exported.params,
      actuatorDelaySec: 240,
      freezeProtectionTemp: 4.0, // invalid (< 8.0)
      mysteryKey: 42, // unknown
    },
  };
  const res = applyConfigImport(s0, JSON.stringify(modified));
  assert.equal(res.ok, true);
  assert.equal(res.state.params.interlockDelay, 240);
  assert.equal(res.state.params.frostTemp, 12.0); // unchanged
  assert.ok(res.applied.includes('params.actuatorDelaySec'));
  assert.ok(res.skipped.some((x) => x.name === 'params.freezeProtectionTemp' && x.reason === 'invalid'));
  assert.ok(res.skipped.some((x) => x.name === 'params.mysteryKey' && x.reason === 'unknown'));
});

test('Virtual console CLI responds to HELP, STATUS, and AWAY commands', () => {
  const s0 = createInitialSystemState();
  const help = executeConsoleCommand(s0, 'HELP');
  assert.ok(help.lines.some((l) => l.includes('STATUS')));

  const away = executeConsoleCommand(s0, 'AWAY ON');
  assert.equal(away.state.schedule.away, true);
});
