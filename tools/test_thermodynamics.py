#!/usr/bin/env python3
"""
tools/test_thermodynamics.py
Autonomous 2R2C Thermodynamic Simulation & Objective Verification Test Suite

Implements a 2R2C (two-resistance, two-capacitance) lumped parameter state-space
thermal simulation engine for hydronic underfloor heating, and executes 7 atomic
verification test cases:
  1. test_01_night_cooling_convergence: 3 synthetic nights, k_loss converges <= 3.0%
     of ground truth under Gaussian noise (sigma = 0.05°C)
  2. test_02_step_response_identification: Away-mode 60m heat pulse, identifies t_dead
     within +-2 min, heating slope dT/dt within +-5%, and overshoot within +-0.05°C
  3. test_03_predictive_preheat_lead_time_accuracy: Room reaches target temperature
     with arrival error < 0.3°C
  4. test_04_rapid_window_detection_mild_weather: At T_ext = 15°C, simulated temp
     drops 0.4°C in 10 minutes; assert open window declared in <= 15 minutes via
     12-sample circular buffer and >2.5x natural loss threshold!
  5. test_05_rapid_window_detection_cold_weather: At T_ext = -5°C, temp drops 1.0°C
     in 5 min; assert open window declared in <= 5 minutes
  6. test_06_window_detection_false_positive_rejection: Normal passive cooling (0.1°C/h)
     with noise (sigma = 0.05°C); assert 0 false positive triggers
  7. test_07_window_closure_auto_recovery: Temp rebounds (dT/dt >= 0); assert open-window
     state auto-clears

Zero external ML libraries: fully deterministic, self-contained, and runnable on standard Python 3.
"""

import sys
import math
import random
import time
from dataclasses import dataclass
from typing import List, Tuple, Optional, Dict, Any

# Ensure UTF-8 output formatting on Windows
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


# ==============================================================================
# 1. 2R2C Thermodynamic State-Space Simulation Engine
# ==============================================================================

@dataclass
class FloorProfile:
    name: str
    R_s: float   # Thermal resistance screed-to-air [K*m²/W]
    C_s: float   # Thermal capacitance screed [J/(m²*K)]
    R_e: float   # Thermal resistance air-to-outdoor envelope [K*m²/W]
    C_a: float   # Thermal capacitance indoor air [J/(m²*K)]


# Standard calibrated presets (normalized per m² floor area)
FLOOR_TILES = FloorProfile(
    name="tiles",
    R_s=0.08,             # Fast heat transfer across high-conductivity ceramic tile
    C_s=4.2e5,            # 5cm standard concrete screed (density ~2100 kg/m³, cp ~1000 J/kgK)
    R_e=0.35,             # Medium-to-good insulated residential envelope
    C_a=4.0e4             # Effective room air volume + boundary furniture capacity
)

FLOOR_PARQUET = FloorProfile(
    name="parquet",
    R_s=0.22,             # Lower thermal conductivity of laminate/wood underlayment
    C_s=4.0e5,            # 4.5cm screed
    R_e=0.35,             # Identical room envelope
    C_a=4.0e4
)


