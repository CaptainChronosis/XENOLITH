/* SPDX-License-Identifier: MPL-2.0
 * Independently implemented cascade of symmetric FIR halfband decimators.
 *63 taps, cutoff0.25cycles/input sample, Kaiser beta8.6, unityDC. The16
 * paired nonzero coefficients plus center need17multiplies per retained
 * sample.4x uses two2:1 stages:51multiplies/output,2x uses17.
 * Generated mathematical coefficients; no copied library implementation.
 */
#include "xenolith_quality.h"

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define XQUALITY_RAM_AUDIO __attribute__((section(".audio_text.xenolith_quality"),noinline))
#define XQUALITY_INLINE static inline __attribute__((always_inline))
#else
#define XQUALITY_RAM_AUDIO
#define XQUALITY_INLINE static inline
#endif

static const float halfband_pair[16] = {
    -1.3682310342e-05f, 7.3679824780e-05f, -2.1592065572e-04f, 4.9873709007e-04f,
    -1.0020225852e-03f, 1.8300546271e-03f, -3.1149912511e-03f, 5.0232894202e-03f,
    -7.7699291801e-03f, 1.1651144673e-02f, -1.7121129290e-02f, 2.4981912269e-02f,
    -3.6911386447e-02f, 5.7271132152e-02f, -1.0215353194e-01f, 3.1697309061e-01f
};
static const float halfband_center = 4.9999910599e-01f;

#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_QUALITY_CACHE)
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
static float halfband_ram[16] __attribute__((section(".pool"),aligned(32)));
#else
static float halfband_ram[16];
#endif
static uint32_t halfband_ready;
#define HALF_BAND_PAIR halfband_ram
static void prepare_halfband(void){
    uint32_t i;if(halfband_ready)return;
    for(i=0;i<16u;i++)halfband_ram[i]=halfband_pair[i];
    halfband_ready=1u;
}
#else
#define HALF_BAND_PAIR halfband_pair
static void prepare_halfband(void){}
#endif

void xenolith_quality_prepare_crush(XenolithCrushConfig *c, float output_rate,
    uint32_t base_control_frames, float rate, uint32_t bits,
    uint32_t quantizer, uint32_t reconstruction) {
    float maximum;
    if (!(output_rate >= 1000.0f)) output_rate = 1000.0f;
    if (output_rate > 192000.0f) output_rate = 192000.0f;
    if (base_control_frames < 1u) base_control_frames = 1u;
    if (base_control_frames > 1024u) base_control_frames = 1024u;
    maximum = output_rate < 44100.0f ? output_rate : 44100.0f;
    if (!(rate >= 1000.0f)) rate = 1000.0f;
    if (rate > maximum) rate = maximum;
    c->rate = rate;
    c->bits = bits < 2u ? 2u : bits > 15u ? 15u : bits;
    c->scale = (float)(1u << c->bits); c->inverse_scale = 1.0f / c->scale;
    c->quantizer = quantizer > 2u ? 2u : quantizer;
    c->reconstruction = reconstruction > 1u ? 1u : reconstruction;
    c->control_frames = (uint32_t)(rate * (float)base_control_frames / output_rate + 0.5f);
    if (c->control_frames < 1u) c->control_frames = 1u;
    if (c->control_frames > 1024u) c->control_frames = 1024u;
}

static void reset_crush(XenolithQuality *q) {
    q->held = 0; q->previous_held = 0; q->quantization_error = 0;
    q->hold_remaining = 0; q->crush_phase = 1.0f;
    q->raw_held = 0.0f;
}

void xenolith_quality_apply_crush(XenolithQuality *q, XenolithCore *core,
                                const XenolithCrushConfig *c) {
    if (q->continuous) xenolith_quality_set_factor(q,core,q->factor);
    q->crush = *c;
    q->configured_bits = c->bits;
    q->crush_step = c->rate / q->output_rate;
    if (q->factor == XENOLITH_RATE_CRUSH) {
        xenolith_reclock(core, c->rate, c->control_frames);
        q->nominal_rate = q->target_rate = core->sample_rate;
        reset_crush(q); q->warmup = 0;
    }
}

