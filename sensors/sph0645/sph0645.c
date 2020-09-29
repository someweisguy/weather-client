#include "sph0645.h"

#include "sos_iir_filter.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "i2s.h"

#include <math.h>

#define MIN(a, b) ((a < b) ? a : b)
#define MAX(a, b) ((a > b) ? a : b)

// value of 420426 == 94dBa @ 10kHz
// see https://hackaday.io/project/166867-esp32-i2s-slm
// https://github.com/ikostoski/esp32-i2s-slm/blob/master/esp32-i2s-slm.ino

/*
n = readout from microphone
20 * log(n / 2**23-1) == dBFS
dBFS + 94 == dB
*/

#define LEQ_PERIOD 1 // second(s)

#define SAMPLE_RATE 48000 // Hz, fixed to design of IIR filters
#define SAMPLE_BITS 32    // bits
#define SAMPLE_T int32_t
#define SAMPLES_SHORT (SAMPLE_RATE / 8) // ~125ms
#define SAMPLES_LEQ (SAMPLE_RATE * LEQ_PERIOD)
#define DMA_BANK_SIZE (SAMPLES_SHORT / 16)
#define DMA_BANKS 32

#define I2S_TASK_PRI 4
#define I2S_TASK_STACK 2048

#define MIC_SENSITIVITY -26   // dBFS value expected at MIC_REF_DB (Sensitivity value from datasheet)
#define MIC_REF_DB 94.0       // Value at which point sensitivity is specified in datasheet (dB)
#define MIC_OVERLOAD_DB 116.0 // dB - Acoustic overload point
#define MIC_NOISE_DB 29       // dB - Noise floor
#define MIC_BITS 24           // valid number of bits in I2S data
#define MIC_CONVERT(s)    (s >> (SAMPLE_BITS - MIC_BITS))

#define MIC_OFFSET_DB -2//3.0103 // Default offset (sine-wave RMS vs. dBFS). Modify this value for linear calibration

static const double MIC_REF_AMPL = 420426.27363163716; // pow(10, (double)MIC_SENSITIVITY / 20) * ((1 << (MIC_BITS - 1)) - 1);

// Data we push to 'samples_queue'
typedef struct
{
    float sum_sqr_SPL;      // Sum of squares of mic samples, after Equalizer filter
    float sum_sqr_weighted; // Sum of squares of weighted mic samples
    uint32_t proc_ticks;    // Debug only, FreeRTOS ticks we spent processing the I2S data
} sum_queue_t;

static float samples[SAMPLES_SHORT] = {};
static QueueHandle_t samples_queue;

static void mic_i2s_reader_task(void *parameter)
{
    // Discard first block, microphone may have startup time (i.e. INMP441 up to 83ms)
    size_t bytes_read = 0;
    i2s_read(0, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, portMAX_DELAY);

    while (true)
    {
        // Block and wait for microphone values from I2S
        //
        // Data is moved from DMA buffers to our 'samples' buffer by the driver ISR
        // and when there is requested ammount of data, task is unblocked
        //
        // Note: i2s_read does not care it is writing in float[] buffer, it will write
        //       integer values to the given address, as received from the hardware peripheral.
        i2s_read(0, &samples, SAMPLES_SHORT * sizeof(SAMPLE_T), &bytes_read, portMAX_DELAY);

        TickType_t start_tick = xTaskGetTickCount();

        // Convert (including shifting) integer microphone values to floats,
        // using the same buffer (assumed sample size is same as size of float),
        // to save a bit of memory
        SAMPLE_T *int_samples = (SAMPLE_T *)&samples;
        for (int i = 0; i < SAMPLES_SHORT; i++)
            samples[i] = MIC_CONVERT(int_samples[i]);

        sum_queue_t q;
        // Apply equalization and calculate Z-weighted sum of squares,
        // writes filtered samples back to the same buffer.
        q.sum_sqr_SPL = equalize(samples, samples, SAMPLES_SHORT);
        //Serial.printf("Val: %f\n", q.sum_sqr_SPL);

        // Apply weighting and calucate weigthed sum of squares
        q.sum_sqr_weighted = weight_dBC(samples, samples, SAMPLES_SHORT);

        // Debug only. Ticks we spent filtering and summing block of I2S data
        q.proc_ticks = xTaskGetTickCount() - start_tick;

        // Send the sums to FreeRTOS queue where main task will pick them up
        // and further calcualte decibel values (division, logarithms, etc...)
        xQueueSend(samples_queue, &q, portMAX_DELAY);
    }
}

