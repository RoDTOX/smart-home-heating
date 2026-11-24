import type {
  ConfigExportFormat1,
  DtcItem,
  GlobalHistoryPoint,
  HeatingSystemState,
  ParamSpec,
  ScheduleLevelName,
  ScheduleSlot,
  SystemParams,
  ZoneHistoryPoint,
  ZoneState,
  ZoneStateCode,
} from './types';

export const RELAY_PINS = [32, 33, 25, 26] as const;
export const TARGET_MIN_C = 5.0;
export const TARGET_MAX_C = 28.0;
export const FROST_HYSTERESIS_C = 1.5;

// Exact §6 defaults and limits ported from src/ParamStore.cpp
export const PARAM_SPECS: ParamSpec[] = [
  {
    key: 'interlockDelay',
    label: 'Întârziere deschidere vane (interlock)',
    unit: 's',
    min: 120,
    max: 600,
    step: 10,
    defaultValue: 180,
    safetyCritical: true,
    description: 'Timp minim de deschidere a vanelor înainte de pornirea centralei (FR-2).',
  },
  {
    key: 'overrun',
    label: 'Post-circulare (overrun)',
    unit: 's',
    min: 180,
    max: 600,
    step: 10,
    defaultValue: 180,
    safetyCritical: true,
    description: 'Ultima vană rămâne deschisă pentru disiparea căldurii reziduale (FR-3).',
  },
  {
    key: 'burnerMinRun',
    label: 'Timp minim ardere arzător',
    unit: 's',
    min: 120,
    max: 600,
    step: 10,
    defaultValue: 180,
    safetyCritical: true,
    description: 'Protecție anti-short-cycling: durată minimă a unui ciclu de ardere (FR-4).',
  },
  {
    key: 'burnerMinPause',
    label: 'Pauză minimă arzător',
    unit: 's',
    min: 120,
    max: 600,
    step: 10,
    defaultValue: 180,
    safetyCritical: true,
    description: 'Pauză minimă obligatorie între oprirea și repornirea centralei (FR-4).',
  },
  {
    key: 'frostTemp',
    label: 'Prag protecție anti-îngheț',
    unit: '°C',
    min: 8.0,
    max: 16.0,
    step: 0.5,
    defaultValue: 12.0,
    safetyCritical: true,
    description: 'Încălzire forțată în orice mod sub acest prag (+1.5 °C histerezis, FR-16.1).',
  },
  {
    key: 'sensorTimeout',
    label: 'Timeout senzor BLE',
    unit: 's',
    min: 60,
    max: 3600,
    step: 60,
    defaultValue: 1200,
    safetyCritical: true,
    description: 'Timp fără pachete BLE înainte de intrarea în regim fallback și DTC-101 (FR-51).',
  },
  {
    key: 'hysteresis',
    label: 'Histerezis global',
    unit: '°C',
    min: 0.1,
    max: 1.0,
    step: 0.1,
    defaultValue: 0.3,
    safetyCritical: false,
    description: 'Prag de decupare supratemperatură peste țintă în ciclul TPI.',
  },
  {
    key: 'windowThreshold',
    label: 'Prag detecție fereastră deschisă',
    unit: '°C',
    min: 0.8,
    max: 2.0,
    step: 0.1,
    defaultValue: 1.2,
    safetyCritical: false,
    description: 'Scădere bruscă de temperatură ce oprește temporar circuitul zonei (FR-40).',
  },
  {
    key: 'hbInterval',
    label: 'Interval heartbeat RF 433 MHz',
    unit: 's',
    min: 30,
    max: 900,
    step: 10,
    defaultValue: 60,
    safetyCritical: true,
    description: 'Retransmisie periodică a stării centralei către receptorul wireless.',
  },
  {
    key: 'gasPrice',
    label: 'Preț gaz natural',
    unit: 'Lei/m³',
    min: 1.0,
    max: 10.0,
    step: 0.05,
    defaultValue: 3.3,
    safetyCritical: false,
    description: 'Tarif utilizat pentru calculul costului zilnic și lunar.',
  },
  {
    key: 'gasFlameRate',
    label: 'Debit nominal arzător',
    unit: 'm³/h',
    min: 0.2,
    max: 2.5,
    step: 0.05,
    defaultValue: 0.6,
    safetyCritical: false,
    description: 'Debit de referință folosit înainte de calibrarea din citirile contorului.',
  },
];

export const DEFAULT_PARAMS: SystemParams = {
  interlockDelay: 180,
  overrun: 180,
  burnerMinRun: 180,
  burnerMinPause: 180,
  frostTemp: 12.0,
  sensorTimeout: 1200,
  hysteresis: 0.3,
  windowThreshold: 1.2,
  hbInterval: 60,
  boilerLinkHeartbeat: true,
  openLoopBypass: false,
  actuatorNormallyClosed: true,
  pumpControlEnabled: false,
  gasPrice: 3.3,
  gasFlameRate: 0.6,
};

export function clamp(val: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, val));
}

export function round1(val: number): number {
  return Math.round(val * 10) / 10;
}

export function round2(val: number): number {
  return Math.round(val * 100) / 100;
}

export function formatMmSs(totalSec: number): string {
  const s = Math.max(0, Math.floor(totalSec));
  const m = Math.floor(s / 60);
  const rem = s % 60;
  return `${String(m).padStart(2, '0')}:${String(rem).padStart(2, '0')}`;
}

export function formatDurationRo(totalSec: number): string {
  const mins = Math.floor(Math.max(0, totalSec) / 60);
  const h = Math.floor(mins / 60);
  const m = mins % 60;
  if (h === 0) return `${m} min`;
  return `${h} h ${String(m).padStart(2, '0')} min`;
}