static void clear_filters(XenolithQuality *q) {
    uint32_t i, stage;
    for (stage = 0; stage < 2u; ++stage) {
        for (i = 0; i < 64u; ++i) q->stage[stage].history[i] = 0;
        q->stage[stage].write = 0;
    }
}

XQUALITY_INLINE void halfband_push(XenolithHalfband *s, float sample) {
    s->history[s->write] = sample;
    s->write = (s->write + 1u) & 63u;
}

XQUALITY_RAM_AUDIO static float halfband_output(const XenolithHalfband *s) {
    uint32_t i, last = (s->write - 1u) & 63u;
    float sum = halfband_center * s->history[(last - 31u) & 63u];
    for (i = 0; i < 16u; ++i) {
        uint32_t offset = 2u * i;
        sum += HALF_BAND_PAIR[i] * (s->history[(last - offset) & 63u]
                                + s->history[(last - (62u - offset)) & 63u]);
    }
    return sum;
}

void xenolith_quality_init(XenolithQuality *q, const XenolithCore *core) {
    prepare_halfband();
    clear_filters(q);
    q->output_rate = core->sample_rate;
    q->control_frames = core->control_block;
    q->factor = 1u;
    q->digital_bits = 0u;
    q->configured_bits = 0u;
    q->continuous = 0u; q->ramp_remaining = 0u;
    q->target_rate = core->sample_rate;
    q->nominal_rate = core->sample_rate; q->load_duty = 1.0f;
    q->high_actual_rate = core->sample_rate;
    q->core_hold = 0u;
    q->warmup = 0;
    q->held = 0; q->hold_remaining = 0;
    xenolith_quality_prepare_crush(&q->crush,q->output_rate,q->control_frames,
                                   11025.0f,7u,0u,0u);
    q->crush_step = q->crush.rate / q->output_rate;
    reset_crush(q);
}

static uint32_t supported_limits(float output_rate, uint32_t control_frames, uint32_t factor) {
    if (factor == XENOLITH_RATE_CRUSH) return factor;
    if (factor >= XENOLITH_RATE_ECO2 && factor <= XENOLITH_RATE_CRUSH) {
        uint32_t divider = factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
        if (output_rate / (float)divider < 8000.0f
            || control_frames % divider != 0u) return 1u;
        return factor;
    }
    if (factor != 2u && factor != 4u) return 1u;
    while (factor > 1u && (output_rate * (float)factor > 192000.0f
                           || control_frames * factor > 1024u)) factor >>= 1;
    return factor;
}

static uint32_t supported_factor(const XenolithQuality *q, uint32_t factor) {
    return supported_limits(q->output_rate,q->control_frames,factor);
}

uint32_t xenolith_quality_factor_for(const XenolithQuality *q, uint32_t mode, uint32_t cells) {
    uint32_t factor;
    if (mode >= XENOLITH_QUALITY_ECO2 && mode <= XENOLITH_QUALITY_CRUSH)
        return supported_factor(q, XENOLITH_RATE_ECO2 + mode - XENOLITH_QUALITY_ECO2);
    if (mode == XENOLITH_QUALITY_HI2) factor = cells <= 16u ? 2u : 1u;
    else if (mode == XENOLITH_QUALITY_AUTO || mode == XENOLITH_QUALITY_HI4)
        factor = cells <= 8u ? 4u : cells <= 16u ? 2u : 1u;
    else factor = 1u;
    return supported_factor(q, factor);
}

