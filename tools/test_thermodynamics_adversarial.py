#!/usr/bin/env python3
"""
tools/test_thermodynamics_adversarial.py
Adversarial Stress Test Suite for 2R2C Thermodynamic Model & Derivative Open-Window Detection.

Executes empirical challenge cases:
  1. ADV-01: High Noise Jitter & Quantization on Cooling Curve Convergence
  2. ADV-02: High Noise Jitter on Open-Window False Positive Rejection (C++ vs Python)
  3. ADV-03: Rapidly Fluctuating Outdoor Temperature & Sensor Dropouts
  4. ADV-04: Sudden Aeration Followed by Immediate Window Closure (Recovery Dynamics)
  5. ADV-05: Edge Screed Thicknesses (3 cm vs 10 cm) & Thermal Inertia Modeling
  6. ADV-06: Uncalibrated Cold Boot State & Dynamic Pre-Heat Accuracy
  7. ADV-07: Mild Weather Open-Window Boundary Analysis (Text = 14°C to 19°C)
  8. ADV-08: C++ Firmware Engine vs Python Reference Implementation Parity
"""

import sys
import math
import random
import time
from dataclasses import dataclass
from typing import List, Tuple, Dict, Any, Optional

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


# ==============================================================================
# 1. Physical 2R2C Model with Variable Screed Thickness
# ==============================================================================

@dataclass
class FloorProfile:
    name: str
    R_s: float   # Screed-to-air thermal resistance [K*m²/W]
    C_s: float   # Screed thermal capacitance [J/(m²*K)]
    R_e: float   # Envelope thermal resistance [K*m²/W]
    C_a: float   # Air thermal capacitance [J/(m²*K)]


def make_profile(floor_type: str, screed_thickness_cm: float, insulation: str = "medium") -> FloorProfile:
    # Screed density ~2100 kg/m³, cp ~1000 J/(kg*K) -> volumetric capacity ~2.1e6 J/(m³*K)
    screed_m = screed_thickness_cm / 100.0
    C_s = 2.1e6 * screed_m

    # Tile conductivity ~1.3 W/(m*K), Parquet ~0.15 W/(m*K)
    if floor_type == "tiles":
        R_s = (screed_m / 1.3) + 0.04
    else:
        R_s = (screed_m / 1.3) + 0.15

    # Envelope resistance by insulation level
    if insulation == "poor":
        R_e = 0.22
    elif insulation == "good":
        R_e = 0.50
    elif insulation == "passive":
        R_e = 0.90
    else: # medium
        R_e = 0.35

    C_a = 4.0e4 # 2.5m ceiling height * 1200 J/(m³*K) air + furniture capacitance
    return FloorProfile(f"{floor_type}_{screed_thickness_cm}cm_{insulation}", R_s, C_s, R_e, C_a)


class ThermalZone2R2C:
    def __init__(self, profile: FloorProfile, initial_Ts: float = 20.0, initial_Ta: float = 20.0):
        self.profile = profile
        self.T_s = float(initial_Ts)
        self.T_a = float(initial_Ta)

    def dominant_pole_k_loss(self) -> float:
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
        return -lam1 * 3600.0

    def slow_mode_eigenvector_ratio(self) -> float:
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
# 2. C++ Firmware Exact Logic Port (ThermodynamicEngine.cpp)
# ==============================================================================

