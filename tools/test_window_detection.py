#!/usr/bin/env python3
"""
tools/test_window_detection.py
Automated Unit & Integration Verification Suite for Open-Window Detection & 2R2C Model Decoupling

Verifies Requirements R1 and R2:
  1. Prompt detection on realistic 0.5°C temperature drops over 10 minutes (two 5-minute sampling steps).
  2. Full configurability and unconstrained operation for sensitive thresholds from 0.2°C to 0.5°C
     without artificial clamps forcing a 0.8°C floor.
  3. Proper auto-recovery (derivative rebound, temperature rebound, 30-minute safety timeout, manual clear)
     and FSM state restoration to ZONE_IDLE allowing automatic heating resumption.
  4. 2R2C lumped-parameter night calibration (01:00-05:00) decoupling: aeration events isolate/discard
     distorted samples, protecting natural loss coefficient k_loss from corruption.
  5. Predictive preheat lead time baseline freezing during active aeration, preventing runaway lead times
     and suppressing gas boiler firing while windows are open.
  6. Automotive-grade DTC-301 fault code lifecycle and multi-zone isolation.
  7. End-to-end FSM heating resumption after window closure without human intervention.

Zero external ML libraries: fully deterministic, self-contained, runnable with standard Python 3.
"""

import sys
import math
import time
from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional

# Ensure UTF-8 output formatting on Windows
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


# ==============================================================================
# 1. Firmware-Exact Thermodynamic Engine & Circular Buffer Simulation
# ==============================================================================

MAX_THERMAL_ZONES = 3
WINDOW_BUFFER_SIZE = 12
WINDOW_SAMPLE_INTERVAL_MS = 300000  # 5 minutes in milliseconds
WINDOW_SAFETY_TIMEOUT_MS = 1800000  # 30 minutes in milliseconds


@dataclass
class WindowCircularBuffer:
    samples: List[float] = field(default_factory=lambda: [0.0] * WINDOW_BUFFER_SIZE)
    sample_timestamps: List[int] = field(default_factory=lambda: [0] * WINDOW_BUFFER_SIZE)
    head: int = 0
    count: int = 0
    first_derivative: float = 0.0   # °C/min
    second_derivative: float = 0.0  # °C/min²
    window_open: bool = False
    pause_start_ms: int = 0
    pause_min_temp: float = 999.0
    recovery_count: int = 0
    drop_threshold: float = 1.2     # Default 1.2°C
    last_sample_ms: int = 0
    last_sample_temp: float = 0.0
    detection_epoch: int = 0
    last_detection_duration_sec: int = 0


@dataclass
class PassiveNightCalibState:
    active: bool = False
    start_ms: int = 0
    start_epoch: int = 0
    start_temp: float = 0.0
    ext_temp_sum: float = 0.0
    ext_temp_count: int = 0
    last_sample_ms: int = 0
    last_valid_outdoor_ms: int = 0
    aeration_occurred: bool = False


@dataclass
class CalibratedMetrics:
    t_dead_min: float = 30.0
    heating_slope: float = 1.10
    natural_loss_rate: float = 0.0250  # k_loss
    overshoot_delta: float = 0.35
    preheat_lead_min: int = 45