// Ported from src/ZoneController.cpp
export function getOutdoorBand(outdoorTemp: number): number {
  if (outdoorTemp < -10) return 0;
  if (outdoorTemp < -5) return 1;
  if (outdoorTemp < 0) return 2;
  if (outdoorTemp < 5) return 3;
  if (outdoorTemp < 10) return 4;
  if (outdoorTemp < 15) return 5;
  return 6;
}

// Ported from src/ZoneController.cpp (FR-47)
export function computeTpiDuty(
  currentTemp: number,
  targetTemp: number,
  outdoorTemp: number,
  weatherValid: boolean,
  learnedDutyPct: number,
  hysteresis: number,
): { duty: number; onMinutesPerHour: number; demand: boolean } {
  const err = targetTemp - currentTemp;

  // Overshoot cut: stop if temp >= target + hysteresis
  if (currentTemp >= targetTemp + hysteresis) {
    return { duty: 0, onMinutesPerHour: 0, demand: false };
  }

  // Deficit rule (FR-47): 100% duty when error >= 0.4 C
  if (err >= 0.4) {
    return { duty: 1.0, onMinutesPerHour: 60, demand: true };
  }

  const outdoorComp =
    weatherValid && outdoorTemp < 15.0 ? clamp((15.0 - outdoorTemp) * 0.015, 0, 0.3) : 0;
  const baseDuty = clamp(learnedDutyPct / 100, 0, 0.6);
  const rawDuty = clamp(err / 1.5 + outdoorComp + baseDuty, 0, 1.0);

  let onMin = Math.round(rawDuty * 60);
  // Minimum valve pulse: 10 min (600s); if within 10 min of 60 min -> full hour
  if (onMin < 10) onMin = 0;
  else if (60 - onMin < 10) onMin = 60;

  const duty = onMin / 60;
  return {
    duty,
    onMinutesPerHour: onMin,
    demand: onMin > 0 && err > -hysteresis,
  };
}

// Ported from src/SafetySupervisor.cpp (FR-16.1)
export function evaluateFrostProtection(
  wasFrostActive: boolean,
  sensorValid: boolean,
  currentTemp: number,
  frostTemp: number,
): boolean {
  if (sensorValid && currentTemp < frostTemp) {
    return true;
  }
  if (wasFrostActive && (!sensorValid || currentTemp >= frostTemp + FROST_HYSTERESIS_C)) {
    return false;
  }
  return wasFrostActive;
}

// Ported from src/HeatUpModel.cpp (FR-48)
export function computePreHeatLeadMinutes(outdoorTemp: number, weatherValid: boolean): number {
  if (!weatherValid) return 90;
  const lead = Math.trunc(120 + (5.0 - outdoorTemp) * 9.0);
  return clamp(lead, 45, 240);
}

export function computeHeatUpEta(
  zone: Pick<ZoneState, 'currentTemp' | 'targetTemp' | 'heatRateDegPerHour' | 'heatRateLearned'>,
  outdoorTemp: number,
): { needed: boolean; minutes: number; label: string } {
  const diff = zone.targetTemp - zone.currentTemp;
  if (diff <= 0.05) {
    return { needed: false, minutes: 0, label: 'La temperatură' };
  }
  const effectiveRate = Math.max(
    0.25,
    zone.heatRateDegPerHour - Math.max(0, zone.currentTemp - outdoorTemp) * 0.008,
  );
  const minutes = Math.max(3, Math.round((diff / effectiveRate) * 60));
  if (!zone.heatRateLearned) {
    return { needed: true, minutes, label: `învățare (~${minutes} min)` };
  }
  return { needed: true, minutes, label: `~${minutes} min până la țintă` };
}

export function deriveZoneStateCode(zone: ZoneState): ZoneStateCode {
  if (zone.frostActive) return 'FROST';
  if (!zone.enabled) return 'DISABLED';
  if (!zone.sensorOnline) return 'FAULT';
  if (zone.windowOpen) return 'WINDOW';
  if (zone.valveOpen) return 'HEATING';
  return 'IDLE';
}

export function validateNumericParam(
  key: keyof SystemParams,
  value: number,
): { ok: boolean; error?: string } {
  const spec = PARAM_SPECS.find((p) => p.key === key);
  if (!spec) return { ok: true };
  if (Number.isNaN(value) || !Number.isFinite(value)) {
    return { ok: false, error: `${spec.label}: valoare numerică invalidă.` };
  }
  if (value < spec.min || value > spec.max) {
    return {
      ok: false,
      error: `${spec.label} trebuie să fie între ${spec.min} și ${spec.max} ${spec.unit} (cerințe §6).`,
    };
  }
  return { ok: true };
}

export function generateZoneHistory24h(
  currentTemp: number,
  targetTemp: number,
  phaseShift: number,
  currentlyHeating: boolean,
): ZoneHistoryPoint[] {
  const points: ZoneHistoryPoint[] = [];
  for (let h = 0; h < 24; h++) {
    const outdoor = round1(3.8 + 3.2 * Math.sin(((h - 8) / 24) * Math.PI * 2));
    const slotTarget = round1(targetTemp + (h < 6 || h >= 22 ? -1.0 : 0));
    const wave = Math.sin(((h + phaseShift) / 6) * Math.PI) * 0.25;
    const isLast = h === 23;
    const temp = isLast ? currentTemp : round1(slotTarget - 0.1 + wave);
    let valve = 0;
    if (isLast && currentlyHeating) {
      valve = 0.85;
    } else if (temp < slotTarget - 0.05 || (h >= 5 && h <= 8) || (h >= 18 && h <= 20)) {
      valve = round2(Math.min(1, 0.4 + Math.abs(slotTarget - temp) * 0.9));
    }
    points.push({
      hourLabel: `${String(h).padStart(2, '0')}:00`,
      temp,
      target: slotTarget,
      outdoor,
      valve,
    });
  }
  return points;
}

