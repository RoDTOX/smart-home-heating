import React, { useState } from 'react';
import {
  AlertTriangle,
  Bluetooth,
  Cpu,
  Download,
  FileJson,
  Flame,
  Radio,
  RefreshCw,
  RotateCcw,
  ShieldCheck,
  Snowflake,
  Terminal,
  Upload,
  Wind,
} from 'lucide-react';
import {
  buildConfigExport,
  DEFAULT_PARAMS,
  PARAM_SPECS,
  validateNumericParam,
} from '../engine/heatingEngine';
import type { HeatingSystemState, SystemParams } from '../engine/types';

type AdminTab = 'ble' | 'safety' | 'config' | 'console' | 'sim';

interface AdminViewProps {
  state: HeatingSystemState;
  consoleLog: string[];
  onRunConsoleCommand: (cmd: string) => void;
  onClearConsole: () => void;
  onUpdateParam: (key: keyof SystemParams, value: number | boolean) => { ok: boolean; error?: string };
  onResetSafetyParams: () => void;
  onImportConfig: (json: string) => {
    ok: boolean;
    error?: string;
    applied: string[];
    skipped: Array<{ name: string; reason: string }>;
  };
  onSetSimSpeed: (speed: 1 | 10 | 60) => void;
  onInjectWindow: (zoneId: number, open: boolean) => void;
  onInjectSensorFault: (zoneId: number, online: boolean) => void;
  onInjectFrostTest: (zoneId: number) => void;
  onToggleAutoMode: (auto: boolean) => void;
  onManualBoilerToggle: (on: boolean) => void;
  onManualRelayToggle: (zoneId: number) => void;
  onScanBle: () => void;
}

