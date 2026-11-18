"""Binary sensor platform for Smart Heating."""
from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
    BinarySensorEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import SmartHeatingData, SmartHeatingDataUpdateCoordinator


@dataclass(frozen=True, kw_only=True)
class SmartHeatingBinarySensorDescription(BinarySensorEntityDescription):
    """Class describing Smart Heating binary sensor entities."""

    is_on_fn: Callable[[SmartHeatingData], bool]


SYSTEM_BINARY_SENSORS: tuple[SmartHeatingBinarySensorDescription, ...] = (
    SmartHeatingBinarySensorDescription(
        key="boiler_demand",
        name="Boiler Demand",
        translation_key="boiler_demand",
        is_on_fn=lambda data: data.boiler.boiler_demand or data.boiler.state == "ON",
    ),
    SmartHeatingBinarySensorDescription(
        key="pump_active",
        name="Circulation Pump Active",
        device_class=BinarySensorDeviceClass.RUNNING,
        is_on_fn=lambda data: data.boiler.pump_active,
    ),
    SmartHeatingBinarySensorDescription(
        key="dtc_problem",
        name="System DTC Fault Warning",
        device_class=BinarySensorDeviceClass.PROBLEM,
        is_on_fn=lambda data: data.diagnostics.dtc_count > 0,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating binary sensor entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities: list[BinarySensorEntity] = [
        SmartHeatingSystemBinarySensor(coordinator, entry, desc)
        for desc in SYSTEM_BINARY_SENSORS
    ]

    # Add per-zone binary sensors
    for zone_id in coordinator.data.zones:
        entities.append(SmartHeatingZoneValveSensor(coordinator, entry, zone_id))
        entities.append(SmartHeatingZoneWindowPauseSensor(coordinator, entry, zone_id))
        entities.append(SmartHeatingZoneSensorFaultSensor(coordinator, entry, zone_id))

    async_add_entities(entities)


class SmartHeatingSystemBinarySensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], BinarySensorEntity
):
    """Representation of a system binary sensor."""

    entity_description: SmartHeatingBinarySensorDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        description: SmartHeatingBinarySensorDescription,
    ) -> None:
        """Initialize the binary sensor."""
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
    def is_on(self) -> bool:
        """Return True if the binary sensor is on."""
        return self.entity_description.is_on_fn(self.coordinator.data)


class SmartHeatingZoneValveSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], BinarySensorEntity
):
    """Binary sensor representing actuator valve relay state."""

    _attr_has_entity_name = True
    _attr_translation_key = "actuator_valve"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the valve sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_valve"
        self._attr_name = "Actuator Valve"

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

    @property
    def is_on(self) -> bool:
        """Return True if valve relay is energized."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return zone.relay_engaged if zone else False


class SmartHeatingZoneWindowPauseSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], BinarySensorEntity
):
    """Binary sensor representing zone window pause."""

    _attr_has_entity_name = True
    _attr_translation_key = "window_pause"

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the window pause sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_window_pause"
        self._attr_name = "Window Pause"

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

    @property
    def is_on(self) -> bool:
        """Return True if window pause is active."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return zone.window_pause_active if zone else False


class SmartHeatingZoneSensorFaultSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], BinarySensorEntity
):
    """Binary sensor representing BLE sensor fault."""

    _attr_has_entity_name = True
    _attr_device_class = BinarySensorDeviceClass.PROBLEM

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the sensor fault entity."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_sensor_fault"
        self._attr_name = "BLE Sensor Offline"

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

    @property
    def is_on(self) -> bool:
        """Return True if BLE sensor is timed out or invalid."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        if not zone:
            return False
        return zone.sensor_fault or not zone.sensor_valid