export function generateHistory5d(zones: ZoneState[]): GlobalHistoryPoint[] {
  const pts: GlobalHistoryPoint[] = [];
  const days = ['Mie', 'Joi', 'Vin', 'Sâm', 'Dum'];
  for (let i = 0; i < 40; i++) {
    const dIdx = Math.floor(i / 8);
    const h = (i % 8) * 3;
    const outdoor = round1(4.2 + 3.8 * Math.sin(((h - 8) / 24) * Math.PI * 2) + (dIdx - 2) * 0.4);
    const burn = h === 6 || h === 18 || h === 21;
    pts.push({
      label: `${days[dIdx]} ${String(h).padStart(2, '0')}h`,
      z1: round1((zones[0]?.currentTemp ?? 23.6) + (burn ? 0.3 : -0.2) + Math.sin(i * 0.4) * 0.15),
      z2: round1((zones[1]?.currentTemp ?? 23.5) + (burn ? 0.25 : -0.15) + Math.cos(i * 0.4) * 0.12),
      z3: round1((zones[2]?.currentTemp ?? 22.8) + (burn ? 0.35 : -0.2) + Math.sin(i * 0.4 + 1) * 0.14),
      outdoor,
      boilerOn: burn,
    });
  }
  return pts;
}

export function createInitialSystemState(): HeatingSystemState {
  const zones: ZoneState[] = [
    {
      id: 1,
      key: 'living',
      name: 'Living + Bucătărie',
      shortName: 'Living',
      relayLabel: 'CH1 · GPIO32',
      relayPin: 32,
      circuits: 3,
      sensorName: 'ATC_Living',
      sensorMac: 'A4:C1:38:4B:12:01',
      enabled: true,
      currentTemp: 23.6,
      humidity: 58,
      targetTemp: 23.0,
      scheduleTarget: 23.0,
      isBoost: false,
      tempOffset: 0.0,
      sensorOnline: true,
      sensorAgeSec: 18,
      windowOpen: false,
      windowPauseRemainingSec: 0,
      frostActive: false,
      valveOpen: false,
      valveEnergisedSec: 0,
      duty: 0,
      learnedDuty: 14.5,
      heatRateDegPerHour: 0.62,
      heatRateLearned: true,
      runtimeTodaySec: 1920,
      runtimeYesterdaySec: 6420,
      state: 'IDLE',
      history24h: generateZoneHistory24h(23.6, 23.0, 0, false),
    },
    {
      id: 2,
      key: 'kids',
      name: 'Dormitor Copii',
      shortName: 'Copii',
      relayLabel: 'CH2 · GPIO33',
      relayPin: 33,
      circuits: 2,
      sensorName: 'ATC_Copii',
      sensorMac: 'A4:C1:38:4B:12:02',
      enabled: true,
      currentTemp: 23.5,
      humidity: 61,
      targetTemp: 23.0,
      scheduleTarget: 23.0,
      isBoost: false,
      tempOffset: 0.0,
      sensorOnline: true,
      sensorAgeSec: 24,
      windowOpen: false,
      windowPauseRemainingSec: 0,
      frostActive: false,
      valveOpen: false,
      valveEnergisedSec: 0,
      duty: 0,
      learnedDuty: 12.0,
      heatRateDegPerHour: 0.58,
      heatRateLearned: true,
      runtimeTodaySec: 1500,
      runtimeYesterdaySec: 5400,
      state: 'IDLE',
      history24h: generateZoneHistory24h(23.5, 23.0, 2, false),
    },
    {
      id: 3,
      key: 'adults',
      name: 'Dormitor Mare',
      shortName: 'Dormitor',
      relayLabel: 'CH3 · GPIO25',
      relayPin: 25,
      circuits: 2,
      sensorName: 'ATC_Dormitor',
      sensorMac: 'A4:C1:38:4B:12:03',
      enabled: true,
      currentTemp: 22.8,
      humidity: 62,
      targetTemp: 23.5,
      scheduleTarget: 23.0,
      isBoost: true,
      tempOffset: 0.0,
      sensorOnline: true,
      sensorAgeSec: 11,
      windowOpen: false,
      windowPauseRemainingSec: 0,
      frostActive: false,
      valveOpen: true,
      valveEnergisedSec: 240,
      duty: 1.0,
      learnedDuty: 15.2,
      heatRateDegPerHour: 0.55,
      heatRateLearned: true,
      runtimeTodaySec: 2700,
      runtimeYesterdaySec: 9900,
      state: 'HEATING',
      history24h: generateZoneHistory24h(22.8, 23.5, 4, true),
    },
  ];

  return {
    firmwareVersion: '1.11.13.10',
    uiVersion: '2.0.0',
    uptimeSec: 184320,
    wifiRssi: -58,
    freeHeapBytes: 148240,
    simSpeed: 1,
    params: { ...DEFAULT_PARAMS },
    zones,
    bleSensors: [
      {
        mac: 'A4:C1:38:4B:12:01',
        name: 'ATC_Living',
        temp: 23.6,
        hum: 58,
        bat: 87,
        rssi: -62,
        ageSec: 18,
        zoneId: 1,
      },
      {
        mac: 'A4:C1:38:4B:12:02',
        name: 'ATC_Copii',
        temp: 23.5,
        hum: 61,
        bat: 79,
        rssi: -69,
        ageSec: 24,
        zoneId: 2,
      },
      {
        mac: 'A4:C1:38:4B:12:03',
        name: 'ATC_Dormitor',
        temp: 22.8,
        hum: 62,
        bat: 92,
        rssi: -65,
        ageSec: 11,
        zoneId: 3,
      },
      {
        mac: 'A4:C1:38:99:FA:10',
        name: 'ATC_Hol_Intrare',
        temp: 21.4,
        hum: 54,
        bat: 64,
        rssi: -77,
        ageSec: 42,
        zoneId: 0,
      },
    ],
    boiler: {
      autoMode: true,
      on: true,
      pendingAction: 'NONE',
      interlockRemainingSec: 0,
      antiCycleRemainingSec: 0,
      overrunRemainingSec: 0,
      overrunKeeperZoneId: 3,
      currentBurnSec: 420,
      todaySec: 3480,
      yesterdaySec: 6480,
      monthSec: 198400,
      totalSec: 1482000,
      pumpEnabled: false,
      pumpActive: false,
    },
    weather: {
      city: 'București',
      coords: '44.4268,26.1025',
      temp: 5.2,
      hum: 76,
      wind: 14,
      code: 2,
      isDay: true,
      updatedAt: 'acum 6 min',
      valid: true,
      forecast: [
        { day: 'Mar', code: 2, lo: 2, hi: 9 },
        { day: 'Mie', code: 61, lo: 3, hi: 7 },
        { day: 'Joi', code: 1, lo: 0, hi: 8 },
        { day: 'Vin', code: 0, lo: -1, hi: 10 },
      ],
    },
    gas: {
      pricePerM3: 3.3,
      measuredFlowM3h: 0.55,
      todayM3: 0.53,
      yesterdayM3: 0.99,
      monthM3: 31.08,
      lastReadingIndex: 4812.45,
      lastReadingTime: Math.floor(Date.now() / 1000) - 86400 * 3,
      estimatedIndex: 4816.12,
      readings: [
        {
          time: Math.floor(Date.now() / 1000) - 86400 * 10,
          index: 4804.2,
          runtimeSec: 1435000,
        },
        {
          time: Math.floor(Date.now() / 1000) - 86400 * 3,
          index: 4812.45,
          runtimeSec: 1475300,
        },
      ],
    },
    schedule: {
      enabled: true,
      preheat: true,
      windowGuard: true,
      away: false,
      activeLevel: 'day',
      nextLevel: 'night',
      nextSlotTime: '22:00',
      minsToNextSlot: 195,
      preHeatActive: false,
      weekday: [
        { time: '06:30', level: 'day' },
        { time: '22:00', level: 'night' },
      ],
      weekend: [
        { time: '08:00', level: 'day' },
        { time: '23:00', level: 'night' },
      ],
      levels: [
        { zoneId: 1, day: 23.0, night: 21.0, away: 16.0 },
        { zoneId: 2, day: 23.0, night: 21.0, away: 16.0 },
        { zoneId: 3, day: 23.0, night: 21.5, away: 16.0 },
      ],
    },
    dtcs: [
      {
        code: 'OK',
        name: 'Fără alarme active',
        severity: 'ok',
        active: true,
        at: 'Live',
        message: 'Toate zonele, vanele și senzorii BLE răspund în parametri.',
        count: 0,
      },
      {
        code: 'DTC-102',
        name: 'Baterie slabă senzor',
        severity: 'warn',
        active: false,
        zoneId: 3,
        at: '08:42',
        message: 'Dormitor Mare — avertisment închis după schimbarea bateriei.',
        count: 1,
      },
    ],
    events: [
      {
        id: 1,
        time: '06:30:02',
        type: 'safety',
        code: 'PREHEAT',
        message: 'Pre-încălzire meteo activă (avans 118 min la exterior 5.2 °C).',
      },
      {
        id: 2,
        time: '00:00:01',
        type: 'restart',
        code: 'SYS-REBOOT',
        message: 'Pornire sigură finalizată — toate vanele închise, comandă OFF cazan.',
      },
    ],
    history5d: generateHistory5d(zones),
  };
}

