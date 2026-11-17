"""Number platform for Smart Heating parameters."""
from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.number import (
    NumberDeviceClass,
    NumberEntity,
    NumberEntityDescription,
    NumberMode,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import UnitOfTemperature, UnitOfTime
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import SmartHeatingData, SmartHeatingDataUpdateCoordinator


@dataclass(frozen=True, kw_only=True)
class SmartHeatingNumberDescription(NumberEntityDescription):
    """Class describing Smart Heating number entities."""

    config_key: str
    value_fn: Callable[[SmartHeatingData], float]


NUMBER_DESCRIPTIONS: tuple[SmartHeatingNumberDescription, ...] = (
    SmartHeatingNumberDescription(
        key="actuator_delay",
        name="Actuator Opening Delay",
        config_key="actuatorDelaySec",
        device_class=NumberDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        native_min_value=120,
        native_max_value=600,
        native_step=10,
        mode=NumberMode.SLIDER,
        value_fn=lambda data: float(data.system_config.get("actuatorDelaySec", 180)),
    ),
    SmartHeatingNumberDescription(
        key="pump_overrun",
        name="Pump Overrun Duration",
        config_key="pumpOverrunSec",
        device_class=NumberDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        native_min_value=180,
        native_max_value=600,
        native_step=10,
        mode=NumberMode.SLIDER,
        value_fn=lambda data: float(data.system_config.get("pumpOverrunSec", 180)),
    ),
    SmartHeatingNumberDescription(
        key="freeze_protection_temp",
        name="Frost Protection Threshold",
        config_key="freezeProtectionTemp",
        device_class=NumberDeviceClass.TEMPERATURE,
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        native_min_value=8.0,
        native_max_value=16.0,
        native_step=0.5,
        mode=NumberMode.SLIDER,
        value_fn=lambda data: float(data.system_config.get("freezeProtectionTemp", 12.0)),
    ),
    SmartHeatingNumberDescription(
        key="gas_price",
        name="Gas Price per m³",
        config_key="gasPrice",
        native_unit_of_measurement="Lei",
        native_min_value=1.0,
        native_max_value=10.0,
        native_step=0.05,
        mode=NumberMode.BOX,
        value_fn=lambda data: float(data.boiler.gas_price),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating number entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities = [
        SmartHeatingNumberEntity(coordinator, entry, desc)
        for desc in NUMBER_DESCRIPTIONS
    ]
    async_add_entities(entities)


class SmartHeatingNumberEntity(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], NumberEntity
):
    """Representation of a Smart Heating configuration number entity."""

    entity_description: SmartHeatingNumberDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        description: SmartHeatingNumberDescription,
    ) -> None:
        """Initialize the number entity."""
        super().__init__(coordinator)
        self.entity_description = description
        self._entry = entry
        self._attr_unique_id = f"{entry.unique_id}_{description.key}"

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
    def native_value(self) -> float:
        """Return the current value."""
        return self.entity_description.value_fn(self.coordinator.data)

    async def async_set_native_value(self, value: float) -> None:
        """Update the configuration parameter on the controller."""
        key = self.entity_description.config_key
        # Convert to int if seconds, float if temp/price
        val: Any = int(value) if "Sec" in key else float(value)
        await self.coordinator.async_set_config_param(key, val)
