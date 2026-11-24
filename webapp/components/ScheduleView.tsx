import React from 'react';
import { CalendarClock, Flame, Plus, RotateCcw, Trash2, Wind } from 'lucide-react';
import { computePreHeatLeadMinutes } from '../engine/heatingEngine';
import type { HeatingSystemState, ScheduleLevelName, ScheduleSlot } from '../engine/types';

interface ScheduleViewProps {
  state: HeatingSystemState;
  onToggleFlag: (flag: 'enabled' | 'preheat' | 'windowGuard' | 'away') => void;
  onUpdateLevel: (zoneId: number, level: ScheduleLevelName, temp: number) => void;
  onUpdateSlots: (weekend: boolean, slots: ScheduleSlot[]) => void;
  onFactoryReset: () => void;
}

const LEVEL_LABELS: Record<ScheduleLevelName, string> = {
  day: 'Zi (Confort)',
  night: 'Noapte (Somn)',
  away: 'Plecat (Eco)',
};

export const ScheduleView: React.FC<ScheduleViewProps> = ({
  state,
  onToggleFlag,
  onUpdateLevel,
  onUpdateSlots,
  onFactoryReset,
}) => {
  const { schedule, zones, weather } = state;
  const preHeatLead = computePreHeatLeadMinutes(weather.temp, weather.valid);

  const handleSlotChange = (
    weekend: boolean,
    index: number,
    patch: Partial<ScheduleSlot>,
  ) => {
    const list = weekend ? [...schedule.weekend] : [...schedule.weekday];
    list[index] = { ...list[index], ...patch };
    list.sort((a, b) => a.time.localeCompare(b.time));
    onUpdateSlots(weekend, list);
  };

  const handleAddSlot = (weekend: boolean) => {
    const list = weekend ? [...schedule.weekend] : [...schedule.weekday];
    if (list.length >= 8) return;
    list.push({ time: '14:00', level: 'day' });
    list.sort((a, b) => a.time.localeCompare(b.time));
    onUpdateSlots(weekend, list);
  };

  const handleRemoveSlot = (weekend: boolean, index: number) => {
    const list = weekend ? [...schedule.weekend] : [...schedule.weekday];
    if (list.length <= 1) return;
    list.splice(index, 1);
    onUpdateSlots(weekend, list);
  };

  return (
    <div className="space-y-4">
      {/* Header & Global Switches */}
      <section className="sh-card p-5" aria-label="Opțiuni globale program">
        <div className="flex flex-wrap items-center justify-between gap-3">
          <div>
            <h1 className="flex items-center gap-2 text-lg font-extrabold tracking-tight">
              <CalendarClock className="h-5 w-5" style={{ color: 'var(--heat)' }} aria-hidden="true" />
              <span>Programare și Niveluri de Temperatură</span>
            </h1>
            <p className="mt-0.5 text-xs" style={{ color: 'var(--muted)' }}>
              Pre-încălzire predictivă Open-Meteo activă: la {weather.temp.toFixed(1)}°C exterior,
              încălzirea pornește cu <strong className="font-mono-tabular">{preHeatLead} min</strong>{' '}
              înainte de intervalul de zi (FR-48).
            </p>
          </div>
          <button
            type="button"
            onClick={onFactoryReset}
            className="inline-flex min-h-[38px] items-center gap-1.5 rounded-xl border px-3.5 py-1.5 text-xs font-semibold"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
          >
            <RotateCcw className="h-3.5 w-3.5" aria-hidden="true" />
            <span>Resetare fabrică</span>
          </button>
        </div>

        <div className="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-4">
          {(
            [
              {
                key: 'enabled',
                label: 'Program săptămânal',
                sub: 'Aplică automat intervalele orare',
                val: schedule.enabled,
              },
              {
                key: 'preheat',
                label: 'Pre-încălzire meteo',
                sub: `Avans calculat: ${preHeatLead} min`,
                val: schedule.preheat,
              },
              {
                key: 'windowGuard',
                label: 'Detecție fereastră',
                sub: `Prag -${state.params.windowThreshold.toFixed(1)}°C`,
                val: schedule.windowGuard,
              },
              {
                key: 'away',
                label: 'Mod Plecat (Away)',
                sub: 'Coboară toate zonele la gardă',
                val: schedule.away,
              },
            ] as const
          ).map((item) => (
            <button
              key={item.key}
              type="button"
              role="switch"
              aria-checked={item.val}
              onClick={() => onToggleFlag(item.key)}
              className="flex items-center justify-between gap-3 rounded-2xl border p-3.5 text-left transition-colors"
              style={{
                background: 'var(--elev)',
                borderColor: item.val ? 'var(--heat)' : 'var(--border)',
              }}
            >
              <div>
                <div className="text-xs font-bold">{item.label}</div>
                <div className="mt-0.5 text-[11px]" style={{ color: 'var(--muted)' }}>
                  {item.sub}
                </div>
              </div>
              <span
                className="font-mono-tabular rounded-lg px-2 py-1 text-[11px] font-bold"
                style={
                  item.val
                    ? { background: 'var(--heat)', color: '#16171b' }
                    : { background: 'var(--surface)', color: 'var(--muted)' }
                }
              >
                {item.val ? 'ON' : 'OFF'}
              </span>
            </button>
          ))}
        </div>
      </section>

      {/* Per-Zone Temperature Levels */}
      <section className="sh-card p-5" aria-label="Temperaturi pe zone și niveluri">
        <h2 className="flex items-center gap-2 text-sm font-bold">
          <Flame className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
          <span>Ținte temperatură per zonă (5.0 – 28.0 °C)</span>
        </h2>

        <div className="mt-3 overflow-x-auto">
          <table className="w-full border-collapse text-left text-xs">
            <thead>
              <tr className="border-b" style={{ borderColor: 'var(--border)', color: 'var(--muted)' }}>
                <th className="py-2 pr-4 font-semibold">Zonă</th>
                <th className="px-3 py-2 font-semibold">Zi (°C)</th>
                <th className="px-3 py-2 font-semibold">Noapte (°C)</th>
                <th className="px-3 py-2 font-semibold">Plecat (°C)</th>
                <th className="px-3 py-2 font-semibold">Țintă activă</th>
              </tr>
            </thead>
            <tbody className="divide-y" style={{ borderColor: 'var(--border)' }}>
              {zones.map((z) => {
                const lvl = schedule.levels.find((l) => l.zoneId === z.id) ?? {
                  zoneId: z.id,
                  day: 23,
                  night: 21,
                  away: 16,
                };
                return (
                  <tr key={z.id} style={{ borderColor: 'var(--border)' }}>
                    <td className="py-3 pr-4">
                      <div className="font-bold">{z.name}</div>
                      <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
                        Actual {z.currentTemp.toFixed(1)}°C · {z.relayLabel}
                      </div>
                    </td>
                    {(['day', 'night', 'away'] as const).map((levelKey) => (
                      <td key={levelKey} className="px-3 py-3">
                        <div className="inline-flex items-center gap-1.5">
                          <button
                            type="button"
                            aria-label={`Scade ${LEVEL_LABELS[levelKey]} pentru ${z.name}`}
                            onClick={() => onUpdateLevel(z.id, levelKey, lvl[levelKey] - 0.5)}
                            className="flex h-8 w-8 items-center justify-center rounded-lg border font-bold"
                            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
                          >
                            −
                          </button>
                          <span className="font-mono-tabular w-12 text-center text-sm font-bold">
                            {lvl[levelKey].toFixed(1)}°
                          </span>
                          <button
                            type="button"
                            aria-label={`Crește ${LEVEL_LABELS[levelKey]} pentru ${z.name}`}
                            onClick={() => onUpdateLevel(z.id, levelKey, lvl[levelKey] + 0.5)}
                            className="flex h-8 w-8 items-center justify-center rounded-lg border font-bold"
                            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
                          >
                            +
                          </button>
                        </div>
                      </td>
                    ))}
                    <td className="px-3 py-3">
                      <span
                        className="font-mono-tabular inline-block rounded-lg px-2.5 py-1 text-xs font-bold"
                        style={{ background: 'var(--elev)', color: 'var(--heat)' }}
                      >
                        {z.targetTemp.toFixed(1)}°C
                      </span>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </section>

      {/* Weekday & Weekend Time Slots */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
        {(
          [
            { weekend: false, title: 'Luni – Vineri (Zile lucrătoare)', slots: schedule.weekday },
            { weekend: true, title: 'Sâmbătă – Duminică (Weekend)', slots: schedule.weekend },
          ] as const
        ).map((group) => (
          <section key={group.title} className="sh-card p-5" aria-label={group.title}>
            <div className="flex items-center justify-between gap-2">
              <h2 className="text-sm font-bold">{group.title}</h2>
              <button
                type="button"
                disabled={group.slots.length >= 8}
                onClick={() => handleAddSlot(group.weekend)}
                className="inline-flex min-h-[34px] items-center gap-1 rounded-xl border px-3 py-1 text-xs font-semibold disabled:opacity-40"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <Plus className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Adaugă interval</span>
              </button>
            </div>

            <div className="mt-3 space-y-2.5">
              {group.slots.map((slot, idx) => (
                <div
                  key={`${group.title}-${idx}`}
                  className="flex flex-wrap items-center justify-between gap-2 rounded-xl border p-2.5"
                  style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
                >
                  <div className="flex items-center gap-2.5">
                    <label className="sr-only" htmlFor={`slot-time-${group.weekend}-${idx}`}>
                      Ora interval
                    </label>
                    <input
                      id={`slot-time-${group.weekend}-${idx}`}
                      type="time"
                      value={slot.time}
                      onChange={(e) => handleSlotChange(group.weekend, idx, { time: e.target.value })}
                      className="font-mono-tabular rounded-lg border px-2.5 py-1.5 text-xs font-bold"
                      style={{
                        background: 'var(--surface)',
                        borderColor: 'var(--border)',
                        color: 'var(--fg)',
                      }}
                    />

                    <label className="sr-only" htmlFor={`slot-lvl-${group.weekend}-${idx}`}>
                      Nivel temperatură
                    </label>
                    <select
                      id={`slot-lvl-${group.weekend}-${idx}`}
                      value={slot.level}
                      onChange={(e) =>
                        handleSlotChange(group.weekend, idx, {
                          level: e.target.value as ScheduleLevelName,
                        })
                      }
                      className="rounded-lg border px-2.5 py-1.5 text-xs font-semibold"
                      style={{
                        background: 'var(--surface)',
                        borderColor: 'var(--border)',
                        color: 'var(--fg)',
                      }}
                    >
                      <option value="day" style={{ background: '#1f2025', color: '#ececf1' }}>
                        Zi (Confort)
                      </option>
                      <option value="night" style={{ background: '#1f2025', color: '#ececf1' }}>
                        Noapte (Somn)
                      </option>
                      <option value="away" style={{ background: '#1f2025', color: '#ececf1' }}>
                        Plecat (Eco)
                      </option>
                    </select>
                  </div>

                  <button
                    type="button"
                    disabled={group.slots.length <= 1}
                    onClick={() => handleRemoveSlot(group.weekend, idx)}
                    aria-label={`Șterge intervalul de la ${slot.time}`}
                    className="flex h-8 w-8 items-center justify-center rounded-lg disabled:opacity-35"
                    style={{ background: 'var(--surface)', color: 'var(--danger)' }}
                  >
                    <Trash2 className="h-3.5 w-3.5" aria-hidden="true" />
                  </button>
                </div>
              ))}
            </div>
            <p className="mt-3 flex items-center gap-1.5 text-[11px]" style={{ color: 'var(--muted)' }}>
              <Wind className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Maxim 8 intervale pe profil, sortate automat cronologic (FR-31).</span>
            </p>
          </section>
        ))}
      </div>
    </div>
  );
};
