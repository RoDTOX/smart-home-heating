"""Select platform for Smart Heating."""
from __future__ import annotations

from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import SmartHeatingDataUpdateCoordinator

SEASON_OPTIONS = ["AUTO", "ON", "OFF"]


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating select entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [SmartHeatingSeasonSelect(coordinator, entry)]
    async_add_entities(entities)


class SmartHeatingSeasonSelect(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SelectEntity
):
    """Select entity for heating season mode."""

    _attr_has_entity_name = True
    _attr_options = SEASON_OPTIONS
    _attr_translation_key = "season_mode"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
    ) -> None:
        """Initialize the select entity."""
        super().__init__(coordinator)
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_season_mode"
        self._attr_name = "Heating Season Mode"

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
    def current_option(self) -> str | None:
        """Return the current selected option."""
        mode = self.coordinator.data.boiler.season_mode.upper()
        return mode if mode in SEASON_OPTIONS else "AUTO"

    async def async_select_option(self, option: str) -> None:
        """Change the selected option."""
        await self.coordinator.async_set_season_mode(option)
