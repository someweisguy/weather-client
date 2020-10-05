#include "sph0645.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s.h"

#include <math.h>
#include <string.h>

#include "sos_iir_filter.h"

#define SAMPLE_RATE 48000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS 32    // Number of bits received in the i2s frame.

#define MIC_SENSITIVITY -26   // dBFS value expected at MIC_REF_DB (value from datasheet)
#define MIC_REF_DB 94.0       // Value at which point sensitivity is specified in datasheet (dB)
#define MIC_OVERLOAD_DB 116.0 // Acoustic overload point of the microphone (dB).
#define MIC_NOISE_DB 29       // Noise floor of the microphone (dB).
#define MIC_BITS 24           // Valid number of bits in i2s frame. Must be less than or equal to SAMPLE_BITS.
#define MIC_POWER_UP_TIME 50  // Power-up time of the microphone (ms).
#define MIC_OFFSET_DB 3.0103  // Default offset (sine-wave RMS vs. dBFS). Modify this value for linear calibration.

#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)

static TaskHandle_t mic_reader_task_handle = NULL; // The task handle for the mic reader task
static sph0645_data_t task_data;                   // Holds the currently collected data. Average is calculated lazily.
static sph0645_config_t task_config;               // Holds the current config data.
static float *samples = NULL;

static void mic_reader_task(void *arg)
{
    const size_t num_samples = SAMPLE_RATE / 1000 * task_config.sample_length;               // Number of samples needed for the configured sample length.
    const double mic_ref_ampl = pow10(MIC_SENSITIVITY / 20.0) * ((1 << (MIC_BITS - 1)) - 1); // Microphone i2s output at 94dB SPL.
    float (*weighing)(float *, float *, size_t);
    if (task_config.weighting == SPH0645_WEIGHTING_C)
        weighing = weight_dBC;
    else if (task_config.weighting == SPH0645_WEIGHTING_A)
        weighing = weight_dBA;
    else
        weighing = weight_none;

    uint64_t acc_samples = 0;
    double acc_sum_sqr = 0;
    bool delay_state_uninitialized = true;

    sph0645_clear_data();

    while (true)
    {
        // Block and wait for microphone values from i2s
        i2s_bus_read(samples, num_samples * sizeof(int32_t), portMAX_DELAY);

        // Convert integer microphone values to floats
        int32_t *int_samples = (int32_t *)samples;
        for (int i = 0; i < num_samples; i++)
            samples[i] = int_samples[i] >> (SAMPLE_BITS - MIC_BITS);

        // Apply equalization and get C-weighted sum of squares
        const float sum_sqr_z = equalize(samples, samples, num_samples);
        const float sum_sqr_c = weighing(samples, samples, num_samples);

        // Discard first round of data because of uninitialized delay state
        if (delay_state_uninitialized)
        {
            delay_state_uninitialized = false;
            continue;
        }

        // Calculate dB values relative to mic_ref_ampl and adjust for microphone reference
        const double rms_z = sqrt((double)sum_sqr_z / num_samples);
        const double dBz = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(rms_z / mic_ref_ampl);

        // In case of acoustic overload or below noise floor measurement, report infinty
        if (dBz > MIC_OVERLOAD_DB)
            acc_sum_sqr = INFINITY;
        else if (isnan(dBz) || (dBz < MIC_NOISE_DB))
            acc_sum_sqr = NAN;

        // Accumulate the C-weighted sum of squares
        acc_sum_sqr += sum_sqr_c;
        acc_samples += num_samples;

        // When we gather enough samples, calculate the RMS C-weighted value
        if (acc_samples >= SAMPLE_RATE * task_config.sample_period / 1000.0)
        {
            vTaskSuspendAll(); // enter critical section, interrupts enabled

            const double rms_c = sqrt(acc_sum_sqr / acc_samples);
            const double dBc = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(rms_c / mic_ref_ampl);

            // Add the data to the currently running data
            task_data.avg += dBc;
            task_data.min = MIN(task_data.min, dBc);
            task_data.max = MAX(task_data.max, dBc);
            ++task_data.samples;

            xTaskResumeAll(); // exit critical section

            // zero out the accumulators
            acc_sum_sqr = 0;
            acc_samples = 0;
        }
    }
}

esp_err_t sph0645_reset()
{
    if (samples == NULL)
    {
        // Discard data to allow for mic startup
        const size_t num_samples = SAMPLE_RATE / 1000 * MIC_POWER_UP_TIME;
        for (int i = 0; i < num_samples; ++i)
        {
            int32_t discard;
            esp_err_t err = i2s_bus_read(&discard, sizeof(discard), 1);
            if (err)
                return err;
        }
    }
    else
    {
        // Delete the task if it is currently running
        if (mic_reader_task_handle != NULL)
            vTaskDelete(mic_reader_task_handle);

        // Create the reader task
        xTaskCreate(mic_reader_task, "i2s_mic_reader", 2048, NULL, 4,
                    &mic_reader_task_handle);
    }

    return ESP_OK;
}

esp_err_t sph0645_set_config(const sph0645_config_t *config)
{
    if (config->sample_length == 0 || config->sample_period == 0)
        return ESP_ERR_INVALID_ARG;

    // Suspend the currently running mic task
    if (mic_reader_task_handle != NULL)
        vTaskSuspend(mic_reader_task_handle);

    // Check to see if we can malloc a large enough block of memory for samples
    const size_t num_samples = SAMPLE_RATE / 1000 * config->sample_length;
    float *tmp_samples = realloc(samples, num_samples * sizeof(float));
    if (tmp_samples == NULL)
    {
        if (mic_reader_task_handle != NULL)
            vTaskResume(mic_reader_task_handle);
        return ESP_ERR_NO_MEM;
    }
    samples = tmp_samples;

    // Copy argument to task_config
    memcpy(&task_config, config, sizeof(task_config));

    // Restart the task automatically if it was running
    if (mic_reader_task_handle != NULL)
        vTaskDelete(mic_reader_task_handle);
    xTaskCreate(mic_reader_task, "i2s_mic_reader", 2048, NULL, 4, &mic_reader_task_handle);

    return ESP_OK;
}

esp_err_t sph0645_get_config(sph0645_config_t *config)
{
    memcpy(config, &task_config, sizeof(task_config));
    return ESP_OK;
}

esp_err_t sph0645_get_data(sph0645_data_t *data)
{
    if (mic_reader_task_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    vTaskSuspendAll(); // enter critical section, interrupts enabled

    // Get the average and copy the data over
    memcpy(data, &task_data, sizeof(task_data));
    data->avg /= data->samples;

    xTaskResumeAll(); // exit critical section

    return ESP_OK;
}

void sph0645_clear_data()
{
    vTaskSuspendAll(); // enter critical section, interrupts enabled

    // Reset the task data
    task_data.avg = 0;
    task_data.samples = 0;
    task_data.max = -INFINITY;
    task_data.min = INFINITY;

    xTaskResumeAll(); // exit critical section
}