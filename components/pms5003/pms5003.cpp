#include "pms5003.hpp"

const discovery_t pms5003_t::discoveries[] = {
  {
    .topic = "sensor/pm2_5",
    .config = {
      .device_class = nullptr,
      .force_update = true,
      .icon = "mdi:smog",
      .name = "PM 2.5",
      .unit_of_measurement = "μg/m³",
      .value_template = "{{ value_json."  PM_2_5_KEY " }}"
    }
  },
  {
    .topic = "sensor/pm10",
    .config = {
      .device_class = nullptr, 
      .force_update = true, 
      .icon = "mdi:smog", 
      .name = "PM 10", 
      .unit_of_measurement = "μg/m³", 
      .value_template = "{{ value_json." PM_10_KEY " }}"
    }
  }
};