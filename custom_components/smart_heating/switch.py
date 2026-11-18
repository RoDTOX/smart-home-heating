"""Switch platform for Smart Heating (Away mode, Schedule enabled)."""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.components.switch import SwitchDeviceClass, SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import SmartHeatingDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating switch entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities: list[SwitchEntity] = [
        SmartHeatingAwaySwitch(coordinator, entry),
        SmartHeatingScheduleSwitch(coordinator, entry),
    ]

    async_add_entities(entities)


class SmartHeatingAwaySwitch(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SwitchEntity
):
    """Switch entity for global Away (Plecat) mode."""

    _attr_has_entity_name = True
    _attr_icon = "mdi:account-arrow-right"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize Away switch."""
        super().__init__(coordinator)
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_away_mode"
        self._attr_name = "Away Mode (Plecat)"

    @property
    def device_info(self) -> DeviceInfo:
        """Link to the main controller hub device."""
        diag = self.coordinator.data.diagnostics
        return DeviceInfo(
            identifiers={(DOMAIN, self._entry.unique_id)},
            name="Smart Heating Controller",
            manufacturer="Smartiva Systems",
            model="ESP32-Relay X4 V1.1",
            sw_version=diag.firmware_version,
            configuration_url=f"http://{self._entry.data.get('host', '192.168.1.20')}",
        )

    @property
    def is_on(self) -> bool:
        """Return true if away mode is active."""
        return self.coordinator.data.schedule.away

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on away mode."""
        await self.coordinator.async_set_away(True)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off away mode."""
        await self.coordinator.async_set_away(False)


class SmartHeatingScheduleSwitch(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SwitchEntity
):
    """Switch entity to enable or disable automated schedule globally."""

    _attr_has_entity_name = True
    _attr_icon = "mdi:calendar-clock"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize Schedule switch."""
        super().__init__(coordinator)
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_schedule_enabled"
        self._attr_name = "Schedule Automated Control"

    @property
    def device_info(self) -> DeviceInfo:
        """Link to the main controller hub device."""
        diag = self.coordinator.data.diagnostics
        return DeviceInfo(
            identifiers={(DOMAIN, self._entry.unique_id)},
            name="Smart Heating Controller",
            manufacturer="Smartiva Systems",
            model="ESP32-Relay X4 V1.1",
            sw_version=diag.firmware_version,
            configuration_url=f"http://{self._entry.data.get('host', '192.168.1.20')}",
        )

    @property
    def is_on(self) -> bool:
        """Return true if schedule is enabled."""
        return self.coordinator.data.schedule.enabled

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Enable automated schedule."""
        await self.coordinator.api.async_set_schedule_enabled(True)
        await self.coordinator.async_request_refresh()

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Disable automated schedule."""
        await self.coordinator.api.async_set_schedule_enabled(False)
        await self.coordinator.async_request_refresh()
