import React, { useMemo, useState } from 'react';
import type { GlobalHistoryPoint, ZoneHistoryPoint, ZoneState } from '../engine/types';

function buildSmoothPath(points: Array<[number, number]>): string {
  if (points.length === 0) return '';
  if (points.length === 1) return `M ${points[0][0].toFixed(1)} ${points[0][1].toFixed(1)}`;
  let d = `M ${points[0][0].toFixed(1)} ${points[0][1].toFixed(1)}`;
  for (let i = 0; i < points.length - 1; i++) {
    const a = points[i - 1] ?? points[i];
    const b = points[i];
    const c = points[i + 1];
    const e = points[i + 2] ?? c;
    const cp1x = b[0] + (c[0] - a[0]) / 6;
    const cp1y = b[1] + (c[1] - a[1]) / 6;
    const cp2x = c[0] - (e[0] - b[0]) / 6;
    const cp2y = c[1] - (e[1] - b[1]) / 6;
    d += ` C ${cp1x.toFixed(1)} ${cp1y.toFixed(1)} ${cp2x.toFixed(1)} ${cp2y.toFixed(1)} ${c[0].toFixed(1)} ${c[1].toFixed(1)}`;
  }
  return d;
}

function buildStepPath(points: Array<[number, number]>): string {
  if (points.length === 0) return '';
  let d = `M ${points[0][0].toFixed(1)} ${points[0][1].toFixed(1)}`;
  for (let i = 1; i < points.length; i++) {
    d += ` H ${points[i][0].toFixed(1)} V ${points[i][1].toFixed(1)}`;
  }
  return d;
}

interface ZoneMiniChartProps {
  zoneName: string;
  data: ZoneHistoryPoint[];
}