export function rebuildDtcs(zones: ZoneState[], prevDtcs: DtcItem[]): DtcItem[] {
  const activeAlerts: DtcItem[] = [];
  const nowStr = new Date().toLocaleTimeString('ro-RO', { hour: '2-digit', minute: '2-digit' });

  for (const z of zones) {
    if (!z.sensorOnline) {
      activeAlerts.push({
        code: 'DTC-101',
        name: 'Senzor BLE fără semnal',
        severity: 'danger',
        active: true,
        zoneId: z.id,
        at: nowStr,
        message: `${z.name} — fără date de la senzor. Zona rulează în regim fallback (max 30%).`,
        count: 1,
      });
    }
    if (z.frostActive) {
      activeAlerts.push({
        code: 'DTC-205',
        name: 'Protecție anti-îngheț activă',
        severity: 'warn',
        active: true,
        zoneId: z.id,
        at: nowStr,
        message: `${z.name} a coborât sub pragul de îngheț. Vana și centrala sunt forțate ON.`,
        count: 1,
      });
    }
  }

  const historical = prevDtcs.filter((d) => !d.active);
  if (activeAlerts.length === 0) {
    return [
      {
        code: 'OK',
        name: 'Fără alarme active',
        severity: 'ok',
        active: true,
        at: 'Live',
        message: 'Toate zonele, vanele și senzorii BLE răspund în parametri.',
        count: 0,
      },
      ...historical,
    ];
  }
  return [...activeAlerts, ...historical];
}

