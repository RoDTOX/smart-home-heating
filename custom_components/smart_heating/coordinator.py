"""DataUpdateCoordinator for Smart Heating."""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import timedelta
import logging
from typing import Any

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import SmartHeatingApiClient, SmartHeatingError
from .const import DEFAULT_SCAN_INTERVAL, DOMAIN

_LOGGER = logging.getLogger(__name__)


@dataclass
class ZoneData:
    """Dataclass representing a heating zone."""

    id: int
    name: str
    target_temp: float
    current_temp: float
    effective_target: float
    schedule_target: float
    is_boost: bool
    hysteresis: float
    enabled: bool
    sensor_mac: str
    sensor_name: str
    sensor_valid: bool
    sensor_fault: bool
    humidity: float | None = None
    battery: int | None = None
    rssi: int | None = None
    relay_pin: int = 0
    relay_engaged: bool = False
    demand: bool = False
    window_pause_active: bool = False
    window_pause_remaining_sec: int = 0
    circuits: int = 1
    heating_type: str = "floor"
    runtime_today_sec: int = 0
    runtime_yesterday_sec: int = 0
    state: str = "IDLE"


@dataclass
class BoilerData:
    """Dataclass representing boiler state and telemetry."""

    state: str = "OFF"
    boiler_demand: bool = False
    cooldown_sec: int = 0
    burn_current_sec: int = 0
    burn_today_sec: int = 0
    burn_yesterday_sec: int = 0
    burn_month_sec: int = 0
    gas_today_m3: float = 0.0
    gas_yesterday_m3: float = 0.0
    gas_month_m3: float = 0.0
    gas_price: float = 3.3
    gas_flow_measured: float = 0.545
    gas_today_cost: float = 0.0
    gas_month_cost: float = 0.0
    pump_active: bool = False
    pump_enabled: bool = False
    season_mode: str = "AUTO"
    season_state: str = "HEATING"


@dataclass
class DiagnosticsData:
    """Dataclass representing controller diagnostics."""

    firmware_version: str = "1.0.0"
    free_heap: int = 0
    loop_stack_free: int = 0
    wifi_rssi: int = 0
    uptime_sec: int = 0
    outdoor_temp: float = 0.0
    outdoor_band: int = 0
    dtc_count: int = 0
    dtcs: list[dict[str, Any]] = field(default_factory=list)
    checks: list[dict[str, Any]] = field(default_factory=list)


@dataclass
class SmartHeatingData:
    """Complete aggregated data structure for the integration."""

    zones: dict[int, ZoneData] = field(default_factory=dict)
    boiler: BoilerData = field(default_factory=BoilerData)
    diagnostics: DiagnosticsData = field(default_factory=DiagnosticsData)
    system_config: dict[str, Any] = field(default_factory=dict)


