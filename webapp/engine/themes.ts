export type ThemeId =
  | 'ha-charcoal'
  | 'glass-glow'
  | 'smartiva-dark'
  | 'nordic-light'
  | 'smartiva-glass';

export interface ThemeTokens {
  id: ThemeId;
  label: string;
  mode: 'dark' | 'light';
  bg: string;
  surface: string;
  elev: string;
  border: string;
  fg: string;
  muted: string;
  heat: string;
  glow: string;
  idle: string;
  ok: string;
  warn: string;
  danger: string;
  frost: string;
  radius: string;
  blur: string;
}

export const THEMES: Record<ThemeId, ThemeTokens> = {
  'ha-charcoal': {
    id: 'ha-charcoal',
    label: 'HA Charcoal',
    mode: 'dark',
    bg: '#16171b',
    surface: '#1f2025',
    elev: '#2a2b32',
    border: '#2f3038',
    fg: '#ececf1',
    muted: '#9a9ba6',
    heat: '#ffa63d',
    glow: 'rgba(255, 179, 92, 0.35)',
    idle: '#6b8cae',
    ok: '#3ec27a',
    warn: '#f2c230',
    danger: '#ef5350',
    frost: '#5fd0e8',
    radius: '16px',
    blur: '0px',
  },
  'glass-glow': {
    id: 'glass-glow',
    label: 'Glass Glow',
    mode: 'dark',
    bg: 'linear-gradient(135deg, #0f1b2d 0%, #2a1738 100%)',
    surface: 'rgba(255, 255, 255, 0.08)',
    elev: 'rgba(255, 255, 255, 0.14)',
    border: 'rgba(255, 255, 255, 0.22)',
    fg: '#ffffff',
    muted: 'rgba(255, 255, 255, 0.65)',
    heat: '#ff9a4d',
    glow: 'rgba(255, 176, 102, 0.45)',
    idle: '#7fb2ff',
    ok: '#5ee0a0',
    warn: '#ffd75e',
    danger: '#ff6b6b',
    frost: '#7de3ff',
    radius: '28px',
    blur: '24px',
  },
  'smartiva-dark': {
    id: 'smartiva-dark',
    label: 'Smartiva Dark',
    mode: 'dark',
    bg: '#000000',
    surface: '#0b0b0d',
    elev: '#15151a',
    border: '#26262c',
    fg: '#fafafa',
    muted: '#8a8a94',
    heat: '#ff8a1f',
    glow: 'rgba(255, 138, 31, 0.5)',
    idle: '#5d7a99',
    ok: '#2fd37a',
    warn: '#ffc61a',
    danger: '#ff4040',
    frost: '#4cc9e6',
    radius: '20px',
    blur: '0px',
  },
  'nordic-light': {
    id: 'nordic-light',
    label: 'Nordic Light',
    mode: 'light',
    bg: '#f6f3ee',
    surface: '#ffffff',
    elev: '#efe9e1',
    border: '#e3dbd0',
    fg: '#2b2622',
    muted: '#8b8178',
    heat: '#f08a3c',
    glow: 'rgba(245, 178, 122, 0.35)',
    idle: '#7e9bb5',
    ok: '#3f9f6f',
    warn: '#d9a21b',
    danger: '#d94a46',
    frost: '#4fb8d0',
    radius: '20px',
    blur: '0px',
  },
  'smartiva-glass': {
    id: 'smartiva-glass',
    label: 'Smartiva Glass',
    mode: 'dark',
    bg: 'radial-gradient(circle at 78% 36%, rgba(255, 145, 36, 0.22) 0%, transparent 44%), radial-gradient(circle at 18% 72%, rgba(62, 125, 178, 0.22) 0%, transparent 48%), linear-gradient(135deg, #1a2028 0%, #282e35 50%, #191d24 100%)',
    surface: 'rgba(255, 255, 255, 0.085)',
    elev: 'rgba(255, 255, 255, 0.14)',
    border: 'rgba(255, 255, 255, 0.18)',
    fg: '#f5f7fa',
    muted: 'rgba(232, 238, 245, 0.66)',
    heat: '#e8f117',
    glow: 'rgba(232, 241, 23, 0.35)',
    idle: '#78a9d6',
    ok: '#4ade80',
    warn: '#ffb72b',
    danger: '#ff5a5f',
    frost: '#59d2ff',
    radius: '24px',
    blur: '28px',
  },
};

export const THEME_ORDER: ThemeId[] = [
  'ha-charcoal',
  'glass-glow',
  'smartiva-dark',
  'nordic-light',
  'smartiva-glass',
];

export function applyThemeTokens(themeId: ThemeId): void {
  const theme = THEMES[themeId] ?? THEMES['smartiva-glass'];
  const root = document.documentElement;
  root.dataset.theme = theme.id;
  root.style.setProperty('--bg', theme.bg);
  root.style.setProperty('--surface', theme.surface);
  root.style.setProperty('--elev', theme.elev);
  root.style.setProperty('--border', theme.border);
  root.style.setProperty('--fg', theme.fg);
  root.style.setProperty('--muted', theme.muted);
  root.style.setProperty('--heat', theme.heat);
  root.style.setProperty('--glow', theme.glow);
  root.style.setProperty('--idle', theme.idle);
  root.style.setProperty('--ok', theme.ok);
  root.style.setProperty('--warn', theme.warn);
  root.style.setProperty('--danger', theme.danger);
  root.style.setProperty('--frost', theme.frost);
  root.style.setProperty('--radius', theme.radius);
  root.style.setProperty('--blur', theme.blur);
}
