"""Button platform for Smart Heating actions."""
from __future__ import annotations

from homeassistant.components.button import ButtonEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import SmartHeatingDataUpdateCoordinator


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating button entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities: list[ButtonEntity] = [
        SmartHeatingClearDtcButton(coordinator, entry),
        SmartHeatingClearAllWindowsButton(coordinator, entry),
    ]

    for zone_id in coordinator.data.zones:
        entities.append(SmartHeatingClearZoneWindowButton(coordinator, entry, zone_id))
        entities.append(SmartHeatingClearZoneHoldButton(coordinator, entry, zone_id))

    async_add_entities(entities)


class SmartHeatingClearDtcButton(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], ButtonEntity
):
    """Button to reset active DTC faults and clear history."""

    _attr_has_entity_name = True
    _attr_icon = "mdi:broom"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_clear_dtc_history"
        self._attr_name = "Reset DTC Faults & History"

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

    async def async_press(self) -> None:
        """Trigger resetting active DTCs and clearing history."""
        await self.coordinator.async_clear_all_dtcs()


class SmartHeatingClearAllWindowsButton(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], ButtonEntity
):
    """Button to clear window pause across all zones."""

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_clear_all_windows"
        self._attr_name = "Resume Heating (Clear All Window Pauses)"

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

    async def async_press(self) -> None:
        """Trigger clearing window pause across all zones."""
        await self.coordinator.async_clear_window_pause(0)


class SmartHeatingClearZoneWindowButton(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], ButtonEntity
):
    """Button to clear window pause on a specific zone."""

    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_clear_window"
        self._attr_name = "Resume Zone Heating"

    @property
    def device_info(self) -> DeviceInfo:
        """Link to the zone sub-device."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        zone_name = zone.name if zone else f"Zone {self._zone_id}"
        return DeviceInfo(
            identifiers={(DOMAIN, f"{self._entry.unique_id}_zone_{self._zone_id}")},
            name=f"Heating - {zone_name}",
            manufacturer="Smartiva Systems",
            model="Underfloor Heating Zone",
            via_device=(DOMAIN, self._entry.unique_id),
        )

    async def async_press(self) -> None:
        """Trigger clearing window pause on this zone."""
        await self.coordinator.async_clear_window_pause(self._zone_id)


class SmartHeatingClearZoneHoldButton(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], ButtonEntity
):
    """Button to clear manual temperature hold and return zone to schedule."""

    _attr_has_entity_name = True
    _attr_icon = "mdi:calendar-sync"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the button."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_clear_hold"
        self._attr_name = "Resume Schedule (Clear Hold)"

    @property
    def device_info(self) -> DeviceInfo:
        """Link to the zone sub-device."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        zone_name = zone.name if zone else f"Zone {self._zone_id}"
        return DeviceInfo(
            identifiers={(DOMAIN, f"{self._entry.unique_id}_zone_{self._zone_id}")},
            name=f"Heating - {zone_name}",
            manufacturer="Smartiva Systems",
            model="Underfloor Heating Zone",
            via_device=(DOMAIN, self._entry.unique_id),
        )

    async def async_press(self) -> None:
        """Trigger clearing manual hold on this zone."""
        await self.coordinator.async_clear_zone_hold(self._zone_id)