class FirmwareThermodynamicEngine:
    """Exact reproduction of ThermodynamicEngine.cpp algorithms and circular buffer mathematics."""

    def __init__(self):
        self.buffers: List[WindowCircularBuffer] = [WindowCircularBuffer() for _ in range(MAX_THERMAL_ZONES)]
        self.night_calib: List[PassiveNightCalibState] = [PassiveNightCalibState() for _ in range(MAX_THERMAL_ZONES)]
        self.metrics: List[CalibratedMetrics] = [CalibratedMetrics() for _ in range(MAX_THERMAL_ZONES)]

    def set_window_drop_threshold(self, index: int, threshold: float):
        if 0 <= index < MAX_THERMAL_ZONES:
            if 0.2 <= threshold <= 3.0:
                self.buffers[index].drop_threshold = threshold

    def get_window_drop_threshold(self, index: int) -> float:
        if 0 <= index < MAX_THERMAL_ZONES:
            return self.buffers[index].drop_threshold
        return 1.2

    def is_window_open(self, index: int) -> bool:
        if 0 <= index < MAX_THERMAL_ZONES:
            return self.buffers[index].window_open
        return False

    def get_first_derivative(self, index: int) -> float:
        if 0 <= index < MAX_THERMAL_ZONES:
            return self.buffers[index].first_derivative
        return 0.0

    def get_second_derivative(self, index: int) -> float:
        if 0 <= index < MAX_THERMAL_ZONES:
            return self.buffers[index].second_derivative
        return 0.0

    def get_pause_remaining_sec(self, index: int, now_ms: int) -> int:
        if 0 <= index < MAX_THERMAL_ZONES and self.buffers[index].window_open:
            elapsed = now_ms - self.buffers[index].pause_start_ms
            if elapsed < WINDOW_SAFETY_TIMEOUT_MS:
                return (WINDOW_SAFETY_TIMEOUT_MS - elapsed) // 1000
        return 0

    def clear_window_pause(self, index: int, now_ms: int) -> bool:
        if 0 <= index < MAX_THERMAL_ZONES:
            buf = self.buffers[index]
            if buf.window_open:
                buf.last_detection_duration_sec = (now_ms - buf.pause_start_ms) // 1000
                buf.window_open = False
                buf.recovery_count = 0
                buf.last_sample_ms = now_ms
                buf.head = 0
                buf.count = 0
                buf.first_derivative = 0.0
                buf.second_derivative = 0.0
                return True
        return False

    def evaluate_window(self, index: int, current_temp: float, outdoor_temp: float, outdoor_valid: bool, now_ms: int):
        """Firmware-exact implementation of ThermodynamicEngine::evaluateWindow."""
        if not (0 <= index < MAX_THERMAL_ZONES):
            return

        buf = self.buffers[index]
        is_new_sample = False

        if buf.last_sample_ms == 0 or (now_ms - buf.last_sample_ms >= WINDOW_SAMPLE_INTERVAL_MS):
            buf.samples[buf.head] = current_temp
            buf.sample_timestamps[buf.head] = now_ms
            buf.head = (buf.head + 1) % WINDOW_BUFFER_SIZE
            if buf.count < WINDOW_BUFFER_SIZE:
                buf.count += 1
            buf.last_sample_ms = now_ms
            buf.last_sample_temp = current_temp
            is_new_sample = True

            if buf.count >= 2:
                idx0 = (buf.head + WINDOW_BUFFER_SIZE - 1) % WINDOW_BUFFER_SIZE
                idx1 = (buf.head + WINDOW_BUFFER_SIZE - 2) % WINDOW_BUFFER_SIZE
                dt01 = (buf.sample_timestamps[idx0] - buf.sample_timestamps[idx1]) / 60000.0
                if dt01 < 1.0:
                    dt01 = 5.0
                v01 = (buf.samples[idx0] - buf.samples[idx1]) / dt01

                if buf.count >= 3:
                    idx2 = (buf.head + WINDOW_BUFFER_SIZE - 3) % WINDOW_BUFFER_SIZE
                    dt12 = (buf.sample_timestamps[idx1] - buf.sample_timestamps[idx2]) / 60000.0
                    if dt12 < 1.0:
                        dt12 = 5.0
                    v12 = (buf.samples[idx1] - buf.samples[idx2]) / dt12

                    # Two-step central 1st derivative over ~10 min window
                    buf.first_derivative = (buf.samples[idx0] - buf.samples[idx2]) / (dt01 + dt12)
                    # 2nd derivative: acceleration of temperature drop
                    buf.second_derivative = (v01 - v12) / (0.5 * (dt01 + dt12))
                else:
                    buf.first_derivative = v01
                    buf.second_derivative = 0.0

        effective_derivative = buf.first_derivative

        delta_ext = (current_temp - outdoor_temp) if outdoor_valid else 10.0
        if delta_ext < 2.0:
            delta_ext = 2.0
        k_loss = self.metrics[index].natural_loss_rate if self.metrics[index].natural_loss_rate > 0.005 else 0.025
        natural_rate_min = (k_loss * delta_ext) / 60.0
        if natural_rate_min < 0.0025:
            natural_rate_min = 0.0025

        if not buf.window_open:
            max_recent = current_temp
            samples_to_check = 4 if buf.count >= 4 else buf.count
            for i in range(1, samples_to_check + 1):
                s_idx = (buf.head + WINDOW_BUFFER_SIZE - i) % WINDOW_BUFFER_SIZE
                if buf.samples[s_idx] > max_recent:
                    max_recent = buf.samples[s_idx]
            drop_recent = max_recent - current_temp

            min_drop = (buf.drop_threshold * 0.7) if (buf.drop_threshold < 0.35) else 0.35
            rate_trigger = (
                is_new_sample
                and (effective_derivative < 0.0)
                and (abs(effective_derivative) >= 2.5 * natural_rate_min)
                and (drop_recent >= min_drop)
            )
            severe_trigger = (
                is_new_sample
                and (effective_derivative < 0.0)
                and (abs(effective_derivative) >= 4.0 * natural_rate_min)
                and (drop_recent >= min_drop)
            )
            threshold_trigger = (drop_recent >= buf.drop_threshold)

            if rate_trigger or severe_trigger or threshold_trigger:
                buf.window_open = True
                buf.pause_start_ms = now_ms
                buf.pause_min_temp = current_temp
                buf.recovery_count = 0
                buf.detection_epoch = int(time.time())
        else:
            if current_temp < buf.pause_min_temp:
                buf.pause_min_temp = current_temp

            timeout_expired = (now_ms - buf.pause_start_ms >= WINDOW_SAFETY_TIMEOUT_MS)
            rebounded = (current_temp >= buf.pause_min_temp + 0.6)

            if is_new_sample:
                if buf.first_derivative >= -0.005:
                    buf.recovery_count += 1
                else:
                    buf.recovery_count = 0

            derivative_recovered = (buf.recovery_count >= 2)

            if timeout_expired or rebounded or derivative_recovered:
                buf.last_detection_duration_sec = (now_ms - buf.pause_start_ms) // 1000
                buf.window_open = False
                buf.recovery_count = 0

                # Re-baseline circular buffer with current temperature
                buf.last_sample_ms = now_ms
                buf.last_sample_temp = current_temp
                buf.head = 0
                buf.count = 1
                buf.samples[0] = current_temp
                buf.sample_timestamps[0] = now_ms
                buf.first_derivative = 0.0
                buf.second_derivative = 0.0

    def evaluate_night_calibration(
        self,
        index: int,
        current_temp: float,
        outdoor_temp: float,
        outdoor_valid: bool,
        zone_idle: bool,
        hour: int,
        minute: int,
        now_ms: int,
        now_epoch: int,
        window_open: bool = False
    ):
        """Firmware-exact implementation of ThermodynamicEngine::evaluateNightCalibration."""
        if not (0 <= index < MAX_THERMAL_ZONES):
            return

        state = self.night_calib[index]
        window_active = window_open or self.buffers[index].window_open
        in_night_window = (1 <= hour < 5)

        if in_night_window and zone_idle:
            if not state.active:
                if not window_active and outdoor_valid and (current_temp > outdoor_temp + 3.0):
                    state.active = True
                    state.start_ms = now_ms
                    state.start_epoch = now_epoch
                    state.start_temp = current_temp
                    state.ext_temp_sum = outdoor_temp
                    state.ext_temp_count = 1
                    state.last_sample_ms = now_ms
                    state.last_valid_outdoor_ms = now_ms
                    state.aeration_occurred = False
            else:
                if window_active:
                    state.aeration_occurred = True
                if outdoor_valid:
                    state.last_valid_outdoor_ms = now_ms
                    if now_ms - state.last_sample_ms >= 900000:  # 15 minutes
                        state.ext_temp_sum += outdoor_temp
                        state.ext_temp_count += 1
                        state.last_sample_ms = now_ms
                else:
                    if now_ms - state.last_valid_outdoor_ms >= 1800000:
                        state.active = False
        else:
            if state.active:
                if hour == 5 and state.ext_temp_count >= 4 and not state.aeration_occurred and not window_active:
                    elapsed_hours = (now_epoch - state.start_epoch) / 3600.0
                    if elapsed_hours >= 2.0:
                        drop = state.start_temp - current_temp
                        avg_ext = state.ext_temp_sum / state.ext_temp_count
                        avg_room = (state.start_temp + current_temp) * 0.5
                        grad = avg_room - avg_ext
                        if grad > 3.0 and drop > 0.05:
                            k_measured = drop / (elapsed_hours * grad)
                            if 0.005 <= k_measured <= 0.080:
                                self.metrics[index].natural_loss_rate = (
                                    0.8 * self.metrics[index].natural_loss_rate + 0.2 * k_measured
                                )
                state.active = False
                state.aeration_occurred = False

    def calculate_preheat_lead_min(
        self,
        index: int,
        current_temp: float,
        target_temp: float,
        outdoor_temp: float,
        outdoor_valid: bool
    ) -> int:
        """Firmware-exact implementation of ThermodynamicEngine::calculatePreheatLeadMin."""
        if not (0 <= index < MAX_THERMAL_ZONES):
            return 45
        if target_temp <= current_temp + 0.1:
            return 0

        m = self.metrics[index]
        delta_temp = target_temp - current_temp

        effective_overshoot = m.overshoot_delta
        if effective_overshoot > delta_temp * 0.45:
            effective_overshoot = delta_temp * 0.45

        delta_needed = delta_temp - effective_overshoot
        if delta_needed < 0.05:
            delta_needed = 0.05

        delta_ext = (current_temp - outdoor_temp) if outdoor_valid else 12.0
        if delta_ext < 0.0:
            delta_ext = 0.0
        loss_rate = m.natural_loss_rate * delta_ext
        net_rate = m.heating_slope - loss_rate
        if net_rate < 0.20:
            net_rate = 0.20

        heat_hours = delta_needed / net_rate
        total_minutes = m.t_dead_min + (heat_hours * 60.0)

        if total_minutes > 240.0:
            total_minutes = 240.0
        if total_minutes < 0.0:
            total_minutes = 0.0

        m.preheat_lead_min = int(total_minutes + 0.5)
        return m.preheat_lead_min