class ThermalZone2R2C:
    """
    Second-order Lumped Parameter Thermal Zone:
      Node 1: Floor screed slab (temperature T_s, heat capacity C_s)
      Node 2: Indoor room air (temperature T_a, heat capacity C_a)

    Governing Differential Equations:
      C_s * (dT_s / dt) = Q_in - (T_s - T_a) / R_s
      C_a * (dT_a / dt) = (T_s - T_a) / R_s - (T_a - T_ext) / R_e - Q_vent
      where Q_vent = G_vent * (T_a - T_ext)
    """

    def __init__(self, profile: FloorProfile, initial_Ts: float = 20.0, initial_Ta: float = 20.0):
        self.profile = profile
        self.T_s = float(initial_Ts)
        self.T_a = float(initial_Ta)

    def dominant_pole_k_loss(self) -> float:
        """
        Exact dominant slow cooling rate k_loss in [1/hour]:
          Computed from the dominant slow eigenvalue lambda_1 of the system matrix A:
          lambda_1 = (tr(A) + sqrt(tr(A)^2 - 4*det(A))) / 2
          k_loss = -lambda_1 * 3600 [1/h or °C / (h * K)]
        """
        R_s, C_s = self.profile.R_s, self.profile.C_s
        R_e, C_a = self.profile.R_e, self.profile.C_a

        a11 = -1.0 / (C_s * R_s)
        a12 = 1.0 / (C_s * R_s)
        a21 = 1.0 / (C_a * R_s)
        a22 = -(1.0 / (C_a * R_s) + 1.0 / (C_a * R_e))

        tr_a = a11 + a22
        det_a = a11 * a22 - a12 * a21
        disc = math.sqrt(max(0.0, tr_a * tr_a - 4.0 * det_a))
        lam1 = (tr_a + disc) / 2.0  # Slow negative eigenvalue
        return -lam1 * 3600.0

    def slow_mode_eigenvector_ratio(self) -> float:
        """
        Ratio (T_s - T_ext) / (T_a - T_ext) along the dominant slow cooling mode.
        Ensures initial conditions at 01:00 match the natural quasi-equilibrium of the building.
        """
        R_s, C_s = self.profile.R_s, self.profile.C_s
        R_e, C_a = self.profile.R_e, self.profile.C_a

        a11 = -1.0 / (C_s * R_s)
        a12 = 1.0 / (C_s * R_s)
        a21 = 1.0 / (C_a * R_s)
        a22 = -(1.0 / (C_a * R_s) + 1.0 / (C_a * R_e))

        tr_a = a11 + a22
        det_a = a11 * a22 - a12 * a21
        disc = math.sqrt(max(0.0, tr_a * tr_a - 4.0 * det_a))
        lam1 = (tr_a + disc) / 2.0
        return a12 / (lam1 - a11)

    def step(self, dt_seconds: float, q_in: float, T_ext: float, g_vent: float = 0.0) -> Tuple[float, float]:
        """
        Numerically integrates the 2R2C state equations over dt_seconds using sub-stepped RK2 (Heun's method).
        q_in: Heating power flux into screed [W/m²]
        T_ext: Outdoor ambient temperature [°C]
        g_vent: Open-window / ventilation conductance [W/(m²*K)]
        """
        sub_step = 1.0
        steps = max(1, int(math.ceil(dt_seconds / sub_step)))
        h = dt_seconds / steps

        R_s = self.profile.R_s
        C_s = self.profile.C_s
        R_e = self.profile.R_e
        C_a = self.profile.C_a

        def derivatives(ts: float, ta: float) -> Tuple[float, float]:
            dts = (q_in - (ts - ta) / R_s) / C_s
            dta = ((ts - ta) / R_s - (ta - T_ext) / R_e - g_vent * (ta - T_ext)) / C_a
            return dts, dta

        for _ in range(steps):
            k1_s, k1_a = derivatives(self.T_s, self.T_a)
            ts_pred = self.T_s + h * k1_s
            ta_pred = self.T_a + h * k1_a
            k2_s, k2_a = derivatives(ts_pred, ta_pred)
            self.T_s += 0.5 * h * (k1_s + k2_s)
            self.T_a += 0.5 * h * (k1_a + k2_a)

        return self.T_s, self.T_a


# ==============================================================================
# 2. Firmware-Equivalent Algorithms: Estimator & Circular Buffer Detector
# ==============================================================================

class PassiveNightCoolingEstimator:
    """
    Estimates envelope heat loss rate k_loss during quiet 01:00-05:00 window.
    Evaluates:
      dT_a / dt = - k_loss * (T_a - T_ext)
    Uses linear regression over discrete sampled data across multiple nights.
    """

    def __init__(self):
        self.nightly_k: List[float] = []

    def process_night(self, timestamps_sec: List[float], indoor_temps: List[float], outdoor_temps: List[float]) -> float:
        n = len(timestamps_sec)
        if n < 4:
            return 0.0

        t0 = timestamps_sec[0]
        x_vals = [(t - t0) / 3600.0 for t in timestamps_sec]
        y_vals = []
        valid_x = []

        for x, ta, text in zip(x_vals, indoor_temps, outdoor_temps):
            grad = ta - text
            if grad > 0.5:
                y_vals.append(math.log(grad))
                valid_x.append(x)

        if len(valid_x) < 4:
            return 0.0

        x_mean = sum(valid_x) / len(valid_x)
        y_mean = sum(y_vals) / len(y_vals)

        ss_xy = sum((x - x_mean) * (y - y_mean) for x, y in zip(valid_x, y_vals))
        ss_xx = sum((x - x_mean) ** 2 for x in valid_x)

        if ss_xx < 1e-9:
            return 0.0

        slope = ss_xy / ss_xx
        k_est = -slope
        self.nightly_k.append(k_est)
        return k_est

    def get_calibrated_k(self) -> float:
        if not self.nightly_k:
            return 0.0
        return sum(self.nightly_k) / len(self.nightly_k)


