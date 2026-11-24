import React from 'react';
import {
  Clock,
  Cloud,
  CloudRain,
  CloudSnow,
  CloudSun,
  Droplets,
  Flame,
  Fuel,
  Gauge,
  MapPin,
  Moon,
  PauseCircle,
  RefreshCw,
  Sun,
  Timer,
  Wind,
} from 'lucide-react';
import { computePreHeatLeadMinutes, formatDurationRo, formatMmSs } from '../engine/heatingEngine';
import type { HeatingSystemState } from '../engine/types';

interface LeftColumnProps {
  state: HeatingSystemState;
  clockHm: string;
  clockSs: string;
  dateRo: string;
  onRefreshWeather: () => void;
  onOpenGasModal: () => void;
}

const WMO_RO: Record<number, string> = {
  0: 'Senin',
  1: 'Mai mult senin',
  2: 'Parțial noros',
  3: 'Noros',
  61: 'Ploaie slabă',
  71: 'Ninsoare slabă',
};

function WeatherIcon({ code, isDay, className }: { code: number; isDay: boolean; className?: string }) {
  if (code === 0) {
    return isDay ? (
      <Sun className={className} style={{ color: 'var(--warn)' }} aria-hidden="true" />
    ) : (
      <Moon className={className} style={{ color: 'var(--idle)' }} aria-hidden="true" />
    );
  }
  if (code <= 2) {
    return isDay ? (
      <CloudSun className={className} style={{ color: 'var(--warn)' }} aria-hidden="true" />
    ) : (
      <Moon className={className} style={{ color: 'var(--idle)' }} aria-hidden="true" />
    );
  }
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    return <CloudRain className={className} style={{ color: 'var(--idle)' }} aria-hidden="true" />;
  }
  if ((code >= 71 && code <= 77) || code === 85 || code === 86) {
    return <CloudSnow className={className} style={{ color: 'var(--frost)' }} aria-hidden="true" />;
  }
  return <Cloud className={className} style={{ color: 'var(--muted)' }} aria-hidden="true" />;
}