void xenolith_quality_prepare_digital(XenolithDigitalConfig *c,
    float output_rate, uint32_t base_control_frames, uint32_t cells, float requested_rate,
    uint32_t bits, uint32_t quantizer, uint32_t reconstruction) {
    uint32_t factor, ceiling;
    float rate;
    if (!(output_rate >= 1000.0f)) output_rate = 1000.0f;
    if (output_rate > 192000.0f) output_rate = 192000.0f;
    if (base_control_frames < 1u) base_control_frames = 1u;
    if (base_control_frames > 1024u) base_control_frames = 1024u;
    if (!(requested_rate >= 1000.0f)) requested_rate = 1000.0f;
    if (requested_rate > 176400.0f) requested_rate = 176400.0f;
    if (requested_rate > 44100.0f)
        requested_rate = requested_rate >= 176400.0f ? 176400.0f
                       : requested_rate >= 88200.0f ? 88200.0f : 44100.0f;
    bits = bits > 24u ? 24u : bits;
    if (cells < 1u) cells = 1u;
    ceiling = cells <= 8u ? 4u : cells <= 16u ? 2u : 1u;
    factor = 1u;
    while (factor < ceiling && output_rate * (float)(factor * 2u) <= requested_rate)
        factor *= 2u;
    factor = supported_limits(output_rate,base_control_frames,factor);
    rate = output_rate * (float)factor;
    if (requested_rate < output_rate) rate = requested_rate;
    c->requested_rate = requested_rate;
    c->effective_rate = rate;
    c->bits = bits;
    c->quantizer = quantizer > 2u ? 2u : quantizer;
    c->reconstruction = reconstruction > 1u ? 1u : reconstruction;
    xenolith_quality_prepare_crush(&c->crush,output_rate,base_control_frames,
        rate,bits ? bits : 2u,c->quantizer,c->reconstruction);
    /* The legacy entry point intentionally clamps its clock to44.1k. A
     * different host clock may place a supported digital request above that
     * boundary while still below the host. Keep the scheduler cache honest. */
    if (rate > c->crush.rate && rate <= output_rate) {
        c->crush.rate = rate;
        c->crush.control_frames = (uint32_t)(rate*(float)base_control_frames/output_rate+0.5f);
        if (c->crush.control_frames < 1u) c->crush.control_frames = 1u;
        if (c->crush.control_frames > 1024u) c->crush.control_frames = 1024u;
    }
    /* This generalized cache may bypass the quantizer; the legacy preparation
     * entry retains its historical2..15 validation unchanged. */
    c->crush.bits = bits;
    c->crush.scale = (float)(1u << bits);
    c->crush.inverse_scale = 1.0f / c->crush.scale;
    if (rate < output_rate || (factor == 1u && c->reconstruction)) {
        factor = XENOLITH_RATE_CRUSH;
        if (!bits && !c->reconstruction && rate == output_rate * 0.5f
            && supported_limits(output_rate,base_control_frames,XENOLITH_RATE_ECO2) == XENOLITH_RATE_ECO2)
            factor = XENOLITH_RATE_ECO2;
        if (!bits && !c->reconstruction && rate == output_rate * 0.25f
            && supported_limits(output_rate,base_control_frames,XENOLITH_RATE_ECO4) == XENOLITH_RATE_ECO4)
            factor = XENOLITH_RATE_ECO4;
    }
    c->factor = factor;
}

void xenolith_quality_apply_digital(XenolithQuality *q, XenolithCore *core,
                                  const XenolithDigitalConfig *c) {
    uint32_t same = !q->continuous && q->factor == c->factor && q->configured_bits == c->bits
        && q->crush.rate == c->crush.rate
        && q->crush.quantizer == c->crush.quantizer
        && q->crush.reconstruction == c->crush.reconstruction
        && q->crush.control_frames == c->crush.control_frames;
    if (q->continuous && q->factor == c->factor && q->configured_bits == c->bits
        && q->nominal_rate == c->effective_rate && q->target_rate == c->effective_rate
        && q->crush.quantizer == c->quantizer
        && q->crush.reconstruction == c->reconstruction) same = 1u;
    if (same) return;
    /* Leave low-rate scheduling before updating its cache to avoid an
     * unnecessary intermediate reclock. All changes remain behind the gate. */
    if (c->factor != XENOLITH_RATE_CRUSH)
        xenolith_quality_set_factor(q,core,c->factor);
    xenolith_quality_apply_crush(q,core,&c->crush);
    if (c->factor == XENOLITH_RATE_CRUSH)
        xenolith_quality_set_factor(q,core,c->factor);
    q->continuous = 0u; q->ramp_remaining = 0u;
    q->digital_bits = c->bits;
    q->configured_bits = c->bits;
    q->target_rate = c->effective_rate;
    q->nominal_rate = c->effective_rate;
    q->high_actual_rate = core->sample_rate;
    q->quantization_error = 0.0f;
    if (c->factor < XENOLITH_RATE_ECO2) {
        clear_filters(q);
        q->warmup = c->factor == 4u ? 48u : c->factor == 2u ? 32u : 0u;
    }
}

