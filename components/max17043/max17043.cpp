#include "max17043.hpp"

 const discovery_t max17043_t::discoveries[] ={
  {
    .topic = "sensor/battery",
    .config = {
      .device_class = "battery",
      .force_update = true,
      .icon = nullptr,
      .name = "Battery",
      .unit_of_measurement = "%",
      .value_template = "{{ value_json." BATTERY_KEY " | round(0) }}"
    }
  }
};