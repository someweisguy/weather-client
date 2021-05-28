#include "bme280.hpp"

const discovery_t bme280_t::discoveries[] = {
  {
    .topic = "sensor/temperature",
    .config = {
      .device_class = "temperature",
      .force_update = true,
      .icon = "mdi:thermometer",
      .name = "Temperature",
      .unit_of_measurement = "°F",
      .value_template = "{{ value_json." TEMPERATURE_KEY " | round(1) }}"
    },
  },
  {
    .topic = "sensor/pressure",
    .config = {
      .device_class = "pressure",
      .force_update = true,
      .icon = "mdi:gauge",
      .name = "Pressure",
      .unit_of_measurement = "inHg",
      .value_template = "{{ value_json." PRESSURE_KEY " | round(2) }}"
    },
  },
  {
    .topic = "sensor/humidity",
    .config = {
      .device_class = "humidity",
      .force_update = true,
      .icon = "mdi:water-percent",
      .name = "Humidity",
      .unit_of_measurement = "%",
      .value_template = "{{ value_json." HUMIDITY_KEY " | round(1) }}"
    }, 
  },
  {
    .topic = "sensor/dew_point",
    .config = {
      .device_class = nullptr,
      .force_update = true,
      .icon = "mdi:weather-fog",
      .name = "Dew Point",
      .unit_of_measurement = "°F",
      .value_template = "{{ value_json." DEW_POINT_KEY " | round(1) }}"
    },
  }
};