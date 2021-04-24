#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <math.h>
#include "serial.h"
#include "sos_iir_filter.h"

#define SAMPLE_LENGTH 125
#define SAMPLE_PERIOD 1000

#define SAMPLE_RATE   48000  // Hz, fixed to design of IIR filters
#define SAMPLE_BITS   32     // Number of bits received in the i2s frame.

#define MIC_REF_DB          94.0
#define MIC_SENSITIVITY     -26     // dBFS value expected at MIC_REF_DB
#define MIC_OVERLOAD_DB     116.0   // Acoustic overload point of the microphone
#define MIC_NOISE_FLOOR_DB  29      // Noise floor of the microphone
#define MIC_BITS            24      // Valid number of bits in i2s frame (must be <= SAMPLE_BITS)
#define MIC_POWER_UP_TIME   50      // Power-up time of the microphone (ms)
#define MIC_OFFSET_DB       3.0103  // Default offset (sine-wave RMS vs. dBFS)

#define AVG_NOISE_KEY   "avg_noise"

class sph0645_t : public Sensor {
private:
  const discovery_t discovery[1] {
        {
          .topic = "test/sensor/avg_noise/config",
          .config = {
            .device_class = nullptr,
            .expire_after = 310,
            .force_update = true,
            .icon = nullptr,
            .name = "Average Noise",
            .unit_of_measurement = "dB",
            .value_template = "{{ json." AVG_NOISE_KEY " }}"
          }
        }
    };
  TaskHandle_t mic_task_handle = nullptr;

  static void mic_task(void *arg) {
    // get number of samples and i2s output value at 94dB SPL
    const size_t num_samples = SAMPLE_RATE / 1000 * SAMPLE_LENGTH; 
    const double mic_ref_amp = pow10(MIC_SENSITIVITY / 20.0) * 
      ((1 << (MIC_BITS - 1)) - 1);

    // allocate space to store audio samples
    float *samples = new float[num_samples];
    if (samples == nullptr) ESP_LOGE("sph", "no mem");

    // declare accumulators
    uint64_t acc_samples = 0;
    double acc_sum_sqr = 0;

    // track whether the delay state has been initialized
    bool delay_state_uninitialized = true;

    // clear the data
    float avg = 0;
    float min = INFINITY;
    float max = -INFINITY;

    while (true) {
      // Block and wait for microphone values from i2s
      esp_err_t err = serial_i2s_read(samples, num_samples * sizeof(int32_t), 
        portMAX_DELAY);
      if (err) ESP_LOGE("sph", "no read!");

      // Convert integer microphone values to floats
      int32_t *int_samples = reinterpret_cast<int32_t *>(samples);
      for (int i = 0; i < num_samples; i++) {
        samples[i] = int_samples[i] >> (SAMPLE_BITS - MIC_BITS);
      }

      // apply equalization and get C-weighted sum of squares
      const float sum_sqr_z = equalize(samples, samples, num_samples);
      const float sum_sqr_c = weight_dBC(samples, samples, num_samples);

      // discard first round of data due to uninitialized delay state
      if (delay_state_uninitialized) {
        delay_state_uninitialized = false;
        continue;
      }

      // calculate volume relative to mic_ref_amp
      const double rms_z = sqrt(static_cast<double>(sum_sqr_z) / num_samples);
      const double dBz = MIC_OFFSET_DB + MIC_REF_DB + 20 * 
        log10(rms_z / mic_ref_amp);

      // handle acoustic overload or data below noise floor
      if (dBz > MIC_OVERLOAD_DB) acc_sum_sqr = INFINITY;
      else if (isnan(dBz) || (dBz < MIC_NOISE_FLOOR_DB)) acc_sum_sqr = NAN;

      // Accumulate the C-weighted sum of squares
      acc_sum_sqr += sum_sqr_c;
      acc_samples += num_samples;

      // When we gather enough samples, calculate the RMS C-weighted value
      if (acc_samples >= SAMPLE_RATE * SAMPLE_PERIOD / 1000.0) {
        //vTaskSuspendAll();  // enter critical section, interrupts enabled

        const double rms_c = sqrt(acc_sum_sqr / acc_samples);
        const double dBc = MIC_OFFSET_DB + MIC_REF_DB + 20 * 
          log10(rms_c / mic_ref_amp);
        ESP_LOGI("sph", "%.2f dB", dBc);

        // Add the data to the currently running data
        avg += dBc;
        min = fmin(min, dBc);
        max = fmax(max, dBc);

        //xTaskResumeAll();  // exit critical section
        

        // zero out the accumulators
        acc_sum_sqr = 0;
        acc_samples = 0;
      }
    }
  }
  
public:
  sph0645_t() : Sensor("sph0645") {
    
  }

  int get_discovery(const discovery_t *&discovery) const {
    discovery = this->discovery;
    return sizeof(this->discovery) / sizeof(discovery_t);
  }

  esp_err_t setup() {
    // discard garbage data until mic is powered up
    const size_t num_samples = SAMPLE_RATE / 1000 * MIC_POWER_UP_TIME;
    for (int i = 0; i < num_samples; ++i) {
      int32_t discard;
      esp_err_t err = serial_i2s_read(&discard, sizeof(discard), 1);
      if (err) return err;
    }

    return ESP_OK;
  }

  esp_err_t ready() {
    // start the mic sampling task
    // TODO: pass queue handle instead of nullptr
    xTaskCreate(mic_task, "mic_task", 2048, nullptr, 4, &mic_task_handle);

    return ESP_OK;
  }

  esp_err_t get_data(cJSON *json) {
    return ESP_OK;
  }

  esp_err_t sleep() {
    // delete the running task
    vTaskDelete(mic_task_handle);

    return ESP_OK;
  }

};
