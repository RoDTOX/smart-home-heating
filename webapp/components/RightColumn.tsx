import React, { useState } from 'react';
import { Activity, AlertTriangle, CheckCircle2, Trash2 } from 'lucide-react';
import type { HeatingSystemState } from '../engine/types';
import { GlobalTelemetryChart } from './Charts';

interface RightColumnProps {
  state: HeatingSystemState;
  onClearEvents: () => void;
}

export const RightColumn: React.FC<RightColumnProps> = ({ state, onClearEvents }) => {
  const [range, setRange] = useState<'24h' | '5d'>('24h');
  const hasActiveAlert = state.dtcs.some((d) => d.active && d.severity !== 'ok');

  return (
    <div className="flex min-w-0 flex-col gap-3.5">
      {/* Global 24h / 5d Telemetry Chart */}
      <section className="sh-card p-4" aria-label="Telemetrie globală toate camerele">
        <div className="mb-2 flex items-center justify-between gap-2">
          <h2 className="flex items-center gap-2 text-sm font-bold">
            <Activity className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
            <span>Ultimele {range === '24h' ? '24 h' : '5 zile'} · toate camerele</span>
          </h2>
          <div
            role="group"
            aria-label="Interval grafic telemetrie"
            className="flex items-center gap-0.5 rounded-lg border p-0.5"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
          >
            {(['24h', '5d'] as const).map((r) => (
              <button
                key={r}
                type="button"
                aria-pressed={range === r}
                onClick={() => setRange(r)}
                className="rounded-md px-2 py-0.5 text-[11px] font-semibold"
                style={
                  range === r
                    ? { background: 'var(--heat)', color: '#16171b' }
                    : { color: 'var(--muted)' }
                }
              >
                {r}
              </button>
            ))}
          </div>
        </div>

        <div className="mb-2.5 flex flex-wrap items-center gap-x-3 gap-y-1 text-[11px]" style={{ color: 'var(--muted)' }}>
          <span className="flex items-center gap-1.5">
            <i className="inline-block h-1.5 w-3 rounded-xs" style={{ background: 'var(--heat)' }} />
            {state.zones[0]?.shortName}
          </span>
          <span className="flex items-center gap-1.5">
            <i className="inline-block h-1.5 w-3 rounded-xs" style={{ background: 'var(--ok)' }} />
            {state.zones[1]?.shortName}
          </span>
          <span className="flex items-center gap-1.5">
            <i className="inline-block h-1.5 w-3 rounded-xs" style={{ background: 'var(--frost)' }} />
            {state.zones[2]?.shortName}
          </span>
          <span className="flex items-center gap-1.5">
            <i className="inline-block h-1.5 w-3 rounded-xs" style={{ background: 'var(--idle)' }} />
            Afară (dreapta)
          </span>
        </div>

        <GlobalTelemetryChart range={range} zones={state.zones} history5d={state.history5d} />
      </section>

      {/* System State & DTC Diagnostics */}
      <section className="sh-card p-4" aria-label="Stare sistem și coduri diagnoză">
        <h2 className="flex items-center gap-2 text-sm font-bold">
          {hasActiveAlert ? (
            <AlertTriangle className="h-4 w-4" style={{ color: 'var(--danger)' }} aria-hidden="true" />
          ) : (
            <CheckCircle2 className="h-4 w-4" style={{ color: 'var(--ok)' }} aria-hidden="true" />
          )}
          <span>Stare sistem</span>
        </h2>

        <ul className="mt-3 divide-y" style={{ borderColor: 'var(--border)' }}>
          {state.dtcs.map((d, idx) => (
            <li
              key={`${d.code}-${idx}`}
              className="py-2.5 first:pt-0 last:pb-0"
              style={{ borderColor: 'var(--border)' }}
            >
              <div className="flex items-center justify-between gap-2">
                <div className="flex items-center gap-2">
                  <span
                    className="font-mono-tabular text-xs font-bold"
                    style={{
                      color:
                        d.severity === 'danger'
                          ? 'var(--danger)'
                          : d.severity === 'warn'
                            ? 'var(--warn)'
                            : 'var(--ok)',
                    }}
                  >
                    {d.code}
                  </span>
                  <span className="text-xs font-semibold">{d.name}</span>
                </div>
                <span className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
                  {d.active ? 'Activ' : 'Istoric'} · {d.at}
                </span>
              </div>
              <p className="mt-1 text-xs" style={{ color: 'var(--muted)' }}>
                {d.message}
              </p>
            </li>
          ))}
        </ul>

        {/* Persistent Event Log preview (FR-62) */}
        <div className="mt-4 border-t pt-3" style={{ borderColor: 'var(--border)' }}>
          <div className="mb-2 flex items-center justify-between">
            <span className="text-xs font-semibold" style={{ color: 'var(--muted)' }}>
              Jurnal evenimente ({state.events.length})
            </span>
            <button
              type="button"
              onClick={onClearEvents}
              className="inline-flex items-center gap-1 rounded-lg px-2 py-1 text-[11px] font-semibold transition-opacity hover:opacity-80"
              style={{ color: 'var(--muted)', background: 'var(--elev)' }}
              aria-label="Curăță jurnalul de evenimente"
            >
              <Trash2 className="h-3 w-3" aria-hidden="true" />
              <span>Curăță</span>
            </button>
          </div>
          <ul className="space-y-1.5 text-[11px]">
            {state.events.slice(0, 4).map((ev) => (
              <li key={ev.id} className="flex items-start gap-2">
                <span className="font-mono-tabular shrink-0" style={{ color: 'var(--muted)' }}>
                  {ev.time}
                </span>
                <span className="font-mono-tabular shrink-0 font-semibold" style={{ color: 'var(--heat)' }}>
                  {ev.code}
                </span>
                <span className="min-w-0 truncate" style={{ color: 'var(--fg)' }} title={ev.message}>
                  {ev.message}
                </span>
              </li>
            ))}
          </ul>
        </div>
      </section>
    </div>
  );
};