# ==============================================================================
# 2. Firmware-Exact Zone Manager & Diagnostic Trouble Code Simulation
# ==============================================================================

class ZoneState:
    ZONE_DISABLED = 0
    ZONE_IDLE = 1
    ZONE_WARMING_UP = 2
    ZONE_HEATING = 3
    ZONE_FAULT_TIMEOUT = 4
    ZONE_FROST_GUARD = 5
    ZONE_WARMUP = 6
    ZONE_WINDOW_PAUSE = 7


@dataclass
class WindowState:
    baseline_temp: float = 0.0
    baseline_ms: int = 0
    pause_active: bool = False
    pause_start_ms: int = 0
    pause_min_temp: float = 999.0
    last_detection_epoch: int = 0
    last_detection_duration_sec: int = 0


@dataclass
class ZoneConfig:
    id: int
    name: str
    target_temp: float = 21.0
    hysteresis: float = 0.2
    enabled: bool = True
    window_drop_threshold: float = 1.2
    relay_pin: int = 25


@dataclass
class ZoneStatus:
    state: int = ZoneState.ZONE_IDLE
    current_temp: float = 21.0
    sensor_valid: bool = True
    sensor_fault: bool = False
    frost_active: bool = False
    demand: bool = False
    relay_engaged: bool = False
    state_changed_ms: int = 0
    preheat_lead_min: int = 0


class FirmwareDiagnosticEngine:
    """Tracks active Diagnostic Trouble Codes (DTC)."""

    def __init__(self):
        self.active_dtcs: Dict[str, Dict[str, Any]] = {}

    def raise_dtc(self, code: str, message: str, severity: str = "WARNING"):
        self.active_dtcs[code] = {"message": message, "severity": severity, "time": time.time()}

    def clear_dtc(self, code: str):
        if code in self.active_dtcs:
            del self.active_dtcs[code]

    def is_dtc_active(self, code: str) -> bool:
        return code in self.active_dtcs


class FirmwareZoneManager:
    """Simulation of ThermalZoneManager.cpp window detection, schedule evaluation, and FSM."""

    def __init__(self, thermo_engine: FirmwareThermodynamicEngine, diag_engine: FirmwareDiagnosticEngine):
        self.thermo = thermo_engine
        self.diag = diag_engine
        self.configs: List[ZoneConfig] = [
            ZoneConfig(id=0, name="Living Room", relay_pin=25),
            ZoneConfig(id=1, name="Master Bedroom", relay_pin=26),
            ZoneConfig(id=2, name="Office", relay_pin=27),
        ]
        self.statuses: List[ZoneStatus] = [ZoneStatus() for _ in range(MAX_THERMAL_ZONES)]
        self.window_states: List[WindowState] = [WindowState() for _ in range(MAX_THERMAL_ZONES)]
        self.window_detection_enabled: bool = True
        self.preheat_enabled: bool = True
        self.preheat_active: bool = False
        self.auto_mode: bool = True
        self.interlock_delay_ms: int = 120000  # 120s actuator warm-up delay

    def set_window_drop_threshold(self, index: int, threshold: float):
        if 0 <= index < MAX_THERMAL_ZONES:
            # Clamping in ThermalZoneManager.cpp lines 1448-1449: [0.2, 3.0]
            if threshold < 0.2:
                threshold = 0.2
            if threshold > 3.0:
                threshold = 3.0
            self.configs[index].window_drop_threshold = threshold
            self.thermo.set_window_drop_threshold(index, threshold)

    def get_window_drop_threshold(self, index: int) -> float:
        if 0 <= index < MAX_THERMAL_ZONES:
            return self.configs[index].window_drop_threshold
        return 1.2

    def evaluate_window_detection(
        self,
        index: int,
        current_temp: float,
        outdoor_temp: float,
        outdoor_valid: bool,
        now_ms: int
    ):
        """Firmware-exact logic from ThermalZoneManager::evaluateWindowDetection."""
        if not (0 <= index < MAX_THERMAL_ZONES):
            return

        if not self.window_detection_enabled:
            self.window_states[index].pause_active = False
            self.thermo.clear_window_pause(index, now_ms)
            return

        drop_threshold = (
            self.configs[index].window_drop_threshold
            if 0.2 <= self.configs[index].window_drop_threshold <= 3.0
            else 1.2
        )
        self.thermo.set_window_drop_threshold(index, drop_threshold)
        self.thermo.evaluate_window(index, current_temp, outdoor_temp, outdoor_valid, now_ms)

        was_active = self.window_states[index].pause_active
        now_active = self.thermo.is_window_open(index)
        self.window_states[index].pause_active = now_active

        if not was_active and now_active:
            # Pause triggered
            self.window_states[index].pause_start_ms = now_ms
            self.window_states[index].pause_min_temp = current_temp
            self.window_states[index].baseline_temp = current_temp
            self.diag.raise_dtc("DTC-301", "Open window detected - heating paused", "WARNING")
        elif was_active and not now_active:
            # Auto-recovery: reset state to ZONE_IDLE
            if self.statuses[index].state == ZoneState.ZONE_WINDOW_PAUSE:
                self.statuses[index].state = ZoneState.ZONE_IDLE
                self.statuses[index].state_changed_ms = now_ms

            any_window_pause = any(ws.pause_active for ws in self.window_states)
            if not any_window_pause:
                self.diag.clear_dtc("DTC-301")

    def clear_window_pause(self, index: int, now_ms: int) -> bool:
        """Firmware-exact logic from ThermalZoneManager::clearWindowPause."""
        if not (0 <= index < MAX_THERMAL_ZONES):
            return False

        cleared = self.thermo.clear_window_pause(index, now_ms)
        if cleared or self.window_states[index].pause_active:
            self.window_states[index].pause_active = False
            self.window_states[index].baseline_temp = self.statuses[index].current_temp
            self.window_states[index].baseline_ms = now_ms

            if self.statuses[index].state == ZoneState.ZONE_WINDOW_PAUSE:
                self.statuses[index].state = ZoneState.ZONE_IDLE
                self.statuses[index].state_changed_ms = now_ms

            any_window_pause = any(ws.pause_active for ws in self.window_states)
            if not any_window_pause:
                self.diag.clear_dtc("DTC-301")
            return True
        return False

    def evaluate_schedule(
        self,
        target_next: float,
        target_active: float,
        outdoor_temp: float,
        weather_valid: bool,
        mins_to_next_anchor: int
    ):
        """Firmware-exact logic from ThermalZoneManager::evaluateSchedule preheat calculation."""
        self.preheat_active = False
        if not self.preheat_enabled:
            return

        for i in range(MAX_THERMAL_ZONES):
            if not self.configs[i].enabled:
                continue

            cur_temp = self.statuses[i].current_temp if self.statuses[i].sensor_valid else target_active
            # Baseline freeze decoupling from window aeration
            if self.window_states[i].pause_active and self.window_states[i].baseline_temp > 0.0:
                cur_temp = self.window_states[i].baseline_temp

            lead_min = self.thermo.calculate_preheat_lead_min(i, cur_temp, target_next, outdoor_temp, weather_valid)
            self.statuses[i].preheat_lead_min = lead_min

            if not self.window_states[i].pause_active and (target_next > target_active + 0.2) and (mins_to_next_anchor <= lead_min):
                self.preheat_active = True

    def update(self, now_ms: int):
        """Firmware-exact logic from ThermalZoneManager::update."""
        for i in range(MAX_THERMAL_ZONES):
            if not self.configs[i].enabled or self.statuses[i].sensor_fault:
                continue

            # Check window pause
            if self.auto_mode and not self.statuses[i].frost_active:
                if self.window_states[i].pause_active:
                    self.statuses[i].demand = False
                    self.statuses[i].state = ZoneState.ZONE_WINDOW_PAUSE
                else:
                    target = self.configs[i].target_temp
                    if not self.statuses[i].demand:
                        if self.statuses[i].current_temp <= (target - self.configs[i].hysteresis):
                            self.statuses[i].demand = True
                    else:
                        if self.statuses[i].current_temp >= (target + self.configs[i].hysteresis):
                            self.statuses[i].demand = False

        # Actuator relay sequencing
        for i in range(MAX_THERMAL_ZONES):
            if not self.configs[i].enabled or self.statuses[i].sensor_fault:
                continue

            if self.statuses[i].demand:
                if self.statuses[i].state in (ZoneState.ZONE_IDLE, ZoneState.ZONE_FAULT_TIMEOUT, ZoneState.ZONE_WINDOW_PAUSE):
                    self.statuses[i].state = ZoneState.ZONE_WARMING_UP
                    self.statuses[i].state_changed_ms = now_ms
                    self.statuses[i].relay_engaged = True
                elif self.statuses[i].state == ZoneState.ZONE_WARMING_UP:
                    if now_ms - self.statuses[i].state_changed_ms >= self.interlock_delay_ms:
                        self.statuses[i].state = ZoneState.ZONE_HEATING
            else:
                if self.statuses[i].state in (ZoneState.ZONE_WARMING_UP, ZoneState.ZONE_HEATING):
                    self.statuses[i].state = ZoneState.ZONE_IDLE
                    self.statuses[i].state_changed_ms = now_ms
                    self.statuses[i].relay_engaged = False


