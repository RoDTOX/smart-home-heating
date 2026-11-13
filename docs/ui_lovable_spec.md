# Smart Home Heating - Lovable Frontend Specification & Master Prompt

This document provides the complete architecture, visual design system, live/mock API integration contracts, and the ready-to-use Master Prompt for **Lovable** to build a modern, responsive, mobile-first Web UI for the autonomous underfloor heating system.

---

## Part 1: Design System & Visual Language

### 1.1 Aesthetic & Principles
- **Style:** Clean Scandinavian Warm Minimalist with Modern Glassmorphism (dark & light mode support, dark mode as primary default for smart home dashboards).
- **Glanceability:** Room temperatures, target setpoints, and burner firing status readable in under 2 seconds.
- **Safety First:** Boiler flame state, interlock cooldown timers, and active DTC fault banners are persistent and never occluded by navigation.
- **Touch-First Mobile UX:** Steppers with large tactile buttons (±0.5 °C), 44px minimum touch targets, haptic visual feedback.

### 1.2 Color Tokens (Semantic HSL)
| Token | Light Mode (HSL) | Dark Mode (HSL) | Meaning / Usage |
|---|---|---|---|
| `--background` | `210 20% 98%` | `222 25% 8%` | Root application background |
| `--surface` | `0 0% 100%` | `222 22% 12%` | Cards, modals, drawers |
| `--surface-elevated`| `210 20% 95%` | `222 20% 16%` | Floating badges, interactive steppers |
| `--foreground` | `222 30% 14%` | `210 20% 95%` | High contrast primary text |
| `--muted` | `215 16% 47%` | `215 15% 65%` | Secondary labels, units, timestamps |
| `--primary` (Heat) | `24 95% 53%` | `24 95% 58%` | Valve open, flame active, heating demand |
| `--cool` (Idle) | `205 90% 52%` | `205 90% 60%` | Room at setpoint, valve closed, idle |
| `--success` | `142 70% 40%` | `142 65% 48%` | Online, Wi-Fi link good, healthy |
| `--warning` | `40 95% 50%` | `40 95% 55%` | Open window pause, pump overrun hold |
| `--danger` | `0 80% 55%` | `0 80% 62%` | DTC fault, sensor timeout fallback |
| `--frost` | `190 80% 60%` | `190 80% 65%` | Emergency frost guard active |

*Note: Never rely on color alone. Every critical status includes a clear Lucide icon and a human-readable text label.*

### 1.3 Typography & Sizing
- **UI Font:** `Inter` (sans-serif)
- **Monospace Font:** `JetBrains Mono` (temperatures, MACs, DTC codes, timestamps)
- **Hero Temperature:** `48px`, font-bold, tabular numerals
- **Card Setpoints:** `20px`, font-semibold
- **Status Badges:** `12px`, uppercase, tracking-wider

---

## Part 2: Component Architecture & UX Hierarchy

```text
src/
├── components/
│   ├── layout/
│   │   ├── AppShell.tsx            # Responsive sidebar (desktop) + bottom nav (mobile)
│   │   ├── TopHeader.tsx           # Global status, outdoor weather, DataSourceSwitch
│   │   └── DataSourceSwitch.tsx    # "Mock (Demo) | Live (ESP32)" segmented control
│   ├── dashboard/
│   │   ├── GlobalFlameBanner.tsx   # Flame status, cooldown countdown, boiler runtime
│   │   ├── FaultAlertBanner.tsx    # DTC alerts banner (DTC-101, DTC-207, etc.)
│   │   ├── ZoneCard.tsx            # Room hero card: temp, target, valve, humidity, window badge
│   │   ├── SetpointStepper.tsx     # Tactile ±0.5°C buttons with quick boost popover
│   │   ├── GasEnergyCard.tsx       # Gas consumption today/yesterday (m³ and Lei), flame flow
│   │   └── MiniHistoryChart.tsx    # 24h temperature & heating overlay
│   ├── zones/
│   │   ├── ZoneDetailModal.tsx     # Extended zone config (offsets, boost, circuit loops)
│   │   └── WindowPauseBanner.tsx   # Dismissable "Fereastră deschisă" pause override
│   ├── schedule/
│   │   ├── WeekScheduleGrid.tsx    # 7-day visual schedule with Day/Night/Away slots
│   │   └── SlotEditorModal.tsx     # Drag/time picker for temperature slots
│   ├── history/
│   │   └── TelemetryChart.tsx      # High-fidelity Recharts view (24h / 5d with boiler burn bands)
│   ├── diagnostics/
│   │   ├── DtcTable.tsx            # Active diagnostic trouble codes
│   │   ├── EventLogViewer.tsx      # LittleFS persistent events ring
│   │   └── ClearHistoryDialog.tsx  # Confirmation modal for "Curăță istoric"
│   └── settings/
│       ├── ConnectionConfig.tsx    # IP address (192.168.1.20), auth credentials, polling rate
│       └── SystemParamsForm.tsx    # Interlock, overrun, boiler min run parameters
├── hooks/
│   ├── useBoilerStatus.ts          # SWR / TanStack Query (5s polling)
│   ├── useZones.ts                 # Zones query & mutation hooks
│   ├── useTelemetry.ts             # History chart query
│   └── useDataSource.ts            # Zustand persisted store ("mock" | "live")
├── services/
│   ├── api.types.ts                # TypeScript domain models matching ESP32 JSON schema
│   ├── mockApi.ts                  # In-memory mock engine with realistic random-walk physics
│   ├── liveApi.ts                  # Axios / Fetch client targeting http://192.168.1.20
│   └── index.ts                    # Factory dispatching to mockApi or liveApi
└── pages/
    ├── DashboardPage.tsx
    ├── ZonesPage.tsx
    ├── SchedulePage.tsx
    ├── HistoryPage.tsx
    └── DiagnosticsPage.tsx
```