export const LeftColumn: React.FC<LeftColumnProps> = ({
  state,
  clockHm,
  clockSs,
  dateRo,
  onRefreshWeather,
  onOpenGasModal,
}) => {
  const { weather, boiler, gas } = state;
  const preHeatLead = computePreHeatLeadMinutes(weather.temp, weather.valid);

  const guardLabel =
    boiler.interlockRemainingSec > 0
      ? 'Deschidere vane'
      : boiler.antiCycleRemainingSec > 0
        ? boiler.on
          ? 'Ardere minimă'
          : 'Pauză arzător'
        : 'Anti-cycling';

  const guardValue =
    boiler.interlockRemainingSec > 0
      ? formatMmSs(boiler.interlockRemainingSec)
      : boiler.antiCycleRemainingSec > 0
        ? formatMmSs(boiler.antiCycleRemainingSec)
        : 'Pregătit';

  const guardWaiting = boiler.interlockRemainingSec > 0 || boiler.antiCycleRemainingSec > 0;

  return (
    <div className="flex min-w-0 flex-col gap-3.5">
      {/* System Clock + Quick Outdoor Summary */}
      <section className="sh-card p-4" aria-label="Ceas sistem și temperatură exterioară">
        <div className="flex items-start justify-between gap-3">
          <div>
            <div className="flex items-center gap-1.5 text-xs font-medium" style={{ color: 'var(--muted)' }}>
              <Clock className="h-3.5 w-3.5" aria-hidden="true" />
              <span>Ora sistem</span>
            </div>
            <div className="font-mono-tabular mt-1.5 flex items-baseline gap-1.5">
              <span className="text-[30px] leading-none font-bold tracking-tight">{clockHm}</span>
              <span className="text-base font-medium" style={{ color: 'var(--muted)' }}>
                {clockSs}
              </span>
            </div>
            <div className="mt-1.5 text-xs" style={{ color: 'var(--muted)' }}>
              {dateRo}
            </div>
          </div>

          <div className="text-right">
            <div className="text-xs font-medium" style={{ color: 'var(--muted)' }}>
              Afară · {weather.city}
            </div>
            <div className="font-mono-tabular mt-1 text-[28px] leading-none font-bold tracking-tight">
              {weather.temp.toFixed(1)}°C
            </div>
            <div className="mt-1.5 flex items-center justify-end gap-1.5 text-[11px]" style={{ color: 'var(--muted)' }}>
              <MapPin className="h-3 w-3" style={{ color: 'var(--heat)' }} aria-hidden="true" />
              <span>{WMO_RO[weather.code] ?? 'Variabil'}</span>
            </div>
          </div>
        </div>

        {/* Weather details & 4-day forecast */}
        <div className="mt-3.5 border-t pt-3" style={{ borderColor: 'var(--border)' }}>
          <div className="flex items-center justify-between gap-2 text-xs" style={{ color: 'var(--muted)' }}>
            <div className="flex items-center gap-3">
              <span className="font-mono-tabular inline-flex items-center gap-1">
                <Droplets className="h-3.5 w-3.5" aria-hidden="true" />
                {weather.hum}%
              </span>
              <span className="font-mono-tabular inline-flex items-center gap-1">
                <Wind className="h-3.5 w-3.5" aria-hidden="true" />
                {weather.wind} km/h
              </span>
              <span className="hidden text-[11px] sm:inline">Avans meteo: {preHeatLead} min</span>
            </div>
            <button
              type="button"
              onClick={onRefreshWeather}
              aria-label="Actualizează datele meteo"
              className="flex h-7 w-7 items-center justify-center rounded-lg border transition-opacity hover:opacity-85"
              style={{ background: 'var(--elev)', borderColor: 'var(--border)', color: 'var(--fg)' }}
            >
              <RefreshCw className="h-3.5 w-3.5" aria-hidden="true" />
            </button>
          </div>

          <div className="mt-3 grid grid-cols-4 gap-2">
            {weather.forecast.map((d) => (
              <div
                key={d.day}
                className="flex flex-col items-center rounded-xl py-1.5 text-center"
                style={{ background: 'var(--elev)' }}
              >
                <span className="text-[11px] font-semibold" style={{ color: 'var(--muted)' }}>
                  {d.day}
                </span>
                <WeatherIcon code={d.code} isDay className="my-1 h-4 w-4" />
                <div className="font-mono-tabular text-[11px] font-semibold">
                  {d.hi}°{' '}
                  <span className="font-normal" style={{ color: 'var(--muted)' }}>
                    {d.lo}°
                  </span>
                </div>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* Boiler Status Card */}
      <section className="sh-card p-4" aria-label="Stare centrală termică">
        <div className="flex items-start justify-between gap-3">
          <div className="flex min-w-0 items-center gap-3">
            <div
              className="flex h-12 w-12 shrink-0 items-center justify-center rounded-2xl border"
              style={{
                background: 'var(--elev)',
                borderColor: boiler.on ? 'var(--heat)' : 'var(--border)',
                color: boiler.on ? 'var(--heat)' : 'var(--idle)',
                boxShadow: boiler.on ? '0 0 24px -4px var(--glow)' : undefined,
              }}
            >
              {boiler.on ? (
                <Flame className="h-6 w-6 animate-sh-flick" aria-hidden="true" />
              ) : (
                <PauseCircle className="h-6 w-6" aria-hidden="true" />
              )}
            </div>
            <div className="min-w-0">
              <div className="text-xs font-medium" style={{ color: 'var(--muted)' }}>
                Centrală termică
              </div>
              <div
                className="text-base font-bold"
                style={{ color: boiler.on ? 'var(--heat)' : 'var(--fg)' }}
              >
                {boiler.on ? 'Pornită' : 'Oprită'}
              </div>
              <div className="text-[11px]" style={{ color: 'var(--muted)' }}>
                {boiler.autoMode ? 'Regim Automat (TPI)' : 'Regim Manual'}
              </div>
            </div>
          </div>

          <div className="shrink-0 text-right">
            <div
              className="flex items-center justify-end gap-1 text-[11px]"
              style={{ color: 'var(--muted)' }}
            >
              <Timer className="h-3.5 w-3.5" aria-hidden="true" />
              <span>{guardLabel}</span>
            </div>
            <div
              className="font-mono-tabular mt-0.5 text-sm font-bold"
              style={{ color: guardWaiting ? 'var(--warn)' : 'var(--ok)' }}
            >
              {guardValue}
            </div>
            {boiler.overrunRemainingSec > 0 && (
              <div className="font-mono-tabular mt-0.5 text-[11px]" style={{ color: 'var(--warn)' }}>
                Post-circ. {formatMmSs(boiler.overrunRemainingSec)}
              </div>
            )}
          </div>
        </div>

        <div
          className="mt-3.5 grid grid-cols-3 gap-2.5 border-t pt-3"
          style={{ borderColor: 'var(--border)' }}
        >
          <div>
            <div className="text-[11px] font-medium" style={{ color: 'var(--muted)' }}>
              Pornită azi
            </div>
            <div className="font-mono-tabular mt-0.5 text-xs font-bold">
              {formatDurationRo(boiler.todaySec)}
            </div>
          </div>
          <div>
            <div className="text-[11px] font-medium" style={{ color: 'var(--muted)' }}>
              Ieri
            </div>
            <div className="font-mono-tabular mt-0.5 text-xs font-bold">
              {formatDurationRo(boiler.yesterdaySec)}
            </div>
          </div>
          <div>
            <div className="text-[11px] font-medium" style={{ color: 'var(--muted)' }}>
              Ardere curentă
            </div>
            <div className="font-mono-tabular mt-0.5 text-xs font-bold">
              {boiler.on ? formatMmSs(boiler.currentBurnSec) : '—'}
            </div>
          </div>
        </div>
      </section>

      {/* Gas Consumption & Cost Card */}
      <section className="sh-card p-4" aria-label="Consum gaz și cost în Lei">
        <div className="flex items-center justify-between gap-2">
          <h2 className="flex items-center gap-2 text-sm font-bold">
            <Fuel className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
            <span>Consum gaz &amp; cost</span>
          </h2>
          <span className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
            {gas.pricePerM3.toFixed(2)} Lei/m³
          </span>
        </div>

        <div
          className="mt-3 flex items-center justify-between border-b pb-2.5 text-xs"
          style={{ borderColor: 'var(--border)' }}
        >
          <span className="flex items-center gap-1.5" style={{ color: 'var(--muted)' }}>
            <Flame className="h-3.5 w-3.5" style={{ color: 'var(--heat)' }} aria-hidden="true" />
            <span>Debit mediu măsurat</span>
          </span>
          <span className="font-mono-tabular text-sm font-bold" style={{ color: 'var(--heat)' }}>
            {gas.measuredFlowM3h.toFixed(2)} m³/h
          </span>
        </div>

        <div className="mt-3 grid grid-cols-3 gap-2">
          <div>
            <div className="text-[11px]" style={{ color: 'var(--muted)' }}>
              Astăzi
            </div>
            <div className="font-mono-tabular mt-0.5 text-[15px] font-bold">
              {(gas.todayM3 * gas.pricePerM3).toFixed(2)} Lei
            </div>
            <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
              {gas.todayM3.toFixed(2)} m³
            </div>
          </div>
          <div>
            <div className="text-[11px]" style={{ color: 'var(--muted)' }}>
              Ieri
            </div>
            <div className="font-mono-tabular mt-0.5 text-[15px] font-bold">
              {(gas.yesterdayM3 * gas.pricePerM3).toFixed(2)} Lei
            </div>
            <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
              {gas.yesterdayM3.toFixed(2)} m³
            </div>
          </div>
          <div>
            <div className="text-[11px]" style={{ color: 'var(--muted)' }}>
              Luna curentă
            </div>
            <div className="font-mono-tabular mt-0.5 text-[15px] font-bold">
              {(gas.monthM3 * gas.pricePerM3).toFixed(0)} Lei
            </div>
            <div className="font-mono-tabular text-[11px]" style={{ color: 'var(--muted)' }}>
              {gas.monthM3.toFixed(2)} m³
            </div>
          </div>
        </div>

        <div
          className="mt-3 flex items-center justify-between gap-2 border-t pt-3"
          style={{ borderColor: 'var(--border)' }}
        >
          <div className="min-w-0 text-[11px]" style={{ color: 'var(--muted)' }}>
            Index estimat:{' '}
            <strong className="font-mono-tabular" style={{ color: 'var(--fg)' }}>
              {gas.estimatedIndex !== null ? `${gas.estimatedIndex.toFixed(3)} m³` : 'necalibrat'}
            </strong>
          </div>
          <button
            type="button"
            onClick={onOpenGasModal}
            className="inline-flex min-h-[34px] shrink-0 items-center gap-1.5 rounded-xl border px-3 py-1 text-xs font-semibold transition-opacity hover:opacity-90"
            style={{ background: 'var(--elev)', borderColor: 'var(--border)', color: 'var(--fg)' }}
          >
            <Gauge className="h-3.5 w-3.5" style={{ color: 'var(--heat)' }} aria-hidden="true" />
            <span>Citire index</span>
          </button>
        </div>
      </section>
    </div>
  );
};
