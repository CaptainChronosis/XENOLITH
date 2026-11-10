/* SPDX-License-Identifier: MPL-2.0
 * XX29 foreground FM INDEX and panel-key pitch controls. No audio-rate work.
 */
#ifndef XX29_XENOLITH_CONTROLS_H
#define XX29_XENOLITH_CONTROLS_H
#include <stdint.h>
#include "fastmath.h"
#include "xenolith_core.h"

#define XENOLITH_INDEX_STEPS 294
#define XENOLITH_EXP_DEPTH_STEPS 45
#define XENOLITH_OCTAVE_MIN (-3)
#define XENOLITH_OCTAVE_MAX 3

static inline float xenolith_index_bound(float value)
{
    if (!(value >= 0.0f)) return 0.0f;
    return value > 64.0f ? 64.0f : value;
}

/* Continuous detent coordinate:0..8 by.1,8..15.95 by.15, then one.05
 * endpoint step to16. Above16 the existing.2/.4 travel is unchanged.
 * The short join avoids jumping past16 or changing persisted numeric units. */
static inline float xenolith_index_position(float value)
{
    value = xenolith_index_bound(value);
    if (value < 8.0f) return value * 10.0f;
    if (value < 15.95f) return 80.0f + (value - 8.0f) * (20.0f/3.0f);
    if (value < 16.0f) return 133.0f + (value - 15.95f) * 20.0f;
    if (value < 32.0f) return 134.0f + (value - 16.0f) * 5.0f;
    return 214.0f + (value - 32.0f) * 2.5f;
}

static inline float xenolith_index_value(float position)
{
    if (!(position > 0.0f)) return 0.0f;
    if (position >= (float)XENOLITH_INDEX_STEPS) return 64.0f;
    if (position < 80.0f) return position * 0.1f;
    if (position < 133.0f) return 8.0f + (position - 80.0f) * 0.15f;
    if (position < 134.0f) return 15.95f + (position - 133.0f) * 0.05f;
    if (position < 214.0f) return 16.0f + (position - 134.0f) * 0.2f;
    return 32.0f + (position - 214.0f) * 0.4f;
}

static inline float xenolith_index_step(float value, int32_t delta)
{
    float position;
    int32_t detent;
    value = xenolith_index_bound(value);
    if (!delta) return value;
    /* Bound before addition, including INT32_MIN. Loaded values between
     * detents move to the next detent in the requested direction. */
    if (delta >= XENOLITH_INDEX_STEPS) return 64.0f;
    if (delta <= -XENOLITH_INDEX_STEPS) return 0.0f;
    position = xenolith_index_position(value);
    detent = (int32_t)position;
    if (delta > 0) detent = (int32_t)(position + 0.0002f);
    else if (position - (float)detent > 0.0002f) ++detent;
    return xenolith_index_value((float)(detent + delta));
}

/* EXP keeps its numerical depth in semitones. This is only panel travel:
 * 12 one-semitone steps to12,12 two-semitone steps to36,21 four-semitone
 * steps to120. It does not remap a loaded value or change the FM algorithm. */
static inline float xenolith_exp_depth_bound(float value)
{
    if (!(value >= 0.0f)) return 0.0f;
    return value > XENOLITH_EXP_DEPTH_MAX ? XENOLITH_EXP_DEPTH_MAX : value;
}

static inline float xenolith_exp_depth_position(float value)
{
    value = xenolith_exp_depth_bound(value);
    if (value < 12.0f) return value;
    if (value < 36.0f) return 12.0f + (value - 12.0f) * 0.5f;
    return 24.0f + (value - 36.0f) * 0.25f;
}

static inline float xenolith_exp_depth_value(float position)
{
    if (!(position > 0.0f)) return 0.0f;
    if (position >= (float)XENOLITH_EXP_DEPTH_STEPS) return XENOLITH_EXP_DEPTH_MAX;
    if (position < 12.0f) return position;
    if (position < 24.0f) return 12.0f + (position - 12.0f) * 2.0f;
    return 36.0f + (position - 24.0f) * 4.0f;
}

static inline float xenolith_exp_depth_step(float value, int32_t delta)
{
    int32_t detent;
    value = xenolith_exp_depth_bound(value);
    if (!delta) return value;
    if (delta >= XENOLITH_EXP_DEPTH_STEPS) return XENOLITH_EXP_DEPTH_MAX;
    if (delta <= -XENOLITH_EXP_DEPTH_STEPS) return 0.0f;
    detent = (int32_t)xenolith_exp_depth_position(value);
    /* Coordinate addition can round an off-grid float up to a detent.
     * Compare actual values to recover floor/ceil without an epsilon zone. */
    if (xenolith_exp_depth_value((float)detent) > value) --detent;
    if (delta < 0 && xenolith_exp_depth_value((float)detent) < value) ++detent;
    return xenolith_exp_depth_value((float)(detent + delta));
}

static inline int32_t xenolith_octave_step(int32_t octave, int32_t delta)
{
    if (octave < XENOLITH_OCTAVE_MIN) octave = XENOLITH_OCTAVE_MIN;
    if (octave > XENOLITH_OCTAVE_MAX) octave = XENOLITH_OCTAVE_MAX;
    if (delta >= XENOLITH_OCTAVE_MAX - XENOLITH_OCTAVE_MIN) return XENOLITH_OCTAVE_MAX;
    if (delta <= XENOLITH_OCTAVE_MIN - XENOLITH_OCTAVE_MAX) return XENOLITH_OCTAVE_MIN;
    octave += delta;
    return octave < XENOLITH_OCTAVE_MIN ? XENOLITH_OCTAVE_MIN :
           octave > XENOLITH_OCTAVE_MAX ? XENOLITH_OCTAVE_MAX : octave;
}

static inline float xenolith_key_pitch(uint32_t key, int32_t octave)
{
    int32_t note;
    if (key > 26u) key = 26u;
    octave = xenolith_octave_step(octave, 0);
    note = 53 + (int32_t)key + 12 * octave;
    return fm_clampf(440.0f * fm_exp2f(((float)note - 69.0f) / 12.0f), 20.0f, 800.0f);
}

static inline float xenolith_octave_pitch(float pitch, int32_t delta)
{
    if (!(pitch >= 20.0f)) pitch = 20.0f;
    if (pitch > 800.0f) pitch = 800.0f;
    if (delta >= 6) return 800.0f;
    if (delta <= -6) return 20.0f;
    return fm_clampf(pitch * fm_exp2f((float)delta), 20.0f, 800.0f);
}
#endif
