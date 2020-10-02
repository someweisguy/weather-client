#include "sos_iir_filter.h"

typedef struct
{
    float b1;
    float b2;
    float a1;
    float a2;
} SOS_Coefficients;

typedef struct
{
    float w0;
    float w1;
} SOS_Delay_State;

typedef struct
{
    const int num_sos;
    const float gain;
    SOS_Coefficients *sos;
    SOS_Delay_State *w;
} SOS_IIR_Filter;

static SOS_Delay_State *mic_w = NULL, *c_weighting_w = NULL;

extern int sos_filter_f32(float *input, float *output, int len, const SOS_Coefficients *coeffs, SOS_Delay_State *w);
__asm__(
    //
    // ESP32 implementation of IIR Second-Order Section filter
    // Assumes a0 and b0 coefficients are one (1.0)
    //
    // float* a2 = input;
    // float* a3 = output;
    // int    a4 = len;
    // float* a5 = coeffs;
    // float* a6 = w;
    // float  a7 = gain;
    //
    ".text                    \n"
    ".align  4                \n"
    ".global sos_filter_f32   \n"
    ".type   sos_filter_f32,@function\n"
    "sos_filter_f32:          \n"
    "  entry   a1, 16         \n"
    "  lsi     f0, a5, 0      \n" // float f0 = coeffs.b1;
    "  lsi     f1, a5, 4      \n" // float f1 = coeffs.b2;
    "  lsi     f2, a5, 8      \n" // float f2 = coeffs.a1;
    "  lsi     f3, a5, 12     \n" // float f3 = coeffs.a2;
    "  lsi     f4, a6, 0      \n" // float f4 = w[0];
    "  lsi     f5, a6, 4      \n" // float f5 = w[1];
    "  loopnez a4, 1f         \n" // for (; len>0; len--) {
    "    lsip    f6, a2, 4    \n" //   float f6 = *input++;
    "    madd.s  f6, f2, f4   \n" //   f6 += f2 * f4; // coeffs.a1 * w0
    "    madd.s  f6, f3, f5   \n" //   f6 += f3 * f5; // coeffs.a2 * w1
    "    mov.s   f7, f6       \n" //   f7 = f6; // b0 assumed 1.0
    "    madd.s  f7, f0, f4   \n" //   f7 += f0 * f4; // coeffs.b1 * w0
    "    madd.s  f7, f1, f5   \n" //   f7 += f1 * f5; // coeffs.b2 * w1 -> result
    "    ssip    f7, a3, 4    \n" //   *output++ = f7;
    "    mov.s   f5, f4       \n" //   f5 = f4; // w1 = w0
    "    mov.s   f4, f6       \n" //   f4 = f6; // w0 = f6
    "  1:                     \n" // }
    "  ssi     f4, a6, 0      \n" // w[0] = f4;
    "  ssi     f5, a6, 4      \n" // w[1] = f5;
    "  movi.n   a2, 0         \n" // return 0;
    "  retw.n                 \n");

extern float sos_filter_sum_sqr_f32(float *input, float *output, int len, const SOS_Coefficients *coeffs, SOS_Delay_State *w, float gain);
__asm__(
    //
    // ESP32 implementation of IIR Second-Order section filter with applied gain.
    // Assumes a0 and b0 coefficients are one (1.0)
    // Returns sum of squares of filtered samples
    //
    // float* a2 = input;
    // float* a3 = output;
    // int    a4 = len;
    // float* a5 = coeffs;
    // float* a6 = w;
    // float  a7 = gain;
    //
    ".text                    \n"
    ".align  4                \n"
    ".global sos_filter_sum_sqr_f32 \n"
    ".type   sos_filter_sum_sqr_f32,@function \n"
    "sos_filter_sum_sqr_f32:  \n"
    "  entry   a1, 16         \n"
    "  lsi     f0, a5, 0      \n" // float f0 = coeffs.b1;
    "  lsi     f1, a5, 4      \n" // float f1 = coeffs.b2;
    "  lsi     f2, a5, 8      \n" // float f2 = coeffs.a1;
    "  lsi     f3, a5, 12     \n" // float f3 = coeffs.a2;
    "  lsi     f4, a6, 0      \n" // float f4 = w[0];
    "  lsi     f5, a6, 4      \n" // float f5 = w[1];
    "  wfr     f6, a7         \n" // float f6 = gain;
    "  const.s f10, 0         \n" // float sum_sqr = 0;
    "  loopnez a4, 1f         \n" // for (; len>0; len--) {
    "    lsip    f7, a2, 4    \n" //   float f7 = *input++;
    "    madd.s  f7, f2, f4   \n" //   f7 += f2 * f4; // coeffs.a1 * w0
    "    madd.s  f7, f3, f5   \n" //   f7 += f3 * f5; // coeffs.a2 * w1;
    "    mov.s   f8, f7       \n" //   f8 = f7; // b0 assumed 1.0
    "    madd.s  f8, f0, f4   \n" //   f8 += f0 * f4; // coeffs.b1 * w0;
    "    madd.s  f8, f1, f5   \n" //   f8 += f1 * f5; // coeffs.b2 * w1;
    "    mul.s   f9, f8, f6   \n" //   f9 = f8 * f6;  // f8 * gain -> result
    "    ssip    f9, a3, 4    \n" //   *output++ = f9;
    "    mov.s   f5, f4       \n" //   f5 = f4; // w1 = w0
    "    mov.s   f4, f7       \n" //   f4 = f7; // w0 = f7;
    "    madd.s  f10, f9, f9  \n" //   f10 += f9 * f9; // sum_sqr += f9 * f9;
    "  1:                     \n" // }
    "  ssi     f4, a6, 0      \n" // w[0] = f4;
    "  ssi     f5, a6, 4      \n" // w[1] = f5;
    "  rfr     a2, f10        \n" // return sum_sqr;
    "  retw.n                 \n" //
);

