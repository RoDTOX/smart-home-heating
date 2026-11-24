import React, { useRef } from 'react';
import {
  AlertTriangle,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  Flame,
  Minus,
  Plus,
  RotateCcw,
  Snowflake,
  Wind,
} from 'lucide-react';
import {
  computeHeatUpEta,
  formatDurationRo,
  formatMmSs,
  TARGET_MAX_C,
  TARGET_MIN_C,
} from '../engine/heatingEngine';
import type { ZoneState } from '../engine/types';
import { ZoneMiniChart } from './Charts';

interface ZoneCardProps {
  zone: ZoneState;
  outdoorTemp: number;
  expanded: boolean;
  onToggleExpand: () => void;
  onSetTarget: (target: number) => void;
  onClearBoost: () => void;
  onResumeWindow: () => void;
}

const PRESETS: Array<{ id: string; label: string; temp: number; frost?: boolean }> = [
  { id: 'comfort', label: 'Confort', temp: 24.0 },
  { id: 'sleep', label: 'Somn', temp: 21.5 },
  { id: 'eco', label: 'Eco', temp: 20.0 },
  { id: 'frost', label: 'Anti-îngheț', temp: 12.0, frost: true },
];

function polar(r: number, deg: number): [number, number] {
  const rad = ((deg - 90) * Math.PI) / 180;
  return [144 + r * Math.cos(rad), 144 + r * Math.sin(rad)];
}

function describeArc(a0: number, a1: number): string {
  const s = polar(116, a1);
  const e = polar(116, a0);
  const large = a1 - a0 <= 180 ? 0 : 1;
  return `M ${s[0].toFixed(2)} ${s[1].toFixed(2)} A 116 116 0 ${large} 0 ${e[0].toFixed(2)} ${e[1].toFixed(2)}`;
}

function getRatio(temp: number): number {
  return Math.max(0, Math.min(1, (temp - TARGET_MIN_C) / (TARGET_MAX_C - TARGET_MIN_C)));
}

function getStatusVisual(zone: ZoneState): {
  accentVar: string;
  label: string;
  icon: 'flame' | 'wind' | 'alert' | 'snow' | 'check';
} {
  if (zone.frostActive) {
    return { accentVar: 'var(--frost)', label: 'Protecție anti-îngheț', icon: 'snow' };
  }
  if (!zone.enabled) {
    return { accentVar: 'var(--muted)', label: 'Zonă dezactivată', icon: 'check' };
  }
  if (!zone.sensorOnline) {
    return { accentVar: 'var(--danger)', label: 'Senzor fără semnal (fallback)', icon: 'alert' };
  }
  if (zone.windowOpen) {
    return { accentVar: 'var(--warn)', label: 'Fereastră deschisă', icon: 'wind' };
  }
  if (zone.valveOpen) {
    const dutyPct = Math.round(zone.duty * 100);
    const onMin = Math.round(zone.duty * 60);
    return {
      accentVar: 'var(--heat)',
      label: `Încălzire TPI · ${onMin} min/h (${dutyPct}%)`,
      icon: 'flame',
    };
  }
  return { accentVar: 'var(--idle)', label: 'Temperatură atinsă', icon: 'check' };
}

function StatusIcon({ kind }: { kind: 'flame' | 'wind' | 'alert' | 'snow' | 'check' }) {
  switch (kind) {
    case 'flame':
      return <Flame className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />;
    case 'wind':
      return <Wind className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />;
    case 'alert':
      return <AlertTriangle className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />;
    case 'snow':
      return <Snowflake className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />;
    default:
      return <CheckCircle2 className="h-3.5 w-3.5 shrink-0" aria-hidden="true" />;
  }
}