// Single deterministic physics & safety-supervisor simulation step (dtSec seconds)
export function tickSystemState(state: HeatingSystemState, dtSec: number): HeatingSystemState {
  const params = state.params;
  const outdoor = state.weather.temp;
  const weatherValid = state.weather.valid;

  let boiler = { ...state.boiler };
  let gas = { ...state.gas };

  // Advance timers
  if (boiler.antiCycleRemainingSec > 0) {
    boiler.antiCycleRemainingSec = Math.max(0, boiler.antiCycleRemainingSec - dtSec);
  }
  if (boiler.overrunRemainingSec > 0) {
    boiler.overrunRemainingSec = Math.max(0, boiler.overrunRemainingSec - dtSec);
  }

  // Evaluate each zone's demand & thermal physics
  let anyDemand = false;
  let anyValveInterlockReady = false;
  let keeperZoneId = boiler.overrunKeeperZoneId;

  const nextZones: ZoneState[] = state.zones.map((orig) => {
    const z = { ...orig };

    // Sensor age
    if (z.sensorOnline) {
      z.sensorAgeSec = (z.sensorAgeSec + dtSec) % 45;
    } else {
      z.sensorAgeSec += dtSec;
    }

    // Window pause countdown
    if (z.windowOpen) {
      z.windowPauseRemainingSec = Math.max(0, z.windowPauseRemainingSec - dtSec);
      if (z.windowPauseRemainingSec === 0) {
        z.windowOpen = false;
      }
    }

    // Frost protection (FR-16.1): applies even when disabled or away
    z.frostActive = evaluateFrostProtection(z.frostActive, z.sensorOnline, z.currentTemp, params.frostTemp);

    // Determine effective demand
    let wantsHeat = false;
    if (z.frostActive) {
      wantsHeat = true;
      z.duty = 1.0;
    } else if (!z.enabled) {
      wantsHeat = false;
      z.duty = 0;
    } else if (z.windowOpen) {
      wantsHeat = false;
      z.duty = 0;
    } else if (!z.sensorOnline) {
      // FR-51: sensor timeout fallback duty (capped at 30%)
      z.duty = Math.min(0.3, Math.max(0.2, z.learnedDuty / 100));
      wantsHeat = true;
    } else {
      const tpi = computeTpiDuty(
        z.currentTemp,
        z.targetTemp,
        outdoor,
        weatherValid,
        z.learnedDuty,
        params.hysteresis,
      );
      z.duty = tpi.duty;
      wantsHeat = tpi.demand;
    }

    if (wantsHeat) {
      anyDemand = true;
    }

    // Valve arbitration + FR-3 overrun hold for last open valve
    if (boiler.autoMode) {
      if (wantsHeat) {
        z.valveOpen = true;
        z.valveEnergisedSec += dtSec;
        if (z.valveEnergisedSec >= params.interlockDelay || params.openLoopBypass) {
          anyValveInterlockReady = true;
          keeperZoneId = z.id;
        }
      } else {
        const shouldHoldForOverrun =
          (boiler.on || boiler.overrunRemainingSec > 0) && keeperZoneId === z.id;
        if (shouldHoldForOverrun) {
          z.valveOpen = true;
          z.valveEnergisedSec += dtSec;
        } else {
          z.valveOpen = false;
          z.valveEnergisedSec = 0;
          if (keeperZoneId === z.id) keeperZoneId = null;
        }
      }
    } else if (z.valveOpen) {
      z.valveEnergisedSec += dtSec;
      if (z.valveEnergisedSec >= params.interlockDelay || params.openLoopBypass) {
        anyValveInterlockReady = true;
        keeperZoneId = z.id;
      }
    }

    // Thermal model: heat when boiler ON + valve open, cool gently toward outdoor otherwise
    const dtHours = dtSec / 3600;
    if (z.valveOpen && boiler.on) {
      z.currentTemp = round2(z.currentTemp + z.heatRateDegPerHour * dtHours);
      z.runtimeTodaySec += dtSec;
    } else {
      const lossRate = z.windowOpen ? 0.12 : 0.008;
      const cooling = lossRate * Math.max(0, z.currentTemp - outdoor) * dtHours;
      z.currentTemp = round2(Math.max(5.0, z.currentTemp - cooling));
    }

    z.state = deriveZoneStateCode(z);
    return z;
  });

  // Boiler interlock & anti-short-cycling arbitration (AUTO mode)
  if (boiler.autoMode) {
    const waitingValves = nextZones.filter(
      (z) => z.valveOpen && z.valveEnergisedSec < params.interlockDelay,
    );
    if (!anyValveInterlockReady && anyDemand && waitingValves.length > 0 && !params.openLoopBypass) {
      const maxElapsed = Math.max(...waitingValves.map((z) => z.valveEnergisedSec));
      boiler.interlockRemainingSec = Math.max(0, params.interlockDelay - maxElapsed);
    } else {
      boiler.interlockRemainingSec = 0;
    }

    if (anyDemand && anyValveInterlockReady) {
      if (!boiler.on) {
        if (boiler.antiCycleRemainingSec === 0) {
          boiler.on = true;
          boiler.currentBurnSec = 0;
          boiler.overrunRemainingSec = 0;
          boiler.pendingAction = 'NONE';
        } else {
          boiler.pendingAction = 'PENDING_ON';
        }
      } else {
        boiler.pendingAction = 'NONE';
      }
    } else {
      if (boiler.on) {
        if (boiler.currentBurnSec >= params.burnerMinRun) {
          boiler.on = false;
          boiler.currentBurnSec = 0;
          boiler.antiCycleRemainingSec = params.burnerMinPause;
          boiler.overrunRemainingSec = params.overrun;
          boiler.pendingAction = 'NONE';
        } else {
          boiler.pendingAction = 'PENDING_OFF';
          boiler.antiCycleRemainingSec = Math.max(0, params.burnerMinRun - boiler.currentBurnSec);
        }
      } else if (boiler.pendingAction === 'PENDING_ON') {
        boiler.pendingAction = 'NONE';
      }
    }
  }

  boiler.overrunKeeperZoneId = keeperZoneId;
  boiler.pumpEnabled = params.pumpControlEnabled;
  boiler.pumpActive = params.pumpControlEnabled && (boiler.on || boiler.overrunRemainingSec > 0);

  if (boiler.on) {
    boiler.currentBurnSec += dtSec;
    boiler.todaySec += dtSec;
    boiler.monthSec += dtSec;
    boiler.totalSec += dtSec;
    const burnedM3 = (dtSec / 3600) * gas.measuredFlowM3h;
    gas = {
      ...gas,
      todayM3: Math.round((gas.todayM3 + burnedM3) * 1000) / 1000,
      monthM3: Math.round((gas.monthM3 + burnedM3) * 1000) / 1000,
      estimatedIndex:
        gas.estimatedIndex !== null
          ? Math.round((gas.estimatedIndex + burnedM3) * 1000) / 1000
          : null,
    };
  }

  // Sync BLE sensors mirror
  const nextBle = state.bleSensors.map((s) => {
    const z = nextZones.find((zn) => zn.sensorMac === s.mac);
    if (!z) return { ...s, ageSec: (s.ageSec + dtSec) % 55 };
    return {
      ...s,
      temp: round1(z.currentTemp),
      hum: z.humidity,
      ageSec: z.sensorAgeSec,
    };
  });

  return {
    ...state,
    uptimeSec: state.uptimeSec + dtSec,
    zones: nextZones,
    bleSensors: nextBle,
    boiler,
    gas,
    dtcs: rebuildDtcs(nextZones, state.dtcs),
  };
}

