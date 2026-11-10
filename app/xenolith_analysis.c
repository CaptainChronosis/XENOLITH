/* SPDX-License-Identifier: MPL-2.0
 * XX29 display-only radix-2 FFT and positive spectral-change measurement.
 * Independently implemented standard DIT butterflies with a periodic Hann.
 * No heap, trigonometric runtime calls, soft-double arithmetic or DSP feedback.
 * fastmath.h is the existing fm1-base MPL-2.0 single-precision implementation.
 */
#include "xenolith_analysis.h"
#include "xenolith_analysis_table.h"
#include "fastmath.h"

/* Same independently generated63tap Kaiser(beta8.6) halfband coefficients
 * as xenolith_quality.c.16 symmetric pairs plus center=17multiplies/output.
 * Used only on a coherent foreground snapshot, never by the audio callback. */
static const float analysis_halfband_pair[16] = {
    -1.3682310342e-05f, 7.3679824780e-05f, -2.1592065572e-04f, 4.9873709007e-04f,
    -1.0020225852e-03f, 1.8300546271e-03f, -3.1149912511e-03f, 5.0232894202e-03f,
    -7.7699291801e-03f, 1.1651144673e-02f, -1.7121129290e-02f, 2.4981912269e-02f,
    -3.6911386447e-02f, 5.7271132152e-02f, -1.0215353194e-01f, 3.1697309061e-01f
};

static float analysis_halfband_sample(const int16_t *input, uint32_t i) {
    uint32_t k, last = 64u + 2u*i;
    float sum = 4.9999910599e-01f * (float)input[last-31u];
    for (k=0; k<16u; ++k) {
        uint32_t offset=2u*k;
        sum += analysis_halfband_pair[k] * (float)((int32_t)input[last-offset]
                                  + (int32_t)input[last-(62u-offset)]);
    }
    return sum * (1.0f/32768.0f);
}

void xenolith_analysis_init(XenolithAnalysis *s) {
    if (!s) return;
    s->sample_rate = 0;
    s->frame_flux = 0;
    s->window_sum = 0;
    s->fft_size = 0;
    s->bins = 0;
    s->processed_frames = 0;
    s->valid = 0;
    /* Work/previous arrays need no clearing: process fills all used cells,
     * and the first frame never reads previous values. */
}

static int analysis_process(XenolithAnalysis *s, const int16_t *input,
                            uint32_t n, float sample_rate, uint32_t half_rate) {
    uint32_t i, j, bit, len, half, base, step, bins;
    uint32_t has_previous;
    float gain, amplitude_sum = 0, positive_sum = 0;
    if (!s || !input || (n != 1024u && n != 2048u && n != 4096u)
        || !(sample_rate >= 8000.0f && sample_rate <= 192000.0f)) return 0;
    has_previous = s->valid && s->fft_size == n && s->sample_rate == sample_rate;
    step = XENOLITH_ANALYSIS_MAX / n;
    /* Input is normalized against32768: signed16 minimum is exactly-1. */
    j = 0;
    for (i = 0; i < n; ++i) {
        uint32_t phase = (i * step + 1024u) & 4095u;
        float window = 0.5f * (1.0f - xenolith_analysis_sine[phase]);
        float sample = half_rate ? analysis_halfband_sample(input,i)
                                 : (float)input[i] * (1.0f/32768.0f);
        s->real[j] = sample * window;
        s->imag[j] = 0;
        bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
    }
    for (len = 2; len <= n; len <<= 1) {
        half = len >> 1;
        step = XENOLITH_ANALYSIS_MAX / len;
        for (base = 0; base < n; base += len) {
            for (j = 0; j < half; ++j) {
                uint32_t a = base + j, b = a + half, phase = j * step;
                float wr = xenolith_analysis_sine[(phase + 1024u) & 4095u];
                float wi = -xenolith_analysis_sine[phase];
                float tr = wr * s->real[b] - wi * s->imag[b];
                float ti = wr * s->imag[b] + wi * s->real[b];
                float ar = s->real[a], ai = s->imag[a];
                s->real[a] = ar + tr;
                s->imag[a] = ai + ti;
                s->real[b] = ar - tr;
                s->imag[b] = ai - ti;
            }
        }
    }
    bins = (n >> 1) + 1u;
    gain = 4.0f / (float)n; /*2/sum(periodic Hann), sum=N/2. */
    for (i = 0; i < bins; ++i) {
        float ar = s->real[i], ai = s->imag[i];
        float magnitude = fm_sqrtf(ar * ar + ai * ai) * gain;
        float delta = 0;
        if (i == 0 || i == (n >> 1)) magnitude *= 0.5f;
        if (has_previous && magnitude > s->previous[i]) delta = magnitude - s->previous[i];
        s->previous[i] = magnitude;
        s->real[i] = magnitude;
        s->imag[i] = delta;
        /* Exclude the DC bin from the frame-flux summary. imag[0] remains the
         * actual DC-bin change; UI colour should skip it if it wants no DC. */
        if (i != 0) { amplitude_sum += magnitude; positive_sum += delta; }
    }
    s->frame_flux = amplitude_sum > 0.00000001f ? positive_sum / amplitude_sum : 0;
    if (s->frame_flux > 1.0f) s->frame_flux = 1.0f;
    s->sample_rate = sample_rate;
    s->window_sum = (float)n * 0.5f;
    s->fft_size = n;
    s->bins = bins;
    ++s->processed_frames;
    s->valid = 1;
    return 1;
}