export const ZoneCard: React.FC<ZoneCardProps> = ({
  zone,
  outdoorTemp,
  expanded,
  onToggleExpand,
  onSetTarget,
  onClearBoost,
  onResumeWindow,
}) => {
  const dialRef = useRef<SVGSVGElement | null>(null);
  const draggingRef = useRef(false);

  const vis = getStatusVisual(zone);
  const eta = computeHeatUpEta(zone, outdoorTemp);
  const ringPct = Math.max(4, Math.min(100, Math.round(getRatio(zone.currentTemp) * 100)));
  const ringCircumference = 149.23;
  const ringDashOffset = ringCircumference * (1 - ringPct / 100);

  const tr = getRatio(zone.targetTemp);
  const targetAngle = 225 + tr * 270;
  const thumbPos = polar(116, targetAngle);
  const curPos = polar(116, 225 + getRatio(zone.currentTemp) * 270);
  const pMin = polar(132, 225);
  const pMax = polar(132, 495);

  const updateFromPointer = (clientX: number, clientY: number) => {
    const svg = dialRef.current;
    if (!svg) return;
    const rect = svg.getBoundingClientRect();
    const px = clientX - (rect.left + rect.width / 2);
    const py = clientY - (rect.top + rect.height / 2);
    let angle = (Math.atan2(px, -py) * 180) / Math.PI;
    if (angle < 0) angle += 360;
    let rel = angle - 225;
    if (rel < 0) rel += 360;
    if (rel > 270) rel = rel > 315 ? 0 : 270;
    const rawTemp = TARGET_MIN_C + (rel / 270) * (TARGET_MAX_C - TARGET_MIN_C);
    const stepped = Math.round(rawTemp * 2) / 2;
    onSetTarget(stepped);
  };

  const handleDialPointerDown = (e: React.PointerEvent<SVGSVGElement>) => {
    draggingRef.current = true;
    e.currentTarget.setPointerCapture(e.pointerId);
    updateFromPointer(e.clientX, e.clientY);
  };

  const handleDialPointerMove = (e: React.PointerEvent<SVGSVGElement>) => {
    if (!draggingRef.current) return;
    updateFromPointer(e.clientX, e.clientY);
  };

  const handleDialPointerUp = () => {
    draggingRef.current = false;
  };

  const handleDialKeyDown = (e: React.KeyboardEvent<SVGSVGElement>) => {
    if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
      e.preventDefault();
      onSetTarget(zone.targetTemp + 0.5);
    } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
      e.preventDefault();
      onSetTarget(zone.targetTemp - 0.5);
    }
  };

  return (
    <section
      className={`sh-card relative overflow-hidden p-0 ${zone.valveOpen ? 'sh-zone-heating' : ''}`}
      style={{ '--zone-acc': vis.accentVar } as React.CSSProperties}
      aria-label={`Zonă ${zone.name}`}
    >
      {/* Subtle warm glow wave at card base while heating */}
      {zone.valveOpen && (
        <div
          className="pointer-events-none absolute right-0 bottom-0 left-0 h-14 overflow-hidden opacity-45"
          aria-hidden="true"
        >
          <svg
            viewBox="0 0 800 80"
            preserveAspectRatio="none"
            className="animate-sh-wave block h-full w-[200%]"
          >
            <defs>
              <linearGradient id={`wv-${zone.key}`} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0" stopColor="var(--heat)" stopOpacity="0" />
                <stop offset="1" stopColor="var(--heat)" stopOpacity="0.48" />
              </linearGradient>
            </defs>
            <path
              d="M0,36 C100,10 200,62 300,36 C400,10 500,62 600,36 C700,10 750,50 800,36 L800,80 L0,80 Z"
              fill={`url(#wv-${zone.key})`}
            />
          </svg>
        </div>
      )}

      {/* Collapsed / Accordion Header Button */}
      <button
        type="button"
        onClick={onToggleExpand}
        aria-expanded={expanded}
        aria-label={`${zone.name}, ${zone.currentTemp.toFixed(1)} grade Celsius, ${vis.label}`}
        className="relative z-10 flex min-h-[92px] w-full items-center justify-between gap-3 px-4 py-3 text-left transition-colors"
      >
        <div className="flex min-w-0 items-center gap-3.5">
          {/* Circular progress ring */}
          <svg
            width="50"
            height="50"
            viewBox="0 0 52 52"
            className="shrink-0 -rotate-90"
            aria-hidden="true"
          >
            <circle cx="26" cy="26" r="23.75" fill="none" stroke="var(--border)" strokeWidth="4.5" />
            <circle
              cx="26"
              cy="26"
              r="23.75"
              fill="none"
              stroke={vis.accentVar}
              strokeWidth="4.5"
              strokeLinecap="round"
              strokeDasharray={ringCircumference}
              strokeDashoffset={ringDashOffset}
              style={{ transition: 'stroke-dashoffset 0.25s cubic-bezier(0.16, 1, 0.3, 1)' }}
            />
          </svg>

          <div className="min-w-0">
            <div className="flex items-center gap-2">
              <h3 className="truncate text-[17px] font-bold tracking-tight">{zone.name}</h3>
              {zone.isBoost && (
                <span
                  className="rounded-md px-1.5 py-0.5 text-[10px] font-bold uppercase tracking-wide"
                  style={{ background: 'var(--elev)', color: 'var(--heat)' }}
                >
                  Boost
                </span>
              )}
            </div>
            <div
              className="font-mono-tabular mt-0.5 flex flex-wrap items-center gap-x-2 gap-y-0.5 text-xs"
              style={{ color: 'var(--muted)' }}
            >
              <span>Umiditate {zone.humidity}%</span>
              <span aria-hidden="true">·</span>
              <span>Țintă {zone.targetTemp.toFixed(1)}°C</span>
              <span aria-hidden="true">·</span>
              <span>{zone.circuits} circuite</span>
            </div>
            <div
              className="mt-1 inline-flex items-center gap-1.5 text-xs font-semibold"
              style={{ color: vis.accentVar }}
            >
              <StatusIcon kind={vis.icon} />
              <span>{vis.label}</span>
            </div>
          </div>
        </div>

        <div className="flex shrink-0 items-center gap-2">
          <div className="font-mono-tabular text-[38px] leading-none font-bold tracking-tight">
            {zone.currentTemp.toFixed(1)}
            <span className="ml-0.5 align-top text-lg" style={{ color: 'var(--muted)' }}>
              °
            </span>
          </div>
          <span
            className="flex h-8 w-8 items-center justify-center"
            style={{ color: 'var(--muted)' }}
            aria-hidden="true"
          >
            {expanded ? <ChevronUp className="h-5 w-5" /> : <ChevronDown className="h-5 w-5" />}
          </span>
        </div>
      </button>

      {/* Expanded Body */}
      {expanded && (
        <div
          className="relative z-10 flex flex-col gap-4 border-t px-4 pt-4 pb-5"
          style={{ borderColor: 'var(--border)' }}
        >
          {/* Open Window Radar Alert Banner */}
          {zone.windowOpen && (
            <div
              role="alert"
              className="flex flex-wrap items-center justify-between gap-3 rounded-xl px-3.5 py-2.5"
              style={{ background: 'var(--elev)', border: '1px solid var(--warn)' }}
            >
              <div className="flex items-center gap-3">
                <div
                  className="relative flex h-10 w-10 shrink-0 items-center justify-center"
                  style={{ color: 'var(--warn)' }}
                >
                  <span
                    className="animate-sh-radar absolute inset-0 rounded-full border-2"
                    style={{ borderColor: 'var(--warn)' }}
                  />
                  <span
                    className="animate-sh-radar-delay absolute inset-0 rounded-full border"
                    style={{ borderColor: 'var(--warn)' }}
                  />
                  <Wind className="h-5 w-5" aria-hidden="true" />
                </div>
                <div>
                  <div className="text-sm font-bold" style={{ color: 'var(--warn)' }}>
                    Fereastră deschisă detectată
                  </div>
                  <div className="font-mono-tabular text-xs" style={{ color: 'var(--muted)' }}>
                    Circuitul {zone.relayLabel} în pauză ({formatMmSs(zone.windowPauseRemainingSec)})
                  </div>
                </div>
              </div>
              <button
                type="button"
                onClick={onResumeWindow}
                className="min-h-[40px] rounded-xl px-3.5 py-2 text-xs font-bold whitespace-nowrap"
                style={{ background: 'var(--warn)', color: '#16171b' }}
              >
                Reia încălzirea
              </button>
            </div>
          )}

          {/* 270-degree Radial Thermostat Dial */}
          <div className="flex flex-col items-center select-none">
            <div className="relative h-[248px] w-[248px] sm:h-[268px] sm:w-[268px]">
              <svg
                ref={dialRef}
                viewBox="0 0 288 288"
                role="slider"
                tabIndex={0}
                aria-label={`Termostat ${zone.name}`}
                aria-valuemin={TARGET_MIN_C}
                aria-valuemax={TARGET_MAX_C}
                aria-valuenow={zone.targetTemp}
                aria-valuetext={`${zone.targetTemp.toFixed(1)} grade Celsius`}
                onPointerDown={handleDialPointerDown}
                onPointerMove={handleDialPointerMove}
                onPointerUp={handleDialPointerUp}
                onPointerCancel={handleDialPointerUp}
                onKeyDown={handleDialKeyDown}
                className="block h-full w-full cursor-pointer overflow-visible touch-none"
              >
                <defs>
                  <linearGradient id={`dg-${zone.key}`} x1="0" y1="1" x2="1" y2="0">
                    <stop offset="0" stopColor="var(--frost)" />
                    <stop offset="0.45" stopColor="var(--idle)" />
                    <stop offset="1" stopColor={vis.accentVar} />
                  </linearGradient>
                </defs>

                {/* Radial ticks */}
                <g>
                  {Array.from({ length: 46 }).map((_, i) => {
                    const a = 225 + (i / 45) * 270;
                    const p = polar(96, a);
                    const q = polar(102, a);
                    const active = i / 45 <= tr;
                    return (
                      <line
                        key={i}
                        x1={p[0].toFixed(1)}
                        y1={p[1].toFixed(1)}
                        x2={q[0].toFixed(1)}
                        y2={q[1].toFixed(1)}
                        stroke={active ? vis.accentVar : 'var(--border)'}
                        strokeWidth={active ? 1.6 : 1.2}
                        strokeLinecap="round"
                        opacity={active ? 0.9 : 0.65}
                      />
                    );
                  })}
                </g>

                {/* Track & Value Arc */}
                <path
                  d={describeArc(225, 495)}
                  fill="none"
                  stroke="var(--border)"
                  strokeWidth={11}
                  strokeLinecap="round"
                />
                {tr > 0.005 && (
                  <path
                    d={describeArc(225, targetAngle)}
                    fill="none"
                    stroke={`url(#dg-${zone.key})`}
                    strokeWidth={11}
                    strokeLinecap="round"
                  />
                )}

                <text
                  x={pMin[0].toFixed(1)}
                  y={(pMin[1] + 12).toFixed(1)}
                  textAnchor="middle"
                  fontSize="11"
                  fill="var(--muted)"
                  className="font-mono-tabular"
                >
                  {TARGET_MIN_C}°
                </text>
                <text
                  x={pMax[0].toFixed(1)}
                  y={(pMax[1] + 12).toFixed(1)}
                  textAnchor="middle"
                  fontSize="11"
                  fill="var(--muted)"
                  className="font-mono-tabular"
                >
                  {TARGET_MAX_C}°
                </text>

                {/* Current room temp dot */}
                <circle cx={curPos[0]} cy={curPos[1]} r="4.5" fill="var(--fg)" opacity="0.85" />
                {/* Draggable Target Thumb */}
                <circle
                  cx={thumbPos[0]}
                  cy={thumbPos[1]}
                  r="11"
                  fill="var(--surface)"
                  stroke={vis.accentVar}
                  strokeWidth="4"
                />
              </svg>

              {/* Center Dial Readout */}
              <div className="pointer-events-none absolute inset-0 flex flex-col items-center justify-center px-8 text-center">
                <span className="text-[11px] font-medium tracking-wide" style={{ color: 'var(--muted)' }}>
                  Actual
                </span>
                <div className="font-mono-tabular text-[52px] leading-none font-bold tracking-tight sm:text-[58px]">
                  {zone.currentTemp.toFixed(1)}
                  <span className="ml-0.5 align-top text-xl" style={{ color: 'var(--muted)' }}>
                    °
                  </span>
                </div>
                <div className="mt-1.5 flex items-center gap-1.5 text-sm font-semibold">
                  <span style={{ color: 'var(--muted)' }}>Țintă</span>
                  <strong className="font-mono-tabular" style={{ color: vis.accentVar }}>
                    {zone.targetTemp.toFixed(1)}°C
                  </strong>
                </div>
                <div className="mt-0.5 text-xs font-medium" style={{ color: vis.accentVar }}>
                  {zone.windowOpen
                    ? 'Încălzire suspendată'
                    : !zone.sensorOnline
                      ? 'Fallback fără senzor'
                      : eta.label}
                </div>
              </div>
            </div>

            {/* Fine Stepper Controls */}
            <div className="mt-1 flex items-center gap-4">
              <button
                type="button"
                onClick={() => onSetTarget(zone.targetTemp - 0.5)}
                aria-label={`Scade temperatura țintă pentru ${zone.name}`}
                className="flex h-12 w-12 items-center justify-center rounded-xl border transition-transform active:scale-95"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <Minus className="h-5 w-5" aria-hidden="true" />
              </button>
              <div className="min-w-[96px] text-center">
                <span className="block text-[11px]" style={{ color: 'var(--muted)' }}>
                  Reglaj fin
                </span>
                <strong className="font-mono-tabular block text-base font-bold">
                  {zone.targetTemp.toFixed(1)} °C
                </strong>
              </div>
              <button
                type="button"
                onClick={() => onSetTarget(zone.targetTemp + 0.5)}
                aria-label={`Crește temperatura țintă pentru ${zone.name}`}
                className="flex h-12 w-12 items-center justify-center rounded-xl border transition-transform active:scale-95"
                style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
              >
                <Plus className="h-5 w-5" aria-hidden="true" />
              </button>
            </div>

            <div className="mt-2 text-[11px]" style={{ color: 'var(--muted)' }}>
              Încălzit azi: <span className="font-mono-tabular font-semibold">{formatDurationRo(zone.runtimeTodaySec)}</span>{' '}
              (ieri {formatDurationRo(zone.runtimeYesterdaySec)})
            </div>
          </div>

          {/* Preset Buttons + Clear Boost */}
          <div className="flex flex-wrap items-center justify-center gap-2">
            {PRESETS.map((p) => {
              const active = Math.abs(zone.targetTemp - p.temp) < 0.05 && !zone.windowOpen;
              return (
                <button
                  key={p.id}
                  type="button"
                  onClick={() => onSetTarget(p.temp)}
                  className="min-h-[38px] rounded-xl border px-3 py-1.5 text-xs font-semibold transition-colors"
                  style={
                    active
                      ? {
                          background: p.frost ? 'var(--frost)' : 'var(--heat)',
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
                  {p.label} {p.temp.toFixed(1)}°
                </button>
              );
            })}
            {zone.isBoost && (
              <button
                type="button"
                onClick={onClearBoost}
                className="inline-flex min-h-[38px] items-center gap-1.5 rounded-xl border px-3 py-1.5 text-xs font-semibold"
                style={{
                  background: 'transparent',
                  borderColor: 'var(--heat)',
                  color: 'var(--heat)',
                }}
              >
                <RotateCcw className="h-3.5 w-3.5" aria-hidden="true" />
                <span>Revino la program ({zone.scheduleTarget.toFixed(1)}°)</span>
              </button>
            )}
          </div>

          {/* Integrated 24h Zone Mini Chart */}
          <div className="border-t pt-3" style={{ borderColor: 'var(--border)' }}>
            <div className="mb-2 flex flex-wrap items-center justify-between gap-2 text-xs">
              <span className="font-medium" style={{ color: 'var(--muted)' }}>
                Ultimele 24 h · {zone.shortName}
              </span>
              <div className="flex flex-wrap items-center gap-3 text-[11px]" style={{ color: 'var(--muted)' }}>
                <span className="flex items-center gap-1">
                  <i className="inline-block h-0.5 w-3" style={{ background: 'var(--fg)' }} />
                  Cameră
                </span>
                <span className="flex items-center gap-1">
                  <i
                    className="inline-block w-3 border-t border-dashed"
                    style={{ borderColor: 'var(--heat)' }}
                  />
                  Țintă
                </span>
                <span className="flex items-center gap-1">
                  <i className="inline-block h-2 w-1.5 rounded-xs" style={{ background: 'var(--heat)' }} />
                  Vană deschisă
                </span>
              </div>
            </div>
            <ZoneMiniChart zoneName={zone.name} data={zone.history24h} />
          </div>
        </div>
      )}
    </section>
  );
};