static inline float filter(float *input, float *output, size_t len, const int num_sos, const float gain, const SOS_Coefficients *sos, SOS_Delay_State *w)
{
    float *source = input;

    // Apply all but last Second-Order-Section
    for (int i = 0; i < (num_sos - 1); i++)
    {
        sos_filter_f32(source, output, len, &sos[i], &w[i]);
        source = output;
    }

    // Apply last SOS with gain and return the sum of squares of all samples
    return sos_filter_sum_sqr_f32(source, output, len, &sos[num_sos - 1], &w[num_sos - 1], gain);
}

float equalize(float *input, float *output, size_t len)
{
    // Knowles SPH0645LM4H-B, rev. B
    // https://cdn-shop.adafruit.com/product-files/3421/i2S+Datasheet.PDF
    // B ~= [1.001234, -1.991352, 0.990149]
    // A ~= [1.0, -1.993853, 0.993863]
    // With additional DC blocking component
    const float gain = 1.00123377961525;
    const SOS_Coefficients sos[] = {
        {-1.0, 0.0, +0.9992, 0}, // DC blocker, a1 = -0.9992
        {-1.988897663539382, +0.988928479008099, +1.993853376183491, -0.993862821429572}};
    const int num_sos = sizeof(sos) / sizeof(SOS_Coefficients);

    if (mic_w == NULL)
    {
        // lazy initialization
        mic_w = calloc(num_sos, sizeof(SOS_Delay_State));
    }
    return filter(input, output, len, num_sos, gain, sos, mic_w);
}

float weight_dBC(float *input, float *output, size_t len)
{
    // C-weighting IIR Filter, Fs = 48KHz
    // Designed by invfreqz curve-fitting, see: https://github.com/ikostoski/esp32-i2s-slm/blob/master/math/c_weighting.m
    // B = [-0.49164716933714026, 0.14844753846498662, 0.74117815661529129, -0.03281878334039314, -0.29709276192593875, -0.06442545322197900, -0.00364152725482682]
    // A = [1.0, -1.0325358998928318, -0.9524000181023488, 0.8936404694728326   0.2256286147169398  -0.1499917107550188, 0.0156718181681081]
    const float gain = -0.491647169337140;
    const SOS_Coefficients sos[] = {
        {+1.4604385758204708, +0.5275070373815286, +1.9946144559930252, -0.9946217070140883},
        {+0.2376222404939509, +0.0140411206016894, -1.3396585608422749, -0.4421457807694559},
        {-2.0000000000000000, +1.0000000000000000, +0.3775800047420818, -0.0356365756680430}};
    const int num_sos = sizeof(sos) / sizeof(SOS_Coefficients);

    if (c_weighting_w == NULL)
    {
        // lazy initialization
        c_weighting_w = calloc(num_sos, sizeof(SOS_Delay_State));
    }

    return filter(input, output, len, num_sos, gain, sos, c_weighting_w);
}

float weight_dBA(float *input, float *output, size_t len)
{
    // A-weighting IIR Filter, Fs = 48KHz
    // (By Dr. Matt L., Source: https://dsp.stackexchange.com/a/36122)
    // B = [0.169994948147430, 0.280415310498794, -1.120574766348363, 0.131562559965936, 0.974153561246036, -0.282740857326553, -0.152810756202003]
    // A = [1.0, -2.12979364760736134, 0.42996125885751674, 1.62132698199721426, -0.96669962900852902, 0.00121015844426781, 0.04400300696788968]
    const float gain = 0.169994948147430;
    const SOS_Coefficients sos[] = {
        {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
        {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
        {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989}};
    const int num_sos = sizeof(sos) / sizeof(SOS_Coefficients);

    if (c_weighting_w == NULL)
    {
        // lazy initialization
        c_weighting_w = calloc(num_sos, sizeof(SOS_Delay_State));
    }

    return filter(input, output, len, num_sos, gain, sos, c_weighting_w);
}

float weight_none(float *input, float *output, size_t len)
{
    float sum_sqr = 0;
    float s;
    for (int i = 0; i < len; i++)
    {
        s = input[i];
        sum_sqr += s * s;
    }
    if (input != output)
    {
        for (int i = 0; i < len; i++)
            output[i] = input[i];
    }
    return sum_sqr;
}