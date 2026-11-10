/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded policy only: no audio state, allocation, hardware I/O or phase edits.
 * Source audit: docs/DEADLINE_GOVERNOR_FEASIBILITY_2026-11-02.md.
 */
#include "xenolith_governor.h"
#include "fastmath.h"

static int sane(float x) {
    fm_bits_t b;b.f=x;
    return (b.u&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);
}
static int within(float x,float lo,float hi) { return sane(x)&&x>=lo&&x<=hi; }
static float clip(float x,float lo,float hi) { return x<lo?lo:x>hi?hi:x; }
static float ramp(float x,float lo,float hi) {
    float p=clip((x-lo)/(hi-lo),0.0f,1.0f);
    return p*p*(3.0f-2.0f*p);
}

void xenolith_governor_defaults(XenolithGovernorConfig *c) {
    if(c)*c=(XenolithGovernorConfig){.60f,.40f,250.0f,1.0f,1.0f,-.15f,1.0f,0.0f,0.0f,1.0f,0.0f,.375f,1.0f};
}
int xenolith_governor_config_valid(const XenolithGovernorConfig *c) {
    return c&&within(c->onset,0.0f,1.0f)&&within(c->span,.05f,.50f)
        &&within(c->recovery_ms,20.0f,2000.0f)
        &&within(c->filter_shape,0.0f,2.0f)&&within(c->reverb_shape,0.0f,2.0f)
        &&within(c->vca_bias,-1.0f,1.0f)&&within(c->table_depth,0.0f,1.0f)
        &&within(c->table_loss,0.0f,5.0f)&&within(c->dither,0.0f,1.0f)
        &&within(c->cell_floor,.375f,1.0f)&&within(c->link_loss,0.0f,1.0f)
        &&within(c->clock_floor,.25f,1.0f)&&within(c->erosion_curve,.25f,4.0f);
}
void xenolith_governor_reset(XenolithGovernor *s) {
    if(s){s->stress=0.0f;s->load=0.0f;s->peak_load=0.0f;s->colour=0.0f;s->blocks=0;}
}
void xenolith_governor_init(XenolithGovernor *s) {
    if(s){xenolith_governor_defaults(&s->config);xenolith_governor_reset(s);}
}
int xenolith_governor_apply(XenolithGovernor *s,const XenolithGovernorConfig *c) {
    if(!s||!xenolith_governor_config_valid(c))return 0;
    s->config=*c;
    return 1;
}
/* Rational progress curve: endpoints stay exact,1 preserves the original
 * schedule. Curve above1 postpones erosion; below1 brings it forward. */
static float progress(float stress,float lo,float hi,float curve) {
    float p=ramp(stress,lo,hi);
    return curve==1.0f?p:p/(curve+(1.0f-curve)*p);
}
XenolithGovernorFrame xenolith_governor_frame(const XenolithGovernor *s) {
    XenolithGovernorFrame f={0.0f,0.0f,1.0f,1.0f,0.0f,0.0f,1.0f,1.0f};
    float x,curve,erosion,cells;
    if(!s||!sane(s->stress))return f;
    x=clip(s->stress,0.0f,1.0f);
    /* A small precursor colours only the downstream stages. It never reduces
     * the core clock, table precision, cell aperture or modulation links. */
    f.effects=sane(s->colour)?clip(s->colour,0.0f,.015f):0.0f;
    f.effects+=(1.0f-f.effects)*x;
    if(x==0.0f)return f;
    curve=s->config.erosion_curve;
    f.core=progress(x,.65f,.98f,curve);
    f.interpolation=1.0f-s->config.table_depth*progress(x,.80f,.98f,curve);
    /* Early/mid stress colours downstream only. Fractional core scheduling
     * and interpolation loss are audible digital processes, reserved for the
     * late range. Independent deadline protection remains unconditional. */
    f.duty=1.0f-(1.0f-s->config.clock_floor)*progress(x,.65f,.95f,curve);
    erosion=progress(x,.60f,.92f,curve);
    f.table_loss=s->config.table_loss*erosion;
    f.dither=s->config.dither*erosion;
    cells=progress(x,.35f,.85f,curve);
    f.cell_ratio=1.0f-(1.0f-s->config.cell_floor)*cells*cells;
    f.link_gain=1.0f-s->config.link_loss*progress(x,.55f,.90f,curve);
    return f;
}
XenolithGovernorFrame xenolith_governor_update_headroom(XenolithGovernor *s,
    uint32_t elapsed,uint32_t budget,uint32_t frames,float rate,uint32_t headroom) {
    float target,normalized,exponent,alpha,colour_target,colour_alpha;
    if(!s||!budget||!frames||!within(rate,1.0f,384000.0f))return xenolith_governor_frame(s);
    s->load=clip((float)elapsed/(float)budget,0.0f,4.0f);
    if(s->load>s->peak_load)s->peak_load=s->load;
    ++s->blocks;
    if(!sane(s->stress))s->stress=0.0f;
    if(!sane(s->colour))s->colour=0.0f;
    target=clip((s->load-s->config.onset)/s->config.span,0.0f,1.0f);
    /* Demand is work; admission also needs unused deadline headroom. Account
     * for that reserve continuously in the late target, not only when HOLD
     * fires (which would remove pressure as soon as one block clears).
     * Raw load/peak and precursor stay unchanged; early/mid targets are exact. */
    if(headroom&&target>.65f)
        target=clip(target+ramp(target,.65f,.85f)*
            ((float)headroom/(float)budget)/s->config.span,0.0f,1.0f);
    normalized=((float)frames/256.0f)*(44100.0f/rate);
    /* log2(.90), log2(.94): alpha=.10/.06 at256frames/44.1kHz.
     * Slower release supplies temporal hysteresis without a discontinuous mode
     * switch. Recovery controls this release only; attack stays quick. */
    exponent=target>s->stress?-.1520030934f:
        -.0892673381f*(100.0f/s->config.recovery_ms);
    alpha=clip(1.0f-fm_exp2f(clip(exponent*normalized,-126.0f,0.0f)),0.0f,1.0f);
    s->stress=clip(s->stress+alpha*(target-s->stress),0.0f,1.0f);
    colour_target=s->config.onset>0.0f?
        .015f*ramp(s->load,clip(s->config.onset-.10f,0.0f,1.0f),s->config.onset):0.0f;
    colour_alpha=1.0f-fm_exp2f(clip((colour_target>s->colour?-.1520030934f:
        -.0892673381f*(100.0f/s->config.recovery_ms))*normalized,-126.0f,0.0f));
    s->colour=clip(s->colour+colour_alpha*(colour_target-s->colour),0.0f,.015f);
    if(colour_target==0.0f&&s->colour<.000001f)s->colour=0.0f;
    if(target==0.0f&&s->stress<.000001f)s->stress=0.0f;
    if(target==1.0f&&s->stress>.9999995f)s->stress=1.0f;
    return xenolith_governor_frame(s);
}
XenolithGovernorFrame xenolith_governor_update(XenolithGovernor *s,
    uint32_t elapsed,uint32_t budget,uint32_t frames,float rate) {
    return xenolith_governor_update_headroom(s,elapsed,budget,frames,rate,0u);
}