class SmartHeatingDataUpdateCoordinator(DataUpdateCoordinator[SmartHeatingData]):
    """Class to manage fetching data from the Smart Heating ESP32 controller."""

    def __init__(
        self,
        hass: HomeAssistant,
        api: SmartHeatingApiClient,
        scan_interval_sec: int = DEFAULT_SCAN_INTERVAL,
    ) -> None:
        """Initialize the coordinator."""
        self.api = api
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
            update_interval=timedelta(seconds=scan_interval_sec),
        )

    async def _async_update_data(self) -> SmartHeatingData:
        """Fetch all data from the Smart Heating controller."""
        try:
            # 1. Fetch zones and system state
            zones_resp = await self.api.async_get_zones()
            boiler_resp = await self.api.async_get_boiler_status()
            diag_resp = await self.api.async_get_diag_status()

            # Optional: system config if needed
            system_config: dict[str, Any] = {}
            try:
                system_config = await self.api.async_get_system_config()
            except Exception as err:
                _LOGGER.debug("Could not fetch system config: %s", err)

            # Map BLE sensors from boiler_resp or ble endpoint
            ble_sensors = boiler_resp.get("sensors", [])
            sensor_map: dict[str, dict[str, Any]] = {}
            for s in ble_sensors:
                mac = s.get("mac", "").upper()
                name = s.get("name", "")
                if mac:
                    sensor_map[mac] = s
                if name:
                    sensor_map[name] = s

            # Parse Zones
            zones_data: dict[int, ZoneData] = {}
            raw_zones = zones_resp.get("zones", [])
            for rz in raw_zones:
                zid = int(rz.get("id", 1))
                mac = rz.get("sensorMac", "").upper()
                s_name = rz.get("sensor", "")
                sensor_info = sensor_map.get(mac) or sensor_map.get(s_name) or {}

                humidity = sensor_info.get("hum")
                battery = sensor_info.get("bat")
                rssi = sensor_info.get("rssi")

                zone = ZoneData(
                    id=zid,
                    name=rz.get("name", f"Zone {zid}"),
                    target_temp=float(rz.get("targetTemp", 21.0)),
                    current_temp=float(rz.get("currentTemp", 21.0)),
                    effective_target=float(rz.get("effectiveTarget", rz.get("targetTemp", 21.0))),
                    schedule_target=float(rz.get("scheduleTarget", 21.0)),
                    is_boost=bool(rz.get("isBoost", False)),
                    hysteresis=float(rz.get("hysteresis", 0.3)),
                    enabled=bool(rz.get("enabled", True)),
                    sensor_mac=mac,
                    sensor_name=s_name,
                    sensor_valid=bool(rz.get("sensorValid", True)),
                    sensor_fault=bool(rz.get("sensorFault", False)),
                    humidity=float(humidity) if humidity is not None else None,
                    battery=int(battery) if battery is not None else None,
                    rssi=int(rssi) if rssi is not None else None,
                    relay_pin=int(rz.get("relayPin", 0)),
                    relay_engaged=bool(rz.get("relayEngaged", False)),
                    demand=bool(rz.get("demand", False)),
                    window_pause_active=bool(rz.get("windowPauseActive", False)),
                    window_pause_remaining_sec=int(rz.get("windowPauseRemainingSec", 0)),
                    circuits=int(rz.get("circuits", 1)),
                    heating_type=rz.get("heatingTypeStr", "floor"),
                    runtime_today_sec=int(rz.get("runtimeTodaySec", 0)),
                    runtime_yesterday_sec=int(rz.get("runtimeYesterdaySec", 0)),
                    state=rz.get("state", "IDLE"),
                )
                zones_data[zid] = zone

            # Parse Boiler and Gas
            gas_meter = boiler_resp.get("gasMeter", {})
            gas_today = float(gas_meter.get("gasTodayM3", 0.0))
            gas_yesterday = float(gas_meter.get("gasYesterdayM3", 0.0))
            gas_month = float(gas_meter.get("gasThisMonthM3", 0.0))
            gas_price = float(gas_meter.get("gasPrice", 3.3))
            gas_flow = float(gas_meter.get("gasFlowMeasured", 0.545))

            boiler_runtime = boiler_resp.get("boilerRuntime", {})

            boiler_data = BoilerData(
                state=boiler_resp.get("boiler", "OFF"),
                boiler_demand=bool(boiler_resp.get("boilerDemand", False)),
                cooldown_sec=int(boiler_resp.get("boilerCooldownSec", 0)),
                burn_current_sec=int(boiler_runtime.get("currentBurnSec", 0)),
                burn_today_sec=int(boiler_runtime.get("todaySec", 0)),
                burn_yesterday_sec=int(boiler_runtime.get("yesterdaySec", 0)),
                burn_month_sec=int(boiler_runtime.get("thisMonthSec", 0)),
                gas_today_m3=gas_today,
                gas_yesterday_m3=gas_yesterday,
                gas_month_m3=gas_month,
                gas_price=gas_price,
                gas_flow_measured=gas_flow,
                gas_today_cost=round(gas_today * gas_price, 2),
                gas_month_cost=round(gas_month * gas_price, 2),
                pump_active=bool(boiler_resp.get("pumpActive", False)),
                pump_enabled=bool(boiler_resp.get("pumpEnabled", False)),
                season_mode=boiler_resp.get("seasonMode", "AUTO"),
                season_state=boiler_resp.get("seasonState", "HEATING"),
            )

            # Parse Diagnostics
            diag_data = DiagnosticsData(
                firmware_version=boiler_resp.get("firmwareVersion", "1.11.13.10"),
                free_heap=int(boiler_resp.get("freeHeap", 0)),
                loop_stack_free=int(boiler_resp.get("loopStackFree", 0)),
                wifi_rssi=int(boiler_resp.get("rssi", 0)),
                uptime_sec=int(boiler_resp.get("uptimeSec", 0)),
                outdoor_temp=float(boiler_resp.get("outdoorTemp", 0.0)),
                outdoor_band=int(zones_resp.get("outdoorBand", 0)),
                dtc_count=int(diag_resp.get("dtcCount", 0)),
                dtcs=diag_resp.get("dtcs", []),
                checks=diag_resp.get("checks", []),
            )

            return SmartHeatingData(
                zones=zones_data,
                boiler=boiler_data,
                diagnostics=diag_data,
                system_config=system_config,
            )

        except SmartHeatingError as err:
            raise UpdateFailed(f"Smart Heating communication failed: {err}") from err
        except Exception as err:
            _LOGGER.exception("Unexpected error fetching Smart Heating data: %s", err)
            raise UpdateFailed(f"Unexpected error: {err}") from err

    async def async_set_zone_temperature(self, zone_id: int, target: float) -> None:
        """Set zone temperature setpoint and request quick update."""
        await self.api.async_set_zone_setpoint(zone_id, target)
        await self.async_request_refresh()

    async def async_clear_window_pause(self, zone_id: int = 0) -> None:
        """Clear window pause state and refresh."""
        await self.api.async_clear_window_pause(zone_id)
        await self.async_request_refresh()

    async def async_clear_dtc_history(self) -> None:
        """Purge inactive DTC events and refresh."""
        await self.api.async_clear_dtc_events()
        await self.async_request_refresh()

    async def async_set_config_param(self, key: str, value: Any) -> None:
        """Update a system configuration parameter."""
        payload = {key: value}
        await self.api.async_set_system_config(payload)
        await self.async_request_refresh()

    async def async_set_season_mode(self, mode: str) -> None:
        """Set heating season mode."""
        await self.api.async_set_season_mode(mode)
        await self.async_request_refresh()