export function setZoneTarget(
  state: HeatingSystemState,
  zoneId: number,
  targetTemp: number,
): HeatingSystemState {
  const clamped = round1(clamp(targetTemp, TARGET_MIN_C, TARGET_MAX_C));
  const zones = state.zones.map((z) => {
    if (z.id !== zoneId) return z;
    const isBoost = Math.abs(clamped - z.scheduleTarget) >= 0.05;
    const updated: ZoneState = {
      ...z,
      targetTemp: clamped,
      isBoost,
      history24h: z.history24h.map((pt, idx) => (idx >= 21 ? { ...pt, target: clamped } : pt)),
    };
    return updated;
  });
  return tickSystemState({ ...state, zones }, 0);
}

export function clearZoneBoost(state: HeatingSystemState, zoneId: number): HeatingSystemState {
  const zones = state.zones.map((z) =>
    z.id === zoneId
      ? {
          ...z,
          targetTemp: z.scheduleTarget,
          isBoost: false,
        }
      : z,
  );
  return tickSystemState({ ...state, zones }, 0);
}

export function toggleAwayMode(state: HeatingSystemState, away: boolean): HeatingSystemState {
  const activeLevel: ScheduleLevelName = away ? 'away' : 'day';
  const zones = state.zones.map((z) => {
    const lvl = state.schedule.levels.find((l) => l.zoneId === z.id);
    const nextTarget = lvl ? (away ? lvl.away : lvl.day) : away ? 16.0 : 23.0;
    return {
      ...z,
      scheduleTarget: nextTarget,
      targetTemp: nextTarget,
      isBoost: false,
    };
  });
  const nowStr = new Date().toLocaleTimeString('ro-RO', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
  return tickSystemState(
    {
      ...state,
      zones,
      schedule: { ...state.schedule, away, activeLevel },
      events: [
        {
          id: Date.now(),
          time: nowStr,
          type: 'admin' as const,
          code: away ? 'AWAY-ON' : 'AWAY-OFF',
          message: away
            ? 'Mod Plecat activat — toate zonele coboară la nivelul de gardă.'
            : 'Mod Plecat dezactivat — revenire la programul activ.',
        },
        ...state.events,
      ].slice(0, 50),
    },
    0,
  );
}

export function recordGasMeterReading(
  state: HeatingSystemState,
  indexM3: number,
  epochSec = Math.floor(Date.now() / 1000),
): { ok: boolean; error?: string; state: HeatingSystemState } {
  if (!Number.isFinite(indexM3) || indexM3 < 0 || indexM3 > 999999.999) {
    return {
      ok: false,
      error: 'Indexul contorului trebuie să fie între 0 și 999999 m³ (FR-35.1).',
      state,
    };
  }
  const rounded = Math.round(indexM3 * 1000) / 1000;
  const newReading = {
    time: epochSec,
    index: rounded,
    runtimeSec: state.boiler.totalSec,
  };
  const readings = [...state.gas.readings, newReading].sort((a, b) => a.time - b.time);

  // Recalculate measured flow from the last two readings with runtime if possible
  let measuredFlow = state.gas.measuredFlowM3h;
  if (readings.length >= 2) {
    const prev = readings[readings.length - 2];
    const curr = readings[readings.length - 1];
    const dM3 = curr.index - prev.index;
    const dSec = (curr.runtimeSec ?? 0) - (prev.runtimeSec ?? 0);
    if (dM3 > 0 && dSec >= 600) {
      measuredFlow = round2(clamp(dM3 / (dSec / 3600), 0.2, 2.5));
    }
  }

  const nowStr = new Date().toLocaleTimeString('ro-RO', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });

  return {
    ok: true,
    state: {
      ...state,
      gas: {
        ...state.gas,
        measuredFlowM3h: measuredFlow,
        lastReadingIndex: rounded,
        lastReadingTime: epochSec,
        estimatedIndex: rounded,
        readings,
      },
      events: [
        {
          id: Date.now(),
          time: nowStr,
          type: 'admin' as const,
          code: 'GAS-IDX',
          message: `Citire index contor salvată: ${rounded.toFixed(3)} m³.`,
        },
        ...state.events,
      ].slice(0, 50),
    },
  };
}