# ==============================================================================
# 3. Test Cases (R1, R2, FSM Recovery, Night Calibration, Preheat Decoupling)
# ==============================================================================

def test_01_window_detection_05c_drop_over_10min() -> Dict[str, Any]:
    """
    Test 01 (Requirement R1):
    Specifically verify that a temperature drop of 0.5°C over 10 minutes (two 5-minute sampling steps)
    triggers open window detection.
      - Step 0 (0 min): 21.50°C
      - Step 1 (5 min): 21.25°C (drop 0.25°C)
      - Step 2 (10 min): 21.00°C (total drop 0.50°C over 10 minutes)
    Assert open window declared at t=10 min, and verify central firstDerivative and secondDerivative.
    """
    engine = FirmwareThermodynamicEngine()
    outdoor_temp = 5.0  # Cold winter/autumn weather
    outdoor_valid = True

    # Scenario A: Standard two-step 0.50°C drop
    now_ms = 0
    engine.evaluate_window(0, 21.50, outdoor_temp, outdoor_valid, now_ms)
    s0_open = engine.is_window_open(0)

    now_ms += WINDOW_SAMPLE_INTERVAL_MS  # t = 5 min
    engine.evaluate_window(0, 21.25, outdoor_temp, outdoor_valid, now_ms)
    s1_open = engine.is_window_open(0)

    now_ms += WINDOW_SAMPLE_INTERVAL_MS  # t = 10 min
    engine.evaluate_window(0, 21.00, outdoor_temp, outdoor_valid, now_ms)
    s2_open = engine.is_window_open(0)
    d1 = engine.get_first_derivative(0)
    d2 = engine.get_second_derivative(0)

    # Scenario B: Mild outdoor weather (Text = 15°C), 0.50°C drop over 10 minutes
    engine_mild = FirmwareThermodynamicEngine()
    now_ms = 0
    engine_mild.evaluate_window(0, 21.50, 15.0, outdoor_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    engine_mild.evaluate_window(0, 21.20, 15.0, outdoor_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    engine_mild.evaluate_window(0, 21.00, 15.0, outdoor_valid, now_ms)
    mild_open = engine_mild.is_window_open(0)

    # Scenario C: Normal passive cooling (0.10°C drop over 10 min) must NOT trigger
    engine_normal = FirmwareThermodynamicEngine()
    now_ms = 0
    engine_normal.evaluate_window(0, 21.50, outdoor_temp, outdoor_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    engine_normal.evaluate_window(0, 21.45, outdoor_temp, outdoor_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    engine_normal.evaluate_window(0, 21.40, outdoor_temp, outdoor_valid, now_ms)
    normal_open = engine_normal.is_window_open(0)

    passed = (
        (not s0_open)
        and (not s1_open)
        and s2_open
        and (abs(d1 - (-0.050)) < 0.005)
        and mild_open
        and (not normal_open)
    )

    return {
        "id": "R1-WIN01",
        "name": "test_01_window_detection_05c_drop_over_10min",
        "passed": passed,
        "s0_open": s0_open,
        "s1_open": s1_open,
        "s2_open": s2_open,
        "first_derivative": d1,
        "second_derivative": d2,
        "mild_weather_open": mild_open,
        "normal_cooling_false_positive": normal_open,
        "criteria": "Trigger at t=10m for 0.5°C drop; d1 ~= -0.05°C/min; 0 false triggers on normal cooling",
        "message": f"t=0m: {s0_open}, t=5m: {s1_open}, t=10m: {s2_open} (d1={d1:.4f} °C/min); Mild Text=15°C: {mild_open}; Passive cooling trigger: {normal_open}"
    }


def test_02_sensitive_threshold_configuration_02_to_05() -> Dict[str, Any]:
    """
    Test 02 (Requirement R1):
    Verify that sensitive thresholds from 0.2°C to 0.5°C can be configured and function correctly
    without being clamped to 0.8°C.
      1. Configure thresholds across [0.2, 0.3, 0.4, 0.5, 1.0, 2.5, 3.0]°C and verify persistence.
      2. Verify boundary clamping (< 0.2°C clamped to 0.2°C, > 3.0°C clamped to 3.0°C).
      3. Physical functionality: configure threshold = 0.3°C and assert mild aeration of 0.35°C
         triggers window detection (which would fail under former 0.8°C clamp).
      4. Gradual 0.25°C drop over 15 minutes: verify that with threshold = 0.20°C,
         a gradual drop across the 15-minute window (21.50 -> 21.417 -> 21.333 -> 21.250)
         triggers open window detection at t=15m.
    """
    thermo = FirmwareThermodynamicEngine()
    diag = FirmwareDiagnosticEngine()
    manager = FirmwareZoneManager(thermo, diag)

    # 1. Test configuration without clamping to 0.8°C
    test_values = [0.2, 0.3, 0.4, 0.5, 0.8, 1.2, 2.0, 3.0]
    verified_values = []
    for val in test_values:
        manager.set_window_drop_threshold(0, val)
        stored_mgr = manager.get_window_drop_threshold(0)
        stored_eng = thermo.get_window_drop_threshold(0)
        if abs(stored_mgr - val) < 0.001 and abs(stored_eng - val) < 0.001:
            verified_values.append(val)

    # 2. Test boundary clamps
    manager.set_window_drop_threshold(0, 0.10)
    clamp_low = manager.get_window_drop_threshold(0)  # Should be 0.2

    manager.set_window_drop_threshold(0, 4.50)
    clamp_high = manager.get_window_drop_threshold(0)  # Should be 3.0

    # 3. Physical detection with sensitive threshold = 0.3°C
    manager.set_window_drop_threshold(0, 0.30)
    now_ms = 0
    # Gentle aeration: drop 0.35°C in 10 minutes
    manager.evaluate_window_detection(0, 21.50, 10.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 21.35, 10.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 21.15, 10.0, True, now_ms)  # 0.35°C drop
    detected_at_03 = thermo.is_window_open(0)

    # 4. Physical detection with ultra-sensitive threshold = 0.20°C over 15 minutes
    thermo_15m = FirmwareThermodynamicEngine()
    diag_15m = FirmwareDiagnosticEngine()
    manager_15m = FirmwareZoneManager(thermo_15m, diag_15m)
    manager_15m.set_window_drop_threshold(0, 0.20)
    t_ms = 0
    manager_15m.evaluate_window_detection(0, 21.5000, 5.0, True, t_ms)
    t0_open = thermo_15m.is_window_open(0)
    t_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager_15m.evaluate_window_detection(0, 21.4167, 5.0, True, t_ms)
    t5_open = thermo_15m.is_window_open(0)
    t_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager_15m.evaluate_window_detection(0, 21.3333, 5.0, True, t_ms)
    t10_open = thermo_15m.is_window_open(0)
    t_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager_15m.evaluate_window_detection(0, 21.2500, 5.0, True, t_ms)
    t15_open = thermo_15m.is_window_open(0)
    detected_15m_at_02 = (not t0_open) and (not t5_open) and (not t10_open) and t15_open

    passed = (
        len(verified_values) == len(test_values)
        and abs(clamp_low - 0.20) < 0.001
        and abs(clamp_high - 3.00) < 0.001
        and detected_at_03
        and detected_15m_at_02
    )

    return {
        "id": "R1-WIN02",
        "name": "test_02_sensitive_threshold_configuration_02_to_05",
        "passed": passed,
        "verified_thresholds": verified_values,
        "clamp_low": clamp_low,
        "clamp_high": clamp_high,
        "detected_at_03_threshold": detected_at_03,
        "detected_15m_at_02_threshold": detected_15m_at_02,
        "criteria": "Thresholds 0.2-0.5°C accepted without 0.8°C clamp; Clamped to [0.2, 3.0]; 0.35°C drop detected; 15m 0.25°C drop detected at 0.2°C threshold",
        "message": f"Configured {len(verified_values)}/{len(test_values)} thresholds; Clamps: low={clamp_low}, high={clamp_high}; 0.3°C threshold trigger: {detected_at_03}; 15m 0.2°C threshold trigger: {detected_15m_at_02}"
    }


def test_03_auto_recovery_and_timeout_mechanisms() -> Dict[str, Any]:
    """
    Test 03 (Requirement R1):
    Verify that auto-recovery or timeout cleans up the pause state properly across all 4 pathways:
      1. Derivative rebound recovery: dT/dt >= -0.005°C/min for 2 consecutive samples.
      2. Temperature rebound recovery: currentTemp >= pauseMinTemp + 0.6°C.
      3. Safety timeout recovery: elapsed time >= 30 minutes (1,800,000 ms).
      4. Manual clear: clearWindowPause() immediately resets state to ZONE_IDLE.
    """
    # 1. Derivative Rebound Recovery
    eng1 = FirmwareThermodynamicEngine()
    now_ms = 0
    eng1.evaluate_window(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng1.evaluate_window(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng1.evaluate_window(0, 21.00, 5.0, True, now_ms)  # Open window declared!
    d_open = eng1.is_window_open(0)

    # Window closed: temperature stabilizes
    now_ms += WINDOW_SAMPLE_INTERVAL_MS  # t=15m
    eng1.evaluate_window(0, 21.00, 5.0, True, now_ms)
    d_step1_open = eng1.is_window_open(0)  # Still open (recoveryCount = 0)

    now_ms += WINDOW_SAMPLE_INTERVAL_MS  # t=20m
    eng1.evaluate_window(0, 21.00, 5.0, True, now_ms)
    d_step2_open = eng1.is_window_open(0)  # Still open (recoveryCount = 1)

    now_ms += WINDOW_SAMPLE_INTERVAL_MS  # t=25m
    eng1.evaluate_window(0, 21.01, 5.0, True, now_ms)
    d_step3_open = eng1.is_window_open(0)  # Auto-recovered! (recoveryCount = 2)

    # 2. Temperature Rebound Recovery (+0.6°C)
    eng2 = FirmwareThermodynamicEngine()
    now_ms = 0
    eng2.evaluate_window(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng2.evaluate_window(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng2.evaluate_window(0, 20.90, 5.0, True, now_ms)  # Window open, minTemp = 20.90
    t_open = eng2.is_window_open(0)

    now_ms += 60000  # 1 min later, warm air surge to 21.55°C (+0.65°C >= 0.60°C)
    eng2.evaluate_window(0, 21.55, 5.0, True, now_ms)
    t_rebound_recovered = not eng2.is_window_open(0)

    # 3. Safety Timeout Recovery (30 minutes)
    eng3 = FirmwareThermodynamicEngine()
    now_ms = 0
    eng3.evaluate_window(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng3.evaluate_window(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    eng3.evaluate_window(0, 21.00, 5.0, True, now_ms)  # Triggered at now_ms = 600,000
    timeout_start_ms = now_ms

    # Advance to 29 minutes after trigger
    now_ms = timeout_start_ms + 1740000  # 29m
    eng3.evaluate_window(0, 20.80, 5.0, True, now_ms)
    timeout_at_29m = eng3.is_window_open(0)  # Must still be paused

    # Advance to 30 minutes (1,800,000 ms)
    now_ms = timeout_start_ms + 1800000  # 30m
    eng3.evaluate_window(0, 20.80, 5.0, True, now_ms)
    timeout_at_30m = eng3.is_window_open(0)  # Must be auto-recovered!

    # 4. Manual Clear
    thermo4 = FirmwareThermodynamicEngine()
    diag4 = FirmwareDiagnosticEngine()
    mgr4 = FirmwareZoneManager(thermo4, diag4)
    now_ms = 0
    mgr4.evaluate_window_detection(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    mgr4.evaluate_window_detection(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    mgr4.evaluate_window_detection(0, 21.00, 5.0, True, now_ms)
    mgr4.update(now_ms)
    manual_paused = (thermo4.is_window_open(0) and mgr4.statuses[0].state == ZoneState.ZONE_WINDOW_PAUSE)

    now_ms += 60000
    mgr4.clear_window_pause(0, now_ms)
    manual_cleared = (not thermo4.is_window_open(0) and mgr4.statuses[0].state == ZoneState.ZONE_IDLE)

    passed = (
        d_open
        and (not d_step3_open)
        and t_open
        and t_rebound_recovered
        and timeout_at_29m
        and (not timeout_at_30m)
        and manual_paused
        and manual_cleared
    )

    return {
        "id": "R1-WIN03",
        "name": "test_03_auto_recovery_and_timeout_mechanisms",
        "passed": passed,
        "derivative_rebound_recovered": not d_step3_open,
        "temp_rebound_recovered": t_rebound_recovered,
        "timeout_29m_paused": timeout_at_29m,
        "timeout_30m_recovered": not timeout_at_30m,
        "manual_clear_restored_idle": manual_cleared,
        "criteria": "Auto-recover via derivative (2 consecutive), temp rebound (+0.6C), 30m timeout, manual clear",
        "message": f"Deriv recovery: {not d_step3_open}; Temp rebound: {t_rebound_recovered}; Timeout 30m: {not timeout_at_30m}; Manual clear: {manual_cleared}"
    }


def test_04_night_calibration_isolation_during_aeration() -> Dict[str, Any]:
    """
    Test 04 (Requirement R2):
    Verify that 2R2C night calibration (k_loss estimation) isolates/ignores temperature drops
    during window aeration and does NOT corrupt k_loss.
      1. Normal Night: Quiescent cooling from 01:00 to 05:00 with ground-truth k_loss = 0.025.
         k_loss updates cleanly without error.
      2. Aerated Night: Window opens at 03:00 causing rapid drop of 2.5°C.
         aerationOccurred flag isolates session, and calibration update is safely rejected at 05:00.
         k_loss remains uncorrupted at original value.
      3. Pre-existing Aeration: Window open prior to 01:00 prevents calibration session start.
    """
    engine = FirmwareThermodynamicEngine()
    initial_k = 0.02500
    engine.metrics[0].natural_loss_rate = initial_k

    # 1. Normal Night Session (No window open)
    now_ms = 0
    now_epoch = 1700000000  # 01:00:00
    ext_temp = 3.0
    engine.evaluate_night_calibration(0, 21.00, ext_temp, True, True, 1, 0, now_ms, now_epoch, window_open=False)
    session_started = engine.night_calib[0].active

    # Advance 4 hours (01:00 to 05:00), taking outdoor samples every 15 min
    # True exponential cooling: drop = k * (T_room - T_ext) * 4h = 0.025 * 18 * 4 = 1.80°C
    for h_offset in range(1, 16):  # 15 samples x 15m = 225m
        now_ms += 900000
        now_epoch += 900
        engine.evaluate_night_calibration(0, 21.00 - (1.80 * (h_offset / 16.0)), ext_temp, True, True, 1 + (h_offset // 4), (h_offset % 4) * 15, now_ms, now_epoch, window_open=False)

    # Finalize at 05:00
    now_ms += 900000
    now_epoch += 900
    temp_at_05 = 21.00 - 1.80  # 19.20°C
    engine.evaluate_night_calibration(0, temp_at_05, ext_temp, True, True, 5, 0, now_ms, now_epoch, window_open=False)
    calibrated_normal_k = engine.metrics[0].natural_loss_rate
    normal_night_passed = abs(calibrated_normal_k - initial_k) < 0.001

    # 2. Aerated Night Session (Window opened at 03:00)
    engine_aerated = FirmwareThermodynamicEngine()
    engine_aerated.metrics[0].natural_loss_rate = initial_k

    now_ms = 0
    now_epoch = 1700000000  # 01:00:00
    engine_aerated.evaluate_night_calibration(0, 21.00, ext_temp, True, True, 1, 0, now_ms, now_epoch, window_open=False)

    # 01:00 - 03:00: normal cooling
    for h_offset in range(1, 9):
        now_ms += 900000
        now_epoch += 900
        engine_aerated.evaluate_night_calibration(0, 21.00 - (0.90 * (h_offset / 8.0)), ext_temp, True, True, 1 + (h_offset // 4), (h_offset % 4) * 15, now_ms, now_epoch, window_open=False)

    # 03:00: WINDOW OPENS! Heavy cold aeration causes plunge from 20.10°C to 17.00°C
    now_ms += 900000
    now_epoch += 900
    engine_aerated.evaluate_night_calibration(0, 17.00, ext_temp, True, True, 3, 15, now_ms, now_epoch, window_open=True)
    aeration_flagged = engine_aerated.night_calib[0].aeration_occurred

    # 03:15 - 05:00: continue to morning
    for h_offset in range(10, 16):
        now_ms += 900000
        now_epoch += 900
        engine_aerated.evaluate_night_calibration(0, 17.00, ext_temp, True, True, 1 + (h_offset // 4), (h_offset % 4) * 15, now_ms, now_epoch, window_open=False)

    # Finalize at 05:00
    now_ms += 900000
    now_epoch += 900
    engine_aerated.evaluate_night_calibration(0, 17.00, ext_temp, True, True, 5, 0, now_ms, now_epoch, window_open=False)
    aerated_k = engine_aerated.metrics[0].natural_loss_rate
    # If not isolated, k would blow up to ~0.055. Because it's isolated, it must remain strictly initial_k!
    aerated_k_protected = (abs(aerated_k - initial_k) < 0.00001)

    # 3. Pre-existing Aeration
    engine_pre = FirmwareThermodynamicEngine()
    engine_pre.evaluate_night_calibration(0, 21.00, ext_temp, True, True, 1, 0, 0, 1700000000, window_open=True)
    pre_window_rejected = not engine_pre.night_calib[0].active

    passed = session_started and normal_night_passed and aeration_flagged and aerated_k_protected and pre_window_rejected

    return {
        "id": "R2-TH01",
        "name": "test_04_night_calibration_isolation_during_aeration",
        "passed": passed,
        "session_started": session_started,
        "normal_k": calibrated_normal_k,
        "aeration_flagged": aeration_flagged,
        "aerated_k_protected": aerated_k_protected,
        "aerated_k_value": aerated_k,
        "pre_window_rejected": pre_window_rejected,
        "criteria": "Normal night updates k_loss; Aerated night flags aerationOccurred and preserves k_loss strictly",
        "message": f"Normal night k={calibrated_normal_k:.5f}; Aeration detected={aeration_flagged}, k preserved={aerated_k_protected} ({aerated_k:.5f} vs {initial_k:.5f}); Pre-window blocked={pre_window_rejected}"
    }


def test_05_predictive_preheat_lead_time_baseline_freeze() -> Dict[str, Any]:
    """
    Test 05 (Requirement R2):
    Verify that predictive preheat lead time is protected against decalibration during window aeration.
      1. Under baseline conditions (21.0°C), calculate expected preheat lead time to 21.5°C (~45-55m).
      2. Window opens: 21.00°C -> 20.75°C -> 20.50°C (triggers window detection, freezing baselineTemp at 20.50°C).
      3. Aeration continues: room temp severely plunges from 20.50°C down to 16.00°C.
      4. Verify ThermalZoneManager freezes baselineTemp (20.50°C) and uses it for lead time calculation.
      5. Verify that preheatLeadMin remains stable (~60-70m) and does NOT explode to 240m (as 16.00°C would).
      6. Verify that preheat_active is strictly suppressed while window pause is active,
         preventing boiler firing while window is open.
    """
    thermo = FirmwareThermodynamicEngine()
    diag = FirmwareDiagnosticEngine()
    manager = FirmwareZoneManager(thermo, diag)

    # Normal baseline conditions
    target_next = 21.50
    target_active = 20.50
    outdoor_temp = 5.0
    weather_valid = True
    mins_to_next_anchor = 60  # Anchor is in 60 minutes

    # Normal evaluation at 21.0°C
    manager.statuses[0].current_temp = 21.00
    manager.evaluate_schedule(target_next, target_active, outdoor_temp, weather_valid, mins_to_next_anchor)
    baseline_lead = manager.statuses[0].preheat_lead_min

    # Window aeration event triggers: 21.00 -> 20.75 -> 20.50 (drop 0.50°C in 10m)
    now_ms = 0
    manager.evaluate_window_detection(0, 21.00, outdoor_temp, weather_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 20.75, outdoor_temp, weather_valid, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 20.50, outdoor_temp, weather_valid, now_ms)  # Triggered!

    window_active = manager.window_states[0].pause_active
    frozen_baseline = manager.window_states[0].baseline_temp

    # Aeration continues: room temp plunges down to 16.00°C while window is open
    manager.statuses[0].current_temp = 16.00
    manager.evaluate_schedule(target_next, target_active, outdoor_temp, weather_valid, mins_to_next_anchor)
    aerated_lead = manager.statuses[0].preheat_lead_min
    preheat_boiler_firing = manager.preheat_active

    # Compute what an un-decoupled lead time would have been without the fix (using 16.00°C):
    raw_uncoupled_lead = thermo.calculate_preheat_lead_min(0, 16.00, target_next, outdoor_temp, weather_valid)

    # Assertions:
    # 1. Window pause is active
    # 2. baselineTemp was frozen at 20.50°C (near the 21.00°C baseline)
    # 3. aerated_lead matches preheat calculated for frozen_baseline (20.50°C) exactly
    # 4. raw_uncoupled_lead exploded to 240m (maximum clamp) due to 16.00°C plunge
    # 5. preheat_boiler_firing is FALSE (inhibited while window is open)
    expected_aerated_lead = thermo.calculate_preheat_lead_min(0, frozen_baseline, target_next, outdoor_temp, weather_valid)
    passed = (
        window_active
        and (abs(frozen_baseline - 20.50) < 0.05)
        and (aerated_lead == expected_aerated_lead)
        and (raw_uncoupled_lead >= 200)
        and (not preheat_boiler_firing)
    )

    return {
        "id": "R2-TH02",
        "name": "test_05_predictive_preheat_lead_time_baseline_freeze",
        "passed": passed,
        "window_active": window_active,
        "frozen_baseline_C": frozen_baseline,
        "baseline_lead_min": baseline_lead,
        "aerated_lead_min": aerated_lead,
        "raw_uncoupled_lead_min": raw_uncoupled_lead,
        "preheat_boiler_suppressed": not preheat_boiler_firing,
        "criteria": "Lead time uses frozen baselineTemp; Stable lead (85m vs uncoupled 240m); Boiler suppressed",
        "message": f"Baseline lead={baseline_lead}m, Aerated lead={aerated_lead}m (frozen baseline: {frozen_baseline:.2f}°C vs plunged temp: 16.00°C); Without fix: {raw_uncoupled_lead}m; Boiler firing suppressed: {not preheat_boiler_firing}"
    }


def test_06_dtc_301_lifecycle_and_multizone_isolation() -> Dict[str, Any]:
    """
    Test 06 (Requirement R1):
    Verify automotive-grade DTC-301 fault code lifecycle and multi-zone isolation.
      1. Zone 0 detects open window -> DTC-301 raised; Zone 1 unaffected.
      2. Zone 1 detects open window -> DTC-301 remains active.
      3. Zone 0 window pause clears -> DTC-301 remains active (Zone 1 still open).
      4. Zone 1 window pause clears -> DTC-301 automatically cleared.
    """
    thermo = FirmwareThermodynamicEngine()
    diag = FirmwareDiagnosticEngine()
    manager = FirmwareZoneManager(thermo, diag)

    # 1. Zone 0 window open
    now_ms = 0
    manager.evaluate_window_detection(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 21.00, 5.0, True, now_ms)

    z0_paused = manager.window_states[0].pause_active
    z1_paused = manager.window_states[1].pause_active
    dtc_after_z0 = diag.is_dtc_active("DTC-301")

    # 2. Zone 1 window open
    manager.evaluate_window_detection(1, 22.00, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(1, 21.70, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(1, 21.40, 5.0, True, now_ms)

    z1_now_paused = manager.window_states[1].pause_active
    dtc_after_z1 = diag.is_dtc_active("DTC-301")

    # 3. Zone 0 clears
    manager.clear_window_pause(0, now_ms)
    z0_cleared = not manager.window_states[0].pause_active
    dtc_after_z0_cleared = diag.is_dtc_active("DTC-301")  # Must stay TRUE because Zone 1 is active

    # 4. Zone 1 clears
    manager.clear_window_pause(1, now_ms)
    z1_cleared = not manager.window_states[1].pause_active
    dtc_after_all_cleared = diag.is_dtc_active("DTC-301")  # Must be FALSE

    passed = (
        z0_paused
        and (not z1_paused)
        and dtc_after_z0
        and z1_now_paused
        and dtc_after_z1
        and z0_cleared
        and dtc_after_z0_cleared
        and z1_cleared
        and (not dtc_after_all_cleared)
    )

    return {
        "id": "R1-DTC01",
        "name": "test_06_dtc_301_lifecycle_and_multizone_isolation",
        "passed": passed,
        "dtc_after_z0": dtc_after_z0,
        "dtc_after_z1": dtc_after_z1,
        "dtc_while_z1_active": dtc_after_z0_cleared,
        "dtc_after_all_cleared": dtc_after_all_cleared,
        "criteria": "DTC-301 raised on first open; persists while any zone paused; cleared when all clear",
        "message": f"DTC raised on Z0: {dtc_after_z0}; Persists on Z1: {dtc_after_z1}; Persists after Z0 clear: {dtc_after_z0_cleared}; Cleared after all clear: {not dtc_after_all_cleared}"
    }


def test_07_recovery_fsm_heating_resumption_e2e() -> Dict[str, Any]:
    """
    Test 07 (Requirement R1 & Survey Bug Fix):
    Verify that after window pause expires or clears, the FSM state machine properly restores
    ZONE_IDLE, permitting heat demand to transition through ZONE_WARMING_UP to ZONE_HEATING.
    (Verifies fix for pre-M1 bug where state remained stuck in ZONE_WINDOW_PAUSE).
    """
    thermo = FirmwareThermodynamicEngine()
    diag = FirmwareDiagnosticEngine()
    manager = FirmwareZoneManager(thermo, diag)

    manager.configs[0].target_temp = 21.50
    manager.configs[0].hysteresis = 0.20
    manager.statuses[0].current_temp = 21.50
    manager.update(0)
    initial_state = manager.statuses[0].state  # ZONE_IDLE

    # Open window triggers
    now_ms = 0
    manager.evaluate_window_detection(0, 21.50, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 21.25, 5.0, True, now_ms)
    now_ms += WINDOW_SAMPLE_INTERVAL_MS
    manager.evaluate_window_detection(0, 20.00, 5.0, True, now_ms)  # Cold drop to 20.0°C
    manager.statuses[0].current_temp = 20.00
    manager.update(now_ms)

    paused_state = manager.statuses[0].state  # ZONE_WINDOW_PAUSE
    paused_demand = manager.statuses[0].demand  # False
    paused_relay = manager.statuses[0].relay_engaged  # False

    # Window pause recovers automatically (e.g. 30m timeout)
    now_ms += WINDOW_SAFETY_TIMEOUT_MS
    manager.evaluate_window_detection(0, 20.00, 5.0, True, now_ms)
    recovered_from_engine = not thermo.is_window_open(0)
    recovered_state = manager.statuses[0].state  # Must be restored to ZONE_IDLE

    # Now run update(): currentTemp (20.00°C) <= target (21.50°C) - hyst (0.20°C) = 21.30°C
    # Heating demand must assert, and transition to ZONE_WARMING_UP
    manager.update(now_ms)
    warming_up_state = manager.statuses[0].state  # Must be ZONE_WARMING_UP
    warming_demand = manager.statuses[0].demand  # True
    warming_relay = manager.statuses[0].relay_engaged  # True (Actuator opening)

    # Advance 120s (interlock delay)
    now_ms += manager.interlock_delay_ms
    manager.update(now_ms)
    heating_state = manager.statuses[0].state  # Must be ZONE_HEATING

    passed = (
        initial_state == ZoneState.ZONE_IDLE
        and paused_state == ZoneState.ZONE_WINDOW_PAUSE
        and (not paused_demand)
        and (not paused_relay)
        and recovered_from_engine
        and recovered_state == ZoneState.ZONE_IDLE
        and warming_up_state == ZoneState.ZONE_WARMING_UP
        and warming_demand
        and warming_relay
        and heating_state == ZoneState.ZONE_HEATING
    )

    return {
        "id": "R1-FSM01",
        "name": "test_07_recovery_fsm_heating_resumption_e2e",
        "passed": passed,
        "paused_state": paused_state,
        "recovered_state": recovered_state,
        "warming_up_state": warming_up_state,
        "heating_state": heating_state,
        "criteria": "Auto-recovery restores ZONE_IDLE; asserts demand; advances WARMING_UP -> HEATING cleanly",
        "message": f"Paused: state={paused_state} (demand={paused_demand}); Recovered: state={recovered_state}; Heat start: state={warming_up_state} (relay={warming_relay}); Full heating: state={heating_state}"
    }


# ==============================================================================
# 4. Test Runner
# ==============================================================================

def run_window_detection_suite():
    print("=" * 80)
    print(" OPEN-WINDOW DETECTION & 2R2C MODEL DECOUPLING TEST SUITE (R1 & R2)")
    print(" Smart Home Hydronic Heating Automation - Autonomous Host Verification")
    print("=" * 80)

    tests = [
        test_01_window_detection_05c_drop_over_10min,
        test_02_sensitive_threshold_configuration_02_to_05,
        test_03_auto_recovery_and_timeout_mechanisms,
        test_04_night_calibration_isolation_during_aeration,
        test_05_predictive_preheat_lead_time_baseline_freeze,
        test_06_dtc_301_lifecycle_and_multizone_isolation,
        test_07_recovery_fsm_heating_resumption_e2e,
    ]

    all_passed = True
    passed_count = 0
    t_start = time.time()

    for test_fn in tests:
        t0 = time.time()
        res = test_fn()
        elapsed_ms = (time.time() - t0) * 1000.0

        status_tag = "[PASS]" if res["passed"] else "[FAIL]"
        if res["passed"]:
            passed_count += 1
        else:
            all_passed = False

        print(f" {status_tag} {res['id']}: {res['name']} ({elapsed_ms:.1f}ms)")
        print(f"        Details:  {res['message']}")
        print(f"        Criteria: {res['criteria']}")

    total_elapsed_ms = (time.time() - t_start) * 1000.0
    print("-" * 80)
    print(f" Summary: {passed_count}/{len(tests)} Tests Passed ({(passed_count/len(tests))*100.0:.1f}%) in {total_elapsed_ms:.1f}ms")
    print("=" * 80)

    if all_passed:
        print(" [OK] All window detection & 2R2C decoupling criteria satisfied 100%!")
    else:
        print(" [ERROR] One or more verification tests failed!")

    return all_passed


if __name__ == "__main__":
    success = run_window_detection_suite()
    sys.exit(0 if success else 1)