int xenolith_analysis_process(XenolithAnalysis *s, const int16_t *input,
                              uint32_t n, float sample_rate) {
    return analysis_process(s,input,n,sample_rate,0u);
}

int xenolith_analysis_process_half(XenolithAnalysis *s, const int16_t *input,
                                   uint32_t count, float input_rate) {
    if (count != XENOLITH_ANALYSIS_HALF_INPUT
        || !(input_rate >= 16000.0f && input_rate <= 192000.0f)) return 0;
    return analysis_process(s,input,2048u,input_rate*0.5f,1u);
}

const float *xenolith_analysis_magnitudes(const XenolithAnalysis *s) {
    return s ? s->real : (const float *)0;
}

const float *xenolith_analysis_flux(const XenolithAnalysis *s) {
    return s ? s->imag : (const float *)0;
}

float xenolith_analysis_dbfs(float amplitude) {
    return amplitude > 0.000001f ? 6.020599913f * fm_log2f(amplitude) : -120.0f;
}

void xenolith_analysis_bessel8(float beta, float coefficients[9]) {
    uint32_t order, j, points;
    if (!coefficients) return;
    if (!(beta >= 0.0f)) beta = 0.0f;
    if (beta > 512.0f) beta = 512.0f;
    points = beta <= 96.0f ? 1024u : 2048u;
    /* Integer-order integral: J_n(beta)=1/pi integral[0,pi]
     * cos(n*theta-beta*sin(theta))dtheta. Midpoint1024/2048 quadrature; sine/cosine
     * use the independent float table, not unstable large-beta power series.
     * Keep the original low-beta arithmetic; above96, midpoint half-indices
     * interpolate the sine table. Cache these display-only values on change.
     */
    for (order = 0; order < 9u; ++order) {
        float sum = 0.0f;
        for (j = 0; j < points; ++j) {
            float phase;
            if (points == 1024u) {
                uint32_t theta_phase = 2u*j+1u;
                phase = (float)(order*theta_phase)
                    - beta*xenolith_analysis_sine[theta_phase]*651.8986469f;
            } else {
                float theta_phase = (float)j+0.5f;
                float theta_sine = 0.5f*(xenolith_analysis_sine[j]
                                      + xenolith_analysis_sine[j+1u]);
                phase = (float)order*theta_phase - beta*theta_sine*651.8986469f;
            }
            float integral = fm_floorf(phase);
            float fraction = phase-integral;
            uint32_t index = ((uint32_t)(int32_t)integral+1024u)&4095u;
            float a = xenolith_analysis_sine[index];
            sum += a+(xenolith_analysis_sine[(index+1u)&4095u]-a)*fraction;
        }
        coefficients[order] = sum/(float)points;
    }
}