uint32_t xenolith_governor_elapsed(uint32_t start,uint32_t now) { return now-start; }
uint32_t xenolith_governor_remaining(uint32_t start,uint32_t now,uint32_t budget) {
    uint32_t elapsed=now-start;return elapsed<budget?budget-elapsed:0u;
}
int xenolith_governor_admit_core(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t frames,uint32_t post,uint32_t core,uint32_t fixed) {
    uint32_t remaining=xenolith_governor_remaining(start,now,budget);
    uint64_t need=(uint64_t)frames*post+(uint64_t)core+fixed;
    return remaining!=0u&&need<(uint64_t)remaining;
}
int xenolith_governor_admit_post(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t post,uint32_t fixed) {
    uint32_t remaining=xenolith_governor_remaining(start,now,budget);
    return remaining!=0u&&(uint64_t)post+fixed<(uint64_t)remaining;
}

static uint32_t saturated_ticks(uint64_t ticks) {
    return ticks>UINT32_MAX?UINT32_MAX:(uint32_t)ticks;
}
uint32_t xenolith_governor_demand(uint32_t elapsed,uint32_t denied,
    uint32_t active,uint32_t held) {
    uint64_t unserved=(uint64_t)denied*active;
    if(unserved>held)unserved-=held;else unserved=0u;
    return saturated_ticks((uint64_t)elapsed+unserved);
}
void xenolith_governor_cost_init(XenolithGovernorCost *c,uint32_t seed) {
    if(c){c->post_chunk_peak=saturated_ticks((uint64_t)seed*8u);
        c->post_quantum_peak=saturated_ticks((uint64_t)seed*32u);
        c->post_quantum_sum=0u;c->post_quantum_chunks=0u;}
}
void xenolith_governor_cost_observe(XenolithGovernorCost *c,uint32_t ticks) {
    if(!c)return;
    if(ticks>c->post_chunk_peak)c->post_chunk_peak=ticks;
    c->post_quantum_sum=saturated_ticks((uint64_t)c->post_quantum_sum+ticks);
    if(++c->post_quantum_chunks==4u){
        if(c->post_quantum_sum>c->post_quantum_peak)c->post_quantum_peak=c->post_quantum_sum;
        c->post_quantum_sum=0u;c->post_quantum_chunks=0u;
    }
}
static uint32_t decay_ticks(uint32_t ticks,uint32_t floor) {
    uint32_t step=ticks/512u+1u;
    return ticks<=floor||ticks-floor<=step?floor:ticks-step;
}
void xenolith_governor_cost_decay(XenolithGovernorCost *c,uint32_t floor) {
    if(!c)return;
    c->post_chunk_peak=decay_ticks(c->post_chunk_peak,saturated_ticks((uint64_t)floor*8u));
    c->post_quantum_peak=decay_ticks(c->post_quantum_peak,saturated_ticks((uint64_t)floor*32u));
}
uint32_t xenolith_governor_post_reserve(const XenolithGovernorCost *c,uint32_t frames) {
    uint64_t quanta=((uint64_t)frames+31u)/32u;
    if(!c)return UINT32_MAX;
    return saturated_ticks(quanta*c->post_quantum_peak);
}
uint32_t xenolith_governor_feedback_headroom(const XenolithGovernorCost *c,uint32_t fixed) {
    uint32_t tail;
    if(!c)return UINT32_MAX;
    /* Last8-frame admission reserves a whole32-frame post quantum. Its
     * excess over the independently measured worst8-frame chunk is unused
     * headroom, alongside the fixed epilogue/commit allowance. Guards retain
     * both original estimates; this only aligns the controller's target. */
    tail=c->post_quantum_peak>c->post_chunk_peak?c->post_quantum_peak-c->post_chunk_peak:0u;
    return saturated_ticks((uint64_t)fixed+tail);
}
int xenolith_governor_admit_core_cost(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t frames,const XenolithGovernorCost *c,uint32_t quality,uint32_t fixed) {
    uint32_t remaining=xenolith_governor_remaining(start,now,budget);
    uint64_t need=(uint64_t)xenolith_governor_post_reserve(c,frames)+quality+fixed;
    return c&&remaining!=0u&&need<(uint64_t)remaining;
}