export function serializeScheduleSlots(slots: ScheduleSlot[]): string {
  return [...slots]
    .sort((a, b) => a.time.localeCompare(b.time))
    .map((s) => `${s.time}=${s.level}`)
    .join(',');
}

export function buildConfigExport(state: HeatingSystemState): ConfigExportFormat1 {
  return {
    format: 1,
    firmwareVersion: state.firmwareVersion,
    params: {
      actuatorDelaySec: state.params.interlockDelay,
      pumpOverrunSec: state.params.overrun,
      burnerMinRunSec: state.params.burnerMinRun,
      burnerMinPauseSec: state.params.burnerMinPause,
      rfSafetyHeartbeatSec: state.params.hbInterval,
      rfSafetyHeartbeatEnabled: state.params.boilerLinkHeartbeat,
      freezeProtectionTemp: state.params.frostTemp,
      sensorTimeoutSec: state.params.sensorTimeout,
      hysteresis: state.params.hysteresis,
      windowThreshold: state.params.windowThreshold,
      openLoopBypass: state.params.openLoopBypass,
      actuatorNormallyClosed: state.params.actuatorNormallyClosed,
      pumpControlEnabled: state.params.pumpControlEnabled,
    },
    zones: state.zones.map((z) => ({
      id: z.id,
      name: z.name,
      mac: z.sensorMac,
      sensorName: z.sensorName,
      relayPin: z.relayPin,
      enabled: z.enabled,
      offset: z.tempOffset,
    })),
    schedule: {
      enabled: state.schedule.enabled,
      preheat: state.schedule.preheat,
      window: state.schedule.windowGuard,
      weekday: serializeScheduleSlots(state.schedule.weekday),
      weekend: serializeScheduleSlots(state.schedule.weekend),
      levels: state.schedule.levels.map((l) => ({
        id: l.zoneId,
        day: l.day,
        night: l.night,
        away: l.away,
      })),
    },
    location: state.weather.coords,
    learnedDuty: state.zones.map((z) => ({ id: z.id, duty: z.learnedDuty })),
  };
}

export function applyConfigImport(
  state: HeatingSystemState,
  rawJson: string,
): {
  ok: boolean;
  error?: string;
  applied: string[];
  skipped: Array<{ name: string; reason: string }>;
  state: HeatingSystemState;
} {
  let parsed: unknown;
  try {
    parsed = JSON.parse(rawJson);
  } catch {
    return { ok: false, error: 'JSON invalid — verifică sintaxa fișierului.', applied: [], skipped: [], state };
  }
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return { ok: false, error: 'Fișierul trebuie să conțină un obiect JSON.', applied: [], skipped: [], state };
  }
  const obj = parsed as Record<string, unknown>;
  if (obj.format !== 1) {
    return {
      ok: false,
      error: 'Câmpul "format" lipsește sau nu este 1 (FR-65.1). Nimic nu a fost modificat.',
      applied: [],
      skipped: [],
      state,
    };
  }

  const applied: string[] = [];
  const skipped: Array<{ name: string; reason: string }> = [];
  const nextParams = { ...state.params };

  const paramMap: Record<string, keyof SystemParams> = {
    actuatorDelaySec: 'interlockDelay',
    pumpOverrunSec: 'overrun',
    burnerMinRunSec: 'burnerMinRun',
    burnerMinPauseSec: 'burnerMinPause',
    rfSafetyHeartbeatSec: 'hbInterval',
    freezeProtectionTemp: 'frostTemp',
    sensorTimeoutSec: 'sensorTimeout',
    hysteresis: 'hysteresis',
    windowThreshold: 'windowThreshold',
  };

  if (obj.params && typeof obj.params === 'object') {
    const pObj = obj.params as Record<string, unknown>;
    for (const [k, v] of Object.entries(pObj)) {
      const targetKey = paramMap[k];
      if (targetKey) {
        if (typeof v === 'number' && validateNumericParam(targetKey, v).ok) {
          (nextParams as Record<string, unknown>)[targetKey] = v;
          applied.push(`params.${k}`);
        } else {
          skipped.push({ name: `params.${k}`, reason: 'invalid' });
        }
      } else if (
        k === 'rfSafetyHeartbeatEnabled' ||
        k === 'openLoopBypass' ||
        k === 'actuatorNormallyClosed' ||
        k === 'pumpControlEnabled'
      ) {
        if (typeof v === 'boolean') {
          if (k === 'rfSafetyHeartbeatEnabled') nextParams.boilerLinkHeartbeat = v;
          if (k === 'openLoopBypass') nextParams.openLoopBypass = v;
          if (k === 'actuatorNormallyClosed') nextParams.actuatorNormallyClosed = v;
          if (k === 'pumpControlEnabled') nextParams.pumpControlEnabled = v;
          applied.push(`params.${k}`);
        } else {
          skipped.push({ name: `params.${k}`, reason: 'invalid' });
        }
      } else {
        skipped.push({ name: `params.${k}`, reason: 'unknown' });
      }
    }
  }

  const nowStr = new Date().toLocaleTimeString('ro-RO', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });

  const nextState: HeatingSystemState = {
    ...state,
    params: nextParams,
    events: [
      {
        id: Date.now(),
        time: nowStr,
        type: 'admin' as const,
        code: 'CFG-IMP',
        message: `Import configurație: ${applied.length} aplicate, ${skipped.length} omise.`,
      },
      ...state.events,
    ].slice(0, 50),
  };

  return { ok: true, applied, skipped, state: nextState };
}

