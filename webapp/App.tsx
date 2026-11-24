import React, { useEffect, useState } from 'react';
import { AdminView } from './components/AdminView';
import { GasReadingModal } from './components/GasReadingModal';
import { Header, type ActiveView } from './components/Header';
import { LeftColumn } from './components/LeftColumn';
import { RightColumn } from './components/RightColumn';
import { ScheduleView } from './components/ScheduleView';
import { ZoneCard } from './components/ZoneCard';
import {
  applyConfigImport,
  clamp,
  clearZoneBoost,
  createInitialSystemState,
  DEFAULT_PARAMS,
  executeConsoleCommand,
  recordGasMeterReading,
  round1,
  setZoneTarget,
  tickSystemState,
  toggleAwayMode,
  validateNumericParam,
} from './engine/heatingEngine';
import { applyThemeTokens, THEMES, type ThemeId } from './engine/themes';
import type { HeatingSystemState, ScheduleLevelName, ScheduleSlot, SystemParams } from './engine/types';

const DAYS_RO = ['Duminică', 'Luni', 'Marți', 'Miercuri', 'Joi', 'Vineri', 'Sâmbătă'];
const MONTHS_RO = [
  'Ianuarie',
  'Februarie',
  'Martie',
  'Aprilie',
  'Mai',
  'Iunie',
  'Iulie',
  'August',
  'Septembrie',
  'Octombrie',
  'Noiembrie',
  'Decembrie',
];

function loadSavedTheme(): ThemeId {
  try {
    const raw = localStorage.getItem('shh-theme') as ThemeId | null;
    if (raw && THEMES[raw]) return raw;
  } catch {
    // ignore storage errors
  }
  return 'smartiva-glass';
}

