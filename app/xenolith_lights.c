/* SPDX-License-Identifier: MPL-2.0
 * XX29 measured-spectrum LED mapping, 2026. Independently written.
 * fastmath.h retains Charles Vestal's MPL-2.0 notice.
 */
#include "xenolith_lights.h"
#include "fastmath.h"

/* Mathematical data: 40 * 300^(i/27), not equal-tempered note pitches. */
static const float band_edge[28] = {
    40.0000000f, 49.4089050f, 61.0309944f, 75.3868637f, 93.1195602f, 115.023384f, 142.079483f,
    175.499802f, 216.781311f, 267.773193f, 330.759491f, 408.561584f, 504.664520f, 623.373047f,
    770.004456f, 951.126892f, 1174.85339f, 1451.20557f, 1792.56189f, 2214.21289f, 2735.04590f,
    3378.39063f, 4173.06445f, 5154.66357f, 6367.15674f, 7864.85596f, 9714.84766f, 12000.0000f
};

void xenolith_lights_init(XenolithLights *s) {
    unsigned i;
    for (i=0; i<XENOLITH_LIGHTS_KEYS; ++i) s->target[i]=s->envelope[i]=0;
    s->bright=s->mid=s->dim=0;
    s->age_ms=500;
}

int xenolith_lights_ingest(XenolithLights *s, const float *magnitudes,
                          uint32_t fft_size, float sample_rate) {
    unsigned i, band=0;
    float bin_hz;
    if (!s || !magnitudes || (fft_size!=1024u && fft_size!=2048u && fft_size!=4096u)
        || !(sample_rate>=8000.0f && sample_rate<=192000.0f)) return 0;
    for (i=0; i<XENOLITH_LIGHTS_KEYS; ++i) s->target[i]=0;
    bin_hz=sample_rate/(float)fft_size;
    for (i=1; i<=fft_size/2u; ++i) {
        float frequency=(float)i*bin_hz, magnitude=magnitudes[i];
        if (frequency<band_edge[0]) continue;
        if (frequency>band_edge[27]) break;
        while (band<26u && frequency>=band_edge[band+1u]) ++band;
        /* A calibrated normalized int16 FFT cannot exceed 2. Invalid values
         * do not poison envelopes or turn an infinity into a bright LED. */
        if (magnitude>0.0f && magnitude<=2.0f && magnitude>s->target[band])
            s->target[band]=magnitude;
    }
    s->age_ms=0;
    return 1;
}

void xenolith_lights_tick(XenolithLights *s, uint32_t elapsed_ms,
                         uint32_t held_keys, int panic) {
    unsigned i;
    float attack, release;
    if (panic) { xenolith_lights_init(s); return; }
    if (elapsed_ms>10000u) elapsed_ms=10000u;
    s->age_ms = s->age_ms+elapsed_ms>10000u ? 10000u : s->age_ms+elapsed_ms;
    attack=1.0f-fm_expf(-(float)elapsed_ms/25.0f);
    release=1.0f-fm_expf(-(float)elapsed_ms/350.0f);
    s->bright=held_keys & XENOLITH_LIGHTS_MASK;
    s->mid=s->dim=0;
    for (i=0; i<XENOLITH_LIGHTS_KEYS; ++i) {
        float target=s->age_ms>=500u ? 0.0f : s->target[i];
        float value=s->envelope[i];
        value+=(target-value)*(target>value ? attack : release);
        if (value<0.0000001f) value=0;
        s->envelope[i]=value;
        if (value>=0.00177827941f) s->bright|=1u<<i;
        else if (value>=0.000316227766f) s->mid|=1u<<i;
        else if (value>=0.0000562341325f) s->dim|=1u<<i;
    }
    s->mid &= ~s->bright;
    s->dim &= ~(s->bright|s->mid);
}
