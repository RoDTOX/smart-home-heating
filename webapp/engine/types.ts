export type ZoneStateCode =
  | 'FROST'
  | 'DISABLED'
  | 'FAULT'
  | 'ACQUIRING'
  | 'WINDOW'
  | 'HEATING'
  | 'IDLE';

export type ScheduleLevelName = 'day' | 'night' | 'away';

export interface ZoneHistoryPoint {
  hourLabel: string;
  temp: number;
  target: number;
  outdoor: number;
  valve: number; // 0..1 duty proportion
}

export interface ZoneState {
  id: number;
  key: string;
  name: string;
  shortName: string;
  relayLabel: string;
  relayPin: number;
  circuits: number;
  sensorName: string;
  sensorMac: string;
  enabled: boolean;
  currentTemp: number;
  humidity: number;
  targetTemp: number;
  scheduleTarget: number;
  isBoost: boolean;
  tempOffset: number;
  sensorOnline: boolean;
  sensorAgeSec: number;
  windowOpen: boolean;
  windowPauseRemainingSec: number;
  frostActive: boolean;
  valveOpen: boolean;
  valveEnergisedSec: number;
  duty: number; // 0..1
  learnedDuty: number; // 0..60 %
  heatRateDegPerHour: number;
  heatRateLearned: boolean;
  runtimeTodaySec: number;
  runtimeYesterdaySec: number;
  state: ZoneStateCode;
  history24h: ZoneHistoryPoint[];
}

export interface BleSensorInfo {
  mac: string;
  name: string;
  temp: number;
  hum: number;
  bat: number;
  rssi: number;
  ageSec: number;
  zoneId: number; // 0 if unassigned
}

export interface ForecastDay {
  day: string;
  code: number;
  lo: number;
  hi: number;
}

export interface WeatherState {
  city: string;
  coords: string;
  temp: number;
  hum: number;
  wind: number;
  code: number;
  isDay: boolean;
  updatedAt: string;
  valid: boolean;
  forecast: ForecastDay[];
}

export interface BoilerState {
  autoMode: boolean;
  on: boolean;
  pendingAction: 'NONE' | 'PENDING_ON' | 'PENDING_OFF';
  interlockRemainingSec: number;
  antiCycleRemainingSec: number;
  overrunRemainingSec: number;
  overrunKeeperZoneId: number | null;
  currentBurnSec: number;
  todaySec: number;
  yesterdaySec: number;
  monthSec: number;
  totalSec: number;
  pumpEnabled: boolean;
  pumpActive: boolean;
}

export interface GasReading {
  time: number; // epoch sec
  index: number; // m3
  runtimeSec: number | null;
}

export interface GasMeterState {
  pricePerM3: number;
  measuredFlowM3h: number;
  todayM3: number;
  yesterdayM3: number;
  monthM3: number;
  lastReadingIndex: number | null;
  lastReadingTime: number | null;
  estimatedIndex: number | null;
  readings: GasReading[];
}

export interface DtcItem {
  code: string;
  name: string;
  severity: 'ok' | 'warn' | 'danger';
  active: boolean;
  zoneId?: number;
  at: string;
  message: string;
  count: number;
}

export interface DiagnosticEvent {
  id: number;
  time: string;
  type: 'dtc-raised' | 'dtc-cleared' | 'restart' | 'admin' | 'safety';
  code: string;
  zone?: number;
  message: string;
}

export interface ScheduleSlot {
  time: string; // HH:MM
  level: ScheduleLevelName;
}

export interface ZoneScheduleLevels {
  zoneId: number;
  day: number;
  night: number;
  away: number;
}

export interface ScheduleState {
  enabled: boolean;
  preheat: boolean;
  windowGuard: boolean;
  away: boolean;
  activeLevel: ScheduleLevelName;
  nextLevel: ScheduleLevelName;
  nextSlotTime: string;
  minsToNextSlot: number;
  preHeatActive: boolean;
  weekday: ScheduleSlot[];
  weekend: ScheduleSlot[];
  levels: ZoneScheduleLevels[];
}

export interface SystemParams {
  interlockDelay: number; // 120..600 s
  overrun: number; // 180..600 s
  burnerMinRun: number; // 120..600 s
  burnerMinPause: number; // 120..600 s
  frostTemp: number; // 8..16 C
  sensorTimeout: number; // 60..3600 s
  hysteresis: number; // 0.1..1.0 C
  windowThreshold: number; // 0.8..2.0 C
  hbInterval: number; // 30..900 s
  boilerLinkHeartbeat: boolean;
  openLoopBypass: boolean;
  actuatorNormallyClosed: boolean;
  pumpControlEnabled: boolean;
  gasPrice: number; // 1.0..10.0 Lei/m3
  gasFlameRate: number; // 0.2..2.5 m3/h
}

export interface ParamSpec {
  key: keyof SystemParams;
  label: string;
  unit: string;
  min: number;
  max: number;
  step: number;
  defaultValue: number;
  safetyCritical: boolean;
  description: string;
}

export interface GlobalHistoryPoint {
  label: string;
  z1: number;
  z2: number;
  z3: number;
  outdoor: number;
  boilerOn: boolean;
}

export interface HeatingSystemState {
  firmwareVersion: string;
  uiVersion: string;
  uptimeSec: number;
  wifiRssi: number;
  freeHeapBytes: number;
  simSpeed: 1 | 10 | 60;
  params: SystemParams;
  zones: ZoneState[];
  bleSensors: BleSensorInfo[];
  boiler: BoilerState;
  weather: WeatherState;
  gas: GasMeterState;
  schedule: ScheduleState;
  dtcs: DtcItem[];
  events: DiagnosticEvent[];
  history5d: GlobalHistoryPoint[];
}

export interface ConfigExportFormat1 {
  format: 1;
  firmwareVersion: string;
  params: {
    actuatorDelaySec: number;
    pumpOverrunSec: number;
    burnerMinRunSec: number;
    burnerMinPauseSec: number;
    rfSafetyHeartbeatSec: number;
    rfSafetyHeartbeatEnabled: boolean;
    freezeProtectionTemp: number;
    sensorTimeoutSec: number;
    hysteresis: number;
    windowThreshold: number;
    openLoopBypass: boolean;
    actuatorNormallyClosed: boolean;
    pumpControlEnabled: boolean;
  };
  zones: Array<{
    id: number;
    name: string;
    mac: string;
    sensorName: string;
    relayPin: number;
    enabled: boolean;
    offset: number;
  }>;
  schedule: {
    enabled: boolean;
    preheat: boolean;
    window: boolean;
    weekday: string;
    weekend: string;
    levels: Array<{ id: number; day: number; night: number; away: number }>;
  };
  location: string;
  learnedDuty: Array<{ id: number; duty: number }>;
}
