/* SPDX-License-Identifier: MPL-2.0
 * XX29 foreground-only spectrum-to-note-LED mapper. No audio/platform calls.
 */
#ifndef XX29_XENOLITH_LIGHTS_H
#define XX29_XENOLITH_LIGHTS_H
#include <stdint.h>
#define XENOLITH_LIGHTS_KEYS 27u
#define XENOLITH_LIGHTS_MASK UINT32_C(0x07ffffff)

typedef struct {
    float target[XENOLITH_LIGHTS_KEYS], envelope[XENOLITH_LIGHTS_KEYS];
    uint32_t bright, mid, dim, age_ms;
} XenolithLights;

void xenolith_lights_init(XenolithLights *s);
/* Reuse a fresh calibrated single-sided FFT magnitude array (N/2+1 entries).
 * N=1024/2048/4096. Bands span 40..12000 Hz logarithmically, low to high keys.
 * DC is excluded. Each bin center belongs to one band; narrow low bands can
 * have no bin at coarse FFT sizes. Invalid/negative magnitudes are ignored.
 * Returns 0 for invalid arguments without changing state, otherwise 1.
 */
int xenolith_lights_ingest(XenolithLights *s, const float *magnitudes,
                          uint32_t fft_size, float sample_rate);
/* Foreground update using elapsed wall-clock milliseconds. Attack 25 ms,
 * release 350 ms, stale-frame timeout 500 ms. Thresholds: -55/-70/-85 dBFS.
 * Held keys win at full brightness; all three output masks are disjoint.
 * Panic clears envelopes and all LEDs immediately, including held indication.
 * Call with panic=0 and held_keys to show held keys after init or SCL-off.
 */
void xenolith_lights_tick(XenolithLights *s, uint32_t elapsed_ms,
                         uint32_t held_keys, int panic);
#endif