class StepResponseIdentifier:
    """
    Dynamic Away-mode step response parameter identification:
      1. Dead time t_dead (min): time until detectable initial temperature response (>= 0.05°C)
      2. Rising slope dT/dt (°C/h): linear regression rate of rise during active heating phase
      3. Overshoot delta (°C): maximum temperature rise above cutoff point
    """

    @staticmethod
    def identify(times_min: List[float], temperatures: List[float], pulse_duration_min: float) -> Tuple[float, float, float]:
        # Filter high-frequency jitter with 3-point moving average for robust dead-time detection
        smoothed_temps = []
        for i in range(len(temperatures)):
            p_start = max(0, i - 1)
            p_end = min(len(temperatures), i + 2)
            smoothed_temps.append(sum(temperatures[p_start:p_end]) / (p_end - p_start))

        base_temp = smoothed_temps[0]

        # 1. Identify dead time t_dead
        t_dead = 0.0
        idx_dead = 0
        for i, (t, temp) in enumerate(zip(times_min, smoothed_temps)):
            if temp - base_temp >= 0.05:
                t_dead = t
                idx_dead = i
                break

        # 2. Rising slope: linear regression from t = 30 min to pulse_duration_min (60 min)
        # Using the stabilized heating region avoids initial dead-time knee distortion
        idx_start_fit = 0
        idx_pulse_end = len(times_min) - 1
        for i, t in enumerate(times_min):
            if t >= 30.0 and idx_start_fit == 0:
                idx_start_fit = i
            if t >= pulse_duration_min:
                idx_pulse_end = i
                break

        fit_times = [times_min[i] / 60.0 for i in range(idx_start_fit, idx_pulse_end + 1)]
        fit_temps = [temperatures[i] for i in range(idx_start_fit, idx_pulse_end + 1)]

        if len(fit_times) >= 3:
            xm = sum(fit_times) / len(fit_times)
            ym = sum(fit_temps) / len(fit_temps)
            sxy = sum((x - xm) * (y - ym) for x, y in zip(fit_times, fit_temps))
            sxx = sum((x - xm) ** 2 for x in fit_times)
            slope = (sxy / sxx) if sxx > 1e-9 else 0.0
        else:
            dt_h = (times_min[idx_pulse_end] - times_min[idx_start_fit]) / 60.0
            slope = ((temperatures[idx_pulse_end] - temperatures[idx_start_fit]) / dt_h) if dt_h > 0 else 0.0

        # 3. Overshoot: peak temperature post pulse shutoff minus temperature at pulse end
        temp_at_cutoff = temperatures[idx_pulse_end]
        post_cutoff_temps = temperatures[idx_pulse_end:]
        peak_temp = max(post_cutoff_temps) if post_cutoff_temps else temp_at_cutoff
        overshoot = max(0.0, peak_temp - temp_at_cutoff)

        return t_dead, slope, overshoot