esp_err_t stub()
{
    samples_queue = xQueueCreate(8, sizeof(sum_queue_t));
    xTaskCreate(mic_i2s_reader_task, "Mic I2S Reader", I2S_TASK_STACK, NULL, I2S_TASK_PRI, NULL);

    sum_queue_t q;
    uint32_t Leq_samples = 0;
    double Leq_sum_sqr = 0;
    double Leq_dB = 0;

    // Read sum of samaples, calculated by 'i2s_reader_task'
    while (xQueueReceive(samples_queue, &q, portMAX_DELAY))
    {

        // Calculate dB values relative to MIC_REF_AMPL and adjust for microphone reference
        double short_RMS = sqrt(((double)q.sum_sqr_SPL) / SAMPLES_SHORT);
        double short_SPL_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(short_RMS / MIC_REF_AMPL);

        // In case of acoustic overload or below noise floor measurement, report infinty Leq value
        if (short_SPL_dB > MIC_OVERLOAD_DB)
        {
            Leq_sum_sqr = INFINITY;
        }
        else if (isnan(short_SPL_dB) || (short_SPL_dB < MIC_NOISE_DB))
        {
            Leq_sum_sqr = -INFINITY;
        }

        // Accumulate Leq sum
        Leq_sum_sqr += q.sum_sqr_weighted;
        Leq_samples += SAMPLES_SHORT;

        // When we gather enough samples, calculate new Leq value
        if (Leq_samples >= SAMPLE_RATE * LEQ_PERIOD)
        {
            double Leq_RMS = sqrt(Leq_sum_sqr / Leq_samples);
            Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(Leq_RMS / MIC_REF_AMPL);
            Leq_sum_sqr = 0;
            Leq_samples = 0;

            // Serial output, customize (or remove) as needed
            printf("%.1f\n", Leq_dB);

            // Debug only
            //printf("%u processing ticks\n", q.proc_ticks);
        }

#if (USE_DISPLAY > 0)

        //
        // Example code that displays the measured value.
        // You should customize the below code for your display
        // and display library used.
        //

        display.clear();

        // It is important to somehow notify when the deivce is out of its range
        // as the calculated values are very likely with big error
        if (Leq_dB > MIC_OVERLOAD_DB)
        {
            // Display 'Overload' if dB value is over the AOP
            display.drawString(0, 24, "Overload");
        }
        else if (isnan(Leq_dB) || (Leq_dB < MIC_NOISE_DB))
        {
            // Display 'Low' if dB value is below noise floor
            display.drawString(0, 24, "Low");
        }

        // The 'short' Leq line
        double short_Leq_dB = MIC_OFFSET_DB + MIC_REF_DB + 20 * log10(sqrt(double(q.sum_sqr_weighted) / SAMPLES_SHORT) / MIC_REF_AMPL);
        uint16_t len = min(max(0, int(((short_Leq_dB - MIC_NOISE_DB) / MIC_OVERLOAD_DB) * (display.getWidth() - 1))), display.getWidth() - 1);
        display.drawHorizontalLine(0, 0, len);
        display.drawHorizontalLine(0, 1, len);
        display.drawHorizontalLine(0, 2, len);

        // The Leq numeric decibels
        display.drawString(0, 4, String(Leq_dB, 1) + " " + DB_UNITS);

        display.display();

#endif // USE_DISPLAY
    }
    return ESP_OK;
}