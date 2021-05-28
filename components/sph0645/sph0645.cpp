#include "sph0645.hpp"

const discovery_t sph0645_t::discoveries[] = {
  {
    .topic = "sensor/noise",
    .config = {
      .device_class = nullptr,
      .force_update = true,
      .icon = "mdi:volume-high",
      .name = "Noise Pollution",
      .unit_of_measurement = "dB",
      .value_template = "{{ value_json." NOISE_KEY " | round(1) }}"
    }
  }
};