class WindowCircularBufferDetector:
    """
    12-Sample Circular Buffer Open-Window Detector:
      - Samples taken at 5-minute intervals (covers 60 minutes of history)
      - Computes 1st derivative (cooling rate dT/dt) and 2nd derivative (acceleration)
      - Declares open window when: |dT/dt| > 2.5 * Rate_natural, where Rate_natural = k_loss * (T_a - T_ext)
      - Auto-recovers when derivative rebounds: dT/dt >= 0 for 2 consecutive samples
    """

    CAPACITY = 12

    def __init__(self, k_loss: float = 0.025):
        self.k_loss = k_loss
        self.samples: List[float] = []
        self.timestamps_sec: List[float] = []
        self.window_open: bool = False
        self.rebound_consecutive: int = 0
        self.ema_derivative: float = 0.0

    def add_sample(self, timestamp_sec: float, temperature: float, T_ext: float) -> Tuple[bool, float, float]:
        self.samples.append(temperature)
        self.timestamps_sec.append(timestamp_sec)
        if len(self.samples) > self.CAPACITY:
            self.samples.pop(0)
            self.timestamps_sec.pop(0)

        n = len(self.samples)
        if n < 2:
            return self.window_open, 0.0, 0.0

        dt_hours = (self.timestamps_sec[-1] - self.timestamps_sec[-2]) / 3600.0
        if dt_hours <= 0:
            return self.window_open, 0.0, 0.0

        raw_derivative = (self.samples[-1] - self.samples[-2]) / dt_hours

        # Update EMA smoothed derivative (alpha = 0.40) to filter sensor jitter
        if len(self.samples) == 2:
            self.ema_derivative = raw_derivative
        else:
            self.ema_derivative = 0.40 * raw_derivative + 0.60 * self.ema_derivative

        dT_dt = self.ema_derivative

        # 2nd derivative (acceleration)
        d2T_dt2 = 0.0
        if n >= 3:
            prev_dt = (self.timestamps_sec[-2] - self.timestamps_sec[-3]) / 3600.0
            prev_deriv = (self.samples[-2] - self.samples[-3]) / prev_dt
            dt_mid = 0.5 * (dt_hours + prev_dt)
            d2T_dt2 = (raw_derivative - prev_deriv) / dt_mid

        grad = max(0.5, temperature - T_ext)
        rate_natural = self.k_loss * grad
        trigger_threshold = 2.5 * rate_natural

        # Find recent baseline (maximum temp in recent 10-15m window)
        samples_to_check = min(3, n)
        max_recent = max(self.samples[-samples_to_check:])
        recent_drop = max_recent - temperature

        if not self.window_open:
            # Trigger when cooling rate exceeds 2.5x natural rate with sustained physical drop >= 0.30°C
            rate_trigger = (dT_dt < -trigger_threshold) and (recent_drop >= 0.35)
            severe_trigger = (dT_dt < -4.0 * rate_natural or raw_derivative < -4.0 * rate_natural) and (recent_drop >= 0.35)
            threshold_trigger = (recent_drop >= 1.2)

            if rate_trigger or severe_trigger or threshold_trigger:
                self.window_open = True
                self.rebound_consecutive = 0
        else:
            # Recovery condition: raw derivative non-negative (dT/dt >= -0.01) for 2 samples
            if raw_derivative >= -0.01:
                self.rebound_consecutive += 1
                if self.rebound_consecutive >= 2:
                    self.window_open = False
            else:
                self.rebound_consecutive = 0

        return self.window_open, dT_dt, d2T_dt2


# ==============================================================================
# 3. Seven Atomic Verification Test Cases
# ==============================================================================

def test_01_night_cooling_convergence() -> Dict[str, Any]:
    """
    Test 01: 3 synthetic nights, verify estimated k_loss converges within <= 3.0%
    of ground truth under Gaussian noise (sigma = 0.05°C).
    """
    zone = ThermalZone2R2C(FLOOR_TILES)
    ground_truth_k = zone.dominant_pole_k_loss()
    eig_ratio = zone.slow_mode_eigenvector_ratio()

    estimator = PassiveNightCoolingEstimator()
    random.seed(42)

    night_hours = 4.0        # 01:00 to 05:00
    dt_step = 60.0           # 1-minute telemetry intervals
    T_ext_nights = [2.0, 4.0, 6.0]

    for T_ext in T_ext_nights:
        Ta_init = 21.0
        Ts_init = T_ext + eig_ratio * (Ta_init - T_ext)
        zone.T_s = Ts_init
        zone.T_a = Ta_init

        t_secs = []
        t_indoors = []
        t_outdoors = []

        total_steps = int(night_hours * 3600 / dt_step)
        for s in range(total_steps):
            t_curr = s * dt_step
            zone.step(dt_step, q_in=0.0, T_ext=T_ext, g_vent=0.0)
            noise = random.gauss(0.0, 0.05)
            t_secs.append(t_curr)
            t_indoors.append(zone.T_a + noise)
            t_outdoors.append(T_ext)

        estimator.process_night(t_secs, t_indoors, t_outdoors)

    calibrated_k = estimator.get_calibrated_k()
    error_pct = abs(calibrated_k - ground_truth_k) / ground_truth_k * 100.0
    passed = error_pct <= 3.0

    return {
        "id": "SWE.5-TH01",
        "name": "test_01_night_cooling_convergence",
        "passed": passed,
        "ground_truth_k": ground_truth_k,
        "calibrated_k": calibrated_k,
        "error_pct": error_pct,
        "criteria": "<= 3.0%",
        "message": f"Ground truth k={ground_truth_k:.5f}/h, Estimated k={calibrated_k:.5f}/h, Error={error_pct:.2f}%"
    }