void xenolith_quality_set_live_bits(XenolithQuality *q,uint32_t bits) {
    uint32_t active;
    if(!q)return;
    if(bits>24u)bits=24u;
    active=q->continuous||q->factor==XENOLITH_RATE_CRUSH?q->crush.bits:q->digital_bits;
    if(active==bits)return;
    q->digital_bits=bits;q->crush.bits=bits;
    q->crush.scale=(float)(1u<<bits);q->crush.inverse_scale=1.0f/q->crush.scale;
    q->quantization_error=0.0f;
}

uint32_t xenolith_quality_update_precision(XenolithQuality *q,
                                         const XenolithDigitalConfig *c) {
    if(!q||!c||q->warmup||q->factor!=c->factor
       ||q->target_rate!=c->effective_rate
       ||q->crush.reconstruction!=c->reconstruction)return 0u;
    /* The converter is already running at this clock (or smoothly approaching
     * this same target). Changing its amplitude grid needs no FIR reset or
     * fade to silence. Error feedback retains its residual only when both the
     * live grid and quantizer law are unchanged. */
    if(q->crush.quantizer!=c->quantizer)q->quantization_error=0.0f;
    q->crush.quantizer=c->quantizer;
    q->configured_bits=c->bits;
    xenolith_quality_set_live_bits(q,c->bits);
    return 1u;
}

XQUALITY_RAM_AUDIO static float quantize(XenolithQuality *q, float raw, uint32_t bits) {
    float scaled, result;
    if (!bits) return raw;
    if(bits==1u){
        /* Keep exact silence silent, even after error-shaped active material.
         * The same guard also prevents a nonfinite NaN poisoning the residual. */
        if(!(raw>0.0f||raw<0.0f)){q->quantization_error=0.0f;return 0.0f;}
        if(q->crush.quantizer==2u){
            if(raw>0.5f)raw=0.5f;if(raw< -0.5f)raw= -0.5f;
            raw+=q->quantization_error;result=raw>=0.0f?0.5f:-0.5f;
            q->quantization_error=raw-result;
            if(q->quantization_error>0.5f)q->quantization_error=0.5f;
            if(q->quantization_error< -0.5f)q->quantization_error= -0.5f;
        }else{result=raw>0.0f?0.5f:-0.5f;q->quantization_error=0.0f;}
        return result;
    }
    if (q->crush.quantizer == 2u) raw += q->quantization_error;
    scaled = raw * q->crush.scale;
    if (q->crush.quantizer && bits <= 15u) {
        /* Preserve the established low-grid arithmetic exactly. */
        scaled += scaled >= 0.0f ? 0.5f : -0.5f;
        result = (float)(int32_t)scaled * q->crush.inverse_scale;
    } else {
        int32_t whole = (int32_t)scaled;
        if (q->crush.quantizer) {
            float fraction = scaled-(float)whole;
            /* Adding.5 to a float integer near2^23 can change that integer
             * through round-to-even. Test its fraction before correcting. */
            if (fraction >= 0.5f) ++whole;
            else if (fraction <= -0.5f) --whole;
        }
        result = (float)whole * q->crush.inverse_scale;
    }
    q->quantization_error = q->crush.quantizer == 2u ? raw-result : 0.0f;
    if (q->quantization_error > q->crush.inverse_scale)
        q->quantization_error = q->crush.inverse_scale;
    if (q->quantization_error < -q->crush.inverse_scale)
        q->quantization_error = -q->crush.inverse_scale;
    return result;
}

XQUALITY_INLINE float quantize_optional(XenolithQuality *q,float raw,uint32_t bits){
    return bits?quantize(q,raw,bits):raw;
}