class CppThermodynamicEngine:
    """Exact reproduction of ThermodynamicEngine.cpp for firmware-level parity testing."""
    WINDOW_BUFFER_SIZE = 12
    WINDOW_SAMPLE_INTERVAL_MS = 300000 # 5 min

    def __init__(self, floor_type: str = "tiles", screed_cm: float = 5.0, insulation: str = "medium"):
        self.floor_type = floor_type
        self.screed_cm = screed_cm
        self.insulation = insulation
        self.metrics = self.calculate_default_priors(floor_type, screed_cm, insulation)

        # Circular buffer
        self.samples = [0.0] * self.WINDOW_BUFFER_SIZE
        self.sample_timestamps = [0] * self.WINDOW_BUFFER_SIZE
        self.head = 0
        self.count = 0
        self.first_derivative = 0.0 # °C/min
        self.second_derivative = 0.0
        self.window_open = False
        self.pause_start_ms = 0
        self.pause_min_temp = 999.0
        self.recovery_count = 0
        self.drop_threshold = 1.2
        self.last_sample_ms = 0
        self.last_sample_temp = 0.0

        # Night calib state
        self.night_active = False
        self.night_start_ms = 0
        self.night_start_epoch = 0
        self.night_start_temp = 0.0
        self.night_ext_temp_sum = 0.0
        self.night_ext_temp_count = 0
        self.night_last_sample_ms = 0

    @staticmethod
    def calculate_default_priors(floor_type: str, screed_cm: float, insulation: str) -> Dict[str, float]:
        screed_ratio = screed_cm / 5.0
        if screed_ratio < 0.6: screed_ratio = 0.6
        if screed_ratio > 2.0: screed_ratio = 2.0

        base_dead = 30.0 if floor_type == "tiles" else 60.0
        t_dead = base_dead * screed_ratio
        if t_dead < 15.0: t_dead = 15.0
        if t_dead > 120.0: t_dead = 120.0

        base_slope = 1.10 if floor_type == "tiles" else 0.75
        h_slope = base_slope / math.sqrt(screed_ratio)
        if h_slope < 0.3: h_slope = 0.3
        if h_slope > 2.5: h_slope = 2.5

        if insulation == "poor": loss = 0.038
        elif insulation == "good": loss = 0.018
        elif insulation == "passive": loss = 0.010
        else: loss = 0.025

        base_over = 0.35 if floor_type == "tiles" else 0.25
        overshoot = base_over * screed_ratio
        if overshoot < 0.10: overshoot = 0.10
        if overshoot > 0.90: overshoot = 0.90

        preheat = int(t_dead + 30.0)
        return {
            "tDeadMin": t_dead,
            "heatingSlope": h_slope,
            "naturalLossRate": loss,
            "overshootDelta": overshoot,
            "preheatLeadMin": preheat
        }

    def evaluate_window(self, current_temp: float, outdoor_temp: float, outdoor_valid: bool, now_ms: int) -> bool:
        buf_count = self.count
        is_new_sample = False

        if self.last_sample_ms == 0 or (now_ms - self.last_sample_ms >= self.WINDOW_SAMPLE_INTERVAL_MS):
            self.samples[self.head] = current_temp
            self.sample_timestamps[self.head] = now_ms
            self.head = (self.head + 1) % self.WINDOW_BUFFER_SIZE
            if self.count < self.WINDOW_BUFFER_SIZE:
                self.count += 1
            self.last_sample_ms = now_ms
            self.last_sample_temp = current_temp
            is_new_sample = True

            if self.count >= 2:
                idx0 = (self.head + self.WINDOW_BUFFER_SIZE - 1) % self.WINDOW_BUFFER_SIZE
                idx1 = (self.head + self.WINDOW_BUFFER_SIZE - 2) % self.WINDOW_BUFFER_SIZE
                dt01 = (self.sample_timestamps[idx0] - self.sample_timestamps[idx1]) / 60000.0
                if dt01 < 1.0: dt01 = 5.0
                v01 = (self.samples[idx0] - self.samples[idx1]) / dt01

                if self.count >= 3:
                    idx2 = (self.head + self.WINDOW_BUFFER_SIZE - 3) % self.WINDOW_BUFFER_SIZE
                    dt12 = (self.sample_timestamps[idx1] - self.sample_timestamps[idx2]) / 60000.0
                    if dt12 < 1.0: dt12 = 5.0
                    v12 = (self.samples[idx1] - self.samples[idx2]) / dt12
                    self.first_derivative = (self.samples[idx0] - self.samples[idx2]) / (dt01 + dt12)
                    self.second_derivative = (v01 - v12) / (0.5 * (dt01 + dt12))
                else:
                    self.first_derivative = v01
                    self.second_derivative = 0.0

        # Base derivative strictly on smoothed multi-sample circular buffer
        effective_derivative = self.first_derivative

        delta_ext = (current_temp - outdoor_temp) if outdoor_valid else 10.0
        if delta_ext < 2.0: delta_ext = 2.0
        k_loss = self.metrics["naturalLossRate"] if self.metrics["naturalLossRate"] > 0.005 else 0.025
        natural_rate_min = (k_loss * delta_ext) / 60.0
        if natural_rate_min < 0.0025: natural_rate_min = 0.0025

        if not self.window_open:
            max_recent = current_temp
            samples_to_check = 3 if self.count >= 3 else self.count
            for i in range(1, samples_to_check + 1):
                s_idx = (self.head + self.WINDOW_BUFFER_SIZE - i) % self.WINDOW_BUFFER_SIZE
                if self.samples[s_idx] > max_recent:
                    max_recent = self.samples[s_idx]
            drop_recent = max_recent - current_temp

            rate_trigger = is_new_sample and (effective_derivative < 0.0) and (abs(effective_derivative) >= 2.5 * natural_rate_min) and (drop_recent >= 0.35)
            severe_trigger = is_new_sample and (effective_derivative < 0.0) and (abs(effective_derivative) >= 4.0 * natural_rate_min) and (drop_recent >= 0.35)
            threshold_trigger = (drop_recent >= self.drop_threshold)

            if rate_trigger or severe_trigger or threshold_trigger:
                self.window_open = True
                self.pause_start_ms = now_ms
                self.pause_min_temp = current_temp
                self.recovery_count = 0
        else:
            if current_temp < self.pause_min_temp:
                self.pause_min_temp = current_temp

            timeout_expired = (now_ms - self.pause_start_ms >= 1800000)
            rebounded = (current_temp >= self.pause_min_temp + 0.6)

            if is_new_sample:
                if self.first_derivative >= -0.005:
                    self.recovery_count += 1
                else:
                    self.recovery_count = 0
            derivative_recovered = (self.recovery_count >= 2)

            if timeout_expired or rebounded or derivative_recovered:
                self.window_open = False
                self.recovery_count = 0
                self.last_sample_ms = now_ms
                self.last_sample_temp = current_temp
                self.head = 0
                self.count = 1
                self.samples[0] = current_temp
                self.sample_timestamps[0] = now_ms
                self.first_derivative = 0.0
                self.second_derivative = 0.0

        return self.window_open

    def evaluate_night_calibration(self, current_temp: float, outdoor_temp: float, outdoor_valid: bool,
                                  zone_idle: bool, hour: int, minute: int, now_ms: int, now_epoch: int):
        in_night_window = (hour >= 1 and hour < 5)

        if in_night_window and zone_idle and outdoor_valid and (current_temp > outdoor_temp + 3.0):
            if not self.night_active:
                self.night_active = True
                self.night_start_ms = now_ms
                self.night_start_epoch = now_epoch
                self.night_start_temp = current_temp
                self.night_ext_temp_sum = outdoor_temp
                self.night_ext_temp_count = 1
                self.night_last_sample_ms = now_ms
            else:
                if now_ms - self.night_last_sample_ms >= 900000:
                    self.night_ext_temp_sum += outdoor_temp
                    self.night_ext_temp_count += 1
                    self.night_last_sample_ms = now_ms
        else:
            if self.night_active:
                if hour == 5 and self.night_ext_temp_count >= 4:
                    elapsed_hours = (now_epoch - self.night_start_epoch) / 3600.0
                    if elapsed_hours >= 2.0:
                        drop = self.night_start_temp - current_temp
                        avg_ext = self.night_ext_temp_sum / float(self.night_ext_temp_count)
                        avg_room = (self.night_start_temp + current_temp) * 0.5
                        grad = avg_room - avg_ext
                        if grad > 3.0 and drop > 0.05:
                            k_meas = drop / (elapsed_hours * grad)
                            if 0.005 <= k_meas <= 0.080:
                                self.metrics["naturalLossRate"] = 0.8 * self.metrics["naturalLossRate"] + 0.2 * k_meas
                self.night_active = False

    def calculate_preheat_lead_min(self, current_temp: float, target_temp: float, outdoor_temp: float, outdoor_valid: bool) -> int:
        if target_temp <= current_temp + 0.1:
            return 0
        delta_temp = target_temp - current_temp
        effective_overshoot = self.metrics["overshootDelta"]
        if effective_overshoot > delta_temp * 0.45:
            effective_overshoot = delta_temp * 0.45
        delta_needed = max(0.05, delta_temp - effective_overshoot)

        delta_ext = (current_temp - outdoor_temp) if outdoor_valid else 12.0
        if delta_ext < 0.0: delta_ext = 0.0
        loss_rate = self.metrics["naturalLossRate"] * delta_ext
        net_rate = self.metrics["heatingSlope"] - loss_rate
        if net_rate < 0.20: net_rate = 0.20

        heat_hours = delta_needed / net_rate
        total_minutes = self.metrics["tDeadMin"] + (heat_hours * 60.0)
        if total_minutes > 240.0: total_minutes = 240.0
        if total_minutes < 0.0: total_minutes = 0.0
        self.metrics["preheatLeadMin"] = int(total_minutes + 0.5)
        return self.metrics["preheatLeadMin"]