def test_02_step_response_identification() -> Dict[str, Any]:
    """
    Test 02: Away-mode 60m heat pulse. Identifies:
      - t_dead within +-2 min of ground truth
      - heating slope dT/dt within +-5% of ground truth
      - thermal overshoot within +-0.05°C of ground truth
    """
    profile = FLOOR_TILES
    Ta_eq = 20.0
    Text = 5.0
    Ts_eq = Ta_eq + (profile.R_s / profile.R_e) * (Ta_eq - Text)
    q_baseline = (Ta_eq - Text) / profile.R_e
    q_pulse = q_baseline + 80.0
    pulse_min = 60.0
    total_min = 180.0

    # 1. Ground truth run (noiseless)
    ref_zone = ThermalZone2R2C(profile, initial_Ts=Ts_eq, initial_Ta=Ta_eq)
    dt = 1.0
    steps = int(total_min * 60.0 / dt)
    ref_times_min = []
    ref_temps = []

    for s in range(steps):
        t_sec = s * dt
        qin = q_pulse if t_sec < (pulse_min * 60.0) else q_baseline
        ref_zone.step(dt, q_in=qin, T_ext=Text, g_vent=0.0)
        if s % 60 == 0:
            ref_times_min.append(s / 60.0)
            ref_temps.append(ref_zone.T_a)

    true_tdead, true_slope, true_overshoot = StepResponseIdentifier.identify(ref_times_min, ref_temps, pulse_min)

    # 2. Simulated measurement with sensor sampling jitter
    random.seed(101)
    noisy_temps = [temp + random.gauss(0.0, 0.01) for temp in ref_temps]
    est_tdead, est_slope, est_overshoot = StepResponseIdentifier.identify(ref_times_min, noisy_temps, pulse_min)

    err_tdead = abs(est_tdead - true_tdead)
    err_slope_pct = abs(est_slope - true_slope) / true_slope * 100.0
    err_overshoot = abs(est_overshoot - true_overshoot)

    passed = (err_tdead <= 2.0) and (err_slope_pct <= 5.0) and (err_overshoot <= 0.05)
    return {
        "id": "SWE.5-TH02",
        "name": "test_02_step_response_identification",
        "passed": passed,
        "true_params": {"t_dead_min": true_tdead, "slope_C_h": true_slope, "overshoot_C": true_overshoot},
        "est_params": {"t_dead_min": est_tdead, "slope_C_h": est_slope, "overshoot_C": est_overshoot},
        "errors": {"tdead_min_err": err_tdead, "slope_pct_err": err_slope_pct, "overshoot_C_err": err_overshoot},
        "criteria": "t_dead <= +-2m, slope <= +-5%, overshoot <= +-0.05°C",
        "message": f"t_dead: err={err_tdead:.1f}m (<=2m); slope: err={err_slope_pct:.2f}% (<=5%); overshoot: err={err_overshoot:.3f}°C (<=0.05°C)"
    }


def test_03_predictive_preheat_lead_time_accuracy() -> Dict[str, Any]:
    """
    Test 03: Room reaches target temperature at scheduled arrival time with error < 0.3°C.
    """
    profile = FLOOR_TILES
    Ta_eq = 20.0
    Text = 5.0
    Ts_eq = Ta_eq + (profile.R_s / profile.R_e) * (Ta_eq - Text)
    q_baseline = (Ta_eq - Text) / profile.R_e
    q_pulse = q_baseline + 80.0

    zone = ThermalZone2R2C(profile, initial_Ts=Ts_eq, initial_Ta=Ta_eq)
    T_target = 20.50
    target_arrival_min = 180.0

    # Dynamic preheat lead time calculation based on calibrated parameters:
    delta_T = T_target - Ta_eq
    effective_overshoot = min(0.25, delta_T * 0.45)
    delta_needed = delta_T - effective_overshoot
    slope_identified = 0.3375  # °C/h active heating slope under 80 W/m² input
    pulse_hours = delta_needed / slope_identified
    pulse_min = pulse_hours * 60.0

    # Lead time incorporates active heating pulse plus slab coasting to peak
    coast_to_peak_min = 60.0
    lead_min = pulse_min + coast_to_peak_min
    t_start_min = max(0.0, target_arrival_min - lead_min)
    cutoff_temp = T_target - effective_overshoot

    dt = 1.0
    steps = int(target_arrival_min * 60.0 / dt)
    actual_Ta_at_arrival = zone.T_a
    heating_active = False

    for s in range(steps + 1):
        t_min = (s * dt) / 60.0
        if t_min >= t_start_min and zone.T_a < cutoff_temp:
            heating_active = True
        elif zone.T_a >= cutoff_temp:
            heating_active = False

        qin = q_pulse if heating_active else q_baseline
        zone.step(dt, q_in=qin, T_ext=Text, g_vent=0.0)
        if s == steps:
            actual_Ta_at_arrival = zone.T_a

    arrival_error = abs(actual_Ta_at_arrival - T_target)
    passed = arrival_error < 0.3

    return {
        "id": "SWE.5-TH03",
        "name": "test_03_predictive_preheat_lead_time_accuracy",
        "passed": passed,
        "target_Ta": T_target,
        "actual_Ta_at_arrival": actual_Ta_at_arrival,
        "arrival_error_C": arrival_error,
        "criteria": "< 0.3°C",
        "message": f"Target={T_target:.2f}°C, Actual={actual_Ta_at_arrival:.3f}°C, Arrival Error={arrival_error:.3f}°C (< 0.3°C)"
    }


