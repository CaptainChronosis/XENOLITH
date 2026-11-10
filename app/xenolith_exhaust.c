/* SPDX-License-Identifier: MPL-2.0
 * XX29 compact exhaust, 2026. Independent implementation of standard
 * Schroeder allpasses and an orthogonal, normalized Hadamard FDN.
 * References and measured limits are recorded with the host test receipt.
 * fastmath.h retains Charles Vestal's MPL-2.0 notice.
 */
#include "xenolith_exhaust.h"
#include "fastmath.h"

static const uint32_t fdn_length[8] = {1499,1601,1867,2081,2333,2591,2897,3253};
static const uint32_t fdn_offset[8] = {0,1499,3100,4967,7048,9381,11972,14869};
static const uint32_t ap_length[4] = {113,337,521,887};
static const uint32_t ap_offset[4] = {0,113,450,971};
static const uint16_t cloud_delay[8] = {19,37,61,97,149,211,307,431};

uint32_t xenolith_exhaust_structure_family(uint32_t mode) {
    if(mode<2u)return 0u;
    if(mode<6u)return mode-1u;
    return mode<10u?mode-5u:0u;
}
uint32_t xenolith_exhaust_structure_reconstruction(uint32_t mode) {
    return mode>=1u && mode<=5u?1u:0u;
}
uint32_t xenolith_exhaust_structure_mode(uint32_t family,uint32_t reconstruction) {
    if(family>=5u)family=0u;
    return reconstruction==1u?family+1u:(family?family+5u:0u);
}

/* Freestanding target has no string.h. An ordinary byte loop also permits
 * compiler lowering to the BSP's already linked freestanding memset. */
static void zero_bytes(void *memory, uint32_t length) {
    uint8_t *p = (uint8_t *)memory;
    while (length--) *p++ = 0;
}

static inline __attribute__((always_inline)) float bound(float x, float low, float high) {
    if (!(x >= low)) return low;
    return x > high ? high : x;
}

float xenolith_exhaust_echo_limit(float wet_rate) {
    wet_rate=bound(wet_rate,250.0f,44100.0f);
    return fm_minf(XENOLITH_EXHAUST_ECHO_MAX,(float)(XENOLITH_EXHAUST_COMB-2u)/wet_rate);
}

float xenolith_exhaust_echo_step(float seconds,int32_t delta,float wet_rate) {
    float limit=xenolith_exhaust_echo_limit(wet_rate);
    if(!delta)return seconds;
    seconds=bound(seconds,XENOLITH_EXHAUST_ECHO_MIN,limit);
    if(delta>2048)return limit;
    if(delta < -2048)return XENOLITH_EXHAUST_ECHO_MIN;
    return bound(seconds*fm_exp2f((float)delta/96.0f),XENOLITH_EXHAUST_ECHO_MIN,limit);
}