export const AdminView: React.FC<AdminViewProps> = ({
  state,
  consoleLog,
  onRunConsoleCommand,
  onClearConsole,
  onUpdateParam,
  onResetSafetyParams,
  onImportConfig,
  onSetSimSpeed,
  onInjectWindow,
  onInjectSensorFault,
  onInjectFrostTest,
  onToggleAutoMode,
  onManualBoilerToggle,
  onManualRelayToggle,
  onScanBle,
}) => {
  const [tab, setTab] = useState<AdminTab>('safety');
  const [paramDraft, setParamDraft] = useState<Record<string, string>>({});
  const [paramError, setParamError] = useState<string | null>(null);
  const [importText, setImportText] = useState('');
  const [importReport, setImportReport] = useState<{
    ok: boolean;
    error?: string;
    applied: string[];
    skipped: Array<{ name: string; reason: string }>;
  } | null>(null);
  const [cmdInput, setCmdInput] = useState('');
  const [cmdHistory, setCmdHistory] = useState<string[]>([]);
  const [histIdx, setHistIdx] = useState<number>(-1);
  const [scanningBle, setScanningBle] = useState(false);

  const handleParamSave = (key: keyof SystemParams) => {
    const raw = paramDraft[key];
    if (raw === undefined) return;
    const num = Number(raw.replace(',', '.'));
    const check = validateNumericParam(key, num);
    if (!check.ok) {
      setParamError(check.error ?? 'Valoare în afara limitelor.');
      return;
    }
    const res = onUpdateParam(key, num);
    if (!res.ok) {
      setParamError(res.error ?? 'Eroare la salvare.');
      return;
    }
    setParamError(null);
    setParamDraft((prev) => {
      const next = { ...prev };
      delete next[key];
      return next;
    });
  };

  const handleExportDownload = () => {
    const data = JSON.stringify(buildConfigExport(state), null, 2);
    const blob = new Blob([data], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    const stamp = new Date().toISOString().slice(0, 16).replace(/[-:T]/g, '');
    a.href = url;
    a.download = `smart-home-config-${stamp}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const handleLoadExportIntoEditor = () => {
    setImportText(JSON.stringify(buildConfigExport(state), null, 2));
    setImportReport(null);
  };

  const handleRunImport = () => {
    const report = onImportConfig(importText);
    setImportReport(report);
  };

  const handleConsoleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const trimmed = cmdInput.trim();
    if (!trimmed) return;
    if (trimmed.toUpperCase() === 'CLEAR') {
      onClearConsole();
    } else {
      onRunConsoleCommand(trimmed);
    }
    setCmdHistory((prev) => [trimmed, ...prev].slice(0, 30));
    setHistIdx(-1);
    setCmdInput('');
  };

  const handleConsoleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'ArrowUp' && cmdHistory.length > 0) {
      e.preventDefault();
      const nextIdx = Math.min(cmdHistory.length - 1, histIdx + 1);
      setHistIdx(nextIdx);
      setCmdInput(cmdHistory[nextIdx] ?? '');
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      if (histIdx <= 0) {
        setHistIdx(-1);
        setCmdInput('');
      } else {
        const nextIdx = histIdx - 1;
        setHistIdx(nextIdx);
        setCmdInput(cmdHistory[nextIdx] ?? '');
      }
    }
  };

  const triggerBleScan = () => {
    setScanningBle(true);
    onScanBle();
    setTimeout(() => setScanningBle(false), 1200);
  };

  const tabs: Array<{ id: AdminTab; label: string; icon: React.ReactNode }> = [
    { id: 'safety', label: 'Parametri Siguranță', icon: <ShieldCheck className="h-3.5 w-3.5" /> },
    { id: 'ble', label: 'Senzori BLE & Relee', icon: <Bluetooth className="h-3.5 w-3.5" /> },
    { id: 'sim', label: 'Simulare & Defecte', icon: <Cpu className="h-3.5 w-3.5" /> },
    { id: 'console', label: 'Terminal CLI', icon: <Terminal className="h-3.5 w-3.5" /> },
    { id: 'config', label: 'Config JSON', icon: <FileJson className="h-3.5 w-3.5" /> },
  ];

  return (
    <div className="space-y-4">
      {/* Sub-navigation bar */}
      <div className="sh-card flex flex-wrap items-center justify-between gap-3 p-4">
        <div>
          <h1 className="text-lg font-extrabold tracking-tight">
            Admin &amp; Inginerie Controler
          </h1>
          <p className="text-xs" style={{ color: 'var(--muted)' }}>
            FW v{state.firmwareVersion} · Simulator local sincronizat cu limitele §6 ale firmware-ului
            ESP32-Relay X4
          </p>
        </div>
        <div
          role="tablist"
          aria-label="Secțiuni administrare"
          className="flex flex-wrap items-center gap-1 rounded-xl border p-1"
          style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
        >
          {tabs.map((t) => {
            const active = tab === t.id;
            return (
              <button
                key={t.id}
                type="button"
                role="tab"
                aria-selected={active}
                onClick={() => setTab(t.id)}
                className="inline-flex min-h-[34px] items-center gap-1.5 rounded-lg px-3 py-1.5 text-xs font-semibold transition-colors"
                style={
                  active
                    ? { background: 'var(--heat)', color: '#16171b' }
                    : { color: 'var(--muted)' }
                }
              >
                {t.icon}
                <span>{t.label}</span>
              </button>
            );
          })}
        </div>
      </div>

      {/* TAB 1: Safety & Hydraulic Parameters */}
      {tab === 'safety' && (
        <section className="sh-card p-5" aria-label="Parametri hidraulici și de siguranță">
          <div className="flex flex-wrap items-center justify-between gap-3">
            <div>
              <h2 className="text-sm font-bold">
                Parametri Hidraulici &amp; Siguranță (ParamStore §6)
              </h2>
              <p className="mt-0.5 text-xs" style={{ color: 'var(--muted)' }}>
                Valorile din afara limitelor de siguranță sunt respinse automat fără a modifica
                configurația activă.
              </p>
            </div>
            <button
              type="button"
              onClick={() => {
                setParamDraft({});
                setParamError(null);
                onResetSafetyParams();
              }}
              className="inline-flex min-h-[36px] items-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
              style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
            >
              <RotateCcw className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Valori implicite §6</span>
            </button>
          </div>

          {paramError && (
            <div
              role="alert"
              className="mt-3 flex items-center gap-2 rounded-xl border px-3.5 py-2.5 text-xs font-semibold"
              style={{
                background: 'var(--elev)',
                borderColor: 'var(--danger)',
                color: 'var(--danger)',
              }}
            >
              <AlertTriangle className="h-4 w-4 shrink-0" aria-hidden="true" />
              <span>{paramError}</span>
            </div>
          )}

          <div className="mt-4 grid grid-cols-1 gap-3 md:grid-cols-2">
            {PARAM_SPECS.map((spec) => {
              const currentVal = state.params[spec.key] as number;
              const draftVal = paramDraft[spec.key] ?? String(currentVal);
              const dirty = paramDraft[spec.key] !== undefined && Number(draftVal) !== currentVal;

              return (
                <div
                  key={spec.key}
                  className="flex flex-col justify-between gap-2 rounded-2xl border p-3.5"
                  style={{
                    background: 'var(--elev)',
                    borderColor: dirty ? 'var(--heat)' : 'var(--border)',
                  }}
                >
                  <div>
                    <div className="flex items-center justify-between gap-2">
                      <label htmlFor={`param-${spec.key}`} className="text-xs font-bold">
                        {spec.label}
                      </label>
                      {spec.safetyCritical && (
                        <span
                          className="rounded-md px-1.5 py-0.5 text-[10px] font-bold uppercase"
                          style={{ background: 'var(--surface)', color: 'var(--warn)' }}
                        >
                          Siguranță
                        </span>
                      )}
                    </div>
                    <p className="mt-0.5 text-[11px]" style={{ color: 'var(--muted)' }}>
                      {spec.description}
                    </p>
                  </div>

                  <div className="flex items-center justify-between gap-2 pt-1">
                    <span className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
                      Interval: {spec.min} – {spec.max} {spec.unit}
                    </span>
                    <div className="flex items-center gap-1.5">
                      <input
                        id={`param-${spec.key}`}
                        type="number"
                        step={spec.step}
                        value={draftVal}
                        onChange={(e) => {
                          setParamDraft((prev) => ({ ...prev, [spec.key]: e.target.value }));
                          setParamError(null);
                        }}
                        className="font-mono-tabular w-24 rounded-lg border px-2.5 py-1.5 text-right text-xs font-bold"
                        style={{
                          background: 'var(--surface)',
                          borderColor: 'var(--border)',
                          color: 'var(--fg)',
                        }}
                      />
                      <button
                        type="button"
                        onClick={() => handleParamSave(spec.key)}
                        className="min-h-[32px] rounded-lg px-3 py-1 text-xs font-bold"
                        style={{ background: 'var(--heat)', color: '#16171b' }}
                      >
                        Aplică
                      </button>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>

          {/* Boolean switches */}
          <div className="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-3">
            {(
              [
                {
                  key: 'boilerLinkHeartbeat',
                  label: 'Heartbeat RF 433 MHz',
                  val: state.params.boilerLinkHeartbeat,
                },
                {
                  key: 'openLoopBypass',
                  label: 'Bypass buclă deschisă',
                  val: state.params.openLoopBypass,
                },
                {
                  key: 'pumpControlEnabled',
                  label: 'Pompă auxiliară (Releu 4)',
                  val: state.params.pumpControlEnabled,
                },
              ] as const
            ).map((sw) => (
              <button
                key={sw.key}
                type="button"
                role="switch"
                aria-checked={sw.val}
                onClick={() => onUpdateParam(sw.key, !sw.val)}
                className="flex items-center justify-between rounded-xl border p-3 text-left text-xs font-semibold"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <span>{sw.label}</span>
                <span
                  className="font-mono-tabular rounded-md px-2 py-0.5 text-[11px] font-bold"
                  style={
                    sw.val
                      ? { background: 'var(--ok)', color: '#16171b' }
                      : { background: 'var(--surface)', color: 'var(--muted)' }
                  }
                >
                  {sw.val ? 'ACTIV' : 'OPRIT'}
                </span>
              </button>
            ))}
          </div>
        </section>
      )}

      {/* TAB 2: BLE Sensors & Relays */}
      {tab === 'ble' && (
        <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
          <section className="sh-card p-5" aria-label="Senzori Xiaomi BLE">
            <div className="flex items-center justify-between gap-2">
              <h2 className="flex items-center gap-2 text-sm font-bold">
                <Bluetooth className="h-4 w-4" style={{ color: 'var(--frost)' }} aria-hidden="true" />
                <span>Senzori Xiaomi BLE (NimBLE Passive Scan)</span>
              </h2>
              <button
                type="button"
                onClick={triggerBleScan}
                className="inline-flex min-h-[34px] items-center gap-1.5 rounded-xl border px-3 py-1 text-xs font-semibold"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <RefreshCw className={`h-3.5 w-3.5 ${scanningBle ? 'animate-spin' : ''}`} />
                <span>{scanningBle ? 'Scanez...' : 'Scanează BLE'}</span>
              </button>
            </div>

            <div className="mt-3 overflow-x-auto">
              <table className="w-full border-collapse text-left text-xs">
                <thead>
                  <tr className="border-b" style={{ borderColor: 'var(--border)', color: 'var(--muted)' }}>
                    <th className="py-2 pr-3">Senzor / MAC</th>
                    <th className="px-2 py-2">Temp</th>
                    <th className="px-2 py-2">Bat</th>
                    <th className="px-2 py-2">RSSI</th>
                    <th className="px-2 py-2">Zonă</th>
                  </tr>
                </thead>
                <tbody className="divide-y" style={{ borderColor: 'var(--border)' }}>
                  {state.bleSensors.map((s) => (
                    <tr key={s.mac} style={{ borderColor: 'var(--border)' }}>
                      <td className="py-2.5 pr-3">
                        <div className="font-semibold">{s.name}</div>
                        <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
                          {s.mac}
                        </div>
                      </td>
                      <td className="font-mono-tabular px-2 py-2.5 font-bold">
                        {s.temp.toFixed(1)}°C
                      </td>
                      <td className="font-mono-tabular px-2 py-2.5">{s.bat}%</td>
                      <td className="font-mono-tabular px-2 py-2.5">{s.rssi} dBm</td>
                      <td className="px-2 py-2.5">
                        {s.zoneId > 0 ? (
                          <span
                            className="font-mono-tabular rounded-md px-2 py-0.5 text-[11px] font-bold"
                            style={{ background: 'var(--elev)', color: 'var(--ok)' }}
                          >
                            Z{s.zoneId}
                          </span>
                        ) : (
                          <span className="text-[11px]" style={{ color: 'var(--muted)' }}>
                            Neasociat
                          </span>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </section>

          <section className="sh-card p-5" aria-label="Matrice Relee și Test Manual">
            <div className="flex items-center justify-between gap-2">
              <h2 className="flex items-center gap-2 text-sm font-bold">
                <Radio className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
                <span>Relee ESP32-Relay X4 &amp; Test Manual</span>
              </h2>
              <span className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
                Actuatori: {state.params.actuatorNormallyClosed ? 'NC (230V)' : 'NO'}
              </span>
            </div>

            <div className="mt-3 flex items-center justify-between rounded-xl border p-3 text-xs" style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}>
              <div>
                <div className="font-bold">Mod operare controler</div>
                <div style={{ color: 'var(--muted)' }}>
                  Testul manual al releelor este permis doar în modul MANUAL (FR-43).
                </div>
              </div>
              <div className="flex gap-1">
                <button
                  type="button"
                  aria-pressed={state.boiler.autoMode}
                  onClick={() => onToggleAutoMode(true)}
                  className="rounded-lg px-2.5 py-1 text-xs font-bold"
                  style={
                    state.boiler.autoMode
                      ? { background: 'var(--heat)', color: '#16171b' }
                      : { background: 'var(--surface)', color: 'var(--muted)' }
                  }
                >
                  AUTO
                </button>
                <button
                  type="button"
                  aria-pressed={!state.boiler.autoMode}
                  onClick={() => onToggleAutoMode(false)}
                  className="rounded-lg px-2.5 py-1 text-xs font-bold"
                  style={
                    !state.boiler.autoMode
                      ? { background: 'var(--warn)', color: '#16171b' }
                      : { background: 'var(--surface)', color: 'var(--muted)' }
                  }
                >
                  MANUAL
                </button>
              </div>
            </div>

            <div className="mt-3 space-y-2">
              {state.zones.map((z) => (
                <div
                  key={z.id}
                  className="flex items-center justify-between rounded-xl border px-3.5 py-2.5 text-xs"
                  style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
                >
                  <div>
                    <div className="font-bold">
                      {z.name}{' '}
                      <span className="font-mono-tabular font-normal" style={{ color: 'var(--muted)' }}>
                        ({z.relayLabel})
                      </span>
                    </div>
                    <div className="text-[11px]" style={{ color: 'var(--muted)' }}>
                      {z.circuits} circuite · TPI bază {z.learnedDuty.toFixed(1)}%
                    </div>
                  </div>
                  <button
                    type="button"
                    disabled={state.boiler.autoMode}
                    onClick={() => onManualRelayToggle(z.id)}
                    className="font-mono-tabular min-h-[34px] rounded-lg px-3 py-1 text-xs font-bold disabled:opacity-45"
                    style={
                      z.valveOpen
                        ? { background: 'var(--heat)', color: '#16171b' }
                        : { background: 'var(--surface)', color: 'var(--fg)' }
                    }
                  >
                    {z.valveOpen ? 'DESCHIS' : 'ÎNCHIS'}
                  </button>
                </div>
              ))}
            </div>
          </section>
        </div>
      )}

      {/* TAB 3: Simulator & Fault Injector */}
      {tab === 'sim' && (
        <section className="sh-card p-5" aria-label="Simulator termic și injecție defecte">
          <div className="flex flex-wrap items-center justify-between gap-3">
            <div>
              <h2 className="text-sm font-bold">Simulator Fizic &amp; Fault Injector</h2>
              <p className="mt-0.5 text-xs" style={{ color: 'var(--muted)' }}>
                Testează comportamentul protecțiilor hidraulice (interlock, overrun, DTC-101,
                fereastră deschisă, anti-îngheț) fără risc pe instalația reală.
              </p>
            </div>
            <div className="flex items-center gap-1.5">
              <span className="text-xs font-semibold" style={{ color: 'var(--muted)' }}>
                Viteză timp:
              </span>
              {([1, 10, 60] as const).map((sp) => (
                <button
                  key={sp}
                  type="button"
                  aria-pressed={state.simSpeed === sp}
                  onClick={() => onSetSimSpeed(sp)}
                  className="font-mono-tabular min-h-[34px] rounded-lg border px-3 py-1 text-xs font-bold"
                  style={
                    state.simSpeed === sp
                      ? { background: 'var(--heat)', borderColor: 'transparent', color: '#16171b' }
                      : { background: 'var(--elev)', borderColor: 'var(--border)' }
                  }
                >
                  ×{sp}
                </button>
              ))}
            </div>
          </div>

          <div className="mt-4 grid grid-cols-1 gap-3 md:grid-cols-3">
            {state.zones.map((z) => (
              <div
                key={z.id}
                className="flex flex-col gap-2.5 rounded-2xl border p-3.5"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <div className="flex items-center justify-between">
                  <span className="text-xs font-bold">{z.name}</span>
                  <span className="font-mono-tabular text-xs font-bold">
                    {z.currentTemp.toFixed(1)}°C
                  </span>
                </div>

                <button
                  type="button"
                  onClick={() => onInjectWindow(z.id, !z.windowOpen)}
                  className="inline-flex min-h-[36px] items-center justify-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
                  style={
                    z.windowOpen
                      ? { background: 'var(--warn)', borderColor: 'transparent', color: '#16171b' }
                      : { background: 'var(--surface)', borderColor: 'var(--border)' }
                  }
                >
                  <Wind className="h-3.5 w-3.5" aria-hidden="true" />
                  <span>{z.windowOpen ? 'Închide fereastra' : 'Simulează fereastră deschisă'}</span>
                </button>

                <button
                  type="button"
                  onClick={() => onInjectSensorFault(z.id, !z.sensorOnline)}
                  className="inline-flex min-h-[36px] items-center justify-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
                  style={
                    !z.sensorOnline
                      ? { background: 'var(--danger)', borderColor: 'transparent', color: '#ffffff' }
                      : { background: 'var(--surface)', borderColor: 'var(--border)' }
                  }
                >
                  <AlertTriangle className="h-3.5 w-3.5" aria-hidden="true" />
                  <span>
                    {z.sensorOnline ? 'Simulează pierdere senzor (DTC-101)' : 'Restabilește senzor'}
                  </span>
                </button>

                <button
                  type="button"
                  onClick={() => onInjectFrostTest(z.id)}
                  className="inline-flex min-h-[36px] items-center justify-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
                  style={{ background: 'var(--surface)', borderColor: 'var(--border)', color: 'var(--frost)' }}
                >
                  <Snowflake className="h-3.5 w-3.5" aria-hidden="true" />
                  <span>Test anti-îngheț (10.5°C)</span>
                </button>
              </div>
            ))}
          </div>

          {/* Direct Boiler Control (Manual mode) */}
          <div
            className="mt-4 flex flex-wrap items-center justify-between gap-3 rounded-2xl border p-3.5"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
          >
            <div className="flex items-center gap-2 text-xs">
              <Flame className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
              <span>
                Comandă directă centrală (433.92 MHz OOK):{' '}
                <strong>{state.boiler.on ? 'PORNITĂ' : 'OPRITĂ'}</strong>
              </span>
            </div>
            <div className="flex gap-2">
              <button
                type="button"
                onClick={() => onManualBoilerToggle(true)}
                className="min-h-[36px] rounded-xl px-3.5 py-1.5 text-xs font-bold"
                style={{ background: 'var(--heat)', color: '#16171b' }}
              >
                Comandă ON
              </button>
              <button
                type="button"
                onClick={() => onManualBoilerToggle(false)}
                className="min-h-[36px] rounded-xl border px-3.5 py-1.5 text-xs font-bold"
                style={{ background: 'var(--surface)', borderColor: 'var(--border)' }}
              >
                Comandă OFF
              </button>
            </div>
          </div>
        </section>
      )}

      {/* TAB 4: Virtual Firmware Console CLI */}
      {tab === 'console' && (
        <section className="sh-card p-5" aria-label="Terminal virtual consolă firmware">
          <div className="flex flex-wrap items-center justify-between gap-2">
            <h2 className="flex items-center gap-2 text-sm font-bold">
              <Terminal className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
              <span>Consolă Virtuală ESP32 (Telnet :23 / HTTP CLI)</span>
            </h2>
            <div className="flex flex-wrap gap-1.5">
              {['HELP', 'STATUS', 'ZONES', 'DIAG', 'BLE', 'CONFIG', 'EVENTS'].map((c) => (
                <button
                  key={c}
                  type="button"
                  onClick={() => onRunConsoleCommand(c)}
                  className="font-mono-tabular rounded-lg border px-2 py-1 text-[11px] font-semibold"
                  style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
                >
                  {c}
                </button>
              ))}
              <button
                type="button"
                onClick={onClearConsole}
                className="font-mono-tabular rounded-lg border px-2 py-1 text-[11px] font-semibold"
                style={{ background: 'var(--surface)', borderColor: 'var(--border)', color: 'var(--muted)' }}
              >
                CLEAR
              </button>
            </div>
          </div>

          <div
            className="font-mono-tabular mt-3 h-72 overflow-y-auto rounded-xl border p-3.5 text-xs leading-relaxed"
            style={{
              background: '#0b0c10',
              borderColor: 'var(--border)',
              color: '#d9e3f0',
            }}
            aria-live="polite"
          >
            {consoleLog.map((line, i) => (
              <div
                key={i}
                style={{
                  color: line.startsWith('>')
                    ? 'var(--heat)'
                    : line.startsWith('[ERR]')
                      ? 'var(--danger)'
                      : line.startsWith('[OK]')
                        ? 'var(--ok)'
                        : undefined,
                }}
              >
                {line}
              </div>
            ))}
          </div>

          <form onSubmit={handleConsoleSubmit} className="mt-3 flex gap-2">
            <label htmlFor="cli-input" className="sr-only">
              Comandă consolă
            </label>
            <input
              id="cli-input"
              type="text"
              value={cmdInput}
              onChange={(e) => setCmdInput(e.target.value)}
              onKeyDown={handleConsoleKeyDown}
              placeholder="Scrie o comandă (ex: STATUS, ZONES, DIAG, HELP)..."
              className="font-mono-tabular flex-1 rounded-xl border px-3.5 py-2.5 text-xs font-medium"
              style={{
                background: 'var(--elev)',
                borderColor: 'var(--border)',
                color: 'var(--fg)',
              }}
            />
            <button
              type="submit"
              className="min-h-[40px] rounded-xl px-4 py-2 text-xs font-bold"
              style={{ background: 'var(--heat)', color: '#16171b' }}
            >
              Trimite
            </button>
          </form>
        </section>
      )}

      {/* TAB 5: Config JSON Export / Import */}
      {tab === 'config' && (
        <section className="sh-card p-5" aria-label="Export și import configurație JSON">
          <div className="flex flex-wrap items-center justify-between gap-3">
            <div>
              <h2 className="text-sm font-bold">Export / Import Configurație (Format 1 · FR-65)</h2>
              <p className="mt-0.5 text-xs" style={{ color: 'var(--muted)' }}>
                Credențialele, contoarele și stările runtime nu sunt incluse niciodată în export.
              </p>
            </div>
            <div className="flex flex-wrap gap-2">
              <button
                type="button"
                onClick={handleLoadExportIntoEditor}
                className="inline-flex min-h-[36px] items-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <FileJson className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Încarcă configurația curentă</span>
              </button>
              <button
                type="button"
                onClick={handleExportDownload}
                className="inline-flex min-h-[36px] items-center gap-1.5 rounded-xl px-3.5 py-1.5 text-xs font-bold"
                style={{ background: 'var(--heat)', color: '#16171b' }}
              >
                <Download className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Descarcă JSON</span>
              </button>
            </div>
          </div>

          <div className="mt-4">
            <label htmlFor="config-json-area" className="mb-1.5 block text-xs font-semibold">
              JSON pentru import (validat pe limite §6)
            </label>
            <textarea
              id="config-json-area"
              rows={10}
              value={importText}
              onChange={(e) => setImportText(e.target.value)}
              placeholder='Lipește aici un fișier JSON format 1 (sau apasă "Încarcă configurația curentă" pentru test)...'
              className="font-mono-tabular w-full rounded-xl border p-3 text-xs"
              style={{
                background: 'var(--elev)',
                borderColor: 'var(--border)',
                color: 'var(--fg)',
              }}
            />
            <div className="mt-2.5 flex items-center justify-end">
              <button
                type="button"
                onClick={handleRunImport}
                className="inline-flex min-h-[38px] items-center gap-1.5 rounded-xl px-4 py-2 text-xs font-bold"
                style={{ background: 'var(--heat)', color: '#16171b' }}
              >
                <Upload className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Validează și aplică import</span>
              </button>
            </div>
          </div>

          {importReport && (
            <div
              role="status"
              className="mt-3 rounded-xl border p-3.5 text-xs"
              style={{
                background: 'var(--elev)',
                borderColor: importReport.ok ? 'var(--ok)' : 'var(--danger)',
              }}
            >
              {!importReport.ok ? (
                <div className="font-semibold" style={{ color: 'var(--danger)' }}>
                  {importReport.error}
                </div>
              ) : (
                <div className="space-y-1">
                  <div className="font-bold" style={{ color: 'var(--ok)' }}>
                    Import aplicat: {importReport.applied.length} setări actualizate,{' '}
                    {importReport.skipped.length} omise.
                  </div>
                  {importReport.skipped.length > 0 && (
                    <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--warn)' }}>
                      Omise:{' '}
                      {importReport.skipped.map((s) => `${s.name} (${s.reason})`).join(', ')}
                    </div>
                  )}
                </div>
              )}
            </div>
          )}
        </section>
      )}
    </div>
  );
};
