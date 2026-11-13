---
name: "Smart Home Heating UI"
version: "2.0.0"
author: "Bogdan Seceleanu"
description: "Design system and visual specification for autonomous ESP32 underfloor heating controller"
tokens:
  themes:
    ha-charcoal:
      mode: "dark"
      bg: "#16171b"
      surface: "#1f2025"
      surfaceElevated: "#2a2b32"
      border: "#2f3038"
      fg: "#ececf1"
      muted: "#9a9ba6"
      heat: "#ffa63d"
      heatGlow: "rgba(255, 179, 92, 0.35)"
      idle: "#6b8cae"
      ok: "#3ec27a"
      warn: "#f2c230"
      danger: "#ef5350"
      frost: "#5fd0e8"
      radius: "16px"
    glass-glow:
      mode: "dark"
      bg: "linear-gradient(135deg, #0f1b2d 0%, #2a1738 100%)"
      surface: "rgba(255, 255, 255, 0.08)"
      surfaceElevated: "rgba(255, 255, 255, 0.14)"
      border: "rgba(255, 255, 255, 0.22)"
      fg: "#ffffff"
      muted: "rgba(255, 255, 255, 0.65)"
      heat: "#ff9a4d"
      heatGlow: "rgba(255, 176, 102, 0.45)"
      idle: "#7fb2ff"
      ok: "#5ee0a0"
      warn: "#ffd75e"
      danger: "#ff6b6b"
      frost: "#7de3ff"
      radius: "28px"
      backdropBlur: "24px"
    smartiva-dark:
      mode: "dark"
      bg: "#000000"
      surface: "#0b0b0d"
      surfaceElevated: "#15151a"
      border: "#26262c"
      fg: "#fafafa"
      muted: "#8a8a94"
      heat: "#ff8a1f"
      heatGlow: "rgba(255, 138, 31, 0.50)"
      idle: "#5d7a99"
      ok: "#2fd37a"
      warn: "#ffc61a"
      danger: "#ff4040"
      frost: "#4cc9e6"
      radius: "20px"
    nordic-light:
      mode: "light"
      bg: "#f6f3ee"
      surface: "#ffffff"
      surfaceElevated: "#efe9e1"
      border: "#e3dbd0"
      fg: "#2b2622"
      muted: "#8b8178"
      heat: "#f08a3c"
      heatGlow: "rgba(245, 178, 122, 0.35)"
      idle: "#7e9bb5"
      ok: "#3f9f6f"
      warn: "#d9a21b"
      danger: "#d94a46"
      frost: "#4fb8d0"
      radius: "20px"
  typography:
    uiFont: "Inter, sans-serif"
    monoFont: "JetBrains Mono, monospace"
    scale:
      collapsedTemp: "40px"
      expandedDial: "72px"
      tabletDial: "88px"
      badge: "12px"
  spacing:
    grid: [4, 8, 12, 16, 24, 32, 48]
    touchTargetMin: "44px"
    tabletTargetMin: "56px"
---

# Design Rationale & Principles (Google Stitch / AI Studio)

## 1. Aesthetic Purpose
An autonomous underfloor heating controller interface blending the high-information density and glanceability of the **Home Assistant Heating Card** with the tactile elegance of **Modern Glassmorphism** and the high-contrast luxury of **Smartiva Wall Panels**.

## 2. Color Language
- **Heat (`{tokens.themes.ha-charcoal.heat}`):** Warm vibrant amber/orange. Strictly reserved for active zone heating, open valve indication, and lit boiler flame.
- **Idle (`{tokens.themes.ha-charcoal.idle}`):** Slate-blue cool tone representing a zone at target temperature with its circuit valve closed.
- **Danger (`{tokens.themes.ha-charcoal.danger}`):** Coral red for Diagnostic Trouble Codes (DTCs), sensor timeout fallbacks, and boiler reversal lockouts. Never hidden behind secondary menus.
- **Warning (`{tokens.themes.ha-charcoal.warn}`):** Golden amber for open-window heating pauses and pump overrun heat dissipation.
- **Accessibility Invariant:** Never rely on color alone. Every thermal state must feature a Lucide icon and an explicit text badge in Romanian.

## 3. Component Hierarchy
1. **Header Bar:**
   - Outdoor Weather Widget: Temperature, dynamic day/night WMO icon, wind speed, humidity, and 3-5 day forecast.
   - Boiler Flame Indicator: Persistent flame badge with live anti-short-cycling / interlock cooldown countdown.
   - Theme Selector (`ha-charcoal`, `glass-glow`, `smartiva-dark`, `nordic-light`).
   - Mock / Live Data Switch segmented control.
2. **Expandable Zone Cards (Home Assistant Style):**
   - **Collapsed:** 96px compact card displaying current temperature in 40px bold font, circular progress ring, and state badge.
   - **Expanded:** 270° arc thermostat dial with thermal gradient, draggable thumb + stepper [-] [+], estimated time to target (*HeatUpModel*), and integrated 24h mini chart featuring:
     - Dotted target setpoint line (`strokeDasharray: 4 4`).
     - Ambient outdoor temperature curve.
     - Vertical colored bars along the bottom indicating exact valve-open burn intervals.
3. **Motion & Feedback:**
   - **Warm Glow Wave:** Subtle SVG wave gradient drifting at the bottom edge of cards while heating is active.
   - **Window Radar:** Concentric expanding warning rings around the window icon when `windowState=1`, with an instant *"Reia încălzirea"* button.
   - Respect `prefers-reduced-motion` across all components.

## 4. Responsive Dual-Layout
- **Mobile (Portrait, <768px):** Accordion stack of zone cards, sticky boiler status banner, minimal bottom navigation.
- **Tablet / Wall Panel (Landscape, >=1024px):** 3-column non-scrolling dashboard (`[1fr 1.6fr 1fr]`):
  - Left: Weather & 5-day forecast, Gas consumption & cost in Lei, system clock.
  - Center: The 3 heating zones with large 88px dials.
  - Right: Boiler status with large animated flame, 24h global multi-zone telemetry chart, DTC diagnostics.

## 5. Protected Admin Suite (`/admin`)
Desktop-first engineering workspace protected by session authentication:
- BLE Sensor live RSSI scanner and zone pairing.
- Output relay mapping matrix with conflict prevention.
- Hydraulic & safety parameter tuning (`interlockDelay`, `pumpOverrun`, `frostTemp`, `k-factor`).
- JSON configuration export & side-by-side visual diff import.
- Live interactive Web Virtual Console CLI.