void xenolith_quality_set_factor(XenolithQuality *q, XenolithCore *core, uint32_t factor) {
    factor = supported_factor(q, factor);
    if (factor == q->factor && !q->continuous) return;
    q->continuous = 0u; q->ramp_remaining = 0u;
    if (factor == XENOLITH_RATE_CRUSH) {
        xenolith_reclock(core,q->crush.rate,q->crush.control_frames);
    } else if (factor >= XENOLITH_RATE_ECO2) {
        uint32_t divider = factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
        xenolith_reclock(core, q->output_rate / (float)divider, q->control_frames / divider);
    } else {
        xenolith_reclock(core, q->output_rate * (float)factor, q->control_frames * factor);
    }
    clear_filters(q);
    q->factor = factor;
    q->target_rate = core->sample_rate;
    q->nominal_rate = core->sample_rate;
    q->high_actual_rate = core->sample_rate;
    q->held = 0; q->hold_remaining = 0;
    reset_crush(q);
    q->warmup = factor == 4u ? 48u : factor == 2u ? 32u : 0u;
}

static uint32_t low_path(uint32_t factor) {
    return factor == 1u || (factor >= XENOLITH_RATE_ECO2 && factor <= XENOLITH_RATE_CRUSH);
}

uint32_t xenolith_quality_retime(XenolithQuality *q, XenolithCore *core,
                                const XenolithDigitalConfig *c) {
    uint32_t scheduled = q->continuous || q->factor == XENOLITH_RATE_CRUSH;
    uint32_t bits = q->configured_bits;
    uint32_t live_bits = scheduled ? q->crush.bits : q->digital_bits;
    float scale=q->crush.scale,inverse=q->crush.inverse_scale;
    uint32_t reconstruction = scheduled ? q->crush.reconstruction : 0u;
    if (!low_path(q->factor) || !low_path(c->factor)
        || !(c->effective_rate >= 1000.0f && c->effective_rate <= q->output_rate)
        || bits != c->bits || q->crush.quantizer != c->quantizer
        || reconstruction != c->reconstruction || q->warmup) return 0u;
    if (c->effective_rate == q->nominal_rate && !q->ramp_remaining
        && q->factor == c->factor) {
        q->crush = c->crush;
        if(live_bits!=c->bits){q->crush.bits=live_bits;q->crush.scale=scale;q->crush.inverse_scale=inverse;}
        if (q->continuous) {
            q->crush.rate = core->sample_rate;
            q->crush.control_frames = core->control_block;
        }
        q->crush_step = core->sample_rate/q->output_rate;
        q->target_rate = q->nominal_rate;
        return 1u;
    }
    if (!scheduled) {
        if (q->factor >= XENOLITH_RATE_ECO2) {
            uint32_t divider = q->factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
            q->crush_phase = 1.0f-(float)q->hold_remaining/(float)divider;
        } else q->crush_phase = 1.0f;
    }
    q->continuous = 1u;
    q->factor = c->factor;
    q->digital_bits = live_bits;
    /* Preserve live clock/history/error; only immutable grid controls copy. */
    q->crush.bits = live_bits;
    q->crush.scale = live_bits==c->bits?c->crush.scale:scale;
    q->crush.inverse_scale = live_bits==c->bits?c->crush.inverse_scale:inverse;
    q->crush.quantizer = c->quantizer; q->crush.reconstruction = c->reconstruction;
    q->crush.rate = core->sample_rate;
    q->crush.control_frames = core->control_block;
    q->crush_step = core->sample_rate/q->output_rate;
    if (q->target_rate != c->effective_rate || !q->ramp_remaining) {
        q->target_rate = c->effective_rate;
        q->ramp_remaining = (uint32_t)(q->output_rate*0.05f+0.5f);
        if (!q->ramp_remaining) q->ramp_remaining = 1u;
    }
    return 1u;
}

