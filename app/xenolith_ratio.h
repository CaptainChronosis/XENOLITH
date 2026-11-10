/* SPDX-License-Identifier: MPL-2.0
 * XX29 foreground MOD RATIO coordinate. No modulation/audio-rate work.
 * Continuous, strictly increasing piecewise-linear mapping and inverse.
 * Integer coordinates are encoder detents; musically useful simple ratios
 * in the central range are exact landmarks rather than logarithmic guesses.
 */
#ifndef XX29_XENOLITH_RATIO_H
#define XX29_XENOLITH_RATIO_H
#include <stdint.h>
#define XENOLITH_RATIO_MIN 0.001f
#define XENOLITH_RATIO_MAX 32.0f
#define XENOLITH_RATIO_STEPS 389

static inline float xenolith_ratio_bound(float value) {
    if (!(value >= XENOLITH_RATIO_MIN)) return XENOLITH_RATIO_MIN;
    return value > XENOLITH_RATIO_MAX ? XENOLITH_RATIO_MAX : value;
}

static inline float xenolith_ratio_position(float value) {
    value = xenolith_ratio_bound(value);
    if (value < 0.010f) return (value - 0.001f) / 0.001f;
    if (value < 0.300f) return 9.0f + (value - 0.010f) / 0.005f;
    if (value < 2.000f) return 67.0f + (value - 0.300f) / 0.010f;
    if (value < 4.000f) return 237.0f + (value - 2.000f) / 0.050f;
    return 277.0f + (value - 4.000f) / 0.250f;
}

static inline float xenolith_ratio_value(float position) {
    if (!(position > 0.0f)) return XENOLITH_RATIO_MIN;
    if (position >= (float)XENOLITH_RATIO_STEPS) return XENOLITH_RATIO_MAX;
    if (position < 9.0f) return 0.001f + position * 0.001f;
    if (position < 67.0f) return 0.010f + (position - 9.0f) * 0.005f;
    if (position < 237.0f) return 0.300f + (position - 67.0f) * 0.010f;
    if (position < 277.0f) return 2.000f + (position - 237.0f) * 0.050f;
    return 4.000f + (position - 277.0f) * 0.250f;
}

static inline float xenolith_ratio_step(float value, int32_t delta) {
    float position;
    int32_t index;
    value = xenolith_ratio_bound(value);
    if (!delta) return value;
    /* Bound before arithmetic; INT32_MIN is never negated or added to a
     * large positive coordinate. A large turn reaches the requested end. */
    if (delta >= XENOLITH_RATIO_STEPS) return XENOLITH_RATIO_MAX;
    if (delta <= -XENOLITH_RATIO_STEPS) return XENOLITH_RATIO_MIN;
    position = xenolith_ratio_position(value);
    index = (int32_t)position;
    if (delta > 0) index = (int32_t)(position + 0.0002f);
    else if (position - (float)index > 0.0002f) ++index;
    return xenolith_ratio_value((float)(index + delta));
}
#endif
