# Smart Home Heating: UI Specification v2 and Lovable Master Prompt

> Replaces `docs/ui_lovable_spec.md`. The document is in English because Lovable reads English. User-facing strings in the app stay in Romanian.
>
> **Notes before use**
> 1. The 10 screenshots did not reach me. The spec is built from your written description of the Home Assistant Heating Card, Neumorphic Glow Thermostat, Smartiva Wall Panel, VisionOS Glassmorphism and Tado AI Assist. Check the card anatomy against the images.
> 2. Endpoints marked ðŸ”¶ are **proposed**. They are not in the firmware contract of v1 and must be confirmed or built on the ESP32 side. Everything else is carried over from v1.
> 3. The admin panel can change safety parameters. The firmware must validate every value against its own limits (see Â§7.3). The UI limits are only a convenience.

---

## Part 1. Product Vision

| Aspect | Decision |
|---|---|
| Users | Household (dashboard) and Owner/Engineer (`/admin`, behind auth) |
| Devices | Phone (portrait) and wall tablet (landscape, 10") |
| Data | Dual mode: **Mock** (cloud preview) and **Live** (ESP32 `http://192.168.1.20`) |
| Principle | The state of the heating is readable in under 2 seconds. Safety state is never hidden. |
| Language | UI in Romanian. Code, comments and tokens in English. |

---

## Part 2. Design System

### 2.1 Theme Switcher (4 themes)

Themes are CSS-variable sets on `<html data-theme="...">`. They are persisted in Zustand, and the switcher sits in the header and in Settings. All components use semantic tokens only and never hard-coded colors.

| Theme id | Label | Character |
|---|---|---|
| `ha-charcoal` (**default**) | "Carbune" | Home Assistant look: matte charcoal `#16171b`, cards `#1f2025`, warm amber accents |
| `glass-glow` | "SticlÄƒ" | VisionOS glassmorphism: frosted blur, specular edges, light waves |
| `smartiva-dark` | "Negru lux" | Luxury black, thin radial gradients, high contrast for a wall panel |
| `nordic-light` | "Nordic" | Scandinavian clean light, airy, warm pastels |

**Token table** (HSL or hex, every theme defines all of them):

| Token | ha-charcoal | glass-glow | smartiva-dark | nordic-light |
|---|---|---|---|---|
| `--bg` | `#16171b` | gradient `#0f1B2D â†’ #2a1738` | `#000000` | `#f6f3ee` |
| `--surface` | `#1f2025` | `rgba(255,255,255,.08)` + `backdrop-blur(24px)` | `#0b0b0d` | `#ffffff` |
| `--surface-2` | `#2a2b32` | `rgba(255,255,255,.14)` | `#15151a` | `#efe9e1` |
| `--border` | `#2f3038` | `rgba(255,255,255,.22)` | `#26262c` | `#e3dbd0` |
| `--fg` | `#ececf1` | `#ffffff` | `#fafafa` | `#2b2622` |
| `--muted` | `#9a9ba6` | `rgba(255,255,255,.65)` | `#8a8a94` | `#8b8178` |
| `--heat` (active) | `#ffa63d` | `#ff9a4d` | `#ff8a1f` | `#f08a3c` |
| `--heat-glow` | `#ffb35c` at 35% | `#ffb066` at 45% | `#ff8a1f` at 50% | `#f5b27a` at 35% |
| `--idle` | `#6b8cae` | `#7fb2ff` | `#5d7a99` | `#7e9bb5` |
| `--ok` | `#3ec27a` | `#5ee0a0` | `#2fd37a` | `#3f9f6f` |
| `--warn` | `#f2c230` | `#ffd75e` | `#ffc61a` | `#d9a21b` |
| `--danger` | `#ef5350` | `#ff6b6b` | `#ff4040` | `#d94a46` |
| `--frost` | `#5fd0e8` | `#7de3ff` | `#4cc9e6` | `#4fb8d0` |
| `--radius` | 16px | 28px | 20px | 20px |
| Extras | flat, 1px border | specular top edge, animated light wave in background | radial gradient halo behind the big dial, high contrast | soft shadow `0 8px 24px rgba(80,60,40,.08)` |

Rules:
- Never use color alone for a state. Always add an icon (Lucide) and a Romanian text label.
- Contrast is at least WCAG AA in all four themes. `smartiva-dark` targets AAA for the dial numerals.
- `prefers-reduced-motion` disables glow waves, the radar pulse and ambient animation. State is still shown by icon and label.
- Touch targets are at least 44px. On the tablet panel, primary controls are at least 56px.

### 2.2 Typography
- UI: `Inter`. Numerals, DTC codes, MAC addresses, terminal and timestamps: `JetBrains Mono` with tabular numerals.
- Collapsed card temperature: 40px bold. Expanded dial: 72px bold. Tablet dial: 88px.
- Badges: 12px uppercase, tracking-wide.

### 2.3 Motion and Effects
1. **Warm glow wave (card base).** When the valve is open or the flame is active, a blurred radial gradient in `--heat-glow` rises from the bottom edge of the card. It is two layered SVG or CSS waves that drift slowly in opposite directions, with a 6â€“8 s cycle and opacity 0.25â€“0.45. When idle, the glow fades out over 1.2 s.
2. **Window radar.** When `windowState=1`, a discreet pair of concentric rings in `--warn` expand from the window icon (2.4 s loop, ease-out, max 2 rings). A **"Reia Ã®ncÄƒlzirea"** button is shown inside the card. Reduced motion shows static rings.
3. **Dial arc.** The arc is drawn with a conic or SVG gradient. Heating active: `--heat` going from deep amber to bright orange. Idle: `--idle` going from blue to grey. The change between the two is a 600 ms cross-fade.
4. **Stepper press.** `active:scale-95` and a soft ripple.
5. **Expand/collapse.** Height animation of 280 ms with a spring ease. Only one card is expanded at a time on phone (accordion). On the tablet, the center column allows a "focused" card.

---

## Part 3. Core Component: Expandable Zone Card (`ZoneCard`)

### 3.1 Collapsed state (about 96px tall on phone)
```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ â—¯(ring) Living + BucÄƒtÄƒrie     [ðŸ”¥ ÃŽncÄƒlzire]â”‚
â”‚  fine    20.6Â°C            â†’ 23.0Â°C   âŒ„    â”‚
â”‚  progress                                   â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```
- Large current temperature (40px) with a **thin circular progress ring** (current relative to target; amber when heating, blue-grey when idle).
- The small target is shown at right.
- **State badge** (one of): `ÃŽncÄƒlzire` (flame), `La temperaturÄƒ` (check), `Standby` (snowflake), `FereastrÄƒ deschisÄƒ` (window, warn), `Boost 2h`, `Senzor pierdut` (danger), `ProtecÈ›ie Ã®ngheÈ›` (frost).
- The glow wave is already visible at the base when heating.
- Tap anywhere on the card to expand.

### 3.2 Expanded state
```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ Living + BucÄƒtÄƒrie                  [ÃŽncÄƒlzire]â”‚
â”‚            â•­â”€â”€â”€â”€ arc 270Â° â”€â”€â”€â”€â•®                â”‚
â”‚           â•±   gradient arc     â•²   target     â”‚
â”‚          â”‚       20.6Â°C         â”‚   23.0Â°C    â”‚
â”‚           â•²   ~18 min pÃ¢nÄƒ la   â•±   [âˆ’] [+]   â”‚
â”‚            â•°â”€â”€ È›intÄƒ â”€â”€â”€â”€â”€â”€â”€â”€â”€â•¯                â”‚
â”‚  Exterior 11.8Â°C Â· Umiditate 60.5%             â”‚
â”‚ [SeteazÄƒ temporar] [Program] [Grafic] [Detalii]â”‚
â”‚ â”Œ mini chart 24h â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â” â”‚
â”‚ â”‚  Â·Â·Â·Â·Â· È›intÄƒ (dotted)                      â”‚ â”‚
â”‚ â”‚  â”€â”€â”€ temperaturÄƒ     â–® â–®â–®   â–®â–®â–®  (valve)   â”‚ â”‚
â”‚ â”‚  â”€ â”€ exterior                              â”‚ â”‚
â”‚ â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜ â”‚
â”‚ â–‘â–‘â–‘â–‘â–‘â–‘ warm glow wave at the base â–‘â–‘â–‘â–‘â–‘â–‘â–‘â–‘â–‘â–‘â–‘â–‘ â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```
- **Dial.** A 270Â° arc thermostat with the thermal gradient. The thumb on the arc can be dragged or tapped to set the target (0.5Â°C steps, clamped 5.0â€“28.0Â°C). It also has [âˆ’] and [+] steppers for accessibility. The change is committed after 800 ms of inactivity (debounced), with a toast "Salvat".
- **Time to target (HeatUpModel).** Shown under the temperature:
  - Learned: `~18 min pÃ¢nÄƒ la È›intÄƒ`
  - Not learned yet: `Ã®nvÄƒÈ›are 60m/1Â°C` (in learning, using the default rate)
  - At target: `La temperaturÄƒ`
  - Not heating or unreachable: `â€”`
- **Quick buttons** (4, icon above label, 44px minimum):
  - `SeteazÄƒ temporar` opens a sheet with the hold durations 1h / 2h / 4h / 8h and a temperature (`POST /api/zone/hold`)
  - `Program` opens the zone's schedule (the Schedule page filtered to the zone)
  - `Grafic` opens the History page with this zone preselected
  - `Detalii` opens the Zone Detail sheet (BLE sensor battery and RSSI, hysteresis, offsets, circuits)
- **Integrated mini chart (the key feature).** Recharts `ComposedChart`, 24h, 140px tall:
  - Temperature line (`--fg`), smooth.
  - **Target as a dotted line** (`strokeDasharray="4 4"`, `--heat` at 70%).
  - **Outdoor temperature** as a thin muted dashed line.
  - **Vertical colored bars at the bottom for periods when the valve was open.** Bars use `--heat` with 0.8 opacity and a height of about 18% of the plot, aligned to the time axis. Closed periods leave no bar.
  - A tooltip shows time, temperature, target and valve state.
- **Window state.** The radar pulse, a banner inside the card ("FereastrÄƒ deschisÄƒ â€” Ã®ncÄƒlzire Ã®n pauzÄƒ de 04:12"), and a button **"Reia Ã®ncÄƒlzirea"** (`POST /api/zone/window/resume?zone={id}` ðŸ”¶). The button must never hide the stepper.

### 3.3 Behavior
- Phone: accordion, one card open at a time, expanded card scrolls into view.
- Tablet: the center column shows cards in a 1Ã—3 stack or 2+1 grid. Taps expand in place and the dial is shown at full size.
- Optimistic updates for setpoint. If the API fails, roll back and show a danger toast ("Nu s-a putut salva").

---

## Part 4. Local Weather Widget

**Data source:** the Open-Meteo data already fetched by the firmware. Endpoint: `GET /api/weather` ðŸ”¶ (if absent, fall back to the `outdoor*` fields of `/api/boiler/status` and hide the forecast).

```ts
interface Weather {
  tempC: number; feelsLikeC?: number;
  weatherCode: number;          // WMO code
  isDay: boolean;
  humidityPct: number; windKmh: number; windDirDeg?: number;
  ageSec: number;               // freshness
  forecast: Array<{ date: string; code: number; tMinC: number; tMaxC: number; precipPct?: number }>; // 3â€“5 days
}
```

- **Header badge (phone):** icon, `11.8Â°C`, `ÃŽnnorat`, tap opens a sheet with details and the forecast.
- **Tablet left column widget:** large temperature, dynamic icon, humidity and wind (with direction arrow), and the 3â€“5 day forecast as a row of mini-columns (day, icon, max/min).
- **Dynamic icons:** map WMO codes to Lucide icons (`Sun`, `Moon` at night, `CloudSun`, `Cloud`, `CloudRain`, `CloudSnow`, `CloudLightning`, `CloudFog`). Light animation: slow rotation of the sun rays and falling raindrops (disabled with reduced motion).
- **Stale data:** if `ageSec > 3h`, show a muted "Date vechi" label with the age. Do not hide the value.
- **Heating link:** a small hint line, "Compensare meteo activÄƒ (k = 0.8)", visible when weather compensation is on.

---

## Part 5. Responsive Dual Layout

Breakpoints and layout selection:
- `< 768px`: **phone**
- `â‰¥ 1024px` and `orientation: landscape`: **wall tablet**
- Between: single column wide (the phone layout with a larger grid).

### 5.1 Phone (portrait)
- Header: title, weather badge, theme switcher, data-source dot.
- A persistent **Boiler banner** below the header (flame state, cooldown countdown) and the **DTC banner** (when there are faults). Neither is hidden by scrolling. The boiler banner is sticky.
- Vertical stack: Zone cards (accordion), Gas card, 24h chart.
- **Bottom navigation (5 items, minimal):** AcasÄƒ, Zone, Program, Istoric, Mai mult (DiagnozÄƒ, SetÄƒri, Admin).
- Safe-area padding for the iOS notch.

### 5.2 Tablet (landscape wall panel, 3 columns)
```
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚ LEFT        â”‚ CENTER                  â”‚ RIGHT           â”‚
â”‚ Weather     â”‚ Rooms (large dials)     â”‚ Boiler status   â”‚
â”‚  + forecast â”‚  Living   â—” 20.6        â”‚  big flame      â”‚
â”‚ Gas & Cost  â”‚  Copii    â—” 24.0        â”‚  cooldown       â”‚
â”‚  mÂ³ today   â”‚  Dormitor â—” 23.3        â”‚ Global 24h chartâ”‚
â”‚  Lei        â”‚                         â”‚  3 rooms + flameâ”‚
â”‚ Clock/date  â”‚                         â”‚ DTC status      â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”´â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
```
- Grid `grid-cols-[1fr_1.6fr_1fr]`, full viewport height (`100dvh`), no page scrolling. Columns scroll internally only if needed.
- Center: rooms with **large thermostat dials**, with the expanded-card content available in place.
- Right: a big **flame status** (animated flame icon, cooldown ring, runtime today) and the **global 24h chart** (three room lines and flame bands).
- Left: weather widget and gas/cost card. Clock and date at the bottom.
- Kiosk features: optional "dim after N minutes" (Settings), no text selection, large hit areas, and a long-press on the header opens the theme switcher.

---

## Part 6. Pages (Operator Side)

| Route | Content |
|---|---|
| `/` Dashboard | Layout per Part 5 |
| `/zones` | Per-room control: setpoint, hold timer, hysteresis, BLE sensor telemetry (battery %, RSSI) |
| `/schedule` | 7-day grid, slots Comfort / Eco / Plecat, pre-heat lead indicator, tap-to-edit |
| `/history` | Recharts 24h / 5d, toggles per zone, outdoor and flame intervals |
| `/diagnostics` | Health cards, DTC table, event log with **"CurÄƒÈ›Äƒ istoric"** |
| `/settings` | Connection (IP, credentials), theme, data source, kiosk options |
| `/admin` | **Engineering panel** (Part 7), behind auth |

### 6.1 Gas Card (`GasEnergyCard`)
- mÂ³: **azi / ieri / luna**, with a small bar sparkline of the last 7 days.
- **Flame flow:** `x.xx mÂ³/h` (live, shown only when the flame is on, otherwise `0.00`).
- **Estimated cost** in Lei: `mÂ³ Ã— 3.30 Lei/mÂ³` (price configurable in Settings, default 3.30). Display example: `0.07 mÂ³ Â· 0.23 Lei`.
- Button **"Citire index manual"** opens a dialog with a numeric field (mÂ³, 3 decimals) and a date. Submit sends `POST /api/gas/index` ðŸ”¶. On success it shows "Index salvat" and recalibrates the estimate.
- A "estimat" label is shown whenever the value is derived from burn time rather than from a measured meter.

### 6.2 Diagnostics
- **Health cards:** Wi-Fi RSSI, free heap, BLE scanner, LittleFS usage, radio.
- **DTC table** (`GET /api/diag/status`): code (mono), description, zone, first seen, count, state (active/cleared).
- **Event log** (`GET /api/diag/events`, last 50): timestamp, type, detail. Filters by type. Export CSV.
- **"CurÄƒÈ›Äƒ istoric"** (danger ghost button) opens a confirmation dialog: "Se È™terg toate evenimentele, cu excepÈ›ia ultimei Ã®nregistrÄƒri de repornire. ContinuaÈ›i?". On confirm it calls `POST /api/diag/events/clear`. The restart record is kept (FR-62).

---

## Part 7. Admin / Engineering Panel (`/admin`)

### 7.1 Access
- Protected route. Login screen with a username and password, using the credentials the ESP32 accepts (HTTP Basic or token, per firmware, discovered the same way as the CLI does it). The token is held in memory (`sessionStorage` at most), never in `localStorage`. Auto-logout after 15 minutes idle.
- In Mock mode the admin is open with a visible "DEMO" banner. In Live mode every call carries the credential. A 401 sends the user to the login page.
- Destructive or safety-relevant actions require a confirmation dialog stating exactly what changes.

### 7.2 Tabs

**A. Senzori BLE** ðŸ”¶ `GET /api/ble/scan`, `POST /api/ble/pair`, `POST /api/ble/unpair`
- Scan button with live progress (10 s). Results table: MAC (mono), name, RSSI bar, battery %, last seen, already paired?
- Pair a sensor to a zone with a dropdown. Unpair with confirmation.
- Show per-sensor freshness. A sensor that has not reported for a configured time shows `Senzor pierdut`.

**B. Relee È™i circuite** ðŸ”¶ `GET/PUT /api/relays/map`
- Matrix of relay channel to circuit to zone. Shows how many circuits belong to each zone (the "Circuite" badge). Edit in place, with a diff before save.
- A relay channel can belong to one zone only. Validation in the UI and in the firmware.
- Manual relay test is allowed only while the system is in **off** mode. A warning is shown, and the safety interlocks still apply.

**C. Parametri hidraulici È™i siguranÈ›Äƒ** `GET/PUT /api/config`

| Parameter | Unit | Note |
|---|---|---|
| `interlockDelay` | s | Delay between valve open and burner start |
| `pumpOverrun` | s | Pump keeps running after burner stop |
| `burnerMinRun` | s | Minimum burner run time |
| `burnerMinPause` | s | Minimum pause between runs |
| `frostTemp` | Â°C | Frost protection threshold |
| `weatherK` (k-factor) | â€“ | Weather compensation factor |
| `solarThrottle` | % / on-off | Solar gain throttle |

- Each field has min, max, default and a "reset to default" icon. A changed field is highlighted with a dot.
- **Safety rules (UI side):** `frostTemp` cannot be set below the firmware's minimum, and the safety delays cannot be set to 0. Fields show the allowed range from `GET /api/config/limits` ðŸ”¶. The firmware rejects out-of-range values with a 422 and the UI shows the reason.
- "SalveazÄƒ" sends only changed keys. A success toast says "Salvat cu succes". Parameters that need a restart show a "NecesitÄƒ repornire" label.

**D. ConfiguraÈ›ie JSON (export / import cu diff)**
- **Export:** downloads `smart-home-config-YYYYMMDD-HHmm.json` (excludes credentials and Wi-Fi password).
- **Import:** drag and drop a file. The UI validates the schema, then shows a **side-by-side diff** (current vs imported): added, changed and removed keys highlighted. Safety-relevant keys are marked with a shield icon. The user can apply all or untick single keys. Nothing is sent until "AplicÄƒ" is confirmed.
- Keep the last 5 exports in browser storage for quick rollback (config only, no credentials).

**E. Terminal virtual** ðŸ”¶ `WS /api/console` (fallback: `POST /api/console` with polling)
- Dark terminal area (xterm-like, mono), live output, history with â†‘/â†“, autocomplete for the known commands: `DIAG`, `RELAY`, `CONFIG`, `STATUS`, `BLE`, `EVENTS`, `HELP` (the actual list is read from `HELP`).
- Pause/resume, clear screen, download log. Output is capped at 2000 lines.
- Commands that change relays or config require a confirmation on the first use in a session. In Mock mode a scripted shell answers the same commands with realistic output.

### 7.3 Admin rules
- Every write in `/admin` is logged in the event log (who, what, old and new value). The UI shows the entry in the Diagnostics log after saving.
- The UI is never the safety authority. If the firmware refuses, the UI shows the refusal and keeps the old value.

---

## Part 8. Data Layer: Mock and Live

### 8.1 Switching
- Zustand store `useDataSource`: `{ source: 'mock' | 'live', baseUrl: 'http://192.168.1.20' }`, persisted. The default is `mock`.
- `services/index.ts` returns `mockApi` or `liveApi` behind a single `HeatingApi` interface, so components never know which one is active.
- Header shows a segmented control **"Demo | Live"**. In Mock mode: a floating "DEMO (MOCK)" badge. In Live mode: a status dot (green connected, amber slow, red unreachable) with the last-update time.

### 8.2 Live endpoints

Existing (v1):
- `GET /api/boiler/status`, `GET /api/zones`, `GET /api/history?range=24h|5d`
- `POST /api/zone/setpoint?zone={id}&target={temp}`
- `POST /api/zone/hold?zone={id}&temp={temp}&hours={hours}`
- `POST /api/mode?mode=auto|manual|off`
- `GET /api/diag/status`, `GET /api/diag/events`, `POST /api/diag/events/clear`

Proposed ðŸ”¶ (to be confirmed with the firmware):
- `GET /api/weather` (with forecast)
- `GET /api/gas`, `POST /api/gas/index`
- `POST /api/zone/window/resume?zone={id}`
- `GET /api/zones/{id}/heatup` (or fields `etaMin`, `learning` in `/api/zones`)
- `GET /api/ble/scan`, `POST /api/ble/pair`, `POST /api/ble/unpair`
- `GET/PUT /api/relays/map`
- `GET/PUT /api/config`, `GET /api/config/limits`
- `WS /api/console`

`liveApi.ts` rules: 5 s timeout with `AbortController`, one retry for GET only, never retry POST automatically. Clear messages for CORS, mixed content (HTTPS page calling HTTP device) and 401. Polling: 5 s for status and zones, 60 s for weather and gas, and none for admin until the tab is open.

### 8.3 Mock engine (`mockApi.ts`)

**Initial state (3 real zones):**

| Zone | Temp | Target | Humidity | Valve | Circuits |
|---|---|---|---|---|---|
| Living + BucÄƒtÄƒrie | 24.1Â°C | 23.0Â°C | 60.5% | closed | 2 |
| Dormitor Copii | 24.0Â°C | 23.5Â°C | 59.5% | closed | 1 |
| Dormitor Mare | 23.3Â°C | 23.5Â°C | 64.1% | **open** | 1 |

**Physics simulator** (tick every 5 s, in-memory):
- With the valve open: `T += heatRate * dt`. With it closed: `T -= lossRate * (T - Toutdoor) * dt`. Add small noise (Â±0.02Â°C).
- Hysteresis opens the valve at `target âˆ’ 0.3` and closes it at `target + 0.2`. The flame is on while any valve is open, respecting `interlockDelay`, `burnerMinRun`, `burnerMinPause` and `pumpOverrun` from the mock config (so the cooldown countdown is real).
- Heat-up model: starts as "learning 60m/1Â°C" and becomes a computed ETA after about 3 simulated cycles.
- Outdoor temperature follows a day curve around 11â€“19Â°C. Weather code changes slowly. 5-day forecast is generated.
- History: pre-generated 5 days at 5 min resolution, extended live. Valve-open intervals stored per zone for the card's bars.
- Gas: yesterday 1.20 mÂ³, today 0.07 mÂ³ growing with burn time (flow about 1.8 mÂ³/h when the flame is on), month total about 31.4 mÂ³, price 3.30 Lei/mÂ³. Index entry recalibrates the estimate.
- Events: seed with a boot record and a few events. `clear` keeps one `SYS-REBOOT` entry.
- BLE scan returns 3 paired sensors plus 2 unpaired. Relay map and config return realistic values. The console answers `HELP`, `STATUS`, `DIAG`, `RELAY`, `CONFIG`.

**Dev panel** (floating, Mock mode only): toggle flame on/cooldown, trigger open window on a zone, inject or clear a DTC (for example `DTC-101` sensor lost), speed up time Ã—1/Ã—10/Ã—60.

---

## Part 9. File Structure

```text
src/
â”œâ”€â”€ theme/            themes.css, ThemeProvider.tsx, ThemeSwitcher.tsx
â”œâ”€â”€ components/
â”‚   â”œâ”€â”€ layout/       AppShell, PhoneLayout, TabletLayout, TopHeader, BottomNav, DataSourceSwitch
â”‚   â”œâ”€â”€ dashboard/    BoilerBanner, FaultBanner, ZoneCard, ThermostatDial, SetpointStepper,
â”‚   â”‚                 ZoneMiniChart, GlowWave, WindowRadar, GasEnergyCard, GlobalChart24h
â”‚   â”œâ”€â”€ weather/      WeatherWidget, WeatherIcon, ForecastRow
â”‚   â”œâ”€â”€ schedule/     WeekScheduleGrid, SlotEditorModal
â”‚   â”œâ”€â”€ history/      TelemetryChart
â”‚   â”œâ”€â”€ diagnostics/  DtcTable, EventLogViewer, ClearHistoryDialog
â”‚   â”œâ”€â”€ admin/        AuthGate, BleScanner, RelayMatrix, SafetyParamsForm,
â”‚   â”‚                 ConfigDiffImport, VirtualTerminal
â”‚   â””â”€â”€ dev/          MockDevPanel
â”œâ”€â”€ hooks/            useBoilerStatus, useZones, useWeather, useGas, useTelemetry, useDataSource, useLayoutMode
â”œâ”€â”€ services/         api.types.ts, HeatingApi.ts, mockApi.ts, mockPhysics.ts, liveApi.ts, index.ts
â””â”€â”€ pages/            Dashboard, Zones, Schedule, History, Diagnostics, Settings, Admin
```

---

## Part 10. Acceptance Criteria

1. The app runs out of the box in Mock mode with the 3 reference zones and moving temperatures.
2. A collapsed card shows the large temperature, the thin progress ring, and the state badge. Tap expands it.
3. An expanded card shows the gradient arc dial, time to target, 4 quick buttons, and the mini chart with a dotted target line, outdoor line and vertical valve-open bars.
4. The glow wave shows when heating. The radar pulse and the "Reia Ã®ncÄƒlzirea" button show on an open window.
5. The weather widget shows temperature, icon, humidity, wind and a 3â€“5 day forecast.
6. Phone portrait uses accordion cards and a bottom nav. Tablet landscape uses the 3-column layout with no page scroll.
7. The 4 themes switch instantly and persist. Contrast is AA or better.
8. `/admin` is auth-gated and offers BLE pairing, relay allocation, safety parameters, JSON export/import with diff, and the live terminal.
9. The Gas card shows mÂ³ today/yesterday/month, flow, cost in Lei at 3.30, and manual index entry.
10. Diagnostics shows the DTC table and the event log, and "CurÄƒÈ›Äƒ istoric" keeps the restart record.
11. Switching Mock/Live never changes the components, only the data. Live errors (timeout, CORS, mixed content, 401) show a clear Romanian message.
12. Reduced-motion users get no animation, but every state is still readable.

---

## Part 11. Master Prompt for Lovable (copy and paste)

````markdown
Build "Smart Home Heating", a responsive web UI for an autonomous ESP32 underfloor heating controller. It must run immediately with rich mock data, and also connect to a live device. All user-facing text is in ROMANIAN. Code and comments are in English.

## TECH STACK
React 18 + Vite + TypeScript, Tailwind CSS, shadcn/ui, Lucide icons, Recharts, Zustand (persisted), TanStack Query v5, React Router. Use semantic CSS variables only; never hard-code colors in components.

## THEMES (4, switchable, persisted, via <html data-theme>)
Define every token for each theme: --bg, --surface, --surface-2, --border, --fg, --muted, --heat, --heat-glow, --idle, --ok, --warn, --danger, --frost, --radius.
1. "ha-charcoal" (DEFAULT): Home Assistant style. bg #16171b, surface #1f2025, border #2f3038, fg #ececf1, heat #ffa63d (warm amber). Flat, 1px borders, radius 16px.
2. "glass-glow": VisionOS glassmorphism. Gradient background (#0f1b2d to #2a1738), surfaces rgba(255,255,255,.08) with backdrop-blur 24px, specular top edge, slow animated light waves, radius 28px.
3. "smartiva-dark": luxury black. bg #000, surface #0b0b0d, thin radial gradient halo behind the main dial, very high contrast, heat #ff8a1f, radius 20px.
4. "nordic-light": Scandinavian clean light. bg #f6f3ee, surface #fff, fg #2b2622, warm pastels, heat #f08a3c, soft shadows, radius 20px.
Add a ThemeSwitcher in the header and in Settings. Never use color alone for a state: always add an icon and a Romanian text label. Meet WCAG AA. Respect prefers-reduced-motion by disabling glow waves, radar pulses and ambient motion.

## TYPOGRAPHY
Inter for UI, JetBrains Mono (tabular numerals) for temperatures, codes, MACs, timestamps, terminal. Collapsed card temperature 40px bold, expanded dial 72px, tablet dial 88px.

## KEY COMPONENT: EXPANDABLE ZONE CARD
Collapsed (about 96px): large current temperature (e.g. 20.6Â°C), a thin circular progress ring around/next to it (current vs target; amber while heating, blue-grey when idle), small target, a state badge with icon (ÃŽncÄƒlzire, La temperaturÄƒ, Standby, FereastrÄƒ deschisÄƒ, Boost 2h, Senzor pierdut, ProtecÈ›ie Ã®ngheÈ›). Tap to expand (phone: accordion, one open at a time).
Expanded:
- A 270Â° arc thermostat dial with thermal gradient: amber to orange when heating is active, blue to grey when idle (600 ms cross-fade). Draggable thumb plus [-] [+] steppers, 0.5Â°C steps, clamp 5.0 to 28.0, debounce 800 ms then save with toast "Salvat". Optimistic update with rollback and a danger toast on failure.
- Estimated time to target from the heat-up model: "~18 min pÃ¢nÄƒ la È›intÄƒ", or while still learning "Ã®nvÄƒÈ›are 60m/1Â°C", or "La temperaturÄƒ", or "â€”".
- Outdoor temperature and humidity line.
- 4 quick buttons (icon over label, min 44px): "SeteazÄƒ temporar" (sheet with 1h/2h/4h/8h plus temperature), "Program", "Grafic", "Detalii".
- Integrated 24h mini chart (Recharts ComposedChart, 140px): smooth temperature line, TARGET as a DOTTED line (strokeDasharray 4 4), outdoor temperature as a thin muted dashed line, and VERTICAL COLORED BARS along the bottom for every period when the valve was open (use --heat). Tooltip with time, temp, target, valve state.
- Visual effects: a fluid warm glow wave at the base of the card (two layered blurred radial/SVG waves drifting in opposite directions, 6-8 s cycle, opacity 0.25-0.45) that fades in when the valve/boiler is active and fades out when idle. For an open window (windowState=1): a discreet radar pulse (two concentric expanding rings in --warn, 2.4 s loop) around a window icon, a banner "FereastrÄƒ deschisÄƒ, Ã®ncÄƒlzire Ã®n pauzÄƒ" and a button "Reia Ã®ncÄƒlzirea" inside the card (never hiding the stepper).

## LOCAL WEATHER WIDGET
From Open-Meteo data via GET /api/weather (fallback to outdoor fields of /api/boiler/status without forecast). Show outdoor temperature, dynamic WMO-code icon (day/night aware: Sun, Moon, CloudSun, Cloud, CloudRain, CloudSnow, CloudLightning, CloudFog), humidity, wind speed with direction arrow, and a 3-5 day forecast (day, icon, max/min). Header badge on phone (tap opens a details sheet); full widget on tablet. If data is older than 3 hours show a muted "Date vechi" label with the age but keep the value.

## RESPONSIVE DUAL LAYOUT
- Phone portrait (<768px): vertical stack, accordion zone cards, minimal bottom navigation (AcasÄƒ, Zone, Program, Istoric, Mai mult). A sticky boiler banner (flame state, cooldown countdown) and a DTC fault banner are never hidden.
- Tablet landscape wall panel (>=1024px and landscape): 3-column grid [1fr 1.6fr 1fr], 100dvh, no page scroll. LEFT: weather with forecast, gas and cost card, clock. CENTER: the rooms with large thermostat dials. RIGHT: boiler status with a big animated flame and cooldown ring, a global 24h chart (3 rooms plus flame bands), DTC status. Large touch targets (56px), optional dim after N minutes, no text selection.

## PAGES
/ Dashboard, /zones (setpoint, hold timer, hysteresis, BLE sensor battery and RSSI), /schedule (7-day grid with Comfort / Eco / Plecat slots, pre-heat indicator), /history (24h and 5d, toggles for zones, outdoor, flame), /diagnostics, /settings (IP, credentials, theme, data source, gas price default 3.30, kiosk options), /admin.

## GAS CARD
m3 azi / ieri / luna with a 7-day sparkline, live flame flow in m3/h, estimated cost in Lei at 3.30 Lei/m3 (configurable), label "estimat" when derived from burn time, and a button "Citire index manual" opening a dialog (m3 with 3 decimals, date) that POSTs /api/gas/index.

## DIAGNOSTICS
Health cards (Wi-Fi RSSI, free heap, BLE scanner, LittleFS, radio). DTC table (code in mono, description, zone, first seen, count, state). Event log (last 50, type filter, CSV export). A danger ghost button "CurÄƒÈ›Äƒ istoric" opens a confirmation dialog, then POST /api/diag/events/clear (the restart record is kept).

## /admin (PROTECTED ROUTE, ZERO COMPROMISE)
Login gate (username/password; token only in memory or sessionStorage; auto-logout after 15 min idle; 401 returns to login). In mock mode it is open with a visible DEMO banner. Tabs:
1. "Senzori BLE": scan with progress (10 s), table (MAC, name, RSSI bar, battery, last seen, paired?), pair to a zone via dropdown, unpair with confirmation.
2. "Relee È™i circuite": matrix relay channel -> circuit -> zone, one zone per channel, diff before save, manual relay test only when mode is off.
3. "Parametri hidraulici È™i siguranÈ›Äƒ": interlockDelay, pumpOverrun, burnerMinRun, burnerMinPause, frostTemp, weather k-factor, solar throttle. Each with unit, min/max/default from GET /api/config/limits, a reset icon, changed-field dot, send only changed keys, show "NecesitÄƒ repornire" when needed. Safety values can never be set to 0 or below the firmware minimum; show the firmware's 422 reason and keep the old value.
4. "ConfiguraÈ›ie JSON": export (no credentials) and import with drag and drop, schema validation, SIDE-BY-SIDE DIFF (added/changed/removed, shield icon on safety keys, per-key checkboxes), apply only after confirmation.
5. "Terminal": live virtual console over WS /api/console (fallback POST polling): history with up/down arrows, autocomplete for DIAG, RELAY, CONFIG, STATUS, BLE, EVENTS, HELP, pause, clear, download, 2000-line cap. First relay/config command in a session asks for confirmation. In mock mode a scripted shell answers realistically.
Every admin write is also logged as an event (who, what, old and new value).

## DUAL DATA SOURCE
Zustand persisted store { source: 'mock' | 'live' (default 'mock'), baseUrl: 'http://192.168.1.20' }. A single HeatingApi interface with mockApi and liveApi implementations chosen by a factory; components must not know which is active. Header segmented control "Demo | Live". Mock: floating badge "DEMO (MOCK)". Live: status dot (green / amber / red) plus last-update time.
LIVE API: base URL configurable; 5 s timeout with AbortController; retry GET once, never auto-retry POST; clear Romanian errors for CORS, mixed content and 401. Polling: 5 s status/zones, 60 s weather/gas. Endpoints: GET /api/boiler/status, /api/zones, /api/history?range=24h|5d, /api/weather, /api/gas, /api/diag/status, /api/diag/events, /api/ble/scan, /api/relays/map, /api/config, /api/config/limits; POST /api/zone/setpoint?zone=&target=, /api/zone/hold?zone=&temp=&hours=, /api/zone/window/resume?zone=, /api/mode?mode=auto|manual|off, /api/gas/index, /api/diag/events/clear, /api/ble/pair, /api/ble/unpair; PUT /api/relays/map, /api/config.
MOCK ENGINE (in-memory, tick 5 s): 3 zones: "Living + BucÄƒtÄƒrie" 24.1Â°C target 23.0 hum 60.5% valve closed (2 circuits); "Dormitor Copii" 24.0Â°C target 23.5 hum 59.5% valve closed; "Dormitor Mare" 23.3Â°C target 23.5 hum 64.1% valve OPEN. Physics: valve open raises temperature, closed cools toward outdoor, small noise, hysteresis (open at target-0.3, close at target+0.2). Boiler flame respects interlockDelay, burnerMinRun, burnerMinPause, pumpOverrun, with a real cooldown countdown. Heat-up model starts as "Ã®nvÄƒÈ›are 60m/1Â°C" then becomes a computed ETA. Outdoor 11-19Â°C day curve with a generated 5-day forecast. 5 days of history at 5 min resolution including per-zone valve-open intervals. Gas: 1.20 m3 yesterday, 0.07 m3 today growing with burn time (about 1.8 m3/h), month about 31.4 m3, 3.30 Lei/m3. Events cleared leave one SYS-REBOOT entry. A floating dev panel (mock only): toggle flame on/cooldown, trigger open window on a zone, inject/clear DTCs (e.g. DTC-101 Sensor lost), time speed x1/x10/x60.

## QUALITY BAR
Out-of-the-box runnable with realistic mock data. All four themes fully polished. Accessible (focus rings, aria labels, 44px targets). Romanian strings such as "TemperaturÄƒ", "Setat", "CentralÄƒ", "Consum gaz", "CurÄƒÈ›Äƒ istoric", "Salvat cu succes", "Reia Ã®ncÄƒlzirea".
````

---

**What needs a decision from you:** the ðŸ”¶ endpoints (weather with forecast, gas index, window resume, BLE scan and pairing, relay map, config limits, console WebSocket) are my proposal. If the firmware does not have them yet, each one is a firmware step with its own requirement and tests. Until then the corresponding UI works in Mock mode only. Tell me which ones you want and I'll write the requirements for them.