export const ZoneMiniChart: React.FC<ZoneMiniChartProps> = ({ zoneName, data }) => {
  const [hoverIdx, setHoverIdx] = useState<number | null>(null);
  const W = 520;
  const H = 124;
  const L = 36;
  const R = 34;
  const T = 8;
  const B = 20;
  const pw = W - L - R;
  const ph = H - T - B;
  const n = Math.max(1, data.length);

  const { minRoom, maxRoom, minOut, maxOut } = useMemo(() => {
    const temps = data.flatMap((d) => [d.temp, d.target]);
    const outs = data.map((d) => d.outdoor);
    const rMin = Math.floor(Math.min(...temps, 18) - 1);
    const rMax = Math.ceil(Math.max(...temps, 25) + 1);
    const oMin = Math.floor(Math.min(...outs, 0) - 1);
    const oMax = Math.ceil(Math.max(...outs, 10) + 1);
    return {
      minRoom: rMin,
      maxRoom: Math.max(rMin + 2, rMax),
      minOut: oMin,
      maxOut: Math.max(oMin + 2, oMax),
    };
  }, [data]);

  const x = (i: number) => L + ((i + 0.5) / n) * pw;
  const yRoom = (v: number) => T + ph - ((v - minRoom) / (maxRoom - minRoom)) * ph;
  const yOut = (v: number) => T + ph - ((v - minOut) / (maxOut - minOut)) * ph;

  const tempPath = useMemo(
    () => buildSmoothPath(data.map((d, i) => [x(i), yRoom(d.temp)])),
    [data, minRoom, maxRoom],
  );
  const targetPath = useMemo(
    () => buildStepPath(data.map((d, i) => [x(i), yRoom(d.target)])),
    [data, minRoom, maxRoom],
  );
  const outPath = useMemo(
    () => buildSmoothPath(data.map((d, i) => [x(i), yOut(d.outdoor)])),
    [data, minOut, maxOut],
  );

  const roomTicks = [minRoom, Math.round(((minRoom + maxRoom) / 2) * 10) / 10, maxRoom];
  const outTicks = [minOut, Math.round(((minOut + maxOut) / 2) * 10) / 10, maxOut];
  const hovered = hoverIdx !== null ? data[hoverIdx] : null;

  return (
    <div className="relative w-full">
      {hovered && (
        <div
          className="font-mono-tabular mb-1.5 flex flex-wrap items-center justify-between gap-2 rounded-lg px-2.5 py-1 text-[11px]"
          style={{ background: 'var(--elev)', border: '1px solid var(--border)' }}
        >
          <span>{hovered.hourLabel}</span>
          <span>
            Cameră: <strong style={{ color: 'var(--fg)' }}>{hovered.temp.toFixed(1)}°C</strong>
          </span>
          <span>
            Țintă: <strong style={{ color: 'var(--heat)' }}>{hovered.target.toFixed(1)}°C</strong>
          </span>
          <span>
            Afară: <strong style={{ color: 'var(--idle)' }}>{hovered.outdoor.toFixed(1)}°C</strong>
          </span>
          <span>
            Vană: <strong style={{ color: hovered.valve > 0 ? 'var(--heat)' : 'var(--muted)' }}>{Math.round(hovered.valve * 100)}%</strong>
          </span>
        </div>
      )}
      <svg
        viewBox={`0 0 ${W} ${H}`}
        className="block h-[124px] w-full"
        role="img"
        aria-label={`Istoric 24 de ore ${zoneName}`}
        onMouseLeave={() => setHoverIdx(null)}
      >
        {roomTicks.map((v, idx) => {
          const yy = yRoom(v);
          return (
            <g key={`rt-${idx}`}>
              <line
                x1={L}
                x2={W - R}
                y1={yy}
                y2={yy}
                stroke="var(--border)"
                strokeDasharray="3 3"
              />
              <text
                x={L - 6}
                y={yy + 3.5}
                textAnchor="end"
                fontSize="10"
                fill="var(--muted)"
                className="font-mono-tabular"
              >
                {v.toFixed(1)}°
              </text>
            </g>
          );
        })}
        {outTicks.map((v, idx) => (
          <text
            key={`ot-${idx}`}
            x={W - R + 5}
            y={yOut(v) + 3.5}
            textAnchor="start"
            fontSize="10"
            fill="var(--idle)"
            opacity={0.85}
            className="font-mono-tabular"
          >
            {v.toFixed(1)}°
          </text>
        ))}

        {[0, 6, 12, 18].map((i) => (
          <text
            key={`xl-${i}`}
            x={x(i)}
            y={H - 4}
            textAnchor="middle"
            fontSize="10"
            fill="var(--muted)"
            className="font-mono-tabular"
          >
            {data[i]?.hourLabel ?? `${String(i).padStart(2, '0')}:00`}
          </text>
        ))}

        {/* Valve open vertical bars along bottom */}
        {data.map((d, i) => {
          if (d.valve <= 0) return null;
          const bh = Math.max(4, d.valve * ph * 0.24);
          return (
            <rect
              key={`bar-${i}`}
              x={x(i) - 4}
              y={T + ph - bh}
              width={8}
              height={bh}
              rx={2}
              fill="var(--heat)"
              opacity={0.62}
            />
          );
        })}

        {/* Outdoor curve */}
        <path
          d={outPath}
          fill="none"
          stroke="var(--idle)"
          strokeWidth={1.5}
          strokeDasharray="3 3"
          opacity={0.8}
        />

        {/* Target stepped dotted line */}
        <path
          d={targetPath}
          fill="none"
          stroke="var(--heat)"
          strokeWidth={1.5}
          strokeDasharray="4 4"
          opacity={0.85}
        />

        {/* Room temperature smooth line */}
        <path d={tempPath} fill="none" stroke="var(--fg)" strokeWidth={2.2} />

        {/* Interactive hover columns */}
        {data.map((_, i) => (
          <rect
            key={`hit-${i}`}
            x={L + (i / n) * pw}
            y={T}
            width={pw / n}
            height={ph}
            fill="transparent"
            onMouseEnter={() => setHoverIdx(i)}
          />
        ))}
        {hoverIdx !== null && (
          <line
            x1={x(hoverIdx)}
            x2={x(hoverIdx)}
            y1={T}
            y2={T + ph}
            stroke="var(--fg)"
            strokeOpacity={0.35}
            strokeDasharray="2 2"
          />
        )}
      </svg>
    </div>
  );
};

