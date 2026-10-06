# Product Context: Smart Heating System

## Problem
In standard residential heating installations, a single central thermostat controls the entire house. As a result, individual rooms become either too hot or too cold, energy is wasted, and heating comfort is uneven. Furthermore, when the primary wireless thermostat remote unit broke, manual boiler switching became necessary. Commercial multi-zone kits cost hundreds or thousands of euros.

## Users
Homeowner and family members controlling apartment heating from smartphones (Android / iOS) or local PCs, with complete hands-off automatic heating based on room temperatures and schedules.

## User Journey
1. User opens the local web app on their phone (or PWA home-screen icon) pointing to `http://heating.local` (or direct IP).
2. The user sees an interactive SVG floorplan of their apartment with real-time room temperatures, humidity, and valve status.
3. The user can adjust any room's target setpoint via slider, toggle presets (`Confort`, `Eco`, `Noapte`, `Plecat`), or set schedules.
4. When a room drops below its setpoint minus hysteresis, the controller powers the corresponding 230V NC actuator.
5. Once the valve physically opens (after ~2.5 minutes), the boiler flame triggers via the reverse-engineered radio transmitter.
6. When all rooms reach setpoint, the boiler flame cuts off immediately, while valves stay open briefly for pump overrun cooling.

## UX Goals
- **Instantaneous local feedback**: Zero cloud lag, fast HTTP/REST and WebSocket updates.
- **Visual clarity**: Interactive apartment floorplan showing heat flow, active zones glowing, and easy setpoint adjustment.
- **Zero maintenance**: Sensor batteries last 2+ years; controller recovers automatically from power or network outages.
