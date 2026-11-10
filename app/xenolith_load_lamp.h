/* SPDX-License-Identifier: MPL-2.0
 * Foreground-only LOAD indication. No audio, timer IRQ or PWM ownership. */
#ifndef XX29_XENOLITH_LOAD_LAMP_H
#define XX29_XENOLITH_LOAD_LAMP_H
#include <stdint.h>

typedef struct {
    uint32_t observed_interventions,guard_at,phase_at,phase_ms,phase;
    uint32_t on,active,load_active,guard_seen;
} XenolithLoadLamp;

static inline uint32_t xenolith_load_lamp_duration(uint32_t on,float stress,uint32_t phase){
    uint32_t spread=(uint32_t)(stress*stress*90.0f);
    uint32_t word=(phase*1664525u+1013904223u)>>16;
    uint32_t jitter=spread?word%(spread+1u):0u;
    /* Near onset: a short pulse about every two seconds. At maximum: an
     * irregular100..205ms ON /200..290ms OFF sequence. Never high-rate PWM. */
    return on?100u+(uint32_t)(stress*60.0f)+jitter/2u:
        1800u-(uint32_t)(stress*1600.0f)+jitter;
}

static inline uint32_t xenolith_load_lamp_tick(XenolithLoadLamp *s,uint32_t now,
                                              float stress,uint32_t interventions,
                                              uint32_t recording){
    if(!(stress>=0.0f))stress=0.0f;
    if(stress>1.0f)stress=1.0f;
    if(interventions!=s->observed_interventions){
        s->observed_interventions=interventions;s->guard_at=now;s->guard_seen=1u;
    }
    /* Smoothed governor stress begins at ONSET; precursor colour alone is
     * excluded. A little hysteresis prevents threshold chatter. */
    if(stress>=.025f)s->load_active=1u;
    else if(stress<=.01f)s->load_active=0u;
    uint32_t guard=s->guard_seen&&(uint32_t)(now-s->guard_at)<600u;
    if(guard&&stress<.85f)stress=.85f;
    if(!s->load_active&&!guard){s->active=s->on=0u;}
    else if(!s->active){
        s->active=s->on=1u;s->phase=0u;s->phase_at=now;
        s->phase_ms=xenolith_load_lamp_duration(s->on,stress,s->phase);
    }else{
        uint32_t duration=xenolith_load_lamp_duration(s->on,stress,s->phase);
        if(duration<s->phase_ms)s->phase_ms=duration;
        if((uint32_t)(now-s->phase_at)>=s->phase_ms){
            /* At most one edge per observed UI frame: a late frame cannot
             * skip OFF. Repeated guard events never restart this phase. */
            s->on^=1u;s->phase++;s->phase_at=now;
            s->phase_ms=xenolith_load_lamp_duration(s->on,stress,s->phase);
        }
    }
    return recording?1u:s->on;
}
#endif
