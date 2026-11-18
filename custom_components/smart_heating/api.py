"""API Client for Smart Heating ESP32 controller."""
from __future__ import annotations

import asyncio
import logging
from typing import Any
import aiohttp

from .const import (
    ENDPOINT_BOILER_STATUS,
    ENDPOINT_BLE_SENSORS,
    ENDPOINT_CONFIG_SYSTEM,
    ENDPOINT_DIAG_CLEAR_ALL,
    ENDPOINT_DIAG_CLEAR_EVENTS,
    ENDPOINT_DIAG_STATUS,
    ENDPOINT_GAS_METER,
    ENDPOINT_SCHEDULE_ENABLE,
    ENDPOINT_SCHEDULE_GET,
    ENDPOINT_SCHEDULE_LEVEL,
    ENDPOINT_SET_SETPOINT,
    ENDPOINT_WINDOW_CLEAR,
    ENDPOINT_ZONE_HOLD_CLEAR,
    ENDPOINT_ZONE_TOGGLE,
    ENDPOINT_ZONES,
)

_LOGGER = logging.getLogger(__name__)


class SmartHeatingError(Exception):
    """Base exception for Smart Heating."""


class SmartHeatingConnectionError(SmartHeatingError):
    """Exception raised when connection to controller fails."""


class SmartHeatingApiResponseError(SmartHeatingError):
    """Exception raised when controller returns an error response."""