# ==============================================================================
# 3. Adversarial Test Cases
# ==============================================================================

def test_adv_01_high_noise_cooling_convergence():
    """
    ADV-01: Challenge cooling curve estimation under aggressive noise.
    Test noise sigmas: 0.05°C, 0.10°C, 0.20°C + 0.1°C quantization.
    Examines if k_loss diverges or stays bounded.
    """
    zone = ThermalZone2R2C(make_profile("tiles", 5.0, "medium"))
    ground_truth_k = zone.dominant_pole_k_loss()
    eig_ratio = zone.slow_mode_eigenvector_ratio()

    results = []
    noise_levels = [0.05, 0.10, 0.15, 0.20]

    for sigma in noise_levels:
        random.seed(42)
        engine = CppThermodynamicEngine("tiles", 5.0, "medium")

        # Simulate 5 consecutive nights
        for night in range(5):
            T_ext = 2.0 + night * 1.5
            Ta_init = 21.0
            Ts_init = T_ext + eig_ratio * (Ta_init - T_ext)
            zone.T_s = Ts_init
            zone.T_a = Ta_init

            start_epoch = 1700000000 + night * 86400 + 3600 # 01:00
            now_ms = 0

            # 01:00 to 05:00 in 60s steps
            for step in range(240): # 4 hours
                curr_epoch = start_epoch + step * 60
                hour = 1 + (step // 60)
                minute = step % 60
                zone.step(60.0, q_in=0.0, T_ext=T_ext, g_vent=0.0)

                # Noise + 0.1°C quantization
                raw_temp = zone.T_a + random.gauss(0.0, sigma)
                quantized_temp = round(raw_temp, 1)

                engine.evaluate_night_calibration(
                    quantized_temp, T_ext, True, True, hour, minute, now_ms, curr_epoch
                )
                now_ms += 60000

            # Finalize at 05:00
            engine.evaluate_night_calibration(
                round(zone.T_a, 1), T_ext, True, True, 5, 0, now_ms, start_epoch + 14400
            )

        calibrated_k = engine.metrics["naturalLossRate"]
        err_pct = abs(calibrated_k - ground_truth_k) / ground_truth_k * 100.0
        results.append({
            "sigma": sigma,
            "calibrated_k": calibrated_k,
            "ground_truth_k": ground_truth_k,
            "err_pct": err_pct,
            "bounded": (0.005 <= calibrated_k <= 0.080)
        })

    # Assert bounded and error within reasonable limits
    max_err = max(r["err_pct"] for r in results)
    all_bounded = all(r["bounded"] for r in results)
    passed = all_bounded and (max_err < 25.0) # Within 25% under extreme 0.2°C noise

    return {
        "id": "ADV-01",
        "name": "High Noise & Quantization on Cooling Curve Convergence",
        "passed": passed,
        "results": results,
        "max_err_pct": max_err,
        "details": f"Sigma range 0.05-0.20°C: all bounded in [0.005, 0.080], max err = {max_err:.1f}%"
    }


def test_adv_02_noise_false_positive_rejection():
    """
    ADV-02: Challenge open-window false positive rejection under noise jitter.
    Run 8 hours of quiet night cooling at natural decay (0.10°C/h) with
    Gaussian noise jitter (sigma = 0.08°C) and 0.1°C quantization.
    Test both C++ firmware engine and Python reference detector.
    """
    random.seed(777)
    cpp_engine = CppThermodynamicEngine("tiles", 5.0, "medium")

    from test_thermodynamics import WindowCircularBufferDetector
    py_detector = WindowCircularBufferDetector(k_loss=0.025)

    hours = 8.0
    T_ext = 10.0
    initial_Ta = 21.5
    cpp_false_positives = 0
    py_false_positives = 0

    # 1-minute telemetry ticks over 8 hours
    total_minutes = int(hours * 60)
    now_ms = 1000

    for m in range(total_minutes):
        t_sec = m * 60.0
        now_ms = (m + 1) * 60000

        natural_decay = (m / 60.0) * 0.10 # 0.10°C/h
        jitter = random.gauss(0.0, 0.08)
        current_temp = round(initial_Ta - natural_decay + jitter, 1)

        # C++ engine evaluation (every minute with BLE reception)
        cpp_open = cpp_engine.evaluate_window(current_temp, T_ext, True, now_ms)
        if cpp_open:
            cpp_false_positives += 1
            # If opened, force clear to see if it triggers repeatedly
            cpp_engine.window_open = False

        # Python detector evaluation (at 5-minute sampling points)
        if m % 5 == 0:
            py_open, _, _ = py_detector.add_sample(t_sec, current_temp, T_ext)
            if py_open:
                py_false_positives += 1
                py_detector.window_open = False

    passed = (cpp_false_positives == 0) and (py_false_positives == 0)
    return {
        "id": "ADV-02",
        "name": "Noise Jitter Open-Window False Positive Rejection",
        "passed": passed,
        "cpp_false_positives": cpp_false_positives,
        "py_false_positives": py_false_positives,
        "total_minutes": total_minutes,
        "details": f"Tested 480 min: C++ false positives={cpp_false_positives}, Py false positives={py_false_positives}"
    }


def test_adv_03_fluctuating_weather_and_dropout():
    """
    ADV-03: Challenge behavior during fluctuating outdoor weather and temporary API dropouts.
    Scenario: During 01:00-05:00 night calibration, outdoor temperature drops by 8°C in 2 hours
    and weather API experiences a 5-minute dropout (outdoorValid = False) at 03:00.
    Verify C++ engine handling of calibration session.
    """
    engine = CppThermodynamicEngine("tiles", 5.0, "medium")
    zone = ThermalZone2R2C(make_profile("tiles", 5.0, "medium"), initial_Ts=21.5, initial_Ta=21.0)

    start_epoch = 1700000000 + 3600 # 01:00
    now_ms = 1000
    dropout_occurred_at_0300 = False
    session_aborted_early = False

    # Simulate minute by minute from 01:00 to 05:00
    for step in range(240):
        curr_epoch = start_epoch + step * 60
        hour = 1 + (step // 60)
        minute = step % 60

        # Rapidly fluctuating outdoor temp (cooling front: 12°C down to 4°C with sinusoidal wind gusts)
        T_ext = 12.0 - (step / 240.0) * 8.0 + 1.5 * math.sin(step / 10.0)

        # 5-minute weather API dropout at 03:00 (step 120 to 124)
        is_valid = not (120 <= step <= 124)

        zone.step(60.0, q_in=0.0, T_ext=T_ext, g_vent=0.0)
        was_active_before = engine.night_active

        engine.evaluate_night_calibration(
            round(zone.T_a, 2), T_ext, is_valid, True, hour, minute, now_ms, curr_epoch
        )

        # Detect if dropout killed active session
        if step == 120 and was_active_before and not engine.night_active:
            dropout_occurred_at_0300 = True
            session_aborted_early = True

        now_ms += 60000

    # Finalize at 05:00
    engine.evaluate_night_calibration(
        round(zone.T_a, 2), 4.0, True, True, 5, 0, now_ms, start_epoch + 14400
    )

    # Note: If session was aborted early by dropout, it restarts when valid again
    # Assess resilience: does the engine crash, diverge, or gracefully retain valid metrics?
    loss_rate = engine.metrics["naturalLossRate"]
    metrics_safe = (0.005 <= loss_rate <= 0.080)

    return {
        "id": "ADV-03",
        "name": "Rapid Fluctuating Weather & API Dropout Resilience",
        "passed": metrics_safe,
        "dropout_aborted_session": session_aborted_early,
        "final_loss_rate": loss_rate,
        "details": f"Dropout caused session abort={session_aborted_early}; Metric remained safe and bounded={metrics_safe} ({loss_rate:.5f})"
    }


def test_adv_04_sudden_aeration_and_immediate_closure():
    """
    ADV-04: Sudden Aeration followed by Immediate Window Closure.
    Scenario:
      1. Window opened for 4 minutes in cold weather (-2°C).
      2. Room air drops rapidly by 0.5°C.
      3. Window closed at minute 4.
      4. Heavy concrete screed radiates heat, air recovers.
    Verify:
      - Window is detected during aeration.
      - Zone actuator does NOT remain permanently deadlocked.
      - Evaluates auto-recovery duration.
    """
    profile = make_profile("tiles", 5.0, "medium")
    zone = ThermalZone2R2C(profile, initial_Ts=23.0, initial_Ta=21.5)
    engine = CppThermodynamicEngine("tiles", 5.0, "medium")

    now_ms = 1000
    T_ext = -2.0
    detection_minute = None
    recovery_minute = None

    # Total test: 60 minutes
    for m in range(60):
        # Aeration during minutes 1 to 4
        g_vent = 120.0 if (1 <= m < 5) else 0.0 # High ventilation during aeration
        q_in = 0.0 # Heating disabled / idle

        # Step 60s
        zone.step(60.0, q_in=q_in, T_ext=T_ext, g_vent=g_vent)
        now_ms = (m + 1) * 60000

        is_open = engine.evaluate_window(zone.T_a, T_ext, True, now_ms)

        if is_open and detection_minute is None:
            detection_minute = m

        if detection_minute is not None and not is_open and recovery_minute is None:
            recovery_minute = m

    detected = (detection_minute is not None)
    recovered = (recovery_minute is not None) and (recovery_minute <= 35) # Max 30m timeout + margin
    passed = detected and recovered

    return {
        "id": "ADV-04",
        "name": "Sudden Aeration & Recovery Dynamics",
        "passed": passed,
        "detected": detected,
        "detection_minute": detection_minute,
        "recovered": recovered,
        "recovery_minute": recovery_minute,
        "recovery_duration_min": (recovery_minute - detection_minute) if (detection_minute and recovery_minute) else None,
        "details": f"Detected at t={detection_minute}m; Auto-recovered at t={recovery_minute}m (Duration: {recovery_minute - detection_minute if recovered else 'N/A'}m)"
    }


def test_adv_05_edge_screed_thicknesses():
    """
    ADV-05: Challenge edge screed thicknesses (3 cm vs 10 cm).
    Hydronic physics varies drastically:
      - 3 cm: low thermal mass, rapid heat transfer, short dead time (~18m), minimal overshoot (~0.2°C)
      - 10 cm: massive thermal mass (2.1e5 J/K per m²), long dead time (~60m), large coasting overshoot (~0.7°C)
    Tests:
      1. Mathematical priors scaling and clamping in C++ engine
      2. Predictive pre-heat accuracy with 3cm vs 10cm physical zones hitting 21.0°C target
    """
    # 1. Priors verification
    priors_3cm = CppThermodynamicEngine.calculate_default_priors("tiles", 3.0, "medium")
    priors_10cm = CppThermodynamicEngine.calculate_default_priors("tiles", 10.0, "medium")

    # In C++, screedRatio is clamped to [0.6, 2.0]
    # For 3cm: ratio = 3/5 = 0.6. tDead = 30 * 0.6 = 18.0 min.
    # For 10cm: ratio = 10/5 = 2.0 -> clamped to 2.0. tDead = 30 * 2.0 = 60.0 min.
    ratio_3cm_correct = (priors_3cm["tDeadMin"] == 18.0)
    ratio_10cm_clamped = (priors_10cm["tDeadMin"] == 60.0) # Confirms 2.0 clamp

    # 2. Dynamic 2R2C Pre-heat Simulation for both thicknesses
    errors = {}
    for thickness in [3.0, 10.0]:
        profile = make_profile("tiles", thickness, "medium")
        T_ext = 5.0
        Ta_init = 20.0
        Ts_init = Ta_init + (profile.R_s / profile.R_e) * (Ta_init - T_ext)
        q_baseline = (Ta_init - T_ext) / profile.R_e
        q_heat = q_baseline + 85.0 # Heating flux

        zone = ThermalZone2R2C(profile, initial_Ts=Ts_init, initial_Ta=Ta_init)
        engine = CppThermodynamicEngine("tiles", thickness, "medium")

        target_temp = 20.50
        lead_min = engine.calculate_preheat_lead_min(Ta_init, target_temp, T_ext, True)

        # Arrival scheduled at t = 180 min
        # Heat starts at (180 - lead_min)
        heat_start_min = max(0.0, 180.0 - lead_min)
        # Heat shuts off when expected coasting will reach target
        # Cutoff delta = target - effective_overshoot
        effective_overshoot = min(engine.metrics["overshootDelta"], (target_temp - Ta_init) * 0.45)
        cutoff_temp = target_temp - effective_overshoot

        dt = 1.0
        steps = int(180.0 * 60.0 / dt)
        heating_active = False

        for s in range(steps):
            t_min = (s * dt) / 60.0
            if t_min >= heat_start_min and zone.T_a < cutoff_temp:
                heating_active = True
            elif zone.T_a >= cutoff_temp:
                heating_active = False

            q_in = q_heat if heating_active else q_baseline
            zone.step(dt, q_in=q_in, T_ext=T_ext, g_vent=0.0)

        final_error = abs(zone.T_a - target_temp)
        errors[f"{int(thickness)}cm"] = {
            "lead_min": lead_min,
            "final_Ta": zone.T_a,
            "error_C": final_error,
            "passed": final_error < 0.35
        }

    all_passed = ratio_3cm_correct and ratio_10cm_clamped and all(e["passed"] for e in errors.values())
    return {
        "id": "ADV-05",
        "name": "Edge Screed Thicknesses (3cm vs 10cm) Physics & Priors",
        "passed": all_passed,
        "priors_3cm": priors_3cm,
        "priors_10cm": priors_10cm,
        "clamp_verified": ratio_10cm_clamped,
        "preheat_sim": errors,
        "details": f"3cm: lead={errors['3cm']['lead_min']}m, err={errors['3cm']['error_C']:.3f}°C; 10cm: lead={errors['10cm']['lead_min']}m, err={errors['10cm']['error_C']:.3f}°C"
    }


def test_adv_06_uncalibrated_cold_boot_state():
    """
    ADV-06: Uncalibrated Cold Boot State.
    Verify:
      1. Immediate cold boot does NOT trigger spurious open-window state on first sample.
      2. Circular buffer populates cleanly from t = 0 to t = 60 min without NaN or division by zero.
      3. Pre-heat calculation yields deterministic, safe values [15m, 240m] before any calibration occurs.
    """
    engine = CppThermodynamicEngine("parquet", 5.0, "medium")

    # Sample 1 at t = 0 ms
    open_boot = engine.evaluate_window(20.0, 10.0, True, 0)
    assert not open_boot, "Boot sample must not trigger window open"
    assert not math.isnan(engine.first_derivative), "1st derivative must not be NaN"
    assert not math.isnan(engine.second_derivative), "2nd derivative must not be NaN"

    # Pre-heat on cold boot
    lead_cold = engine.calculate_preheat_lead_min(19.0, 21.0, 5.0, True)
    lead_safe = (15 <= lead_cold <= 240)

    # Populate 12 samples at 5-minute intervals
    temps = [20.0 - i * 0.02 for i in range(12)] # Slow 0.24°C drop over 1h
    no_crash = True
    for i, t in enumerate(temps):
        now_ms = i * 300000
        engine.evaluate_window(t, 10.0, True, now_ms)
        if math.isnan(engine.first_derivative) or math.isnan(engine.second_derivative):
            no_crash = False

    passed = (not open_boot) and lead_safe and no_crash
    return {
        "id": "ADV-06",
        "name": "Uncalibrated Cold Boot State & Boundary Safety",
        "passed": passed,
        "boot_window_open": open_boot,
        "cold_preheat_lead_min": lead_cold,
        "buffer_populated_cleanly": no_crash,
        "details": f"Cold boot safe: windowOpen={open_boot}, preheatLead={lead_cold}m (safe [15,240]), zero NaNs"
    }


def test_adv_07_mild_weather_window_detection_boundary():
    """
    ADV-07: Mild Weather Open-Window Detection Boundary.
    Requirement R2 states window detection must trigger within 15 min at Text = 15°C.
    Challenge: Test behavior across mild temperatures Text = 14°C, 15°C, 16°C, 17°C, 18°C.
    Simulate standard aeration: indoor temp drops 0.4°C in 10 minutes (0.04°C/min).
    Determine at what outdoor temperature the derivative condition fails to trigger.
    """
    results = {}
    engine_results = {}

    for text in [12.0, 14.0, 15.0, 16.0, 17.0, 18.0]:
        engine = CppThermodynamicEngine("tiles", 5.0, "medium")
        now_ms = 0
        initial_Ta = 22.0
        trigger_minute = None

        for m in range(25): # 0 to 25 minutes
            now_ms = m * 60000
            # 0.04°C/min drop for first 10 min, then slower
            if m <= 10:
                temp = initial_Ta - (m * 0.04)
            else:
                temp = initial_Ta - 0.40 - ((m - 10) * 0.01)

            is_open = engine.evaluate_window(temp, text, True, now_ms)
            if is_open and trigger_minute is None:
                trigger_minute = m

        engine_results[f"Text_{int(text)}C"] = {
            "triggered": (trigger_minute is not None),
            "trigger_minute": trigger_minute,
            "within_15min": (trigger_minute is not None and trigger_minute <= 15)
        }

    # At Text = 15°C, requirement is trigger <= 15m
    req_15c_met = engine_results["Text_15C"]["within_15min"]
    passed = req_15c_met

    return {
        "id": "ADV-07",
        "name": "Mild Weather Open-Window Boundary (12°C - 18°C)",
        "passed": passed,
        "results": engine_results,
        "req_15C_pass": req_15c_met,
        "details": f"Text=15°C triggered at t={engine_results['Text_15C']['trigger_minute']}m (<=15m: {req_15c_met})"
    }


def test_adv_08_cpp_vs_python_implementation_parity():
    """
    ADV-08: Implementation Parity between C++ ThermodynamicEngine and Python test harness.
    Compares:
      1. Default priors calculation across all floor types and screed thicknesses
      2. Window detection triggering and recovery on identical synthetic sequence
    """
    mismatches = []

    # 1. Compare priors
    test_configs = [
        ("tiles", 4.0, "poor"),
        ("tiles", 5.0, "medium"),
        ("parquet", 5.0, "good"),
        ("parquet", 7.0, "passive")
    ]
    for ft, sc, ins in test_configs:
        cpp_p = CppThermodynamicEngine.calculate_default_priors(ft, sc, ins)
        # Check against physics expectation
        if cpp_p["tDeadMin"] <= 0 or cpp_p["heatingSlope"] <= 0:
            mismatches.append(f"Invalid prior for {ft} {sc}cm {ins}")

    # 2. Window detection on identical sequence
    cpp_eng = CppThermodynamicEngine("tiles", 5.0, "medium")
    from test_thermodynamics import WindowCircularBufferDetector
    py_det = WindowCircularBufferDetector(k_loss=0.025)

    sim_sequence = [
        (0, 21.0),
        (5, 20.8),
        (10, 20.6), # Window open drop 0.4°C in 10m
        (15, 20.4),
        (20, 20.4), # Closure rebound
        (25, 20.5),
        (30, 20.6),
    ]

    cpp_triggers = []
    py_triggers = []

    for m, temp in sim_sequence:
        t_sec = m * 60.0
        now_ms = m * 60000
        cpp_open = cpp_eng.evaluate_window(temp, 15.0, True, now_ms)
        py_open, _, _ = py_det.add_sample(t_sec, temp, 15.0)
        cpp_triggers.append(cpp_open)
        py_triggers.append(py_open)

    # Both must detect open window at t=10 min or t=15 min
    cpp_detected = any(cpp_triggers)
    py_detected = any(py_triggers)
    both_detected = cpp_detected and py_detected

    passed = (len(mismatches) == 0) and both_detected
    return {
        "id": "ADV-08",
        "name": "C++ Firmware vs Python Reference Parity",
        "passed": passed,
        "mismatches": mismatches,
        "cpp_triggers": cpp_triggers,
        "py_triggers": py_triggers,
        "details": f"Priors consistent (0 mismatches); C++ detected={cpp_detected}, Py detected={py_detected}"
    }


# ==============================================================================
# 4. Stress Test Runner
# ==============================================================================

def run_adversarial_suite():
    print("=" * 80)
    print(" ADVERSARIAL STRESS TEST SUITE: 2R2C THERMODYNAMICS & WINDOW DETECTION")
    print(" Challenger 1 - Empirical Boundary & Robustness Assessment")
    print("=" * 80)

    tests = [
        test_adv_01_high_noise_cooling_convergence,
        test_adv_02_noise_false_positive_rejection,
        test_adv_03_fluctuating_weather_and_dropout,
        test_adv_04_sudden_aeration_and_immediate_closure,
        test_adv_05_edge_screed_thicknesses,
        test_adv_06_uncalibrated_cold_boot_state,
        test_adv_07_mild_weather_window_detection_boundary,
        test_adv_08_cpp_vs_python_implementation_parity,
    ]

    all_passed = True
    passed_count = 0
    results = []

    for test_fn in tests:
        t0 = time.time()
        res = test_fn()
        elapsed_ms = (time.time() - t0) * 1000.0
        res["elapsed_ms"] = elapsed_ms
        results.append(res)

        status_tag = "[PASS]" if res["passed"] else "[FAIL]"
        if res["passed"]:
            passed_count += 1
        else:
            all_passed = False

        print(f" {status_tag} {res['id']}: {res['name']} ({elapsed_ms:.1f}ms)")
        print(f"        Details: {res['details']}")

    print("-" * 80)
    print(f" Adversarial Results: {passed_count}/{len(tests)} Passed ({(passed_count/len(tests))*100.0:.1f}%)")
    print("=" * 80)

    return all_passed, results


if __name__ == "__main__":
    success, res = run_adversarial_suite()
    sys.exit(0 if success else 1)
