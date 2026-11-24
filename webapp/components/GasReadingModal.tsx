import React, { useState } from 'react';
import { Gauge, X } from 'lucide-react';
import type { GasMeterState } from '../engine/types';

interface GasReadingModalProps {
  open: boolean;
  gas: GasMeterState;
  onClose: () => void;
  onSubmit: (indexM3: number) => { ok: boolean; error?: string };
}

export const GasReadingModal: React.FC<GasReadingModalProps> = ({
  open,
  gas,
  onClose,
  onSubmit,
}) => {
  const [value, setValue] = useState(
    gas.estimatedIndex !== null ? gas.estimatedIndex.toFixed(3) : '',
  );
  const [error, setError] = useState<string | null>(null);

  if (!open) return null;

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const num = Number(value.replace(',', '.'));
    if (!value.trim() || Number.isNaN(num)) {
      setError('Introdu un index numeric valid (ex: 4816.250).');
      return;
    }
    const res = onSubmit(num);
    if (!res.ok) {
      setError(res.error ?? 'Index invalid.');
      return;
    }
    setError(null);
    onClose();
  };

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/65 p-4 backdrop-blur-xs"
      role="dialog"
      aria-modal="true"
      aria-labelledby="gas-modal-title"
    >
      <div
        className="sh-card w-full max-w-md p-5"
        style={{ background: 'var(--surface)', borderColor: 'var(--border)' }}
      >
        <div className="flex items-center justify-between gap-2">
          <h2 id="gas-modal-title" className="flex items-center gap-2 text-base font-bold">
            <Gauge className="h-4 w-4" style={{ color: 'var(--heat)' }} aria-hidden="true" />
            <span>Citire index contor gaz</span>
          </h2>
          <button
            type="button"
            onClick={onClose}
            aria-label="Închide fereastra citire index"
            className="flex h-8 w-8 items-center justify-center rounded-lg"
            style={{ background: 'var(--elev)', color: 'var(--muted)' }}
          >
            <X className="h-4 w-4" aria-hidden="true" />
          </button>
        </div>

        <p className="mt-2 text-xs" style={{ color: 'var(--muted)' }}>
          Introdu valoarea afișată pe contorul de gaz (m³, până la 3 zecimale). Sistemul recalibrează
          debitul măsurat și estimarea consumului (FR-35.1).
        </p>

        <form onSubmit={handleSubmit} className="mt-4 space-y-3">
          <div>
            <label htmlFor="gas-index-input" className="mb-1 block text-xs font-semibold">
              Index contor (m³)
            </label>
            <input
              id="gas-index-input"
              type="text"
              inputMode="decimal"
              value={value}
              onChange={(e) => {
                setValue(e.target.value);
                setError(null);
              }}
              placeholder="ex: 4816.250"
              className="font-mono-tabular w-full rounded-xl border px-3 py-2.5 text-sm font-semibold"
              style={{
                background: 'var(--elev)',
                borderColor: error ? 'var(--danger)' : 'var(--border)',
                color: 'var(--fg)',
              }}
              autoFocus
            />
            {error && (
              <p role="alert" className="mt-1.5 text-xs font-medium" style={{ color: 'var(--danger)' }}>
                {error}
              </p>
            )}
          </div>

          <div
            className="rounded-xl p-3 text-xs"
            style={{ background: 'var(--elev)', color: 'var(--muted)' }}
          >
            <div className="flex justify-between">
              <span>Ultima citire salvată:</span>
              <strong className="font-mono-tabular" style={{ color: 'var(--fg)' }}>
                {gas.lastReadingIndex !== null ? `${gas.lastReadingIndex.toFixed(3)} m³` : '—'}
              </strong>
            </div>
            <div className="mt-1 flex justify-between">
              <span>Citiri înregistrate:</span>
              <strong className="font-mono-tabular" style={{ color: 'var(--fg)' }}>
                {gas.readings.length}
              </strong>
            </div>
          </div>

          <div className="flex items-center justify-end gap-2 pt-1">
            <button
              type="button"
              onClick={onClose}
              className="min-h-[40px] rounded-xl border px-4 py-2 text-xs font-semibold"
              style={{ background: 'var(--elev)', borderColor: 'var(--border)' }}
            >
              Anulează
            </button>
            <button
              type="submit"
              className="min-h-[40px] rounded-xl px-4 py-2 text-xs font-bold"
              style={{ background: 'var(--heat)', color: '#16171b' }}
            >
              Salvează index
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};