export const App: React.FC = () => {
  const [themeId, setThemeId] = useState<ThemeId>(loadSavedTheme);
  const [activeView, setActiveView] = useState<ActiveView>('dashboard');
  const [expandedZoneId, setExpandedZoneId] = useState<number | null>(3);
  const [gasModalOpen, setGasModalOpen] = useState(false);
  const [toast, setToast] = useState<string | null>(null);
  const [now, setNow] = useState(() => new Date());
  const [system, setSystem] = useState<HeatingSystemState>(() => createInitialSystemState());
  const [consoleLog, setConsoleLog] = useState<string[]>([
    '[BOOT] Smart Home Heating ESP32 Simulator (FW v1.11.13.10)',
    '[BOOT] NimBLE passive observer ready — 4 sensors detected, 3 zones mapped.',
    '[BOOT] Scrie HELP pentru lista de comenzi disponibile.',
  ]);

  const showToast = (msg: string) => {
    setToast(msg);
  };

  useEffect(() => {
    if (!toast) return;
    const t = setTimeout(() => setToast(null), 2600);
    return () => clearTimeout(t);
  }, [toast]);

  // Apply theme CSS tokens and persist
  useEffect(() => {
    applyThemeTokens(themeId);
    try {
      localStorage.setItem('shh-theme', themeId);
    } catch {
      // ignore storage errors
    }
  }, [themeId]);

  // 1-second real-time clock & physics simulation loop
  useEffect(() => {
    const timer = setInterval(() => {
      setNow(new Date());
      setSystem((prev) => tickSystemState(prev, prev.simSpeed));
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  const clockHm = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
  const clockSs = String(now.getSeconds()).padStart(2, '0');
  const dateRo = `${DAYS_RO[now.getDay()]}, ${now.getDate()} ${MONTHS_RO[now.getMonth()]} ${now.getFullYear()}`;

  const handleSetZoneTarget = (zoneId: number, target: number) => {
    setSystem((prev) => setZoneTarget(prev, zoneId, target));
  };

  const handleClearZoneBoost = (zoneId: number) => {
    setSystem((prev) => clearZoneBoost(prev, zoneId));
    showToast('Revenire la temperatura din program.');
  };

  const handleResumeWindow = (zoneId: number) => {
    setSystem((prev) =>
      tickSystemState(
        {
          ...prev,
          zones: prev.zones.map((z) =>
            z.id === zoneId ? { ...z, windowOpen: false, windowPauseRemainingSec: 0 } : z,
          ),
        },
        0,
      ),
    );
    showToast('Încălzirea zonei a fost reluată.');
  };

  const handleToggleAway = () => {
    const nextAway = !system.schedule.away;
    setSystem((prev) => toggleAwayMode(prev, nextAway));
    showToast(nextAway ? 'Mod Plecat activat.' : 'Mod Plecat dezactivat.');
  };

  const handleRefreshWeather = () => {
    setSystem((prev) => ({
      ...prev,
      weather: {
        ...prev.weather,
        temp: round1(prev.weather.temp + (Math.random() > 0.5 ? 0.1 : -0.1)),
        updatedAt: 'acum',
      },
    }));
    showToast('Date meteo Open-Meteo actualizate.');
  };

  const handleGasReadingSubmit = (indexM3: number) => {
    const res = recordGasMeterReading(system, indexM3);
    if (res.ok) {
      setSystem(res.state);
      showToast(`Index contor ${indexM3.toFixed(3)} m³ salvat.`);
    }
    return { ok: res.ok, error: res.error };
  };

  const handleClearEvents = () => {
    setSystem((prev) => {
      const lastReboot = prev.events.find((e) => e.type === 'restart') ?? {
        id: Date.now(),
        time: clockHm + ':' + clockSs,
        type: 'restart' as const,
        code: 'SYS-REBOOT',
        message: 'Pornire sigură păstrată conform FR-62.',
      };
      return { ...prev, events: [lastReboot] };
    });
    showToast('Jurnal curățat (ultima repornire păstrată, FR-62).');
  };

  const handleScheduleFlag = (flag: 'enabled' | 'preheat' | 'windowGuard' | 'away') => {
    if (flag === 'away') {
      handleToggleAway();
      return;
    }
    setSystem((prev) => ({
      ...prev,
      schedule: { ...prev.schedule, [flag]: !prev.schedule[flag] },
    }));
    showToast('Setare program actualizată.');
  };

  const handleScheduleLevel = (zoneId: number, level: ScheduleLevelName, temp: number) => {
    const clamped = round1(clamp(temp, 5.0, 28.0));
    setSystem((prev) => {
      const levels = prev.schedule.levels.map((l) =>
        l.zoneId === zoneId ? { ...l, [level]: clamped } : l,
      );
      const isCurrentLevel = prev.schedule.activeLevel === level;
      const zones = isCurrentLevel
        ? prev.zones.map((z) =>
            z.id === zoneId
              ? {
                  ...z,
                  scheduleTarget: clamped,
                  targetTemp: z.isBoost ? z.targetTemp : clamped,
                }
              : z,
          )
        : prev.zones;
      return tickSystemState({ ...prev, zones, schedule: { ...prev.schedule, levels } }, 0);
    });
  };

  const handleScheduleSlots = (weekend: boolean, slots: ScheduleSlot[]) => {
    setSystem((prev) => ({
      ...prev,
      schedule: {
        ...prev.schedule,
        [weekend ? 'weekend' : 'weekday']: slots,
      },
    }));
  };

  const handleScheduleFactoryReset = () => {
    setSystem((prev) => {
      const levels = prev.zones.map((z) => ({
        zoneId: z.id,
        day: 22.0,
        night: 20.0,
        away: 16.0,
      }));
      const zones = prev.zones.map((z) => ({
        ...z,
        scheduleTarget: 22.0,
        targetTemp: 22.0,
        isBoost: false,
      }));
      return tickSystemState(
        {
          ...prev,
          zones,
          schedule: {
            ...prev.schedule,
            away: false,
            activeLevel: 'day',
            weekday: [
              { time: '06:30', level: 'day' },
              { time: '22:30', level: 'night' },
            ],
            weekend: [
              { time: '08:00', level: 'day' },
              { time: '23:00', level: 'night' },
            ],
            levels,
          },
        },
        0,
      );
    });
    showToast('Program resetat la valorile din fabrică (22 / 20 / 16 °C).');
  };

  const handleUpdateParam = (key: keyof SystemParams, value: number | boolean) => {
    if (typeof value === 'number') {
      const check = validateNumericParam(key, value);
      if (!check.ok) return check;
    }
    setSystem((prev) => {
      const nextParams = { ...prev.params, [key]: value };
      const nextGas =
        key === 'gasPrice' && typeof value === 'number'
          ? { ...prev.gas, pricePerM3: value }
          : prev.gas;
      return {
        ...prev,
        params: nextParams,
        gas: nextGas,
        events: [
          {
            id: Date.now(),
            time: `${clockHm}:${clockSs}`,
            type: 'admin' as const,
            code: 'PARAM',
            message: `Parametru ${key} actualizat la ${String(value)}.`,
          },
          ...prev.events,
        ].slice(0, 50),
      };
    });
    showToast('Parametru salvat cu succes.');
    return { ok: true };
  };

  const handleResetSafetyParams = () => {
    setSystem((prev) => ({
      ...prev,
      params: { ...DEFAULT_PARAMS },
      gas: { ...prev.gas, pricePerM3: DEFAULT_PARAMS.gasPrice },
    }));
    showToast('Parametrii §6 au fost resetați la valorile implicite.');
  };

  const handleRunConsoleCommand = (cmd: string) => {
    const res = executeConsoleCommand(system, cmd);
    setSystem(res.state);
    setConsoleLog((logs) => [...logs, `> ${cmd}`, ...res.lines].slice(-500));
  };

  const handleImportConfig = (json: string) => {
    const res = applyConfigImport(system, json);
    if (res.ok) {
      setSystem(res.state);
      showToast(`Import aplicat (${res.applied.length} setări).`);
    }
    return { ok: res.ok, error: res.error, applied: res.applied, skipped: res.skipped };
  };

  const handleInjectWindow = (zoneId: number, open: boolean) => {
    setSystem((prev) =>
      tickSystemState(
        {
          ...prev,
          zones: prev.zones.map((z) =>
            z.id === zoneId
              ? { ...z, windowOpen: open, windowPauseRemainingSec: open ? 900 : 0 }
              : z,
          ),
        },
        0,
      ),
    );
    showToast(open ? 'Fereastră deschisă simulată — circuit suspendat.' : 'Fereastră închisă.');
  };

  const handleInjectSensorFault = (zoneId: number, online: boolean) => {
    setSystem((prev) =>
      tickSystemState(
        {
          ...prev,
          zones: prev.zones.map((z) =>
            z.id === zoneId
              ? { ...z, sensorOnline: online, sensorAgeSec: online ? 5 : prev.params.sensorTimeout + 30 }
              : z,
          ),
        },
        0,
      ),
    );
    showToast(
      online
        ? 'Semnal senzor BLE restabilit.'
        : 'DTC-101 activat — zona trece în regim fallback (max 30%).',
    );
  };

  const handleInjectFrostTest = (zoneId: number) => {
    setSystem((prev) =>
      tickSystemState(
        {
          ...prev,
          zones: prev.zones.map((z) => (z.id === zoneId ? { ...z, currentTemp: 10.5 } : z)),
        },
        0,
      ),
    );
    showToast('Temperatură coborâtă la 10.5°C — protecția anti-îngheț (FR-16.1) a pornit.');
  };

  const handleToggleAutoMode = (auto: boolean) => {
    setSystem((prev) => ({
      ...prev,
      boiler: { ...prev.boiler, autoMode: auto },
    }));
    showToast(auto ? 'Regim AUTO (TPI) activat.' : 'Regim MANUAL activat.');
  };

  const handleManualBoilerToggle = (on: boolean) => {
    if (on && !system.zones.some((z) => z.valveOpen)) {
      showToast('Interlock FR-2: deschide cel puțin o vană înainte de pornirea centralei.');
      return;
    }
    setSystem((prev) => {
      if (on && !prev.zones.some((z) => z.valveOpen)) {
        return prev;
      }
      return {
        ...prev,
        boiler: {
          ...prev.boiler,
          autoMode: false,
          on,
          currentBurnSec: on ? prev.boiler.currentBurnSec : 0,
          overrunRemainingSec: !on && prev.boiler.on ? prev.params.overrun : 0,
        },
      };
    });
    showToast(on ? 'Comandă radio 433 MHz: Centrală ON.' : 'Comandă radio 433 MHz: Centrală OFF.');
  };

  const handleManualRelayToggle = (zoneId: number) => {
    setSystem((prev) =>
      tickSystemState(
        {
          ...prev,
          zones: prev.zones.map((z) =>
            z.id === zoneId
              ? {
                  ...z,
                  valveOpen: !z.valveOpen,
                  valveEnergisedSec: !z.valveOpen ? prev.params.interlockDelay : 0,
                }
              : z,
          ),
        },
        0,
      ),
    );
  };

  const handleScanBle = () => {
    setSystem((prev) => ({
      ...prev,
      bleSensors: prev.bleSensors.map((s) => ({
        ...s,
        ageSec: Math.floor(Math.random() * 12) + 1,
        rssi: Math.max(-88, Math.min(-52, s.rssi + (Math.random() > 0.5 ? 1 : -1))),
      })),
    }));
    showToast('Scanare pasivă NimBLE finalizată.');
  };

  return (
    <div className="flex min-h-screen flex-col">
      <Header
        state={system}
        themeId={themeId}
        activeView={activeView}
        clockHm={clockHm}
        onThemeChange={setThemeId}
        onViewChange={setActiveView}
        onToggleAway={handleToggleAway}
      />

      <main className="mx-auto w-full max-w-[1440px] flex-1 p-3 sm:p-5">
        {activeView === 'dashboard' && (
          <div className="grid grid-cols-1 items-start gap-4 lg:grid-cols-[1fr_1.55fr_1fr]">
            <div className="order-2 min-w-0 lg:order-1">
              <LeftColumn
                state={system}
                clockHm={clockHm}
                clockSs={clockSs}
                dateRo={dateRo}
                onRefreshWeather={handleRefreshWeather}
                onOpenGasModal={() => setGasModalOpen(true)}
              />
            </div>

            <div className="order-1 flex min-w-0 flex-col gap-3 lg:order-2">
              {system.zones.map((zone) => (
                <ZoneCard
                  key={zone.id}
                  zone={zone}
                  outdoorTemp={system.weather.temp}
                  expanded={expandedZoneId === zone.id}
                  onToggleExpand={() =>
                    setExpandedZoneId((cur) => (cur === zone.id ? null : zone.id))
                  }
                  onSetTarget={(t) => handleSetZoneTarget(zone.id, t)}
                  onClearBoost={() => handleClearZoneBoost(zone.id)}
                  onResumeWindow={() => handleResumeWindow(zone.id)}
                />
              ))}
            </div>

            <div className="order-3 min-w-0">
              <RightColumn state={system} onClearEvents={handleClearEvents} />
            </div>
          </div>
        )}

        {activeView === 'schedule' && (
          <ScheduleView
            state={system}
            onToggleFlag={handleScheduleFlag}
            onUpdateLevel={handleScheduleLevel}
            onUpdateSlots={handleScheduleSlots}
            onFactoryReset={handleScheduleFactoryReset}
          />
        )}

        {activeView === 'admin' && (
          <AdminView
            state={system}
            consoleLog={consoleLog}
            onRunConsoleCommand={handleRunConsoleCommand}
            onClearConsole={() => setConsoleLog(['[CLI] Ecran curățat.'])}
            onUpdateParam={handleUpdateParam}
            onResetSafetyParams={handleResetSafetyParams}
            onImportConfig={handleImportConfig}
            onSetSimSpeed={(sp) => setSystem((prev) => ({ ...prev, simSpeed: sp }))}
            onInjectWindow={handleInjectWindow}
            onInjectSensorFault={handleInjectSensorFault}
            onInjectFrostTest={handleInjectFrostTest}
            onToggleAutoMode={handleToggleAutoMode}
            onManualBoilerToggle={handleManualBoilerToggle}
            onManualRelayToggle={handleManualRelayToggle}
            onScanBle={handleScanBle}
          />
        )}
      </main>

      {/* Subtle footer matching docs/ui_dashboard.jpg */}
      <footer
        className="mx-auto flex w-full max-w-[1440px] flex-wrap items-center justify-between gap-2 px-5 py-3 text-[11px]"
        style={{ color: 'var(--muted)' }}
      >
        <div className="font-mono-tabular flex flex-wrap items-center gap-2">
          <span>Smart Home Heating</span>
          <span>·</span>
          <span>FW v{system.firmwareVersion}</span>
          <span>·</span>
          <span>UI v{system.uiVersion}</span>
          <span>·</span>
          <span>Simulator local (×{system.simSpeed})</span>
        </div>
        <div>100% Local · ESP32-Relay X4 · Immergas Victrix Tera 24</div>
      </footer>

      <GasReadingModal
        open={gasModalOpen}
        gas={system.gas}
        onClose={() => setGasModalOpen(false)}
        onSubmit={handleGasReadingSubmit}
      />

      {/* Toast notification */}
      {toast && (
        <div
          role="status"
          aria-live="polite"
          className="fixed right-4 bottom-4 z-50 rounded-xl border px-4 py-2.5 text-xs font-semibold shadow-xl"
          style={{
            background: 'var(--surface)',
            borderColor: 'var(--heat)',
            color: 'var(--fg)',
            backdropFilter: 'blur(16px)',
          }}
        >
          {toast}
        </div>
      )}
    </div>
  );
};

export default App;
