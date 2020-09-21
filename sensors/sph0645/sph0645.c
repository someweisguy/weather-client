#include "sph0645.h"

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

esp_err_t stub()
{
    const int SAMPLES = 256;

    int samples[SAMPLES];
    for (int i = 0; i < SAMPLES; ++i)
    {
        int sample[2] = {};
      
        i2s_master_read(sample, 8, 1000);

        sample[0] >>= 14;
        samples[i] = sample[0];
    }

    // get the average sample value
    int meanval = 0;
    for (int i = 0; i < SAMPLES; i++)
    {
        meanval += samples[i];
    }
    meanval /= SAMPLES;

    // subtract avg from all to normalize
    for (int i = 0; i < SAMPLES; i++)
    {
        samples[i] -= meanval;
    }

    // find the 'peak to peak' max
    int maxsample, minsample;
    minsample = 1000000;
    maxsample = -1000000;
    for (int i = 0; i < SAMPLES; i++)
    {
        minsample = MIN(minsample, samples[i]);
        maxsample = MAX(maxsample, samples[i]);
    }
    int peak_to_peak = maxsample - minsample;

    uint64_t squares = 0;
    for (int i = 0; i < SAMPLES; i++)
    {
        squares += samples[i] * samples[i];
    }
    float rms = sqrt(squares);

    float dB = 20 * log10(rms / 8388607.0) + 94;
    float dBp2p = 20 * log10(peak_to_peak / 8388607.0) + 94;
    printf("%.2f dB RMS (p2p=%.2f dB)\n", dB, dBp2p);

    return ESP_OK;
}