---

## Part 3: Live API Contract & Mock Engine

### 3.1 Live ESP32 Endpoints (SSOT)
- **`GET /api/boiler/status`**: System state, boiler flame (`ON`/`OFF`), `boilerCooldownSec`, outdoor temperature, weather age, season, gas meter estimates (`todayM3`, `gasPrice`).
- **`GET /api/zones`**: Array of 3 zones (ID 1: Living, ID 2: Copii, ID 3: Dormitor) with `temp`, `target`, `humidity`, `valve` (0/1), `mode`, `windowState` (0=Closed, 1=Open/Paused).
- **`POST /api/zone/setpoint?zone={id}&target={temp}`**: Change target setpoint.
- **`POST /api/zone/hold?zone={id}&temp={temp}&hours={hours}`**: Temporary setpoint override.
- **`POST /api/mode?mode=auto|manual|off`**: Global heating mode.
- **`GET /api/history?range=24h|5d`**: Historic temperature series & burner runs.
- **`GET /api/diag/status`**: Active diagnostic checks (Wi-Fi, Heap, BLE, Sensors, Radio) and active DTC list.
- **`GET /api/diag/events`**: LittleFS persistent event ring (last 50 events).
- **`POST /api/diag/events/clear`**: Purge persistent event log (keeps current `restart` record, wipes test DTCs).

### 3.2 Mock Engine Capabilities (`mockApi.ts`)
- Initialized with the exact 3 reference zones:
  - **Living + Bucătărie**: 24.1 °C, target 23.0 °C, hum 60.5%, valve closed (idle).
  - **Dormitor Copii**: 24.0 °C, target 23.5 °C, hum 59.5%, valve closed (idle).
  - **Dormitor Mare**: 23.3 °C, target 23.5 °C, hum 64.1%, valve open (heating).
- Realistic thermal drift simulation: temperature increases when valve is open, slowly cools down when closed.
- Interactive dev panel toggle for:
  - Firing boiler flame ON / Cooldown
  - Triggering Open Window detection on a zone
  - Injecting and clearing test DTCs (e.g. DTC-101 Sensor Lost)

---

## Part 4: Master Prompt for Lovable (Copy & Paste)