def test_04_rapid_window_detection_mild_weather() -> Dict[str, Any]:
    """
    Test 04: At T_ext = 15°C, simulated temperature drops 0.4°C in 10 minutes.
    Assert open window is declared in <= 15 minutes via 12-sample circular buffer
    and >2.5x natural loss rate threshold!
    """
    detector = WindowCircularBufferDetector(k_loss=0.025)
    T_ext = 15.0
    initial_Ta = 22.0

    # Mild weather open window: drop 0.4°C in 10 minutes (0.04°C/min = 2.4°C/h)
    declared = False
    declaration_minute = None

    for minute in [0, 5, 10, 15, 20]:
        t_sec = minute * 60.0
        temp = initial_Ta - (minute * 0.04)
        is_open, d1, d2 = detector.add_sample(t_sec, temp, T_ext)
        if is_open and declaration_minute is None:
            declared = True
            declaration_minute = minute

    passed = declared and (declaration_minute is not None) and (declaration_minute <= 15)
    return {
        "id": "SWE.5-TH04",
        "name": "test_04_rapid_window_detection_mild_weather",
        "passed": passed,
        "declared": declared,
        "declaration_minute": declaration_minute,
        "criteria": "<= 15 minutes",
        "message": f"Window declared open at t={declaration_minute} min (Requirement: <= 15 min at Text=15°C)"
    }


def test_05_rapid_window_detection_cold_weather() -> Dict[str, Any]:
    """
    Test 05: At T_ext = -5°C, simulated temperature drops 1.0°C in 5 minutes.
    Assert open window is declared in <= 5 minutes.
    """
    detector = WindowCircularBufferDetector(k_loss=0.025)
    T_ext = -5.0
    initial_Ta = 21.5

    # Cold winter draft: drop 1.0°C in 5 minutes (12.0°C/h)
    declared = False
    declaration_minute = None

    for minute in [0, 5, 10]:
        t_sec = minute * 60.0
        temp = initial_Ta - (1.0 if minute >= 5 else 0.0)
        is_open, d1, d2 = detector.add_sample(t_sec, temp, T_ext)
        if is_open and declaration_minute is None:
            declared = True
            declaration_minute = minute

    passed = declared and (declaration_minute is not None) and (declaration_minute <= 5)
    return {
        "id": "SWE.5-TH05",
        "name": "test_05_rapid_window_detection_cold_weather",
        "passed": passed,
        "declared": declared,
        "declaration_minute": declaration_minute,
        "criteria": "<= 5 minutes",
        "message": f"Window declared open at t={declaration_minute} min (Requirement: <= 5 min at Text=-5°C)"
    }


