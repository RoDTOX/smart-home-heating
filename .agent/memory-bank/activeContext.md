# Active Context: Smart Heating System

## Current Focus
- Establishing repository infrastructure, project plan ([`docs/plan_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan_oct2026.md)), and Memory Bank ([`.agent/memory-bank/`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/.agent/memory-bank/)).
- Awaiting user input on hardware specifics (Open Points OP-1 to OP-4: relay board model, radio trigger signal type, room/zone names, outdoor temperature sensor preference).

## Recent Changes
- **06.10.2026:**
  - Initialized git repository in `smart-heating/`.
  - Created dynamic engineering plan in Romanian: [`docs/plan_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan_oct2026.md).
  - Created changelog: [`docs/changelog_oct2026.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/changelog_oct2026.md).
  - Scaffolded `.agent/memory-bank/` with 7 core context files for cross-machine / cross-IDE persistence.

## Next Steps
1. User provides relay board model / pinout and radio trigger specifications.
2. Initialize `platformio.ini` project structure with PlatformIO for ESP32.
3. Implement `BleSensorScanner` (NimBLE BTHome v2 decoder).
4. Implement `ThermalZoneManager` with hysteresis and safety interlocks.
5. Create LittleFS web application skeleton with interactive apartment SVG.

## Active Decisions
- Primary controller is ESP32 (running standalone C++ firmware), not a script on the phone, to guarantee heating failsafe if phone/Wi-Fi reboots.
- Samsung A6 acts as complementary telemetry broker and long-term Grafana recorder.
