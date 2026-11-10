/* SPDX-License-Identifier: MPL-2.0
 * Independently implemented linear ADSR; no oscillator or audio-route changes.
 */
#include "xenolith_envelope.h"

static float bound(float value, float low, float high) {
    if (!(value>=low)) return low;
    return value>high ? high : value;
}

static void segment(XenolithEnvelope *s, uint32_t stage, float target, float ms) {
    s->stage=stage; s->target=target;
    s->remaining=(uint32_t)(ms*(s->sample_rate*0.001f)+0.5f);
    if (s->remaining) s->step=(target-s->level)/(float)s->remaining;
    else { s->level=target; s->step=0; }
}

static void decay(XenolithEnvelope *s) {
    segment(s,XENOLITH_ENV_DECAY,s->sustain,s->decay_ms);
    if (!s->remaining) s->stage=XENOLITH_ENV_SUSTAIN;
}

void xenolith_envelope_clear(XenolithEnvelope *s) {
    s->level=s->target=s->step=0;
    s->remaining=0; s->stage=XENOLITH_ENV_IDLE;
}

void xenolith_envelope_init(XenolithEnvelope *s, float sample_rate) {
    s->sample_rate=bound(sample_rate,8000.0f,192000.0f);
    s->attack_ms=10.0f; s->decay_ms=250.0f;
    s->sustain=0.8f; s->release_ms=400.0f;
    xenolith_envelope_clear(s);
}

void xenolith_envelope_set(XenolithEnvelope *s, float attack_ms, float decay_ms,
                          float sustain, float release_ms) {
    float old_a=s->attack_ms, old_d=s->decay_ms;
    float old_s=s->sustain, old_r=s->release_ms;
    s->attack_ms=bound(attack_ms,0.0f,10000.0f);
    s->decay_ms=bound(decay_ms,0.0f,10000.0f);
    s->sustain=bound(sustain,0.0f,1.0f);
    s->release_ms=bound(release_ms,0.0f,10000.0f);
    if (s->stage==XENOLITH_ENV_ATTACK && old_a!=s->attack_ms) {
        segment(s,XENOLITH_ENV_ATTACK,1.0f,s->attack_ms);
        if (!s->remaining) decay(s);
    } else if (s->stage==XENOLITH_ENV_DECAY
               && (old_d!=s->decay_ms || old_s!=s->sustain)) decay(s);
    else if (s->stage==XENOLITH_ENV_SUSTAIN && old_s!=s->sustain)
        segment(s,XENOLITH_ENV_SUSTAIN,s->sustain,5.0f);
    else if (s->stage==XENOLITH_ENV_RELEASE && old_r!=s->release_ms) {
        segment(s,XENOLITH_ENV_RELEASE,0.0f,s->release_ms);
        if (!s->remaining) s->stage=XENOLITH_ENV_IDLE;
    }
}

void xenolith_envelope_note_on(XenolithEnvelope *s) {
    segment(s,XENOLITH_ENV_ATTACK,1.0f,s->attack_ms);
    if (!s->remaining) decay(s);
}

void xenolith_envelope_note_off(XenolithEnvelope *s) {
    if (s->stage==XENOLITH_ENV_IDLE) return;
    segment(s,XENOLITH_ENV_RELEASE,0.0f,s->release_ms);
    if (!s->remaining) s->stage=XENOLITH_ENV_IDLE;
}

float xenolith_envelope_sample(XenolithEnvelope *s) {
    float result=s->level;
    if (s->remaining) {
        --s->remaining;
        /* Reconstruct the linear segment instead of accumulating millions of
         * tiny increments; long attacks cannot drift above unity. */
        s->level=bound(s->target-s->step*(float)s->remaining,0.0f,1.0f);
        if (!s->remaining) {
            s->level=s->target;
            if (s->stage==XENOLITH_ENV_ATTACK) decay(s);
            else if (s->stage==XENOLITH_ENV_DECAY) s->stage=XENOLITH_ENV_SUSTAIN;
            else if (s->stage==XENOLITH_ENV_RELEASE) s->stage=XENOLITH_ENV_IDLE;
        }
    }
    return result;
}
