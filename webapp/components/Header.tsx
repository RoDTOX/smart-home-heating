import React from 'react';
import {
  CalendarClock,
  Flame,
  Home,
  PauseCircle,
  Plane,
  Settings,
  Thermometer,
} from 'lucide-react';
import { THEME_ORDER, THEMES, type ThemeId } from '../engine/themes';
import { formatMmSs } from '../engine/heatingEngine';
import type { HeatingSystemState } from '../engine/types';

export type ActiveView = 'dashboard' | 'schedule' | 'admin';

interface HeaderProps {
  state: HeatingSystemState;
  themeId: ThemeId;
  activeView: ActiveView;
  clockHm: string;
  onThemeChange: (id: ThemeId) => void;
  onViewChange: (view: ActiveView) => void;
  onToggleAway: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  state,
  themeId,
  activeView,
  clockHm,
  onThemeChange,
  onViewChange,
  onToggleAway,
}) => {
  const { boiler, weather, schedule } = state;
  const boilerLabel = boiler.on
    ? 'centrală pornită'
    : boiler.interlockRemainingSec > 0
      ? `interlock ${formatMmSs(boiler.interlockRemainingSec)}`
      : boiler.antiCycleRemainingSec > 0
        ? `pauză ${formatMmSs(boiler.antiCycleRemainingSec)}`
        : 'centrală oprită';

  const levelRo =
    schedule.activeLevel === 'away'
      ? 'plecat'
      : schedule.activeLevel === 'night'
        ? 'noapte'
        : 'zi';
  const nextRo = schedule.nextLevel === 'night' ? 'noapte' : 'zi';

  return (
    <header
      className="sticky top-0 z-30 border-b px-3 py-2.5 sm:px-5"
      style={{
        background: 'var(--surface)',
        borderColor: 'var(--border)',
        backdropFilter: 'blur(var(--blur))',
        WebkitBackdropFilter: 'blur(var(--blur))',
      }}
    >
      <div className="mx-auto flex max-w-[1440px] flex-wrap items-center justify-between gap-2 lg:flex-nowrap">
        {/* Brand + Live status strip */}
        <div className="flex min-w-0 items-center gap-3">
          <button
            type="button"
            onClick={() => onViewChange('dashboard')}
            className="flex shrink-0 items-center gap-2 text-left"
            aria-label="Înapoi la panoul principal Încălzire"
          >
            <span
              className="flex h-8 w-8 shrink-0 items-center justify-center rounded-xl border"
              style={{
                background: 'var(--elev)',
                borderColor: 'var(--border)',
                color: 'var(--heat)',
              }}
            >
              <Thermometer className="h-4 w-4" aria-hidden="true" />
            </span>
            <span className="text-[15px] font-extrabold tracking-tight whitespace-nowrap">
              Încălzire
            </span>
          </button>

          {/* Compact live strip visible on xl+ */}
          <div className="hidden min-w-0 items-center gap-2.5 text-xs whitespace-nowrap xl:flex">
            <span className="font-mono-tabular font-semibold">{clockHm}</span>
            <span style={{ color: 'var(--muted)' }}>
              afară{' '}
              <strong className="font-mono-tabular" style={{ color: 'var(--fg)' }}>
                {weather.temp.toFixed(1)}°
              </strong>
            </span>
            <span className="h-3.5 w-px shrink-0" style={{ background: 'var(--border)' }} />
            <span
              className="inline-flex items-center gap-1 font-semibold"
              style={{ color: boiler.on ? 'var(--heat)' : 'var(--idle)' }}
            >
              {boiler.on ? (
                <Flame className="h-3.5 w-3.5 animate-sh-flick" aria-hidden="true" />
              ) : (
                <PauseCircle className="h-3.5 w-3.5" aria-hidden="true" />
              )}
              <span className="font-mono-tabular">{boilerLabel}</span>
            </span>
            <span className="hidden h-3.5 w-px shrink-0 2xl:block" style={{ background: 'var(--border)' }} />
            <span className="hidden truncate 2xl:inline" style={{ color: 'var(--muted)' }}>
              {schedule.away
                ? 'Mod Plecat activ'
                : `Program: ${levelRo} → ${schedule.nextSlotTime} (${nextRo})`}
            </span>
          </div>
        </div>

        {/* Right controls: Theme pills / select, Away toggle, View switcher */}
        <div className="flex w-full min-w-0 flex-wrap items-center justify-between gap-1.5 sm:w-auto lg:shrink-0 lg:flex-nowrap">
          {/* Desktop theme switcher */}
          <div
            role="group"
            aria-label="Selector temă vizuală"
            className="hidden items-center gap-0.5 rounded-xl border p-0.5 lg:flex"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
          >
            {THEME_ORDER.map((id) => {
              const active = themeId === id;
              return (
                <button
                  key={id}
                  type="button"
                  aria-pressed={active}
                  onClick={() => onThemeChange(id)}
                  className="rounded-lg px-2 py-1 text-[11px] font-semibold whitespace-nowrap transition-colors"
                  style={
                    active
                      ? { background: 'var(--heat)', color: '#16171b' }
                      : { color: 'var(--muted)' }
                  }
                >
                  {THEMES[id].label}
                </button>
              );
            })}
          </div>

          {/* Mobile / tablet theme select */}
          <select
            aria-label="Temă vizuală"
            value={themeId}
            onChange={(e) => onThemeChange(e.target.value as ThemeId)}
            className="min-h-[36px] rounded-xl border px-2.5 text-xs font-semibold lg:hidden"
            style={{
              background: 'var(--elev)',
              borderColor: 'var(--border)',
              color: 'var(--fg)',
            }}
          >
            {THEME_ORDER.map((id) => (
              <option key={id} value={id} style={{ background: '#1f2025', color: '#ececf1' }}>
                {THEMES[id].label}
              </option>
            ))}
          </select>

          {/* One-touch Away button */}
          <button
            type="button"
            aria-pressed={schedule.away}
            onClick={onToggleAway}
            className="inline-flex min-h-[36px] items-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold transition-colors"
            style={
              schedule.away
                ? {
                    background: 'var(--frost)',
                    borderColor: 'transparent',
                    color: '#16171b',
                  }
                : {
                    background: 'var(--elev)',
                    borderColor: 'var(--border)',
                    color: 'var(--fg)',
                  }
            }
          >
            <Plane className="h-3.5 w-3.5" aria-hidden="true" />
            <span>Plecat</span>
          </button>

          {/* Navigation segmented control */}
          <nav
            aria-label="Navigare principală"
            className="flex items-center gap-0.5 rounded-xl border p-0.5"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
          >
            <button
              type="button"
              aria-current={activeView === 'dashboard' ? 'page' : undefined}
              onClick={() => onViewChange('dashboard')}
              className="inline-flex min-h-[32px] items-center gap-1.5 rounded-lg px-2.5 py-1 text-xs font-semibold transition-colors"
              style={
                activeView === 'dashboard'
                  ? { background: 'var(--surface)', color: 'var(--fg)' }
                  : { color: 'var(--muted)' }
              }
            >
              <Home className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Panou</span>
            </button>
            <button
              type="button"
              aria-current={activeView === 'schedule' ? 'page' : undefined}
              onClick={() => onViewChange('schedule')}
              className="inline-flex min-h-[32px] items-center gap-1.5 rounded-lg px-2.5 py-1 text-xs font-semibold transition-colors"
              style={
                activeView === 'schedule'
                  ? { background: 'var(--surface)', color: 'var(--fg)' }
                  : { color: 'var(--muted)' }
              }
            >
              <CalendarClock className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Program</span>
            </button>
            <button
              type="button"
              aria-current={activeView === 'admin' ? 'page' : undefined}
              onClick={() => onViewChange('admin')}
              className="inline-flex min-h-[32px] items-center gap-1.5 rounded-lg px-2.5 py-1 text-xs font-semibold transition-colors"
              style={
                activeView === 'admin'
                  ? { background: 'var(--surface)', color: 'var(--fg)' }
                  : { color: 'var(--muted)' }
              }
            >
              <Settings className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Admin</span>
            </button>
          </nav>
        </div>
      </div>
    </header>
  );
};