interface GlobalTelemetryChartProps {
  range: '24h' | '5d';
  zones: ZoneState[];
  history5d: GlobalHistoryPoint[];
}

export const GlobalTelemetryChart: React.FC<GlobalTelemetryChartProps> = ({
  range,
  zones,
  history5d,
}) => {
  const [hoverIdx, setHoverIdx] = useState<number | null>(null);
  const W = 420;
  const H = 168;
  const L = 36;
  const R = 34;
  const T = 8;
  const B = 20;
  const pw = W - L - R;
  const ph = H - T - B;

  const series: GlobalHistoryPoint[] = useMemo(() => {
    if (range === '5d') return history5d;
    const z0 = zones[0]?.history24h ?? [];
    const z1 = zones[1]?.history24h ?? [];
    const z2 = zones[2]?.history24h ?? [];
    return z0.map((pt, i) => ({
      label: pt.hourLabel,
      z1: pt.temp,
      z2: z1[i]?.temp ?? pt.temp,
      z3: z2[i]?.temp ?? pt.temp,
      outdoor: pt.outdoor,
      boilerOn:
        pt.valve >= 0.65 ||
        (z1[i]?.valve ?? 0) >= 0.65 ||
        (z2[i]?.valve ?? 0) >= 0.65 ||
        i === 6 ||
        i === 7 ||
        i === 18 ||
        i === 23,
    }));
  }, [range, zones, history5d]);

  const n = Math.max(1, series.length);

  const { minRoom, maxRoom, minOut, maxOut } = useMemo(() => {
    const rooms = series.flatMap((p) => [p.z1, p.z2, p.z3]);
    const outs = series.map((p) => p.outdoor);
    const rMin = Math.floor((Math.min(...rooms, 20) - 0.5) * 2) / 2;
    const rMax = Math.ceil((Math.max(...rooms, 24.5) + 0.5) * 2) / 2;
    const oMin = Math.floor(Math.min(...outs, -1));
    const oMax = Math.ceil(Math.max(...outs, 8));
    return {
      minRoom: rMin,
      maxRoom: Math.max(rMin + 2, rMax),
      minOut: oMin,
      maxOut: Math.max(oMin + 2, oMax),
    };
  }, [series]);

  const x = (i: number) => L + (n > 1 ? (i / (n - 1)) * pw : pw / 2);
  const yRoom = (v: number) => T + ph - ((v - minRoom) / (maxRoom - minRoom)) * ph;
  const yOut = (v: number) => T + ph - ((v - minOut) / (maxOut - minOut)) * ph;

  const pathZ1 = useMemo(
    () => buildSmoothPath(series.map((p, i) => [x(i), yRoom(p.z1)])),
    [series, minRoom, maxRoom],
  );
  const pathZ2 = useMemo(
    () => buildSmoothPath(series.map((p, i) => [x(i), yRoom(p.z2)])),
    [series, minRoom, maxRoom],
  );
  const pathZ3 = useMemo(
    () => buildSmoothPath(series.map((p, i) => [x(i), yRoom(p.z3)])),
    [series, minRoom, maxRoom],
  );
  const pathOut = useMemo(
    () => buildSmoothPath(series.map((p, i) => [x(i), yOut(p.outdoor)])),
    [series, minOut, maxOut],
  );

  const roomTicks = [minRoom, Math.round(((minRoom + maxRoom) / 2) * 10) / 10, maxRoom];
  const outTicks = [minOut, Math.round(((minOut + maxOut) / 2) * 10) / 10, maxOut];
  const labelIndices =
    range === '24h' ? [0, 6, 12, 18, 23] : [0, 8, 16, 24, 32, Math.max(0, n - 1)];
  const hovered = hoverIdx !== null ? series[hoverIdx] : null;

  return (
    <div className="relative w-full">
      {hovered && (
        <div
          className="font-mono-tabular mb-2 flex flex-wrap items-center justify-between gap-1.5 rounded-lg px-2.5 py-1 text-[11px]"
          style={{ background: 'var(--elev)', border: '1px solid var(--border)' }}
        >
          <span>{hovered.label}</span>
          <span style={{ color: 'var(--heat)' }}>
            {zones[0]?.shortName}: {hovered.z1.toFixed(1)}°
          </span>
          <span style={{ color: 'var(--ok)' }}>
            {zones[1]?.shortName}: {hovered.z2.toFixed(1)}°
          </span>
          <span style={{ color: 'var(--frost)' }}>
            {zones[2]?.shortName}: {hovered.z3.toFixed(1)}°
          </span>
          <span style={{ color: 'var(--idle)' }}>Ext: {hovered.outdoor.toFixed(1)}°</span>
        </div>
      )}
      <svg
        viewBox={`0 0 ${W} ${H}`}
        className="block h-[168px] w-full"
        role="img"
        aria-label={`Telemetrie ${range === '24h' ? '24 de ore' : '5 zile'}, toate camerele`}
        onMouseLeave={() => setHoverIdx(null)}
      >
        {roomTicks.map((v, idx) => {
          const yy = yRoom(v);
          return (
            <g key={`gr-${idx}`}>
              <line x1={L} x2={W - R} y1={yy} y2={yy} stroke="var(--border)" strokeDasharray="3 3" />
              <text
                x={L - 6}
                y={yy + 3.5}
                textAnchor="end"
                fontSize="10"
                fill="var(--muted)"
                className="font-mono-tabular"
              >
                {v.toFixed(1)}°
              </text>
            </g>
          );
        })}
        {outTicks.map((v, idx) => (
          <text
            key={`go-${idx}`}
            x={W - R + 6}
            y={yOut(v) + 3.5}
            textAnchor="start"
            fontSize="10"
            fill="var(--idle)"
            opacity={0.85}
            className="font-mono-tabular"
          >
            {v.toFixed(1)}°
          </text>
        ))}

        {labelIndices.map((idx) => {
          const pt = series[idx];
          if (!pt) return null;
          return (
            <text
              key={`gx-${idx}`}
              x={x(idx)}
              y={H - 4}
              textAnchor="middle"
              fontSize="10"
              fill="var(--muted)"
              className="font-mono-tabular"
            >
              {pt.label}
            </text>
          );
        })}

        {/* Boiler active bottom strips strictly inside plot bounds */}
        {series.map((p, i) => {
          if (!p.boilerOn) return null;
          const segW = Math.min(8, Math.max(4, pw / n - 4));
          const bx = Math.max(L, Math.min(W - R - segW, x(i) - segW / 2));
          return (
            <rect
              key={`gb-${i}`}
              x={bx}
              y={T + ph - 5}
              width={segW}
              height={4.5}
              rx={1.5}
              fill="var(--heat)"
              opacity={0.85}
            />
          );
        })}

        {/* Outdoor dashed curve */}
        <path
          d={pathOut}
          fill="none"
          stroke="var(--idle)"
          strokeWidth={1.4}
          strokeDasharray="3 3"
          opacity={0.85}
        />

        {/* 3 zone curves */}
        <path d={pathZ3} fill="none" stroke="var(--frost)" strokeWidth={1.8} />
        <path d={pathZ2} fill="none" stroke="var(--ok)" strokeWidth={1.8} />
        <path d={pathZ1} fill="none" stroke="var(--heat)" strokeWidth={2.1} />

        {/* Hover overlay */}
        {series.map((_, i) => (
          <rect
            key={`gh-${i}`}
            x={L + (i / n) * pw}
            y={T}
            width={pw / n}
            height={ph}
            fill="transparent"
            onMouseEnter={() => setHoverIdx(i)}
          />
        ))}
        {hoverIdx !== null && (
          <line
            x1={x(hoverIdx)}
            x2={x(hoverIdx)}
            y1={T}
            y2={T + ph}
            stroke="var(--fg)"
            strokeOpacity={0.3}
            strokeDasharray="2 2"
          />
        )}
      </svg>
    </div>
  );
};