```markdown
Build a stunning, responsive, mobile-first Web UI called "Smart Home Heating" for an autonomous ESP32-based underfloor heating system.

### TECH STACK
- Framework: React 18 + Vite + TypeScript
- Styling: Tailwind CSS + Lucide Icons + Shadcn UI components
- State & Data: Zustand (persisted state) + TanStack Query v5 (polling & cache)
- Charts: Recharts (smooth area/line charts with heating overlay bands)
- Styling Tone: Scandinavian Warm Minimalist, Dark Mode by default, Glassmorphism accents

### DESIGN SYSTEM & PALETTE
1. Colors (defined via CSS variables in HSL):
   - Background: Dark slate/zinc `hsl(222 25% 8%)`, Surface: `hsl(222 22% 12%)`
   - Heating Active / Flame Accent: Vibrant Warm Amber/Orange `hsl(24 95% 55%)`
   - Idle / Normal Room: Cool Blue `hsl(205 90% 55%)`
   - Eco / Online: Emerald Green `hsl(142 70% 45%)`
   - Open Window / Warning: Golden Amber `hsl(40 95% 50%)`
   - Fault / DTC Alert: Coral Red `hsl(0 80% 60%)`
2. Typography:
   - Body font: Inter. Numerical readings & DTC codes: JetBrains Mono (monospace).
   - Zone temperatures displayed in 48px bold font with tabular numerals.
3. Micro-Interactions:
   - Tactile tactile stepper buttons (+/- 0.5°C) with active scale-95 animations.
   - Animated pulsing ring when heating valve is open or boiler flame is lit.
   - Never use color alone: combine with icons (Flame, Snowflake, Window, AlertTriangle) and Romanian text badges.

### DUAL DATA SOURCE (MOCK & LIVE SWITCH)
The app MUST support seamless switching between "Mock Demo" and "Live ESP32" mode via a segmented switch in the top bar:
1. DataSourceStore: Zustand store persisted in localStorage with `source: 'mock' | 'live'` (default: `'mock'`).
2. LiveApi: Calls `http://192.168.1.20` (configurable in Settings), with 5s timeout using AbortController and clear error handling for CORS/Mixed Content.
3. MockApi: Rich in-memory mock engine with:
   - 3 real zones: "Living + Bucătărie" (24.1°C), "Dormitor Copii" (24.0°C), "Dormitor Mare" (23.3°C).
   - Realistic thermal physics: temps gently fluctuate every 5s; valve opens when temp < target.
   - Gas consumption simulation: 1.20 m³ yesterday, 0.07 m³ today, cost calculated at 3.30 Lei/m³.
   - Mock event clearing leaving 1 `SYS-REBOOT` entry.
4. UI indicator: In Mock mode, display a subtle floating badge "DEMO MODE (MOCK)"; in Live mode, display a real-time connectivity status dot (Green connected, Red unreachable).

### PAGES & NAVIGATION
Use a responsive layout with a bottom navigation bar on mobile and a slim collapsible sidebar on desktop:

1. DASHBOARD (Overview):
   - Global Header: Outdoor weather badge (e.g. "19.1°C Exterior, Toamnă"), Global heating mode badge ("AUTO"), and Boiler status banner (Flame icon "Cazan ACTIV" or "Pauză flacără", with live cooldown seconds if active).
   - Active DTC Banner: If any fault exists (e.g. DTC-101 Sensor lost), display a high-priority dismissible banner with direct link to Diagnostics.
   - 3 Zone Cards (Grid 1 col on mobile, 3 cols on desktop):
     * Room Name & Circuit count badge.
     * Large Current Temperature (e.g. 24.1°C) with relative humidity (e.g. 60.5% RH).
     * Animated valve status: "Robinet Deschis (Încălzire)" vs "Închis (Standby)".
     * Target Setpoint with tactile Stepper: [-] [ 23.0°C ] [+] (0.5°C increments).
     * Contextual badges: "Fereastră deschisă (Pauză)" with one-click "Reia încălzirea" button, "Boost +2h", or "Program Normal".
   - Gas & Energy Mini-Card:
     * Today & Yesterday consumption in m³ and estimated cost in Lei.
     * Instantaneous measured gas flow (m³/h).
   - 24h Mini Telemetry Chart: smooth temperature curves for all 3 rooms with vertical orange shading for boiler firing periods.

2. ZONES VIEW:
   - Detailed control for each room: manual setpoint override, temporary hold timer (1h, 2h, 4h, 8h), hysteresis info, and assigned BLE sensor telemetry (battery %, signal RSSI).

3. SCHEDULE (Programare):
   - Visual 7-day schedule grid with color-coded slots for Day (Comfort), Night (Eco), and Away (Plecat).
   - Tap-to-edit slot interface with pre-heat lead indicator.

4. HISTORY (Telemetrie):
   - Interactive Recharts graph with 24h and 5d selectors.
   - Toggle visibility of individual zones, outdoor temperature, and boiler flame intervals.

5. DIAGNOSTICS (Diagnoză & Erori):
   - System Health Cards: Wi-Fi RSSI, Free Heap RAM, BLE Scanner, LittleFS Storage.
   - Active DTCs table (Code, Description, Zone, First Seen timestamp).
   - Event History log table with a prominent red/ghost button: "🗑️ Curăță istoric" (opens a confirmation modal before triggering `/api/diag/events/clear`).

6. SETTINGS:
   - ESP32 Controller IP Address (default: `http://192.168.1.20`), optional API credentials, theme selector (Dark/Light), and Data Source toggle.

Ensure all user-facing strings are in Romanian (e.g. "Temperatură", "Setat", "Centrală", "Consum gaz", "Curăță istoric", "Salvat cu succes"). Make the app immediately runnable out of the box with realistic mock data!
```