void xenolith_quality_begin_block(XenolithQuality *q, XenolithCore *core,
                                  uint32_t output_frames) {
    float rate;uint32_t frames;
    if (!q || !core || !output_frames) return;
    if(q->factor==2u||q->factor==4u){
        /* Keep both halfband stages on their original uniform clock. Only
         * raw oscillator updates use the fractional scheduler below. */
        rate=q->nominal_rate*q->load_duty;
        if(rate!=core->sample_rate){
            frames=(uint32_t)(rate*(float)q->control_frames/q->output_rate+0.5f);
            if(frames<1u)frames=1u;if(frames>1024u)frames=1024u;
            xenolith_retime(core,rate,frames);
        }
        q->high_actual_rate=core->sample_rate;
        return;
    }
    if(!low_path(q->factor))return;
    if (q->continuous && q->ramp_remaining) {
        if (output_frames >= q->ramp_remaining) {
            q->nominal_rate = q->target_rate; q->ramp_remaining = 0u;
        } else {
            q->nominal_rate += (q->target_rate-q->nominal_rate)
                * ((float)output_frames/(float)q->ramp_remaining);
            q->ramp_remaining -= output_frames;
        }
    }
    rate = q->nominal_rate*q->load_duty;
    if (rate < 1000.0f) rate = 1000.0f;
    if (rate == core->sample_rate) return;
    if (!q->continuous && q->factor != XENOLITH_RATE_CRUSH) {
        if (q->factor >= XENOLITH_RATE_ECO2) {
            uint32_t divider = q->factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
            q->crush_phase = 1.0f-(float)q->hold_remaining/(float)divider;
        } else q->crush_phase = 1.0f;
        q->crush.bits = q->digital_bits;
        q->crush.reconstruction = 0u;
    }
    q->continuous = 1u;
    frames = (uint32_t)(rate*(float)q->control_frames/q->output_rate+0.5f);
    if (frames < 1u) frames = 1u;
    if (frames > 1024u) frames = 1024u;
    xenolith_retime(core,rate,frames);
    q->crush.rate = rate; q->crush.control_frames = frames;
    q->crush_step = rate/q->output_rate;
}

void xenolith_quality_set_load_duty(XenolithQuality *q,float duty) {
    if (!q) return;
    if (duty != duty) duty = 1.0f;
    if (duty < 0.25f) duty = 0.25f;
    if (duty > 1.0f) duty = 1.0f;
    q->load_duty = duty;
}

float xenolith_quality_nominal_rate(const XenolithQuality *q) {
    return q ? q->nominal_rate : 0.0f;
}

void xenolith_quality_set_core_hold(XenolithQuality *q,int hold) {
    if (q) q->core_hold = hold ? 1u : 0u;
}

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
/* Public core/filter trampolines remain in XIP; use full-width indirect calls
 * from SRAM rather than the target's range-limited relative branch. */
static float (*volatile quality_core_entry)(XenolithCore*)=xenolith_sample;
static float (*volatile quality_filter_entry)(XenolithFilter*,float)=xenolith_filter_process_mono;
#endif
XQUALITY_INLINE float core_sample(XenolithQuality *q,XenolithCore *core) {
    if(!q->core_hold&&q->load_duty<1.0f&&(q->factor==2u||q->factor==4u)){
        if(q->crush_phase<1.0f){q->crush_phase+=q->load_duty;return q->raw_held;}
        q->crush_phase-=1.0f;q->crush_phase+=q->load_duty;
    }
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
    if (!q->core_hold) q->raw_held = quality_core_entry(core);
#else
    if (!q->core_hold) q->raw_held = xenolith_sample(core);
#endif
    return q->raw_held;
}

XQUALITY_INLINE float quality_filter_sample(XenolithFilter *filter,float raw){
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
    return quality_filter_entry(filter,raw);
#else
    return xenolith_filter_process_mono(filter,raw);
#endif
}

XQUALITY_RAM_AUDIO static float quality_process_audio(XenolithQuality *q, XenolithCore *core) {
    float result;
    uint32_t i;
    if (q->factor == 1u && !q->continuous) {
        q->previous_held = q->held;
        q->held = quantize_optional(q,core_sample(q,core),q->digital_bits);
        return q->held;
    }
    if (q->continuous || q->factor == XENOLITH_RATE_CRUSH) {
        if (q->crush_phase >= 1.0f) {
            q->crush_phase -= 1.0f;
            q->previous_held = q->held;
            q->held = quantize_optional(q,core_sample(q,core),q->crush.bits);
        }
        result = q->crush.reconstruction
            ? q->previous_held + (q->held-q->previous_held)*q->crush_phase : q->held;
        q->crush_phase += q->crush_step;
        return result;
    }
    if (q->factor >= XENOLITH_RATE_ECO2) {
        if (q->hold_remaining == 0u) {
            q->previous_held = q->held;
            q->held = quantize_optional(q,core_sample(q,core),q->digital_bits);
            q->hold_remaining = q->factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
        }
        --q->hold_remaining;
        return q->held;
    }
    for (i = 0; i < q->factor / 2u; ++i) {
        halfband_push(&q->stage[0], quantize_optional(q,core_sample(q,core),q->digital_bits));
        halfband_push(&q->stage[0], quantize_optional(q,core_sample(q,core),q->digital_bits));
        result = halfband_output(&q->stage[0]);
        if (q->factor == 4u) halfband_push(&q->stage[1], result);
    }
    if (q->factor == 4u) result = halfband_output(&q->stage[1]);
    if (q->warmup > 0u) --q->warmup;
    return result;
}

