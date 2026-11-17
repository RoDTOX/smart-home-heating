"""Constants for the Smart Heating integration."""
from homeassistant.const import Platform

DOMAIN = "smart_heating"
DEFAULT_NAME = "Smart Heating"
DEFAULT_HOST = "192.168.1.20"
DEFAULT_PORT = 80
DEFAULT_SCAN_INTERVAL = 10

CONF_HOST = "host"
CONF_PORT = "port"
CONF_SCAN_INTERVAL = "scan_interval"

PLATFORMS: list[Platform] = [
    Platform.CLIMATE,
    Platform.SENSOR,
    Platform.BINARY_SENSOR,
    Platform.NUMBER,
    Platform.SELECT,
    Platform.BUTTON,
]

# Firmware Endpoints
ENDPOINT_ZONES = "/api/zones"
ENDPOINT_BOILER_STATUS = "/api/boiler/status"
ENDPOINT_BLE_SENSORS = "/api/ble/sensors"
ENDPOINT_CONFIG_SYSTEM = "/api/config/system"
ENDPOINT_DIAG_STATUS = "/api/diag/status"
ENDPOINT_SET_SETPOINT = "/api/zones/setpoint"
ENDPOINT_WINDOW_CLEAR = "/api/zones/window/clear"
ENDPOINT_DIAG_CLEAR = "/api/diag/events/clear"
ENDPOINT_SCHEDULE_LEVEL = "/api/schedule/level"

# Services
SERVICE_CLEAR_DTC = "clear_dtc_history"
SERVICE_CLEAR_WINDOW_PAUSE = "clear_window_pause"
SERVICE_SET_WINDOW_PAUSE = "set_window_pause"
SERVICE_SET_ZONE_BOOST = "set_zone_boost"

# Presets
PRESET_COMFORT = "comfort"
PRESET_ECO = "eco"
PRESET_AWAY = "away"

PRESET_TEMPERATURES = {
    PRESET_COMFORT: 23.5,
    PRESET_ECO: 20.5,
    PRESET_AWAY: 17.0,
}
