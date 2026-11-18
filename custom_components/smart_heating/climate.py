"""Climate platform for Smart Heating."""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.climate import (
    ClimateEntity,
    ClimateEntityFeature,
    HVACAction,
    HVACMode,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import ATTR_TEMPERATURE, UnitOfTemperature
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import (
    DOMAIN,
    PRESET_AWAY,
    PRESET_COMFORT,
    PRESET_ECO,
    PRESET_TEMPERATURES,
)
from .coordinator import SmartHeatingDataUpdateCoordinator, ZoneData

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating climate entities based on a config entry."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        SmartHeatingZoneClimate(coordinator, entry, zone_id)
        for zone_id in coordinator.data.zones
    ]
    async_add_entities(entities)


class SmartHeatingZoneClimate(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], ClimateEntity
):
    """Representation of a Smart Heating zone climate entity."""

    _attr_has_entity_name = True
    _attr_temperature_unit = UnitOfTemperature.CELSIUS
    _attr_target_temperature_step = 0.5
    _attr_min_temp = 15.0
    _attr_max_temp = 28.0
    _attr_hvac_modes = [HVACMode.HEAT, HVACMode.OFF]
    _attr_supported_features = (
        ClimateEntityFeature.TARGET_TEMPERATURE
        | getattr(ClimateEntityFeature, "TURN_OFF", 0)
        | getattr(ClimateEntityFeature, "TURN_ON", 0)
    )

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the climate entity."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_climate"

    @property
    def _zone(self) -> ZoneData | None:
        """Get current zone data."""
        return self.coordinator.data.zones.get(self._zone_id)

    @property
    def name(self) -> str:
        """Return the display name of the zone."""
        zone = self._zone
        return zone.name if zone else f"Zone {self._zone_id}"

    @property
    def device_info(self) -> DeviceInfo:
        """Return device information linking to the zone device."""
        zone = self._zone
        zone_name = zone.name if zone else f"Zone {self._zone_id}"
        return DeviceInfo(
            identifiers={(DOMAIN, f"{self._entry.unique_id}_zone_{self._zone_id}")},
            name=f"Heating - {zone_name}",
            manufacturer="Smartiva Systems",
            model="Underfloor Heating Zone",
            via_device=(DOMAIN, self._entry.unique_id),
        )

    @property
    def current_temperature(self) -> float | None:
        """Return the current temperature."""
        zone = self._zone
        return zone.current_temp if zone else None

    @property
    def current_humidity(self) -> float | None:
        """Return the current humidity."""
        zone = self._zone
        return zone.humidity if zone else None

    @property
    def target_temperature(self) -> float | None:
        """Return the effective temperature we try to reach."""
        zone = self._zone
        if not zone:
            return None
        return zone.effective_target if zone.effective_target > 0 else zone.target_temp

    @property
    def hvac_mode(self) -> HVACMode:
        """Return hvac operation mode."""
        zone = self._zone
        if not zone or not zone.enabled:
            return HVACMode.OFF
        return HVACMode.HEAT

    @property
    def hvac_action(self) -> HVACAction:
        """Return the current running hvac action."""
        zone = self._zone
        if not zone or not zone.enabled:
            return HVACAction.OFF
        if zone.relay_engaged or zone.demand:
            return HVACAction.HEATING
        return HVACAction.IDLE

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return zone specific extra state attributes."""
        zone = self._zone
        if not zone:
            return {}
        return {
            "state_desc": zone.state,
            "circuits": zone.circuits,
            "relay_pin": zone.relay_pin,
            "relay_engaged": zone.relay_engaged,
            "demand": zone.demand,
            "effective_target": zone.effective_target,
            "target_temp": zone.target_temp,
            "schedule_target": zone.schedule_target,
            "is_boost": zone.is_boost,
            "hysteresis": zone.hysteresis,
            "sensor_mac": zone.sensor_mac,
            "sensor_valid": zone.sensor_valid,
            "sensor_battery": zone.battery,
            "sensor_rssi": zone.rssi,
            "window_pause_active": zone.window_pause_active,
            "window_pause_remaining_sec": zone.window_pause_remaining_sec,
            "heating_type": zone.heating_type,
            "runtime_today_sec": zone.runtime_today_sec,
            "runtime_yesterday_sec": zone.runtime_yesterday_sec,
        }

    async def async_set_temperature(self, **kwargs: Any) -> None:
        """Set new target temperature (triggers boost on ESP32)."""
        temp = kwargs.get(ATTR_TEMPERATURE)
        if temp is None:
            return
        await self.coordinator.async_set_zone_temperature(self._zone_id, float(temp))

    async def async_set_hvac_mode(self, hvac_mode: HVACMode) -> None:
        """Set new hvac mode using native firmware toggle."""
        if hvac_mode == HVACMode.OFF:
            if self._zone and self._zone.enabled:
                await self.coordinator.async_toggle_zone(self._zone_id)
        elif hvac_mode == HVACMode.HEAT:
            if self._zone and not self._zone.enabled:
                await self.coordinator.async_toggle_zone(self._zone_id)

    async def async_turn_off(self) -> None:
        """Turn the zone heating off."""
        if self._zone and self._zone.enabled:
            await self.coordinator.async_toggle_zone(self._zone_id)

    async def async_turn_on(self) -> None:
        """Turn the zone heating on."""
        if self._zone and not self._zone.enabled:
            await self.coordinator.async_toggle_zone(self._zone_id)