class SmartHeatingApiClient:
    """Async API client for Smart Heating ESP32 controller."""

    def __init__(
        self,
        host: str,
        port: int = 80,
        username: str = "bogdan",
        password: str = "bogdan123",
        session: aiohttp.ClientSession | None = None,
    ) -> None:
        """Initialize API client."""
        self._host = host
        self._port = port
        self._username = username
        self._password = password
        self._session = session
        self._close_session = False
        self._base_url = f"http://{host}:{port}"
        # Serialize access to ESP32 to prevent overloading single-threaded embedded server
        self._semaphore = asyncio.Semaphore(1)

    async def _get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp ClientSession."""
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession()
            self._close_session = True
        return self._session

    async def close(self) -> None:
        """Close session if created locally."""
        if self._close_session and self._session and not self._session.closed:
            await self._session.close()

    async def _request(
        self,
        method: str,
        endpoint: str,
        params: dict[str, Any] | None = None,
        json_data: dict[str, Any] | None = None,
        timeout_sec: float = 10.0,
    ) -> dict[str, Any] | list[Any]:
        """Execute HTTP request with semaphore and timeout."""
        session = await self._get_session()
        url = f"{self._base_url}{endpoint}"
        auth = aiohttp.BasicAuth(self._username, self._password) if self._username and self._password else None

        async with self._semaphore:
            try:
                async with asyncio.timeout(timeout_sec):
                    async with session.request(
                        method=method,
                        url=url,
                        params=params,
                        json=json_data,
                        auth=auth,
                    ) as response:
                        if response.status >= 400:
                            text = await response.text()
                            _LOGGER.warning(
                                "Smart Heating API error %s on %s: %s",
                                response.status,
                                endpoint,
                                text,
                            )
                            raise SmartHeatingApiResponseError(
                                f"HTTP {response.status}: {text}"
                            )
                        return await response.json(content_type=None)
            except (asyncio.TimeoutError, aiohttp.ClientError) as err:
                _LOGGER.error("Connection error to %s: %s", url, err)
                raise SmartHeatingConnectionError(f"Error connecting to {url}: {err}") from err

    async def async_ping(self) -> dict[str, Any]:
        """Test connection and return boiler status overview."""
        res = await self._request("GET", ENDPOINT_BOILER_STATUS)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in ping response")

    async def async_get_zones(self) -> dict[str, Any]:
        """Fetch zones telemetry and state."""
        res = await self._request("GET", ENDPOINT_ZONES)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in zones response")

    async def async_get_boiler_status(self) -> dict[str, Any]:
        """Fetch boiler and system metrics."""
        res = await self._request("GET", ENDPOINT_BOILER_STATUS)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in boiler response")

    async def async_get_ble_sensors(self) -> list[dict[str, Any]]:
        """Fetch BLE sensors live readings."""
        res = await self._request("GET", ENDPOINT_BLE_SENSORS)
        if isinstance(res, list):
            return res
        if isinstance(res, dict) and "sensors" in res and isinstance(res["sensors"], list):
            return res["sensors"]
        return []

    async def async_get_diag_status(self) -> dict[str, Any]:
        """Fetch active diagnostics and DTC status."""
        res = await self._request("GET", ENDPOINT_DIAG_STATUS)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in diag response")

    async def async_get_system_config(self) -> dict[str, Any]:
        """Fetch system configuration."""
        res = await self._request("GET", ENDPOINT_CONFIG_SYSTEM)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in config response")

    async def async_get_schedule(self) -> dict[str, Any]:
        """Fetch schedule configuration and active state."""
        res = await self._request("GET", ENDPOINT_SCHEDULE_GET)
        if isinstance(res, dict):
            return res
        raise SmartHeatingApiResponseError("Expected dict in schedule response")

    async def async_set_zone_setpoint(self, zone_id: int, target: float) -> bool:
        """Set zone temporary boost or setpoint without corrupting schedule baseline."""
        params = {"id": str(zone_id), "target": f"{target:.1f}"}
        await self._request("POST", ENDPOINT_SET_SETPOINT, params=params)
        return True

    async def async_set_schedule_level(self, zone_id: int, level: str, temp: float) -> bool:
        """Set schedule baseline level temperature (day, night, away)."""
        params = {"zone": str(zone_id), "level": level, "temp": f"{temp:.1f}"}
        await self._request("POST", ENDPOINT_SCHEDULE_LEVEL, params=params)
        return True

    async def async_clear_window_pause(self, zone_id: int = 0) -> bool:
        """Clear window pause state on a specific zone or all zones."""
        params = {"id": "all" if zone_id == 0 else str(zone_id)}
        await self._request("POST", ENDPOINT_WINDOW_CLEAR, params=params)
        return True

    async def async_clear_dtc(self, include_events: bool = True) -> bool:
        """Reset active DTC faults and optionally purge event history."""
        params = {"events": "1" if include_events else "0"}
        await self._request("POST", ENDPOINT_DIAG_CLEAR_ALL, params=params)
        return True

    async def async_clear_dtc_events(self) -> bool:
        """Purge DTC event log history in LittleFS."""
        await self._request("POST", ENDPOINT_DIAG_CLEAR_EVENTS)
        return True

    async def async_toggle_zone(self, zone_id: int) -> bool:
        """Toggle zone enabled state persistently via native firmware toggle."""
        params = {"id": str(zone_id)}
        await self._request("POST", ENDPOINT_ZONE_TOGGLE, params=params)
        return True

    async def async_clear_zone_hold(self, zone_id: int) -> bool:
        """Clear manual temperature hold and return zone to schedule."""
        params = {"id": str(zone_id)}
        await self._request("POST", ENDPOINT_ZONE_HOLD_CLEAR, params=params)
        return True

    async def async_set_away_mode(self, away: bool) -> bool:
        """Set global Away (Plecat) mode."""
        params = {"away": "1" if away else "0"}
        await self._request("POST", ENDPOINT_SCHEDULE_ENABLE, params=params)
        return True

    async def async_set_schedule_enabled(self, enabled: bool) -> bool:
        """Enable or disable scheduled heating globally."""
        params = {"enabled": "1" if enabled else "0"}
        await self._request("POST", ENDPOINT_SCHEDULE_ENABLE, params=params)
        return True

    async def async_submit_gas_reading(self, index: float, epoch: int | None = None) -> bool:
        """Submit a new physical gas meter reading index."""
        params = {"index": f"{index:.3f}"}
        if epoch is not None:
            params["time"] = str(epoch)
        await self._request("POST", ENDPOINT_GAS_METER, params=params)
        return True

    async def async_set_system_config(self, payload: dict[str, Any]) -> bool:
        """Update system hydraulic and safety configuration as form parameters."""
        # Convert dictionary to form parameters for WebApi parsing
        form_params = {k: str(v).lower() if isinstance(v, bool) else str(v) for k, v in payload.items()}
        await self._request("POST", ENDPOINT_CONFIG_SYSTEM, params=form_params)
        return True

    async def async_set_season_mode(self, mode: str) -> bool:
        """Set heating season mode (auto, on, off)."""
        params = {"mode": mode.lower()}
        await self._request("POST", "/api/season", params=params)
        return True