static inline __attribute__((always_inline)) float random_bipolar(XenolithExhaust *s) {
    uint32_t x = s->random_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    s->random_state = x;
    return (float)(x >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

void xenolith_exhaust_prepare(XenolithExhaustConfig *c, float sample_rate,
                             float time, float absorb, float decay, float mix) {
    xenolith_exhaust_prepare_clock(c, sample_rate, 1u, time, absorb, decay, mix);
}

void xenolith_exhaust_prepare_clock(XenolithExhaustConfig *c, float host_rate,
                                   uint32_t divider, float time, float absorb,
                                   float decay, float mix) {
    unsigned i;
    float cutoff, damping_cutoff, sample_rate, remaining;
    XenolithDispersionConfig dispersion;
    xenolith_dispersion_defaults(&dispersion);
    c->host_rate = bound(host_rate, 8000.0f, 44100.0f);
    c->divider = divider < 1u ? 1u : (divider > 32u ? 32u : divider);
    c->reconstruction = XENOLITH_EXHAUST_ZOH;
    c->structure = XENOLITH_EXHAUST_TANK_ZOH;
    c->phase_increment = 0u;
    c->diffusion = 0.65f; c->width = 1.0f;
    c->requested_damp_hz=800.0f;c->damp_model=0u;c->input_pole=0.0f;
    sample_rate = c->sample_rate = c->host_rate / (float)c->divider;
    xenolith_dispersion_prepare(&c->dispersion,&dispersion,sample_rate);
    c->fade_step = 1.0f / (c->host_rate * 0.005f);
    /* Preserve the original coefficient bit-for-bit at divider1. At lower
     * clocks, preserve its wall-time constant without exp work in audio. */
    remaining = 1.0f;
    for (i = 0; i < c->divider; ++i) remaining *= 1.0f - 0.0002f;
    c->delay_slew = c->divider == 1u ? 0.0002f : 1.0f - remaining;
    c->time = bound(time, XENOLITH_EXHAUST_ECHO_MIN, xenolith_exhaust_echo_limit(sample_rate));
    c->absorb = bound(absorb, 0.0f, 1.0f);
    c->decay = bound(decay, 0.3f, 12.0f);
    c->mix = bound(mix, 0.0f, 1.0f);
    cutoff = 200.0f * fm_exp2f(5.0f * (1.0f - c->absorb));
    cutoff = fm_minf(cutoff, sample_rate * 0.4f);
    c->damp_hz = cutoff;
    damping_cutoff = fm_minf(1500.0f + 1.5f * cutoff, sample_rate * 0.4f);
    c->comb_pole = fm_expf(-FM_TWO_PI * cutoff / sample_rate);
    c->damping_pole = fm_expf(-FM_TWO_PI * damping_cutoff / sample_rate);
    c->dc_pole = fm_expf(-FM_TWO_PI * 15.0f / sample_rate);
    c->comb_gain = fm_expf(-6.907755279f * c->time / (0.6f * c->decay));
    for (i = 0; i < 8; ++i)
        c->fdn_gain[i] = fm_expf(-6.907755279f * (float)fdn_length[i]
                               / (sample_rate * c->decay));
}

void xenolith_exhaust_prepare_rate(XenolithExhaustConfig *c, float host_rate,
                                  float wet_rate, uint32_t reconstruction,
                                  float time, float absorb, float decay, float mix) {
    uint32_t divider;
    unsigned i;
    float cutoff, damping_cutoff;
    host_rate = bound(host_rate, 8000.0f, 44100.0f);
    wet_rate = bound(wet_rate, host_rate / 32.0f, host_rate);
    /* Equality is intentional: a saved exact legacy rate retains its original
     * scheduler and rounded coefficients, not a nearby fractional clock. */
    for (divider = 1u; divider <= 32u; ++divider) {
        if (wet_rate == host_rate / (float)divider) {
            xenolith_exhaust_prepare_clock(c, host_rate, divider, time, absorb, decay, mix);
            c->reconstruction = reconstruction == XENOLITH_EXHAUST_LINEAR ? reconstruction : 0u;
            c->structure = c->reconstruction;
            return;
        }
    }
    xenolith_exhaust_prepare_clock(c, host_rate, 1u, time, absorb, decay, mix);
    c->divider = 0u;
    c->sample_rate = wet_rate;
    { XenolithDispersionConfig d; xenolith_dispersion_defaults(&d);
      xenolith_dispersion_prepare(&c->dispersion,&d,wet_rate); }
    c->time = bound(time, XENOLITH_EXHAUST_ECHO_MIN, xenolith_exhaust_echo_limit(wet_rate));
    c->comb_gain = fm_expf(-6.907755279f * c->time / (0.6f * c->decay));
    c->reconstruction = reconstruction == XENOLITH_EXHAUST_LINEAR ? reconstruction : 0u;
    c->structure = c->reconstruction;
    /* Float conversion near unity can round to2^32. Bound to the greatest
     * representable float below it before the uint32 conversion. */
    c->phase_increment = (uint32_t)fm_minf((wet_rate / host_rate) * 4294967296.0f, 4294967040.0f);
    c->delay_slew = 1.0f - fm_expf(fm_logf(1.0f - 0.0002f) * (host_rate / wet_rate));
    cutoff = fm_minf(200.0f * fm_exp2f(5.0f * (1.0f - c->absorb)), wet_rate * 0.4f);
    c->damp_hz = cutoff;
    damping_cutoff = fm_minf(1500.0f + 1.5f * cutoff, wet_rate * 0.4f);
    c->comb_pole = fm_expf(-FM_TWO_PI * cutoff / wet_rate);
    c->damping_pole = fm_expf(-FM_TWO_PI * damping_cutoff / wet_rate);
    c->dc_pole = fm_expf(-FM_TWO_PI * 15.0f / wet_rate);
    for (i = 0; i < 8; ++i)
        c->fdn_gain[i] = fm_expf(-6.907755279f * (float)fdn_length[i]
                               / (wet_rate * c->decay));
}

void xenolith_exhaust_prepare_tank_rate(XenolithExhaustConfig *c,float host_rate,
    float wet_rate,uint32_t reconstruction,float time,float absorb,float decay,float mix,
    float diffusion,float width) {
    xenolith_exhaust_prepare_rate(c,host_rate,wet_rate,reconstruction,time,absorb,decay,mix);
    c->diffusion=bound(diffusion,0.0f,0.85f);c->width=bound(width,0.0f,1.0f);
}

void xenolith_exhaust_prepare_structure_rate(XenolithExhaustConfig *c,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width) {
    if(structure>=XENOLITH_EXHAUST_STRUCTURE_COUNT)structure=0u;
    xenolith_exhaust_prepare_tank_rate(c,host_rate,wet_rate,xenolith_exhaust_structure_reconstruction(structure),
        time,absorb,decay,mix,diffusion,width);
    c->structure=structure;
}

void xenolith_exhaust_prepare_fx_rate(XenolithExhaustConfig *c,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width,float damp_hz,uint32_t damp_model) {
    xenolith_exhaust_prepare_structure_rate(c,host_rate,wet_rate,structure,
        time,absorb,decay,mix,diffusion,width);
    c->requested_damp_hz=bound(damp_hz,20.0f,16000.0f);
    if(damp_model==1u){
        float sine,q,p;
        c->damp_model=1u;
        c->damp_hz=fm_minf(c->requested_damp_hz,0.45f*c->sample_rate);
        /* Same exact-corner equation as b-sqrt(b*b-1), evaluated through
         * 1-cos(w)=2*sin(w/2)^2 to avoid cancellation at20Hz/44100Hz. */
        sine=fm_sinf(FM_PI*c->damp_hz/c->sample_rate);q=2.0f*sine*sine;
        p=bound(1.0f+q-fm_sqrtf(q*(q+2.0f)),0.0f,0.999999f);
        c->comb_pole=c->damping_pole=c->input_pole=p;
    }
}

void xenolith_exhaust_prepare_extended_rate(XenolithExhaustConfig *c,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width,float damp_hz,uint32_t damp_model,
    const XenolithDispersionConfig *dispersion) {
    xenolith_exhaust_prepare_fx_rate(c,host_rate,wet_rate,structure,time,absorb,decay,mix,
        diffusion,width,damp_hz,damp_model);
    (void)xenolith_dispersion_prepare(&c->dispersion,dispersion,c->sample_rate);
}

static void tank_target(XenolithExhaust *s,const XenolithExhaustConfig *c) {
    float diffusion=bound(c->diffusion,0.0f,0.85f),width=bound(c->width,0.0f,1.0f);
    uint32_t family=xenolith_exhaust_structure_family(c->structure);
    float cloud=family==1u?1.0f:family==2u?2.0f:0.0f;
    float ring=family==3u?1.0f:0.0f,pairs=family==4u?1.0f:0.0f,input=(float)c->damp_model;
    s->structure=c->structure;s->damp_model=c->damp_model;s->requested_damp_hz=c->requested_damp_hz;
    if(diffusion==s->diffusion_target && width==s->width_target && cloud==s->cloud_target
       && ring==s->ring_target && pairs==s->pairs_target && input==s->input_blend_target)return;
    s->diffusion_target=diffusion;s->width_target=width;s->cloud_target=cloud;
    s->ring_target=ring;s->pairs_target=pairs;s->input_blend_target=input;
    if(diffusion==s->diffusion && width==s->width && cloud==s->cloud && ring==s->ring_weight
       && pairs==s->pairs_weight && input==s->input_blend){s->tank_remaining=0u;return;}
    s->tank_remaining=(uint32_t)(s->host_rate*0.05f+0.5f);
    if(!s->tank_remaining)s->tank_remaining=1u;
    s->diffusion_step=(diffusion-s->diffusion)/(float)s->tank_remaining;
    s->width_step=(width-s->width)/(float)s->tank_remaining;
    s->cloud_step=(cloud-s->cloud)/(float)s->tank_remaining;
    s->ring_step=(ring-s->ring_weight)/(float)s->tank_remaining;
    s->pairs_step=(pairs-s->pairs_weight)/(float)s->tank_remaining;
    s->input_blend_step=(input-s->input_blend)/(float)s->tank_remaining;
}

static void apply_now(XenolithExhaust *s, const XenolithExhaustConfig *c) {
    unsigned i;
    if(s->sag_remaining && s->host_rate!=c->host_rate){
        s->sag_remaining=(uint32_t)((float)s->sag_remaining*c->host_rate/s->host_rate+0.5f);
        if(!s->sag_remaining)s->sag_remaining=1u;
        s->sag_diffuser_step=(s->sag_diffuser_target-s->sag_diffuser)/(float)s->sag_remaining;
        s->sag_interpolation_step=(s->sag_interpolation_target-s->sag_interpolation)/(float)s->sag_remaining;
        s->sag_damping_step=(s->sag_damping_target-s->sag_damping)/(float)s->sag_remaining;
    }
    if(s->tank_remaining && s->host_rate!=c->host_rate){
        /* A structural host change may round once; wet-clock gestures never
         * enter here with a different host. Keep the scalar ramp's wall time. */
        s->tank_remaining=(uint32_t)((float)s->tank_remaining*c->host_rate/s->host_rate+0.5f);
        if(!s->tank_remaining)s->tank_remaining=1u;
        s->diffusion_step=(s->diffusion_target-s->diffusion)/(float)s->tank_remaining;
        s->width_step=(s->width_target-s->width)/(float)s->tank_remaining;
        s->cloud_step=(s->cloud_target-s->cloud)/(float)s->tank_remaining;
        s->ring_step=(s->ring_target-s->ring_weight)/(float)s->tank_remaining;
        s->pairs_step=(s->pairs_target-s->pairs_weight)/(float)s->tank_remaining;
        s->input_blend_step=(s->input_blend_target-s->input_blend)/(float)s->tank_remaining;
    }
    if (s->sample_rate != c->sample_rate) {
        float ratio = c->sample_rate / s->sample_rate;
        s->comb_delay = bound(s->comb_delay * ratio, 1.0f, (float)(XENOLITH_EXHAUST_COMB-2u));
        if (s->modulation_count) {
            s->modulation_count = (uint32_t)((float)s->modulation_count * ratio);
            if (!s->modulation_count) s->modulation_count = 1;
            s->modulation_step = (s->modulation_target - s->modulation)
                               / (float)s->modulation_count;
        }
        s->coefficient_count = 0;
    }
    s->sample_rate = c->sample_rate; s->host_rate = c->host_rate;
    s->divider = c->divider; s->delay_slew = c->delay_slew;
    s->reconstruction = c->reconstruction; s->phase_increment = c->phase_increment;
    s->damp_hz = c->damp_hz; s->fade_step = c->fade_step;
    s->time = c->time; s->absorb = c->absorb;
    s->decay = c->decay; s->mix = c->mix;
    s->comb_gain = c->comb_gain; s->comb_pole = c->comb_pole;
    s->damping_pole = c->damping_pole; s->dc_pole = c->dc_pole;
    s->input_pole=c->input_pole;
    for (i = 0; i < 8; ++i) s->fdn_gain[i] = c->fdn_gain[i];
    s->target_delay = bound(s->time * (1.0f + 0.2f * s->modulation)
                           * s->sample_rate, 1.0f, (float)(XENOLITH_EXHAUST_COMB-2u));
    tank_target(s,c);
    (void)xenolith_dispersion_apply(&s->dispersion,&c->dispersion);
}

/* Structural changes may round remaining time once. Live gestures keep it in
 * seconds, avoiding repeated floor/ceil bias while the wet rate is moving. */
static void leave_live_clock(XenolithExhaust *s) {
    if (s->live_clock) {
        float remaining = fm_maxf(0.0f, s->wander_seconds - s->wander_elapsed);
        s->modulation_count = (uint32_t)(remaining * s->sample_rate / 64.0f);
        if (!s->modulation_count) s->modulation_count = 1u;
        s->modulation_step = (s->modulation_target - s->modulation) / (float)s->modulation_count;
        s->live_clock = 0u; s->sweep_remaining = 0u;
    }
}

static uint32_t live_phase_increment(float rate, float host) {
    /* Zero is the exact one-tick/output sentinel, not a stopped clock. */
    return rate == host ? 0u
        : (uint32_t)fm_minf((rate / host) * 4294967296.0f, 4294967040.0f);
}

static int retime_config_valid(const XenolithExhaustConfig *c) {
    unsigned i;
    if (!(c->host_rate>=8000.0f && c->host_rate<=44100.0f)
        || c->structure>=XENOLITH_EXHAUST_STRUCTURE_COUNT
        || c->reconstruction!=xenolith_exhaust_structure_reconstruction(c->structure)
        || c->damp_model>1u
        || !(c->requested_damp_hz>=20.0f && c->requested_damp_hz<=16000.0f)
        || !(c->input_pole>=0.0f && c->input_pole<1.0f)
        || !(c->sample_rate >= c->host_rate / 32.0f && c->sample_rate <= c->host_rate)
        || !(c->time >= XENOLITH_EXHAUST_ECHO_MIN && c->time <= xenolith_exhaust_echo_limit(c->sample_rate))
        || !(c->absorb >= 0.0f && c->absorb <= 1.0f)
        || !(c->decay >= 0.3f && c->decay <= 12.0f)
        || !(c->mix >= 0.0f && c->mix <= 1.0f)
        || !(c->diffusion >= 0.0f && c->diffusion <= 0.85f)
        || !(c->width >= 0.0f && c->width <= 1.0f)
        || !(c->comb_gain >= 0.0f && c->comb_gain < 1.0f)
        || !(c->comb_pole >= 0.0f && c->comb_pole < 1.0f)
        || !(c->damping_pole >= 0.0f && c->damping_pole < 1.0f)
        || !(c->dc_pole >= 0.0f && c->dc_pole < 1.0f)
        || !(c->delay_slew > 0.0f && c->delay_slew <= 1.0f)) return 0;
    for (i = 0; i < 8u; ++i)
        if (!(c->fdn_gain[i] >= 0.0f && c->fdn_gain[i] < 1.0f)) return 0;
    return 1;
}

int xenolith_exhaust_retime(XenolithExhaust *s, const XenolithExhaustConfig *c) {
    if (!s || !c || s->transition || c->host_rate != s->host_rate
        || c->reconstruction != s->reconstruction || !retime_config_valid(c)) return 0;
    if (!s->live_clock && c->sample_rate == s->sample_rate
        && ((!c->damp_model && !s->damp_model)
            || (c->damping_pole==s->damping_pole && c->input_pole==s->input_pole))) {
        apply_now(s, c);
        return 1;
    }
    if (!s->live_clock) {
        if (s->divider) {
            float phase = (float)(s->divider - s->clock_count - 1u) / (float)s->divider;
            s->clock_phase = s->wet_ticks
                ? (uint32_t)fm_minf(phase * 4294967296.0f, 4294967040.0f) : 0u;
            /* count0 before any processing means the initial immediate tick
             * is still due. Otherwise phase conversion schedules the next. */
            s->clock_count = s->wet_ticks ? 1u : 0u;
        }
        s->phase_increment = live_phase_increment(s->sample_rate, s->host_rate);
        s->divider = 0u;
        s->wander_seconds = (float)s->modulation_count * 64.0f / s->sample_rate;
        s->wander_elapsed = s->coefficient_count
            ? (float)(64u - s->coefficient_count) / s->sample_rate : 0.0f;
        s->live_clock = 1u;
    }
    s->sweep_target = *c;
    (void)xenolith_dispersion_apply(&s->dispersion,&c->dispersion);
    tank_target(s,c);
    s->sweep_remaining = (uint32_t)(s->host_rate * 0.05f + 0.5f);
    return 1;
}

static float ramp_value(float value, float target, float fraction) {
    return fraction == 1.0f ? target : value + fraction * (target - value);
}

void xenolith_exhaust_begin_block(XenolithExhaust *s, uint32_t frames) {
    const XenolithExhaustConfig *c;
    float fraction, rate, ratio;
    unsigned i;
    if (!s || !s->live_clock || !s->sweep_remaining || s->transition || !frames) return;
    c = &s->sweep_target;
    if (frames >= s->sweep_remaining) { fraction = 1.0f; s->sweep_remaining = 0u; }
    else { fraction = (float)frames / (float)s->sweep_remaining; s->sweep_remaining -= frames; }
    rate = ramp_value(s->sample_rate, c->sample_rate, fraction);
    ratio = rate / s->sample_rate;
    s->comb_delay = bound(s->comb_delay * ratio, 1.0f, (float)(XENOLITH_EXHAUST_COMB-2u));
    s->sample_rate = rate;
    if(xenolith_dispersion_active(&s->dispersion))
        (void)xenolith_dispersion_retime(&s->dispersion,rate);
    s->phase_increment = live_phase_increment(rate, s->host_rate);
    s->time = ramp_value(s->time, c->time, fraction);
    s->absorb = ramp_value(s->absorb, c->absorb, fraction);
    s->decay = ramp_value(s->decay, c->decay, fraction);
    s->mix = ramp_value(s->mix, c->mix, fraction);
    s->comb_gain = ramp_value(s->comb_gain, c->comb_gain, fraction);
    s->comb_pole = ramp_value(s->comb_pole, c->comb_pole, fraction);
    s->damping_pole = ramp_value(s->damping_pole, c->damping_pole, fraction);
    s->input_pole = ramp_value(s->input_pole, c->input_pole, fraction);
    s->dc_pole = ramp_value(s->dc_pole, c->dc_pole, fraction);
    s->delay_slew = ramp_value(s->delay_slew, c->delay_slew, fraction);
    s->damp_hz = ramp_value(s->damp_hz, c->damp_hz, fraction);
    for (i = 0; i < 8u; ++i) s->fdn_gain[i] = ramp_value(s->fdn_gain[i], c->fdn_gain[i], fraction);
    s->target_delay = bound(s->time * (1.0f + 0.2f * s->modulation) * rate, 1.0f, (float)(XENOLITH_EXHAUST_COMB-2u));
}

void xenolith_exhaust_apply(XenolithExhaust *s, const XenolithExhaustConfig *c) {
    if(!s || !c || !retime_config_valid(c))return;
    if (s->transition == 1u || c->divider != s->divider
        || c->host_rate != s->host_rate || c->sample_rate != s->sample_rate
        || c->reconstruction != s->reconstruction
        || (s->transition && (s->damp_model || c->damp_model))) {
        /* Audio-owned bounded copy. New requests replace the pending target;
         * they do not restart the fade or allocate another tank. */
        s->pending = *c;
        s->transition = 1u;
        s->mix = c->mix;
    } else if (!s->live_clock && !s->damp_model && !c->damp_model) {
        apply_now(s, c);
    } else {
        (void)xenolith_exhaust_retime(s, c);
    }
}

void xenolith_exhaust_set(XenolithExhaust *s, float time, float absorb,
                         float decay, float mix) {
    XenolithExhaustConfig config;
    uint32_t divider = s->transition == 1u ? s->pending.divider : s->divider;
    uint32_t recon = s->transition == 1u ? s->pending.reconstruction : s->reconstruction;
    uint32_t structure=s->transition==1u?s->pending.structure:s->structure;
    float rate = s->transition == 1u ? s->pending.sample_rate : s->sample_rate;
    float diffusion=s->transition==1u?s->pending.diffusion:s->diffusion_target;
    float width=s->transition==1u?s->pending.width:s->width_target;
    uint32_t model=s->transition==1u?s->pending.damp_model:s->damp_model;
    float damp=s->transition==1u?s->pending.requested_damp_hz:s->requested_damp_hz;
    if (divider && recon == XENOLITH_EXHAUST_ZOH)
        xenolith_exhaust_prepare_clock(&config, s->host_rate, divider, time, absorb, decay, mix);
    else xenolith_exhaust_prepare_rate(&config, s->host_rate, rate, recon, time, absorb, decay, mix);
    config.diffusion=diffusion;config.width=width;config.structure=structure;
    if(model)xenolith_exhaust_prepare_fx_rate(&config,s->host_rate,rate,structure,
        time,absorb,decay,mix,diffusion,width,damp,model);
    config.dispersion=s->transition==1u?s->pending.dispersion:s->dispersion.target;
    xenolith_exhaust_apply(s, &config);
}

void xenolith_exhaust_clear(XenolithExhaust *s) {
    if (s->transition == 1u) { leave_live_clock(s); apply_now(s, &s->pending); }
    else if (s->live_clock) { leave_live_clock(s); apply_now(s, &s->sweep_target); }
    xenolith_dispersion_clear(&s->dispersion);
    zero_bytes(s->comb, sizeof(s->comb));
    zero_bytes(s->fdn, sizeof(s->fdn));
    zero_bytes(s->diffuser, sizeof(s->diffuser));
    zero_bytes(s->fdn_write, sizeof(s->fdn_write));
    zero_bytes(s->diffuser_write, sizeof(s->diffuser_write));
    zero_bytes(s->damping_state, sizeof(s->damping_state));
    s->comb_write = 0; s->comb_lowpass = 0;
    s->previous_input = 0; s->dc_state = 0;
    s->random_state = UINT32_C(0x29e3a175);
    s->modulation_count = 0; s->coefficient_count = 0;
    s->modulation = 0; s->modulation_target = 0; s->modulation_step = 0;
    s->clipped_writes = 0;
    s->comb_delay = bound(s->time * s->sample_rate,1.0f,(float)(XENOLITH_EXHAUST_COMB-2u));
    s->target_delay = s->comb_delay;
    s->held_left = 0; s->held_right = 0;
    s->previous_left = 0; s->previous_right = 0; s->clock_phase = 0u;
    s->live_clock = 0u; s->sweep_remaining = 0u;
    s->wander_seconds = 0.0f; s->wander_elapsed = 0.0f;
    s->clock_count = 0; s->transition = 0; s->wet_ticks = 0;
    s->clock_fade = 1.0f;
    s->diffusion=s->diffusion_target;s->width=s->width_target;
    s->diffusion_step=s->width_step=0.0f;s->tank_remaining=0u;
    s->cloud=s->cloud_target;s->cloud_step=0.0f;
    s->ring_weight=s->ring_target;s->pairs_weight=s->pairs_target;
    s->ring_step=s->pairs_step=0.0f;
    s->input_blend=s->input_blend_target;s->input_blend_step=0.0f;s->input_state=0.0f;
    s->sag_diffuser=s->sag_diffuser_target;s->sag_interpolation=s->sag_interpolation_target;
    s->sag_damping=s->sag_damping_target;s->sag_remaining=0u;
    s->sag_diffuser_step=s->sag_interpolation_step=s->sag_damping_step=0.0f;
}

void xenolith_exhaust_init(XenolithExhaust *s, float sample_rate) {
    s->load_sag=0.0f;s->sag_diffuser=s->sag_interpolation=s->load_shape=1.0f;s->sag_damping=0.0f;
    s->sag_diffuser_target=s->sag_interpolation_target=1.0f;s->sag_damping_target=0.0f;
    s->sag_diffuser_step=s->sag_interpolation_step=s->sag_damping_step=0.0f;s->sag_remaining=0u;
    s->sample_rate = bound(sample_rate, 8000.0f, 44100.0f);
    s->host_rate = s->sample_rate;
    xenolith_dispersion_init(&s->dispersion,s->sample_rate);
    s->divider = 1u; s->transition = 0;
    s->reconstruction = XENOLITH_EXHAUST_ZOH; s->phase_increment = 0u;
    s->live_clock = 0u; s->sweep_remaining = 0u;
    s->modulation = 0;
    s->diffusion=s->diffusion_target=0.65f;s->width=s->width_target=1.0f;
    s->structure=0u;s->cloud=s->cloud_target=s->cloud_step=0.0f;
    s->requested_damp_hz=800.0f;s->damp_model=0u;s->input_pole=s->input_state=0.0f;
    s->input_blend=s->input_blend_target=s->input_blend_step=0.0f;
    s->ring_weight=s->pairs_weight=s->ring_target=s->pairs_target=s->ring_step=s->pairs_step=0.0f;
    s->diffusion_step=s->width_step=0.0f;s->tank_remaining=0u;
    xenolith_exhaust_set(s, 0.4f, 0.6f, 5.0f, 0.25f);
    xenolith_exhaust_clear(s);
}

static inline __attribute__((always_inline)) float wet_flush(float x) {
    return x > -1e-20f && x < 1e-20f ? 0.0f : x;
}
static inline __attribute__((always_inline)) float wet_min(float a,float b) {return a<b?a:b;}
static void load_targets(XenolithExhaust *s,float diffuser,float interpolation,float damping) {
    float reciprocal;
    if(diffuser==s->sag_diffuser_target&&interpolation==s->sag_interpolation_target
       &&damping==s->sag_damping_target)return;
    s->sag_diffuser_target=diffuser;s->sag_interpolation_target=interpolation;s->sag_damping_target=damping;
    s->sag_remaining=(uint32_t)(s->host_rate*.020f+.5f);
    if(!s->sag_remaining)s->sag_remaining=1u;
    reciprocal=1.0f/(float)s->sag_remaining;
    s->sag_diffuser_step=(diffuser-s->sag_diffuser)*reciprocal;
    s->sag_interpolation_step=(interpolation-s->sag_interpolation)*reciprocal;
    s->sag_damping_step=(damping-s->sag_damping)*reciprocal;
}
void xenolith_exhaust_set_load_sag(XenolithExhaust *s,float stress) {
    if(!s||!(stress>=0.0f&&stress<=1.0f))return;
    s->load_sag=bound(stress/.50f,0.0f,1.0f);
    load_targets(s,1.0f-bound((stress-.15f)/.25f,0.0f,1.0f),
        1.0f-.15f*bound((stress-.75f)/.25f,0.0f,1.0f),
        bound(.55f*s->load_sag*s->load_shape,0.0f,.98f));
}
void xenolith_exhaust_set_load_shape(XenolithExhaust *s,float shape) {
    if(!s||!(shape>=0.0f&&shape<=2.0f))return;
    s->load_shape=shape;load_targets(s,s->sag_diffuser_target,s->sag_interpolation_target,
        bound(.55f*s->load_sag*shape,0.0f,.98f));
}
static inline __attribute__((always_inline)) float diffuse(XenolithExhaust *s, float input) {
    unsigned i;
    float coefficient=s->diffusion;
    for (i = 0; i < 4; ++i) {
        float previous=input;
        uint32_t p = ap_offset[i] + s->diffuser_write[i];
        float delayed = s->diffuser[p];
        if(i>=2u && s->sag_diffuser==0.0f){
            /* Zero-input allpass decay, still on the wet clock. Advancing this
             * ring costs less than driven scattering and cannot resurrect a
             * history frozen for an arbitrary time during load relief. */
            s->diffuser[p]=wet_flush(coefficient*delayed);
            if(++s->diffuser_write[i]==ap_length[i])s->diffuser_write[i]=0;
            continue;
        }
        float output = delayed - coefficient * input;
        s->diffuser[p] = wet_flush(input + coefficient * output);
        input = output;
        if(i>=2u && s->sag_diffuser!=1.0f)
            input=previous+s->sag_diffuser*(output-previous);
        if (++s->diffuser_write[i] == ap_length[i]) s->diffuser_write[i] = 0;
    }
    return input;
}

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define EXHAUST_RAM __attribute__((section(".audio_text.xenolith_exhaust"),noinline))
static void (*volatile dispersion_tick)(XenolithDispersion *,float *)=xenolith_dispersion_process;
#else
#define EXHAUST_RAM
#define dispersion_tick xenolith_dispersion_process
#endif
#define fm_minf wet_min
#define fm_flush wet_flush
static EXHAUST_RAM void wet_tick(XenolithExhaust *s, float input,
                     float *left, float *right) {
    float vector[8], output_l, output_r, wet_input, delayed, position, fraction;
    float comb_write, dc_input, pole;
    uint32_t p, next;
    unsigned i, stride, base, j;
    /* Slow bounded random wander, deliberately not an LFNoise2 emulation.
     * One new target per ten seconds; linear movement at control rate. */
    if (s->coefficient_count == 0) {
        if (s->live_clock) {
            float elapsed = s->wander_elapsed;
            if (s->wander_seconds <= elapsed) {
                if (s->wander_seconds > 0.0f) {
                    s->modulation = s->modulation_target;
                    elapsed -= s->wander_seconds;
                }
                s->modulation_target = random_bipolar(s);
                s->wander_seconds = 10.0f;
            }
            elapsed = fm_minf(elapsed, s->wander_seconds);
            s->modulation += (s->modulation_target - s->modulation) * (elapsed / s->wander_seconds);
            s->wander_seconds -= elapsed;
            s->wander_elapsed = 0.0f;
        } else {
        if (s->modulation_count == 0) {
            s->modulation_target = random_bipolar(s);
            s->modulation_count = (uint32_t)(s->sample_rate * (10.0f / 64.0f));
            if (s->modulation_count < 1) s->modulation_count = 1;
            s->modulation_step = (s->modulation_target - s->modulation)
                                / (float)s->modulation_count;
        }
        s->modulation += s->modulation_step;
        --s->modulation_count;
        }
        s->target_delay = bound(s->time * (1.0f + 0.2f * s->modulation)
                               * s->sample_rate, 1.0f, (float)(XENOLITH_EXHAUST_COMB-2u));
        s->coefficient_count = 64;
    }
    --s->coefficient_count;
    s->comb_delay += (s->target_delay - s->comb_delay) * s->delay_slew;
    position = (float)s->comb_write - s->comb_delay;
    if (position < 0) position += (float)XENOLITH_EXHAUST_COMB;
    /* A tiny negative position can round to exactly the ring length when
     * adding it in float32. Wrap that endpoint before either table access. */
    if (position >= (float)XENOLITH_EXHAUST_COMB) position = 0.0f;
    p = (uint32_t)position;
    fraction = position - (float)p;
    next = p + 1u;
    if (next == XENOLITH_EXHAUST_COMB) next = 0;
    if(s->sag_interpolation==1.0f)
        delayed = ((float)s->comb[p] + ((float)s->comb[next] - (float)s->comb[p])
                   * fraction) * (1.0f / 32768.0f);
    else if(s->sag_interpolation==0.0f)delayed=(float)s->comb[p]*(1.0f/32768.0f);
    else delayed=((float)s->comb[p]+((float)s->comb[next]-(float)s->comb[p])
                    *(fraction*s->sag_interpolation))*(1.0f/32768.0f);

    /* DC is removed only from the wet excitation; dry samples remain exact. */
    dc_input = input - s->previous_input + s->dc_pole * s->dc_state;
    s->previous_input = input;
    s->dc_state = fm_flush(dc_input);
    /* Short delay + long decay can otherwise amplify a resonant drone enough
     * to clip the int16 store. Bound the comb's resonant input gain as feedback
     * approaches one; the default .4 s / 5 s setting still uses 0.25. */
    comb_write = fm_minf(0.25f, 1.0f - s->comb_gain) * dc_input
                 + s->comb_gain * delayed;
    if (comb_write > 0.9999694824f || comb_write < -1.0f) ++s->clipped_writes;
    comb_write = bound(comb_write, -1.0f, 0.9999694824f);
    /* Truncation toward zero ensures a decaying sub-LSB comb state cannot
     * become a rounded positive-feedback fixed point. No injected dither. */
    s->comb[s->comb_write] = (int16_t)(comb_write * 32768.0f);
    if (++s->comb_write == XENOLITH_EXHAUST_COMB) s->comb_write = 0;
    s->comb_lowpass = fm_flush((1.0f - s->comb_pole) * delayed
                              + s->comb_pole * s->comb_lowpass);
    wet_input=dc_input;
    if(s->input_blend>0.0f){
        s->input_state=fm_flush((1.0f-s->input_pole)*dc_input+s->input_pole*s->input_state);
        wet_input+=s->input_blend*(s->input_state-dc_input);
    }
    wet_input = diffuse(s, wet_input + 2.0f * s->comb_lowpass) * 0.2f;

    pole=s->damping_pole;
    if(s->sag_damping>0.0f)pole+=s->sag_damping*(1.0f-pole);
    for (i = 0; i < 8; ++i) {
        float value = s->fdn[fdn_offset[i] + s->fdn_write[i]];
        s->damping_state[i] = fm_flush((1.0f - pole) * value
                                    + pole * s->damping_state[i]);
        vector[i] = s->damping_state[i] * s->fdn_gain[i];
    }
    output_l = (vector[0] + vector[1] - vector[2] + vector[3]
              - vector[4] + vector[5] - vector[6] - vector[7]) * 0.353553391f;
    output_r = (vector[0] - vector[1] + vector[2] + vector[3]
              + vector[4] - vector[5] - vector[6] - vector[7]) * 0.353553391f;
    if(s->cloud>0.0f){
        float z[8],early_l,early_r,weight;
        /* Read before writing. Fixed positive distances always fit their line.
         * Undo alternating excitation polarity for a distributed stereo field.
         * Four/eight decoders each have per-channel squared gain sum.25;
         * L1 gains never exceed1. Correlated histories need not sound equal. */
        for(i=0;i<8u;++i){
            if(s->cloud<=1.0f && (i&2u))continue;
            uint32_t at=s->fdn_write[i]+fdn_length[i]-cloud_delay[i];
            if(at>=fdn_length[i])at-=fdn_length[i];
            z[i]=s->fdn[fdn_offset[i]+at];
            if(i&1u)z[i]=-z[i];
        }
        early_l=(z[0]+z[4])*0.353553391f;
        early_r=(z[1]+z[5])*0.353553391f;
        if(s->cloud>1.0f){
            float f=s->cloud-1.0f;
            early_l+=f*((z[0]+z[2]+z[4]+z[6])*0.25f-early_l);
            early_r+=f*((z[1]+z[3]+z[5]+z[7])*0.25f-early_r);
        }
        weight=0.5f*fm_minf(s->cloud,1.0f);
        output_l+=weight*(early_l-output_l);
        output_r+=weight*(early_r-output_r);
    }
    if(s->width!=1.0f){
        float mid=(output_l+output_r)*0.5f;
        float side=(output_l-output_r)*(0.5f*s->width);
        output_l=mid+side;output_r=mid-side;
    }
    /* Decode first: the short allpass bank changes feedback, not direct taps. */
    if(xenolith_dispersion_active(&s->dispersion))dispersion_tick(&s->dispersion,vector);
    if(s->ring_weight==0.0f && s->pairs_weight==0.0f){
        /* Original arithmetic endpoint:12 butterflies,24 add/sub operations. */
        for (stride = 1; stride < 8; stride <<= 1)
            for (base = 0; base < 8; base += stride << 1)
                for (j = 0; j < stride; ++j) {
                    float a = vector[base+j], b = vector[base+j+stride];
                    vector[base+j] = a+b; vector[base+j+stride] = a-b;
                }
        for(i=0;i<8u;i++)vector[i]*=0.353553391f;
    }else{
        float original[8],h=bound(1.0f-s->ring_weight-s->pairs_weight,0.0f,1.0f);
        float hg=h*0.353553391f,pg=s->pairs_weight*0.707106781f;
        for(i=0;i<8u;i++)original[i]=vector[i];
        if(s->ring_weight==1.0f){
            for(i=0;i<8u;i++)vector[i]=original[(i+7u)&7u];
        }else if(s->pairs_weight==1.0f){
            for(i=0;i<8u;i+=2u){
                vector[i]=(original[i]+original[i+1u])*0.707106781f;
                vector[i+1u]=(original[i]-original[i+1u])*0.707106781f;
            }
        }else{
        if(h>0.0f){
            for(stride=1;stride<8;stride<<=1)
                for(base=0;base<8;base+=stride<<1)
                    for(j=0;j<stride;j++){
                        float a=vector[base+j],b=vector[base+j+stride];
                        vector[base+j]=a+b;vector[base+j+stride]=a-b;
                    }
        }
        for(i=0;i<8u;i++){
            float a=original[i&~1u],b=original[i|1u];
            float pair=(i&1u)?a-b:a+b;
            vector[i]=hg*vector[i]+pg*pair+s->ring_weight*original[(i+7u)&7u];
        }
        }
    }
    for (i = 0; i < 8; ++i) {
        float injection = (i & 1u) ? -wet_input : wet_input;
        s->fdn[fdn_offset[i] + s->fdn_write[i]] =
            fm_flush(vector[i] + injection);
        if (++s->fdn_write[i] == fdn_length[i]) s->fdn_write[i] = 0;
    }
    *left = output_l; *right = output_r;
}
#undef fm_minf
#undef fm_flush

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
static void (*volatile wet_kernel)(XenolithExhaust *,float,float *,float *)=wet_tick;
#else
#define wet_kernel wet_tick
#endif

void xenolith_exhaust_process(XenolithExhaust *s, float input,
                             float *left, float *right) {
    float output_l, output_r;
    if(s->sag_remaining){
        if(--s->sag_remaining==0u){
            s->sag_diffuser=s->sag_diffuser_target;s->sag_interpolation=s->sag_interpolation_target;
            s->sag_damping=s->sag_damping_target;
        }else{
            s->sag_diffuser=bound(s->sag_diffuser+s->sag_diffuser_step,0.0f,1.0f);
            s->sag_interpolation=bound(s->sag_interpolation+s->sag_interpolation_step,.85f,1.0f);
            s->sag_damping=bound(s->sag_damping+s->sag_damping_step,0.0f,.98f);
        }
    }
    if(s->tank_remaining){
        if(--s->tank_remaining==0u){
            s->diffusion=s->diffusion_target;s->width=s->width_target;s->cloud=s->cloud_target;
            s->ring_weight=s->ring_target;s->pairs_weight=s->pairs_target;
            s->input_blend=s->input_blend_target;
            if(s->input_blend==0.0f)s->input_state=0.0f;
        }
        else{
            s->diffusion=bound(s->diffusion+s->diffusion_step,0.0f,0.85f);
            s->width=bound(s->width+s->width_step,0.0f,1.0f);
            s->cloud=bound(s->cloud+s->cloud_step,0.0f,2.0f);
            s->ring_weight=bound(s->ring_weight+s->ring_step,0.0f,1.0f);
            s->pairs_weight=bound(s->pairs_weight+s->pairs_step,0.0f,1.0f-s->ring_weight);
            s->input_blend=bound(s->input_blend+s->input_blend_step,0.0f,1.0f);
        }
    }
    if (s->transition == 1u) {
        s->clock_fade -= s->fade_step;
        if (s->clock_fade <= 0.0f) {
            s->clock_fade = 0.0f;
            leave_live_clock(s);
            apply_now(s, &s->pending);
            s->clock_count = 0;
            s->clock_phase = 0u;
            s->previous_left = s->held_left; s->previous_right = s->held_right;
            s->transition = 2u;
        }
    } else if (s->transition == 2u) {
        s->clock_fade += s->fade_step;
        if (s->clock_fade >= 1.0f) {
            s->clock_fade = 1.0f;
            s->transition = 0;
        }
    }
    if (s->live_clock) s->wander_elapsed += 1.0f / s->host_rate;
    if (s->divider) {
        /* Keep legacy ZOH scheduling/arithmetic intact, including sample0. */
        if (s->clock_count == 0u) {
            s->previous_left = s->held_left; s->previous_right = s->held_right;
            wet_kernel(s, input, &s->held_left, &s->held_right);
            ++s->wet_ticks;
            s->clock_count = s->divider;
        }
        --s->clock_count;
        output_l = s->held_left; output_r = s->held_right;
        if (s->reconstruction == XENOLITH_EXHAUST_LINEAR) {
            float fraction = (float)(s->divider - s->clock_count - 1u) / (float)s->divider;
            output_l = s->previous_left + fraction * (s->held_left - s->previous_left);
            output_r = s->previous_right + fraction * (s->held_right - s->previous_right);
        }
    } else {
        uint32_t tick = s->clock_count == 0u;
        if (!tick && s->live_clock && s->phase_increment == 0u) tick = 1u;
        else if (!tick) {
            uint32_t previous = s->clock_phase;
            s->clock_phase += s->phase_increment;
            tick = s->clock_phase < previous;
        }
        if (tick) {
            s->previous_left = s->held_left; s->previous_right = s->held_right;
            wet_kernel(s, input, &s->held_left, &s->held_right);
            ++s->wet_ticks;
            s->clock_count = 1u;
        }
        output_l = s->held_left; output_r = s->held_right;
        if (s->reconstruction == XENOLITH_EXHAUST_LINEAR) {
            float fraction = (float)s->clock_phase * (1.0f / 4294967296.0f);
            output_l = s->previous_left + fraction * (s->held_left - s->previous_left);
            output_r = s->previous_right + fraction * (s->held_right - s->previous_right);
        }
    }
    /* No multiply on the settled full-rate path; preserve its old PCM. */
    if (s->transition) {
        output_l *= s->clock_fade; output_r *= s->clock_fade;
    }
    if (s->mix == 0.0f) {
        *left = input; *right = input;
    } else {
        *left = input + s->mix * (output_l - input);
        *right = input + s->mix * (output_r - input);
    }
}