export function executeConsoleCommand(
  state: HeatingSystemState,
  rawCmd: string,
): { lines: string[]; state: HeatingSystemState } {
  const trimmed = rawCmd.trim();
  const parts = trimmed.split(/\s+/);
  const cmd = (parts[0] ?? '').toUpperCase();
  const arg1 = (parts[1] ?? '').toUpperCase();

  if (!cmd) return { lines: [], state };

  switch (cmd) {
    case 'HELP':
      return {
        lines: [
          'Comenzi disponibile (ESP32 Smart Heating CLI):',
          '  STATUS           - stare generală cazan, meteo, uptime și heap',
          '  ZONES            - telemetrie TPI, temperaturi și vane pe zone',
          '  DIAG             - coduri DTC active și verificări siguranță',
          '  BLE              - listă senzori Xiaomi BLE recepționați',
          '  CONFIG           - parametri hidraulici și de siguranță §6',
          '  EVENTS           - ultimele evenimente din jurnalul LittleFS',
          '  MODE AUTO|MANUAL - schimbă modul de operare al centralei',
          '  AWAY ON|OFF      - activează/dezactivează modul Plecat',
          '  CLEAR            - curăță ecranul terminalului',
        ],
        state,
      };

    case 'STATUS': {
      const lead = computePreHeatLeadMinutes(state.weather.temp, state.weather.valid);
      return {
        lines: [
          `[STATUS] FW v${state.firmwareVersion} | Mode: ${state.boiler.autoMode ? 'AUTO' : 'MANUAL'} | Away: ${state.schedule.away ? 'ON' : 'OFF'}`,
          `[BOILER] Flame: ${state.boiler.on ? 'ON' : 'OFF'} | Pending: ${state.boiler.pendingAction} | Interlock: ${state.boiler.interlockRemainingSec}s | AntiCycle: ${state.boiler.antiCycleRemainingSec}s | Overrun: ${state.boiler.overrunRemainingSec}s`,
          `[WEATHER] ${state.weather.city}: ${state.weather.temp.toFixed(1)}°C, RH ${state.weather.hum}%, pre-heat lead: ${lead} min`,
          `[SYS] Uptime: ${formatDurationRo(state.uptimeSec)} | WiFi RSSI: ${state.wifiRssi} dBm | Free Heap: ${state.freeHeapBytes} B`,
        ],
        state,
      };
    }

    case 'ZONES':
      return {
        lines: state.zones.map(
          (z) =>
            `  Z${z.id} ${z.shortName.padEnd(9)} | ${z.currentTemp.toFixed(1)}°C -> ${z.targetTemp.toFixed(1)}°C | Valve: ${z.valveOpen ? 'OPEN  ' : 'CLOSED'} | Duty: ${Math.round(z.duty * 100)}% (base ${z.learnedDuty.toFixed(1)}%) | State: ${z.state}`,
        ),
        state,
      };

    case 'DIAG': {
      const active = state.dtcs.filter((d) => d.active);
      return {
        lines: [
          `[DIAG] Active entries: ${active.length}`,
          ...active.map((d) => `  ${d.code} [${d.severity.toUpperCase()}] ${d.name} — ${d.message}`),
        ],
        state,
      };
    }

    case 'BLE':
      return {
        lines: [
          `[BLE] Passive NimBLE observer — ${state.bleSensors.length} sensors seen:`,
          ...state.bleSensors.map(
            (s) =>
              `  ${s.mac}  ${s.name.padEnd(15)}  ${s.temp.toFixed(1)}°C  ${s.hum}%  Bat:${s.bat}%  RSSI:${s.rssi}dBm  Age:${s.ageSec}s  ${s.zoneId ? `Z${s.zoneId}` : 'unpaired'}`,
          ),
        ],
        state,
      };

    case 'CONFIG':
      return {
        lines: [
          `[CONFIG] interlockDelay=${state.params.interlockDelay}s overrun=${state.params.overrun}s burnerMinRun=${state.params.burnerMinRun}s burnerMinPause=${state.params.burnerMinPause}s`,
          `[CONFIG] frostTemp=${state.params.frostTemp.toFixed(1)}°C hysteresis=${state.params.hysteresis.toFixed(1)}°C windowThreshold=${state.params.windowThreshold.toFixed(1)}°C sensorTimeout=${state.params.sensorTimeout}s`,
          `[CONFIG] actuator=${state.params.actuatorNormallyClosed ? 'NC' : 'NO'} pump=${state.params.pumpControlEnabled ? 'ON' : 'OFF'} openLoopBypass=${state.params.openLoopBypass ? 'ON' : 'OFF'}`,
        ],
        state,
      };

    case 'EVENTS':
      return {
        lines: state.events.slice(0, 8).map((e) => `  [${e.time}] ${e.code} (${e.type}): ${e.message}`),
        state,
      };

    case 'MODE':
      if (arg1 === 'AUTO' || arg1 === 'MANUAL') {
        const autoMode = arg1 === 'AUTO';
        return {
          lines: [`[OK] Mod operare setat la ${arg1}.`],
          state: { ...state, boiler: { ...state.boiler, autoMode } },
        };
      }
      return { lines: ['[ERR] Utilizare: MODE AUTO | MODE MANUAL'], state };

    case 'AWAY':
      if (arg1 === 'ON' || arg1 === 'OFF') {
        const next = toggleAwayMode(state, arg1 === 'ON');
        return { lines: [`[OK] Mod Plecat: ${arg1}.`], state: next };
      }
      return { lines: ['[ERR] Utilizare: AWAY ON | AWAY OFF'], state };

    default:
      return {
        lines: [`[ERR] Comandă necunoscută: "${trimmed}". Scrie HELP pentru lista de comenzi.`],
        state,
      };
  }
}
