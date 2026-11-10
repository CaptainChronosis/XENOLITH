/* SPDX-License-Identifier: MPL-2.0
 * XX29 bounded linear ADSR. Call only from the audio owner.
 */
#ifndef XX29_XENOLITH_ENVELOPE_H
#define XX29_XENOLITH_ENVELOPE_H
#include <stdint.h>
enum {
    XENOLITH_ENV_IDLE, XENOLITH_ENV_ATTACK, XENOLITH_ENV_DECAY,
    XENOLITH_ENV_SUSTAIN, XENOLITH_ENV_RELEASE
};
typedef struct {
    float sample_rate, attack_ms, decay_ms, sustain, release_ms;
    float level, target, step;
    uint32_t remaining, stage;
} XenolithEnvelope;

void xenolith_envelope_init(XenolithEnvelope *s, float sample_rate);
/* Times 0..10000 ms, sustain 0..1. Same settings are a no-op. An edited active
 * segment restarts its new duration from the present level without a jump.
 * Sustain edits slew from the current level over 5 ms. Configure at a block
 * boundary under audio ownership; no prepare/exp/table/allocation required.
 */
void xenolith_envelope_set(XenolithEnvelope *s, float attack_ms, float decay_ms,
                          float sustain, float release_ms);
/* Event calls, not a held-gate poll: every note_on retriggers from current
 * level. The app sends note_off only when its last held key is released.
 */
void xenolith_envelope_note_on(XenolithEnvelope *s);
void xenolith_envelope_note_off(XenolithEnvelope *s);
void xenolith_envelope_clear(XenolithEnvelope *s);
/* Returns current level, then advances one sample. Segment endpoints are
 * assigned exactly; release reaches exact zero, avoiding an asymptotic tail.
 */
float xenolith_envelope_sample(XenolithEnvelope *s);
#endif
