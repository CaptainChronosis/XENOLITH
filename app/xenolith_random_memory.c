/* SPDX-License-Identifier: GPL-3.0-or-later
 * Distribution and event-memory extension; derivation in RANDOM_RESEARCH.
 * This is neither a hardware-noise source nor a clone of a modular circuit. */
#include "xenolith_random_memory.h"

static int finite_value(float x) {
    union { float f; uint32_t u; } b; b.f=x;
    return (b.u&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);
}
static float unit(float x) {
    if (!finite_value(x)) return 0.5f;
    return x<0.0f?0.0f:x>1.0f?1.0f:x;
}
static float uniform(XenolithRandomMemory *s) {
    uint32_t x=s->rng;
    if (!x) x=UINT32_C(0x726e6432);
    x^=x<<13; x^=x>>17; x^=x<<5; s->rng=x;
    return (float)(x>>8)*(1.0f/16777216.0f);
}
void xenolith_random_memory_defaults(XenolithRandomMemoryConfig *c) {
    if (!c) return;
    c->renew=1.0f; c->law=XRM_CURRENT; c->length=0;
    c->reserved[0]=c->reserved[1]=0;
}
int xenolith_random_memory_validate(const XenolithRandomMemoryConfig *c) {
    return c && finite_value(c->renew) && c->renew>=0.0f && c->renew<=1.0f
        && c->law<XRM_LAW_COUNT && (c->length==0 || (c->length>=2 && c->length<=32))
        && !c->reserved[0] && !c->reserved[1];
}
void xenolith_random_memory_init(XenolithRandomMemory *s,uint32_t seed) {
    unsigned i;
    if (!s) return;
    xenolith_random_memory_defaults(&s->config);
    for(i=0;i<32;++i)s->value[i]=0.0f;
    s->rng=seed?seed:UINT32_C(0x726e6432);
    s->position=s->filled=s->reserved[0]=s->reserved[1]=0;
}
int xenolith_random_memory_apply(XenolithRandomMemory *s,
                                const XenolithRandomMemoryConfig *c) {
    if (!s || !xenolith_random_memory_validate(c)) return 0;
    if(s->config.length!=c->length)s->position=s->filled=0;
    s->config=*c;
    return 1;
}
static float edge(float u) {
    /* Inverse CDF of Beta(1/2,1/2): .5+.5*sin(pi*(u-.5)).
     * Degree11 on[-pi/2,pi/2]; omitted term <5.7e-8 before float rounding. */
    float x=3.14159265358979323846f*(u-0.5f), x2=x*x;
    float y=x*(1.0f+x2*(-1.0f/6.0f+x2*(1.0f/120.0f+x2*(-1.0f/5040.0f
        +x2*(1.0f/362880.0f-x2/39916800.0f)))));
    return unit(0.5f+0.5f*y);
}
float xenolith_random_memory_next(XenolithRandomMemory *s,float current,float bias) {
    float fresh, decision;
    unsigned n;
    if (!s) return unit(current);
    fresh=unit(current); n=s->config.length;
    if(s->config.law==XRM_CURRENT && !n)return fresh;
    if(s->config.law==XRM_BELL) {
        float a=uniform(s), b=uniform(s), c=uniform(s), d=uniform(s);
        fresh=0.25f*((a+b)+(c+d));
    } else if(s->config.law==XRM_EDGE)fresh=edge(uniform(s));
    else if(s->config.law==XRM_BINARY)fresh=uniform(s)<unit(0.5f+0.5f*bias)?1.0f:0.0f;
    if(!n)return fresh;
    decision=uniform(s);
    if(s->filled<n || decision<s->config.renew) {
        s->value[s->position]=fresh;
        if(s->filled<n)++s->filled;
    }
    fresh=s->value[s->position];
    if(++s->position>=n)s->position=0;
    return fresh;
}
