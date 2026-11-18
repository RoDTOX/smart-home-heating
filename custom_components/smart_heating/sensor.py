"""Sensor platform for Smart Heating."""
from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from datetime import timedelta

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import (
    PERCENTAGE,
    SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
    UnitOfInformation,
    UnitOfTemperature,
    UnitOfTime,
    UnitOfVolume,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity
from homeassistant.util import dt as dt_util

from .const import DOMAIN
from .coordinator import SmartHeatingData, SmartHeatingDataUpdateCoordinator


@dataclass(frozen=True, kw_only=True)
class SmartHeatingSensorDescription(SensorEntityDescription):
    """Class describing Smart Heating controller sensor entities."""

    value_fn: Callable[[SmartHeatingData], Any]


SYSTEM_SENSORS: tuple[SmartHeatingSensorDescription, ...] = (
    SmartHeatingSensorDescription(
        key="gas_today",
        name="Gas Consumption Today",
        device_class=SensorDeviceClass.GAS,
        state_class=SensorStateClass.TOTAL_INCREASING,
        native_unit_of_measurement=UnitOfVolume.CUBIC_METERS,
        value_fn=lambda data: data.boiler.gas_today_m3,
    ),
    SmartHeatingSensorDescription(
        key="gas_today_cost",
        name="Gas Cost Today",
        device_class=SensorDeviceClass.MONETARY,
        state_class=SensorStateClass.TOTAL,
        native_unit_of_measurement="Lei",
        value_fn=lambda data: data.boiler.gas_today_cost,
    ),
    SmartHeatingSensorDescription(
        key="gas_yesterday",
        name="Gas Consumption Yesterday",
        device_class=SensorDeviceClass.GAS,
        native_unit_of_measurement=UnitOfVolume.CUBIC_METERS,
        value_fn=lambda data: data.boiler.gas_yesterday_m3,
    ),
    SmartHeatingSensorDescription(
        key="gas_month",
        name="Gas Consumption This Month",
        device_class=SensorDeviceClass.GAS,
        state_class=SensorStateClass.TOTAL_INCREASING,
        native_unit_of_measurement=UnitOfVolume.CUBIC_METERS,
        value_fn=lambda data: data.boiler.gas_month_m3,
    ),
    SmartHeatingSensorDescription(
        key="gas_month_cost",
        name="Gas Cost This Month",
        device_class=SensorDeviceClass.MONETARY,
        native_unit_of_measurement="Lei",
        value_fn=lambda data: data.boiler.gas_month_cost,
    ),
    SmartHeatingSensorDescription(
        key="gas_flow_measured",
        name="Boiler Gas Flow Rate",
        native_unit_of_measurement="m³/h",
        value_fn=lambda data: data.boiler.gas_flow_measured,
    ),
    SmartHeatingSensorDescription(
        key="outdoor_temp",
        name="Outdoor Temperature",
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=UnitOfTemperature.CELSIUS,
        value_fn=lambda data: data.diagnostics.outdoor_temp,
    ),
    SmartHeatingSensorDescription(
        key="boiler_cooldown",
        name="Boiler Cooldown Remaining",
        device_class=SensorDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        value_fn=lambda data: data.boiler.cooldown_sec,
    ),
    SmartHeatingSensorDescription(
        key="boiler_burn_today",
        name="Boiler Burn Time Today",
        device_class=SensorDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        suggested_display_precision=0,
        value_fn=lambda data: data.boiler.burn_today_sec,
    ),
    SmartHeatingSensorDescription(
        key="free_heap",
        name="Free Heap",
        device_class=SensorDeviceClass.DATA_SIZE,
        native_unit_of_measurement=UnitOfInformation.KILOBYTES,
        suggested_display_precision=0,
        value_fn=lambda data: round(data.diagnostics.free_heap / 1024),
    ),
    SmartHeatingSensorDescription(
        key="wifi_rssi",
        name="Wi-Fi Signal",
        device_class=SensorDeviceClass.SIGNAL_STRENGTH,
        native_unit_of_measurement=SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
        value_fn=lambda data: data.diagnostics.wifi_rssi,
    ),
    SmartHeatingSensorDescription(
        key="uptime",
        name="Uptime",
        device_class=SensorDeviceClass.TIMESTAMP,
        value_fn=lambda data: dt_util.utcnow() - timedelta(seconds=data.diagnostics.uptime_sec),
    ),
    SmartHeatingSensorDescription(
        key="dtc_count",
        name="Active DTC Fault Count",
        value_fn=lambda data: data.diagnostics.dtc_count,
    ),
    SmartHeatingSensorDescription(
        key="season_state",
        name="Season Operational State",
        icon="mdi:weather-sunset-up",
        value_fn=lambda data: "Se încălzește" if data.boiler.season_state == "HEATING" else "Oprit (Vară)",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up Smart Heating sensor entities."""
    coordinator: SmartHeatingDataUpdateCoordinator = hass.data[DOMAIN][entry.entry_id]

    entities: list[SensorEntity] = [
        SmartHeatingSystemSensor(coordinator, entry, desc)
        for desc in SYSTEM_SENSORS
    ]

    # Add per-zone sensors (Humidity, Battery, RSSI, Runtime)
    for zone_id in coordinator.data.zones:
        entities.append(SmartHeatingZoneHumiditySensor(coordinator, entry, zone_id))
        entities.append(SmartHeatingZoneBatterySensor(coordinator, entry, zone_id))
        entities.append(SmartHeatingZoneRssiSensor(coordinator, entry, zone_id))
        entities.append(SmartHeatingZoneRuntimeSensor(coordinator, entry, zone_id))

    async_add_entities(entities)


class SmartHeatingSystemSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SensorEntity
):
    """Representation of a Smart Heating controller system sensor."""

    entity_description: SmartHeatingSensorDescription
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        description: SmartHeatingSensorDescription,
    ) -> None:
        """Initialize the sensor."""
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
    def native_value(self) -> Any:
        """Return the state of the sensor."""
        return self.entity_description.value_fn(self.coordinator.data)


class SmartHeatingZoneHumiditySensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SensorEntity
):
    """Representation of a zone humidity sensor."""

    _attr_has_entity_name = True
    _attr_device_class = SensorDeviceClass.HUMIDITY
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = PERCENTAGE

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_humidity"
        self._attr_name = "Humidity"

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
    def native_value(self) -> float | None:
        """Return the humidity level."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return zone.humidity if zone else None


class SmartHeatingZoneBatterySensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SensorEntity
):
    """Representation of a zone sensor battery level."""

    _attr_has_entity_name = True
    _attr_device_class = SensorDeviceClass.BATTERY
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = PERCENTAGE

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_battery"
        self._attr_name = "Sensor Battery"

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
    def native_value(self) -> int | None:
        """Return the battery percentage."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return zone.battery if zone else None


class SmartHeatingZoneRssiSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SensorEntity
):
    """Representation of a zone sensor BLE signal strength."""

    _attr_has_entity_name = True
    _attr_device_class = SensorDeviceClass.SIGNAL_STRENGTH
    _attr_state_class = SensorStateClass.MEASUREMENT
    _attr_native_unit_of_measurement = SIGNAL_STRENGTH_DECIBELS_MILLIWATT

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_ble_rssi"
        self._attr_name = "Sensor BLE Signal"

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
    def native_value(self) -> int | None:
        """Return the BLE RSSI in dBm."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return zone.rssi if zone else None


class SmartHeatingZoneRuntimeSensor(
    CoordinatorEntity[SmartHeatingDataUpdateCoordinator], SensorEntity
):
    """Representation of a zone heating runtime today."""

    _attr_has_entity_name = True
    _attr_device_class = SensorDeviceClass.DURATION
    _attr_state_class = SensorStateClass.TOTAL_INCREASING
    _attr_native_unit_of_measurement = UnitOfTime.SECONDS
    _attr_suggested_display_precision = 0

    def __init__(
        self,
        coordinator: SmartHeatingDataUpdateCoordinator,
        entry: ConfigEntry,
        zone_id: int,
    ) -> None:
        """Initialize the sensor."""
        super().__init__(coordinator)
        self._entry = entry
        self._zone_id = zone_id
        self._attr_unique_id = f"{entry.unique_id}_zone_{zone_id}_runtime_today"
        self._attr_name = "Heating Runtime Today"

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
    def native_value(self) -> int:
        """Return runtime in seconds."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        if not zone or not zone.runtime_today_sec:
            return 0
        return int(zone.runtime_today_sec)

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return raw seconds as extra attribute."""
        zone = self.coordinator.data.zones.get(self._zone_id)
        return {
            "runtime_seconds": zone.runtime_today_sec if zone else 0,
            "runtime_yesterday_seconds": zone.runtime_yesterday_sec if zone else 0,
        }