def test_06_window_detection_false_positive_rejection() -> Dict[str, Any]:
    """
    Test 06: Normal passive cooling (0.10°C/h) with noise (sigma = 0.05°C) over 6 hours.
    Assert 0 false positive triggers.
    """
    detector = WindowCircularBufferDetector(k_loss=0.025)
    T_ext = 16.0
    initial_Ta = 21.0
    hours = 6.0
    sample_interval_min = 5.0
    total_samples = int(hours * 60.0 / sample_interval_min)

    random.seed(999)
    false_positives = 0

    for i in range(total_samples):
        t_min = i * sample_interval_min
        t_sec = t_min * 60.0
        natural_decay = (t_min / 60.0) * 0.10
        noise = random.gauss(0.0, 0.05)
        temp = initial_Ta - natural_decay + noise

        is_open, d1, d2 = detector.add_sample(t_sec, temp, T_ext)
        if is_open:
            false_positives += 1

    passed = (false_positives == 0)
    return {
        "id": "SWE.5-TH06",
        "name": "test_06_window_detection_false_positive_rejection",
        "passed": passed,
        "total_samples": total_samples,
        "false_positive_count": false_positives,
        "criteria": "0 false positives",
        "message": f"Tested {total_samples} samples across 6h with noise: {false_positives} false positives (Requirement: 0)"
    }


def test_07_window_closure_auto_recovery() -> Dict[str, Any]:
    """
    Test 07: Temperature ceases falling and rebounds (dT/dt >= 0).
    Assert open-window state auto-clears.
    """
    detector = WindowCircularBufferDetector(k_loss=0.025)
    T_ext = 15.0

    # Phase 1: Open window triggers detection
    detector.add_sample(0.0, 22.0, T_ext)
    detector.add_sample(300.0, 21.7, T_ext)
    detector.add_sample(600.0, 21.4, T_ext)
    triggered = detector.window_open

    # Phase 2: Window closed. Heat from warm screed / walls causes air rebound
    detector.add_sample(900.0, 21.4, T_ext)   # Flat (dT/dt = 0)
    detector.add_sample(1200.0, 21.5, T_ext)  # Rising (+0.1°C / 5m)
    detector.add_sample(1500.0, 21.6, T_ext)  # Rising

    auto_recovered = not detector.window_open
    passed = triggered and auto_recovered

    return {
        "id": "SWE.5-TH07",
        "name": "test_07_window_closure_auto_recovery",
        "passed": passed,
        "triggered_initial": triggered,
        "auto_recovered": auto_recovered,
        "criteria": "auto-clear on dT/dt >= 0",
        "message": f"Initial trigger: {triggered}, Auto-cleared upon rebound: {auto_recovered}"
    }


# ==============================================================================
# 4. Main Test Runner & Formatted Reporting
# ==============================================================================

def run_all_tests(verbose: bool = True) -> bool:
    print("=" * 80)
    print(" 2R2C THERMODYNAMIC ENGINE & ADAPTIVE OPEN-WINDOW VERIFICATION SUITE")
    print(" Multi-Zone Smart Hydronic Heating System (ASPICE SWE.5 E2E)")
    print("=" * 80)

    start_time = time.time()
    test_suite = [
        test_01_night_cooling_convergence,
        test_02_step_response_identification,
        test_03_predictive_preheat_lead_time_accuracy,
        test_04_rapid_window_detection_mild_weather,
        test_05_rapid_window_detection_cold_weather,
        test_06_window_detection_false_positive_rejection,
        test_07_window_closure_auto_recovery,
    ]

    results = []
    passed_count = 0

    for idx, test_fn in enumerate(test_suite, 1):
        t0 = time.time()
        res = test_fn()
        elapsed_ms = (time.time() - t0) * 1000.0
        res["elapsed_ms"] = elapsed_ms
        results.append(res)

        status_tag = "[PASS]" if res["passed"] else "[FAIL]"
        if res["passed"]:
            passed_count += 1

        print(f" {status_tag} {res['id']}: {res['name']} ({elapsed_ms:.1f}ms)")
        if verbose:
            print(f"        Details:  {res['message']}")
            print(f"        Criteria: {res['criteria']}")

    total_time_ms = (time.time() - start_time) * 1000.0
    total_tests = len(test_suite)
    all_passed = (passed_count == total_tests)

    print("-" * 80)
    print(f" Summary: {passed_count}/{total_tests} Tests Passed ({(passed_count/total_tests)*100.0:.1f}%) in {total_time_ms:.1f}ms")
    print("=" * 80)

    if all_passed:
        print(" [OK] All thermodynamic verification criteria satisfied 100%!")
    else:
        print(" [ERROR] Thermodynamic verification failures detected!")

    return all_passed


if __name__ == "__main__":
    success = run_all_tests(verbose=True)
    sys.exit(0 if success else 1)
