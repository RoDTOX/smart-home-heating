"""The Smart Heating Integration."""
from __future__ import annotations

import logging
from typing import Any

import voluptuous as vol

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .api import SmartHeatingApiClient
from .const import (
    CONF_HOST,
    CONF_PORT,
    CONF_SCAN_INTERVAL,
    DEFAULT_PORT,
    DEFAULT_SCAN_INTERVAL,
    DOMAIN,
    PLATFORMS,
    SERVICE_CLEAR_DTC,
    SERVICE_CLEAR_WINDOW_PAUSE,
    SERVICE_CLEAR_ZONE_HOLD,
    SERVICE_SET_ZONE_BOOST,
    SERVICE_SUBMIT_GAS_READING,
)
from .coordinator import SmartHeatingDataUpdateCoordinator

_LOGGER = logging.getLogger(__name__)

SERVICE_CLEAR_WINDOW_PAUSE_SCHEMA = vol.Schema(
    {
        vol.Optional("zone_id", default=0): cv.positive_int,
    }
)

SERVICE_CLEAR_ZONE_HOLD_SCHEMA = vol.Schema(
    {
        vol.Required("zone_id"): cv.positive_int,
    }
)

SERVICE_SUBMIT_GAS_READING_SCHEMA = vol.Schema(
    {
        vol.Required("index"): vol.Coerce(float),
        vol.Optional("time"): cv.positive_int,
    }
)

SERVICE_SET_ZONE_BOOST_SCHEMA = vol.Schema(
    {
        vol.Required("zone_id"): cv.positive_int,
        vol.Required("boost_temp"): vol.Coerce(float),
    }
)


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up Smart Heating from a config entry."""
    host = entry.data[CONF_HOST]
    port = entry.data.get(CONF_PORT, DEFAULT_PORT)
    scan_interval = entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL)

    session = async_get_clientsession(hass)
    username = entry.data.get("username", "bogdan")
    password = entry.data.get("password", "bogdan123")
    api = SmartHeatingApiClient(
        host=host,
        port=port,
        username=username,
        password=password,
        session=session,
    )

    coordinator = SmartHeatingDataUpdateCoordinator(
        hass=hass,
        api=api,
        scan_interval_sec=scan_interval,
    )

    await coordinator.async_config_entry_first_refresh()

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = coordinator

    # Set up all platforms
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    # Register services
    async def async_handle_clear_dtc(call: ServiceCall) -> None:
        """Handle resetting active DTC faults and clearing history."""
        _LOGGER.info("Resetting DTC faults and clearing event history")
        for coord in hass.data[DOMAIN].values():
            if isinstance(coord, SmartHeatingDataUpdateCoordinator):
                await coord.async_clear_all_dtcs()

    async def async_handle_clear_window(call: ServiceCall) -> None:
        """Handle clearing window pause."""
        zone_id = call.data.get("zone_id", 0)
        _LOGGER.info("Clearing window pause on zone %s", zone_id)
        for coord in hass.data[DOMAIN].values():
            if isinstance(coord, SmartHeatingDataUpdateCoordinator):
                await coord.async_clear_window_pause(zone_id)

    async def async_handle_clear_hold(call: ServiceCall) -> None:
        """Handle clearing manual hold and returning zone to schedule."""
        zone_id = call.data["zone_id"]
        _LOGGER.info("Clearing manual hold on zone %s", zone_id)
        for coord in hass.data[DOMAIN].values():
            if isinstance(coord, SmartHeatingDataUpdateCoordinator):
                await coord.async_clear_zone_hold(zone_id)

    async def async_handle_submit_gas(call: ServiceCall) -> None:
        """Handle submitting gas meter reading index."""
        index = call.data["index"]
        epoch = call.data.get("time")
        _LOGGER.info("Submitting gas meter reading: %.3f m3 (time: %s)", index, epoch)
        for coord in hass.data[DOMAIN].values():
            if isinstance(coord, SmartHeatingDataUpdateCoordinator):
                await coord.async_submit_gas_reading(index, epoch)

    async def async_handle_set_boost(call: ServiceCall) -> None:
        """Handle setting zone boost temperature."""
        zone_id = call.data["zone_id"]
        boost_temp = call.data["boost_temp"]
        _LOGGER.info("Setting zone %s boost temperature to %.1f C", zone_id, boost_temp)
        for coord in hass.data[DOMAIN].values():
            if isinstance(coord, SmartHeatingDataUpdateCoordinator):
                await coord.async_set_zone_temperature(zone_id, boost_temp)

    if not hass.services.has_service(DOMAIN, SERVICE_CLEAR_DTC):
        hass.services.async_register(
            DOMAIN,
            SERVICE_CLEAR_DTC,
            async_handle_clear_dtc,
        )

    if not hass.services.has_service(DOMAIN, SERVICE_CLEAR_WINDOW_PAUSE):
        hass.services.async_register(
            DOMAIN,
            SERVICE_CLEAR_WINDOW_PAUSE,
            async_handle_clear_window,
            schema=SERVICE_CLEAR_WINDOW_PAUSE_SCHEMA,
        )

    if not hass.services.has_service(DOMAIN, SERVICE_CLEAR_ZONE_HOLD):
        hass.services.async_register(
            DOMAIN,
            SERVICE_CLEAR_ZONE_HOLD,
            async_handle_clear_hold,
            schema=SERVICE_CLEAR_ZONE_HOLD_SCHEMA,
        )

    if not hass.services.has_service(DOMAIN, SERVICE_SUBMIT_GAS_READING):
        hass.services.async_register(
            DOMAIN,
            SERVICE_SUBMIT_GAS_READING,
            async_handle_submit_gas,
            schema=SERVICE_SUBMIT_GAS_READING_SCHEMA,
        )

    if not hass.services.has_service(DOMAIN, SERVICE_SET_ZONE_BOOST):
        hass.services.async_register(
            DOMAIN,
            SERVICE_SET_ZONE_BOOST,
            async_handle_set_boost,
            schema=SERVICE_SET_ZONE_BOOST_SCHEMA,
        )

    entry.async_on_unload(entry.add_update_listener(async_reload_entry))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        coordinator = hass.data[DOMAIN].pop(entry.entry_id, None)
        if coordinator:
            await coordinator.api.close()
    return unload_ok


async def async_reload_entry(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Reload config entry when options change."""
    await hass.config_entries.async_reload(entry.entry_id)