XQUALITY_RAM_AUDIO static float quality_process_filtered_audio(XenolithQuality *q, XenolithCore *core,XenolithFilter *filter) {
    float result;
    uint32_t i;
    if (q->factor == 1u && !q->continuous) {
        q->previous_held = q->held;
        q->held = quantize_optional(q,quality_filter_sample(filter,core_sample(q,core)),q->digital_bits);
        return q->held;
    }
    if (q->continuous || q->factor == XENOLITH_RATE_CRUSH) {
        if (q->crush_phase >= 1.0f) {
            q->crush_phase -= 1.0f;
            q->previous_held = q->held;
            q->held = quantize_optional(q,quality_filter_sample(filter,core_sample(q,core)),q->crush.bits);
        }
        result = q->crush.reconstruction
            ? q->previous_held + (q->held-q->previous_held)*q->crush_phase : q->held;
        q->crush_phase += q->crush_step;
        return result;
    }
    if (q->factor >= XENOLITH_RATE_ECO2) {
        if (q->hold_remaining == 0u) {
            q->previous_held = q->held;
            q->held = quantize_optional(q,quality_filter_sample(filter,core_sample(q,core)),q->digital_bits);
            q->hold_remaining = q->factor == XENOLITH_RATE_ECO2 ? 2u : 4u;
        }
        --q->hold_remaining;
        return q->held;
    }
    for (i = 0; i < q->factor / 2u; ++i) {
        halfband_push(&q->stage[0], quantize_optional(q,quality_filter_sample(filter,core_sample(q,core)),q->digital_bits));
        halfband_push(&q->stage[0], quantize_optional(q,quality_filter_sample(filter,core_sample(q,core)),q->digital_bits));
        result = halfband_output(&q->stage[0]);
        if (q->factor == 4u) halfband_push(&q->stage[1], result);
    }
    if (q->factor == 4u) result = halfband_output(&q->stage[1]);
    if (q->warmup > 0u) --q->warmup;
    return result;
}

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
static float (*volatile quality_process_entry)(XenolithQuality*,XenolithCore*)=quality_process_audio;
static float (*volatile quality_filtered_entry)(XenolithQuality*,XenolithCore*,XenolithFilter*)=quality_process_filtered_audio;
float xenolith_quality_process(XenolithQuality *q,XenolithCore *core){return quality_process_entry(q,core);}
float xenolith_quality_process_filtered(XenolithQuality *q,XenolithCore *core,XenolithFilter *filter){return quality_filtered_entry(q,core,filter);}
#else
float xenolith_quality_process(XenolithQuality *q,XenolithCore *core){return quality_process_audio(q,core);}
float xenolith_quality_process_filtered(XenolithQuality *q,XenolithCore *core,XenolithFilter *filter){return quality_process_filtered_audio(q,core,filter);}
#endif

uint32_t xenolith_quality_ready(const XenolithQuality *q) { return q->warmup == 0u; }

float xenolith_quality_rate(const XenolithQuality *q) {
    if(q->factor==2u||q->factor==4u)return q->high_actual_rate;
    if (q->continuous || q->factor == XENOLITH_RATE_CRUSH) return q->crush.rate;
    if (q->factor >= XENOLITH_RATE_ECO2)
        return q->output_rate / (q->factor == XENOLITH_RATE_ECO2 ? 2.0f : 4.0f);
    return q->output_rate * (float)q->factor;
}

float xenolith_quality_filter_rate(const XenolithQuality *q){
    if(q->factor==2u||q->factor==4u)return q->output_rate*(float)q->factor;
    return xenolith_quality_rate(q);
}
