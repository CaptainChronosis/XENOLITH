/* SPDX-License-Identifier: MPL-2.0
 * XX29 original polynomial coloration; generic circuit-inspired transfer,
 * not a valve model. See docs/AMPLIFIER_2026-11-01.md for source grounding. */
#include "xenolith_amplifier.h"
#include "fastmath.h"
#include "xenolith_amplifier_tables.inc"
#include "xenolith_folder_tables.inc"
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define XAMP_RAM_AUDIO __attribute__((section(".audio_text.xenolith_amplifier"),noinline))
#define XAMP_AUDIO_INLINE static inline __attribute__((always_inline))
#else
#define XAMP_RAM_AUDIO
#define XAMP_AUDIO_INLINE static inline
#endif
/* pi32v2 may outline ordinary static inline fastmath helpers. These helpers
 * must stay inside their caller across the XIP/RAM address gap. */
XAMP_AUDIO_INLINE float audio_absolute(float x){fm_bits_t b;b.f=x;b.u&=0x7fffffffu;return b.f;}
XAMP_AUDIO_INLINE float audio_minimum(float a,float b){return a<b?a:b;}
XAMP_AUDIO_INLINE float audio_maximum(float a,float b){return a>b?a:b;}
XAMP_AUDIO_INLINE float audio_clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
/* Loaded once while stopped; no XIP data fetches in the folder audio path. */
#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_FOLDER_CACHE)
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define FOLDER_STORAGE __attribute__((section(".pool"),aligned(32)))
#else
#define FOLDER_STORAGE __attribute__((aligned(32)))
#endif
static float folder_curve_ram[4][257] FOLDER_STORAGE;
/* Split the two banks across internal SRAM regions; preserve the platform's
 * 8KiB pool reserve rather than consume it for an immutable lookup cache. */
static float folder_integral_ram[4][257] __attribute__((aligned(32)));
static uint32_t folder_cache_ready;
#define FOLDER_CURVE folder_curve_ram
#define FOLDER_INTEGRAL folder_integral_ram
static void prepare_folder_cache(void){
    uint32_t i,j;if(folder_cache_ready)return;
    for(i=0;i<4u;i++)for(j=0;j<257u;j++){
        folder_curve_ram[i][j]=folder_curve[i][j];
        folder_integral_ram[i][j]=folder_integral[i][j];
    }
    folder_cache_ready=1u;
}
#else
#define FOLDER_CURVE folder_curve
#define FOLDER_INTEGRAL folder_integral
static void prepare_folder_cache(void){}
#endif
static int bounded(float x,float low,float high){return x>=low && x<=high;}
XAMP_AUDIO_INLINE float transfer(float x){
    x=audio_clamp(x,-1.5f,1.5f);
    return x-(4.0f/27.0f)*x*x*x;
}
XAMP_AUDIO_INLINE float table_transfer(const float *table,float x){
    float at,part;uint32_t i;
    if(x<=-1.5f)return table[0];if(x>=1.5f)return table[256];
    at=(x+1.5f)*(256.0f/3.0f);i=(uint32_t)at;
    if(i>=256u)return table[256];
    part=at-(float)i;return table[i]+part*(table[i+1u]-table[i]);
}
/* Independent broken-line shaping, informed by MIT Waveforms' functional
 * treatment of nonlinear characteristics, not a copied circuit model.
 * Period1 and threshold.25 simplify the general triangle reflection:
 * .25*(1-abs((4*x+1 modulo4)-2)). Bounded cast implements floor safely. */
XAMP_AUDIO_INLINE float fold_transfer(float x){
    float phase;int32_t whole;
    if(!(x>=-65.0f))x=-65.0f;if(x>65.0f)x=65.0f;
    phase=x+0.25f;whole=(int32_t)phase;
    if(phase<(float)whole)--whole;
    phase-=(float)whole;
    return 0.25f-audio_absolute(phase-0.5f);
}
/* A continuous coordinate permits rapid retargeting without dropping the
 * in-flight blend. Settled enum endpoints evaluate exactly one transfer. */
XAMP_RAM_AUDIO static float curve_transfer_audio(float curve,float x){
    float a,b;
    if(curve<=0.0f)return transfer(x);
    if(curve>=3.0f)return fold_transfer(x);
    if(curve==2.0f)return table_transfer(amp_softsign,x);
    if(curve>2.0f){
        a=table_transfer(amp_softsign,x);b=fold_transfer(x);curve-=2.0f;
        return a+curve*(b-a);
    }
    if(curve==1.0f)return table_transfer(amp_space32,x);
    if(curve<1.0f){a=transfer(x);b=table_transfer(amp_space32,x);}
    else{a=table_transfer(amp_space32,x);b=table_transfer(amp_softsign,x);curve-=1.0f;}
    return a+curve*(b-a);
}
/* The ordinary CUBIC endpoint is already an exact polynomial. Keep its
 * arithmetic in the audio owner instead of paying a RAM call for each wet
 * sample and LOAD quiescent offset; other curves retain their shared body. */
XAMP_AUDIO_INLINE float colour_transfer_audio(float curve,float x){
    return curve<=0.0f?transfer(x):curve_transfer_audio(curve,x);
}
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
/* Foreground preparation also uses this transfer; cross the XIP/RAM gap with
 * an indirect call. Audio helpers call the RAM body directly. */
static float (*volatile curve_transfer_entry)(float,float)=curve_transfer_audio;
static float curve_transfer(float curve,float x){return curve_transfer_entry(curve,x);}
#else
#define curve_transfer curve_transfer_audio
#endif
XAMP_AUDIO_INLINE float feedback_table(const float *table,float x,float scale){
    float at,part;uint32_t i;
    at=x*scale+63.5f;
    if(at<=0.0f)return table[0];if(at>=127.0f)return table[127];
    i=(uint32_t)at;
    if(i>=127u)return table[127];part=at-(float)i;
    return table[i]+part*(table[i+1u]-table[i]);
}
XAMP_AUDIO_INLINE float feedback_trifold(float x,float k){
    float phase,absolute,bound;int32_t whole;
    if(k==0.0f)return fold_transfer(x);
    x=audio_clamp(x,-97.0f,97.0f);phase=x+.5f;whole=(int32_t)phase;
    if(phase<(float)whole)--whole;phase=x-(float)whole;
    absolute=audio_absolute(phase);bound=.25f*(1.0f+k);
    if(absolute<=bound)return phase/(1.0f+k);
    return (phase<0.0f?-1.0f:1.0f)*(.5f-absolute)/(1.0f-k);
}
XAMP_RAM_AUDIO static float feedback_transfer(XenolithAmplifier *amp,float x){
    XenolithFeedback *s=&amp->feedback;float a,b,value,curve=amp->curve;
    if(!s->configured || (s->k==0.0f && !s->remaining))return curve_transfer_audio(curve,x);
    if(curve>=3.0f)return feedback_trifold(x,s->k);
    if(s->mix>=1.0f){
        value=s->to_zero?curve_transfer_audio(audio_minimum(curve,2.0f),x):feedback_table(s->target,x,s->scale);
    }else{
        a=s->from_zero?curve_transfer_audio(audio_minimum(curve,2.0f),x):feedback_table(s->current,x,s->scale);
        b=s->to_zero?curve_transfer_audio(audio_minimum(curve,2.0f),x):feedback_table(s->target,x,s->scale);
        value=a+s->mix*(b-a);
    }
    if(curve>2.0f)value+=(curve-2.0f)*(feedback_trifold(x,s->k)-value);
    return value;
}
int xenolith_feedback_prepare(XenolithFeedbackPrepared *out,float amount,uint32_t curve){
    uint32_t i,j,nonperiodic;float k;
    if(!out || !bounded(amount,0.0f,1.0f) || curve>XAMP_FOLD)return 0;
    nonperiodic=curve==XAMP_FOLD?XAMP_SOFTSIGN:curve;k=.75f*amount;
    for(i=0;i<128u;i++){
        /* At either clipped endpoint y=+/-1, hence x=+/-(1.5+k).
         * Placing the table endpoints there avoids interpolating across the
         * SPACE3/2 hard knee. Table scale and shape both ramp in audio. */
        float x=(1.5f+k)*(2.0f*(float)i/127.0f-1.0f),lo=-1.0f,hi=1.0f;
        /* f' >= 0 here. The periodic branch is handled analytically in audio.
         * Fixed iterations and no dynamic allocation in foreground prepare. */
        for(j=0;j<24u;j++){
            float y=.5f*(lo+hi),residual=y-curve_transfer((float)nonperiodic,x-k*y);
            if(residual>0.0f)hi=y;else lo=y;
        }
        out->table[i]=.5f*(lo+hi);
    }
    out->amount=amount;out->curve=curve;return 1;
}
void xenolith_amplifier_apply_feedback(XenolithAmplifier *amp,const XenolithFeedbackPrepared *p){
    XenolithFeedback *s;float inverse,scale;uint32_t i,n;
    if(!amp || !p)return;s=&amp->feedback;
    scale=127.0f/(3.0f+1.5f*p->amount);
    if(s->configured && s->amount_target==p->amount && s->curve_target==p->curve)return;
    if(!s->configured){
        for(i=0;i<128u;i++)s->current[i]=s->target[i]=p->table[i];
        s->mix=1.0f;s->k=s->k_target=.75f*p->amount;s->remaining=0u;
        s->scale=s->scale_target=scale;
        s->from_zero=s->to_zero=p->amount==0.0f;s->configured=1u;
    }else if(s->k==0.0f && !s->remaining && p->amount==0.0f){
        s->from_zero=s->to_zero=1u;s->mix=1.0f;
        s->scale=s->scale_target=scale;
    }else{
        inverse=1.0f/s->scale;
        for(i=0;i<128u;i++){
            float x=((float)i-63.5f)*inverse;
            float a=s->from_zero?curve_transfer(audio_minimum(amp->curve,2.0f),x):s->current[i];
            float b=s->to_zero?curve_transfer(audio_minimum(amp->curve,2.0f),x):s->target[i];
            s->current[i]=a+s->mix*(b-a);s->target[i]=p->table[i];
        }
        n=amp->target.ramp_samples?amp->target.ramp_samples:221u;inverse=1.0f/(float)n;
        s->remaining=n;s->mix=0.0f;s->mix_step=inverse;
        s->k_target=.75f*p->amount;s->k_step=(s->k_target-s->k)*inverse;
        s->scale_target=scale;s->scale_step=(scale-s->scale)*inverse;
        s->from_zero=0u;s->to_zero=p->amount==0.0f;
    }
    s->amount_target=p->amount;s->curve_target=p->curve;
}
XAMP_AUDIO_INLINE void advance_feedback(XenolithFeedback *s){
    if(s->remaining){
        if(!--s->remaining){s->mix=1.0f;s->k=s->k_target;s->scale=s->scale_target;}
        else{s->mix=audio_minimum(1.0f,s->mix+s->mix_step);s->k=audio_clamp(s->k+s->k_step,0.0f,.75f);s->scale+=s->scale_step;}
    }
}
void xenolith_amplifier_defaults(XenolithAmplifierConfig *c){
    if(c){c->drive=0.0f;c->bias=0.0f;c->colour=0.0f;}
}
int xenolith_amplifier_config_valid(const XenolithAmplifierConfig *c){
    return c && bounded(c->drive,0.0f,1.0f) && bounded(c->bias,-0.75f,0.75f)
        && bounded(c->colour,0.0f,1.0f);
}
void xenolith_amplifier_shape_defaults(XenolithAmplifierShapeConfig *c){
    if(c){c->curve=XAMP_CUBIC;c->highpass_hz=10.0f;c->lowpass_hz=0.0f;c->gain_db=0.0f;}
}
int xenolith_amplifier_shape_valid(const XenolithAmplifierShapeConfig *c){
    return c && c->curve<=XAMP_FOLD && bounded(c->highpass_hz,5.0f,200.0f)
        && (c->lowpass_hz==0.0f || bounded(c->lowpass_hz,100.0f,20000.0f))
        && bounded(c->gain_db,-18.0f,6.0f);
}
int xenolith_amplifier_prepare(XenolithAmplifierPrepared *out,
                              const XenolithAmplifierConfig *c,float rate){
    XenolithAmplifierShapeConfig shape;xenolith_amplifier_shape_defaults(&shape);
    return xenolith_amplifier_prepare_extended(out,c,&shape,rate);
}
int xenolith_amplifier_prepare_extended(XenolithAmplifierPrepared *out,
                              const XenolithAmplifierConfig *c,
                              const XenolithAmplifierShapeConfig *shape,float rate){
    XenolithAmplifierPrepared p;float duration;
    if(!out || !xenolith_amplifier_config_valid(c) || !xenolith_amplifier_shape_valid(shape)
       || !bounded(rate,1000.0f,192000.0f))return 0;
    p.gain=1.0f+7.0f*c->drive;p.bias=c->bias;p.colour=c->colour;p.curve=shape->curve;
    p.bias_offset=curve_transfer((float)p.curve,c->bias);
    p.dc_pole=fm_expf(-FM_TWO_PI*audio_minimum(shape->highpass_hz,rate*0.45f)/rate);
    p.dc_scale=0.5f*(1.0f+p.dc_pole);
    p.tone_mix=shape->lowpass_hz==0.0f?0.0f:1.0f;
    p.tone_pole=p.tone_mix?fm_expf(-FM_TWO_PI*audio_minimum(shape->lowpass_hz,rate*0.45f)/rate):0.0f;
    p.output_gain=shape->gain_db==0.0f?1.0f:fm_expf(shape->gain_db*0.1151292546497022842f);
    duration=0.005f*rate;p.ramp_samples=(uint32_t)duration;
    if((float)p.ramp_samples<duration)p.ramp_samples++;
    *out=p;return 1;
}
void xenolith_amplifier_init(XenolithAmplifier *s){
    uint32_t i;uint8_t *p=(uint8_t*)s;if(!s)return;
    prepare_folder_cache();
    for(i=0;i<sizeof *s;i++)p[i]=0;
    s->gain=s->target.gain=s->output_gain=s->target.output_gain=1.0f;
    xenolith_folder_defaults(&s->folder.target.config);
    s->folder.mix=1.0f;s->folder.stage=1.0f;
}
XAMP_AUDIO_INLINE void clear_colour(XenolithAmplifier *s){s->previous_wet=0.0f;s->dc_state=0.0f;s->tone_state=0.0f;s->load_bias_envelope=0.0f;s->load_supply=0.0f;}
XAMP_AUDIO_INLINE void clear_history(XenolithAmplifier *s){
    clear_colour(s);s->folder.previous_input=0.0f;s->folder.previous_u=0.0f;
    s->folder.previous_wet=0.0f;s->folder.dc_state=0.0f;s->folder.ready=0u;
    s->load_bias_envelope=0.0f;
}
void xenolith_amplifier_clear(XenolithAmplifier *s){if(s)clear_history(s);}
static void prepare_load_bias(XenolithAmplifier *s){
    float target=.22f*s->load_bias_control*s->load_bias_stress;uint32_t n;
    if(target==s->load_bias_target)return;
    n=s->target.ramp_samples?s->target.ramp_samples:221u;
    s->load_bias_target=target;s->load_bias_remaining=n;
    s->load_bias_step=(target-s->load_bias)/(float)n;
    s->load_bias_attack=1.0f/(float)n;s->load_bias_release=.1f/(float)n;
    s->load_supply_step=1.0f/(16.0f*(float)n);
}
XAMP_AUDIO_INLINE int load_colour_active(const XenolithAmplifier *s){
    return s->load_bias!=0.0f || s->load_bias_target!=0.0f || s->load_bias_remaining!=0u || s->load_supply!=0.0f;
}
int xenolith_amplifier_load_active(const XenolithAmplifier *s){return s && load_colour_active(s);}
void xenolith_amplifier_set_load_bias(XenolithAmplifier *s,float bias){
    if(!s||!(bias>=-1.0f&&bias<=1.0f)||bias==s->load_bias_control)return;
    s->load_bias_control=bias;prepare_load_bias(s);
}
void xenolith_amplifier_set_load_sag(XenolithAmplifier *s,float stress){
    fm_bits_t bits;
    if(!s)return;bits.f=stress;
    if((bits.u&UINT32_C(0x7f800000))==UINT32_C(0x7f800000))return;
    /* Skip bias preparation when both its control and history are zero. */
    if(s->load_bias_control!=0.0f||s->load_bias!=0.0f||s->load_bias_remaining){
        s->load_bias_stress=audio_clamp(stress,0.0f,1.0f);prepare_load_bias(s);
    }else s->load_bias_stress=audio_clamp(stress,0.0f,1.0f);
}
int xenolith_amplifier_set_fold_offset(XenolithAmplifier *s,float offset){
    fm_bits_t bits;float inverse;
    if(!s)return 0;bits.f=offset;
    if((bits.u&UINT32_C(0x7f800000))==UINT32_C(0x7f800000))return 0;
    offset=audio_clamp(offset,-1.0f,1.0f);
    if(offset!=s->folder.offset_target){
        s->folder.offset_target=offset;
        s->folder.offset_remaining=s->folder.target.ramp_samples?s->folder.target.ramp_samples:221u;
        s->folder.offset_step=(offset-s->folder.offset)/(float)s->folder.offset_remaining;
    }
    if(s->folder.configured)return 1;
    if(offset==s->fold_target)return 1;
    s->fold_target=offset;s->fold_remaining=s->target.ramp_samples?s->target.ramp_samples:221u;
    inverse=1.0f/(float)s->fold_remaining;s->fold_step=(offset-s->fold_offset)*inverse;return 1;
}
void xenolith_amplifier_apply(XenolithAmplifier *s,const XenolithAmplifierPrepared *p){
    float inverse;
    if(!s || !p)return;
    if(s->target.gain==p->gain && s->target.bias==p->bias && s->target.colour==p->colour
       && s->target.dc_pole==p->dc_pole && s->target.ramp_samples==p->ramp_samples
       && s->target.curve==p->curve && s->target.tone_pole==p->tone_pole
       && s->target.tone_mix==p->tone_mix && s->target.output_gain==p->output_gain)return;
    s->target=*p;
    s->load_supply_step=1.0f/(16.0f*(float)(p->ramp_samples?p->ramp_samples:221u));
    if(!s->coefficients_ready){s->dc_pole=p->dc_pole;s->dc_scale=p->dc_scale;s->coefficients_ready=1u;}
    if(s->colour==0.0f && p->colour==0.0f && !load_colour_active(s)){
        s->gain=p->gain;s->bias=p->bias;s->bias_offset=p->bias_offset;s->remaining=0u;
        s->curve=(float)p->curve;s->dc_pole=p->dc_pole;s->dc_scale=p->dc_scale;
        s->tone_pole=p->tone_pole;s->tone_mix=p->tone_mix;s->output_gain=p->output_gain;
        clear_colour(s);return;
    }
    s->remaining=p->ramp_samples;inverse=1.0f/(float)s->remaining;
    s->gain_step=(p->gain-s->gain)*inverse;s->bias_step=(p->bias-s->bias)*inverse;
    s->colour_step=(p->colour-s->colour)*inverse;
    s->curve_step=((float)p->curve-s->curve)*inverse;
    s->dc_pole_step=(p->dc_pole-s->dc_pole)*inverse;s->dc_scale_step=(p->dc_scale-s->dc_scale)*inverse;
    s->tone_pole_step=(p->tone_pole-s->tone_pole)*inverse;s->tone_mix_step=(p->tone_mix-s->tone_mix)*inverse;
    s->output_gain_step=(p->output_gain-s->output_gain)*inverse;
}
XAMP_AUDIO_INLINE float process_colour(XenolithAmplifier *s,float input){
    float wet,dc,bias,offset,colour,headroom=1.0f,gain;fm_bits_t bits;
    /* The control ramp runs even while the user COLOUR control is zero.
     * Its tiny automatic wet fraction then enters and leaves continuously. */
    if(s->load_bias_remaining){
        if(!--s->load_bias_remaining){
            s->load_bias=s->load_bias_target;
            if(s->load_bias==0.0f && s->load_supply==0.0f && s->colour==0.0f && !s->remaining)clear_colour(s);
        }
        else s->load_bias=audio_clamp(s->load_bias+s->load_bias_step,-.22f,.22f);
    }
    if(s->colour==0.0f && !s->remaining && s->load_bias==0.0f && s->load_supply==0.0f){
        if(s->fold_remaining){s->fold_offset=s->fold_target;s->fold_remaining=0u;}
        return input;
    }
    if(s->fold_remaining){
        --s->fold_remaining;
        if(!s->fold_remaining)s->fold_offset=s->fold_target;
        else s->fold_offset=audio_clamp(s->fold_offset+s->fold_step,-1.0f,1.0f);
    }
    if(s->remaining){
        s->remaining--;
        if(!s->remaining){
            s->gain=s->target.gain;s->bias=s->target.bias;s->colour=s->target.colour;
            s->bias_offset=s->target.bias_offset;
            s->curve=(float)s->target.curve;s->dc_pole=s->target.dc_pole;s->dc_scale=s->target.dc_scale;
            s->tone_pole=s->target.tone_pole;s->tone_mix=s->target.tone_mix;s->output_gain=s->target.output_gain;
            if(s->colour==0.0f && s->load_bias==0.0f && s->load_supply==0.0f){clear_colour(s);return input;}
        }else{
            s->gain+=s->gain_step;s->bias+=s->bias_step;s->colour+=s->colour_step;
            s->gain=audio_clamp(s->gain,1.0f,8.0f);s->bias=audio_clamp(s->bias,-0.75f,0.75f);
            s->colour=audio_clamp(s->colour,0.0f,1.0f);
            s->curve=audio_clamp(s->curve+s->curve_step,0.0f,3.0f);
            s->dc_pole+=s->dc_pole_step;s->dc_scale+=s->dc_scale_step;
            s->tone_pole+=s->tone_pole_step;s->tone_mix=audio_clamp(s->tone_mix+s->tone_mix_step,0.0f,1.0f);
            s->output_gain+=s->output_gain_step;s->bias_offset=colour_transfer_audio(s->curve,s->bias);
        }
    }
    bits.f=input;
    if((bits.u&UINT32_C(0x7f800000))==UINT32_C(0x7f800000)){
        clear_history(s);return 0.0f;
    }
    input=audio_clamp(input,-8.0f,8.0f);
    gain=s->gain;
    if(s->load_bias!=0.0f || s->load_supply!=0.0f){
        /* Averaged finite-source/reservoir KCL, informed by Radiotron4e
         * pp547-548. Audio demand is a bounded proxy, not measured current.
         * rho<=.5 bounds depletion to1/3; no ripple oscillator is injected. */
        float demand=(.5f/.22f)*audio_absolute(s->load_bias)*
            audio_minimum(audio_absolute(gain*input),1.0f);
        s->load_supply+=s->load_supply_step*(demand-(1.0f+demand)*s->load_supply);
        s->load_supply=audio_clamp(s->load_supply,0.0f,1.0f/3.0f);
        if(s->load_bias==0.0f && s->load_supply<.0000001f)s->load_supply=0.0f;
        if(s->load_supply==0.0f && s->load_bias==0.0f && s->colour==0.0f && !s->remaining){clear_colour(s);return input;}
        headroom=1.0f-s->load_supply;gain/=headroom;
    }
    bias=s->bias;offset=s->bias_offset;
    if(s->load_bias!=0.0f){
        float magnitude=audio_minimum(audio_absolute(input),1.0f);
        s->load_bias_envelope+=(magnitude-s->load_bias_envelope)*
            (magnitude>s->load_bias_envelope?s->load_bias_attack:s->load_bias_release);
        bias+=s->load_bias*s->load_bias_envelope;
        offset=colour_transfer_audio(s->curve,bias);
    }
    if(s->feedback.configured && (s->feedback.k!=0.0f || s->feedback.remaining)){
        /* Deliberate partial input makeup, not a cathode-resistor law.
         * Leave BIAS and its quiescent subtraction unchanged. k<=.75 bounds
         * makeup to1.4875; the periodic branch covers the resulting+/-95.95. */
        float makeup=1.0f+.65f*s->feedback.k;
        wet=feedback_transfer(s,gain*input*makeup+bias)-feedback_transfer(s,bias);
    }else if(s->curve>2.0f && s->fold_offset!=0.0f){
        float fold_gain=audio_clamp(s->gain+7.0f*s->fold_offset,1.0f,8.0f)/headroom;
        float folded=fold_transfer(fold_gain*input+bias);
        if(s->curve>=3.0f)wet=folded;
        else{
            float original=table_transfer(amp_softsign,gain*input+bias);
            wet=original+(s->curve-2.0f)*(folded-original);
        }
        wet-=offset;
    }else wet=colour_transfer_audio(s->curve,gain*input+bias)-offset;
    wet*=headroom;
    dc=wet-s->previous_wet+s->dc_pole*s->dc_state;
    s->previous_wet=wet;s->dc_state=dc;
    wet=s->dc_scale*dc;
    if(s->tone_mix>0.0f){
        s->tone_state=wet+s->tone_pole*(s->tone_state-wet);
        wet+=s->tone_mix*(s->tone_state-wet);
    }
    /* Bounded load coupling, not an inferred valve supply law. With the
     * default LOAD bias -.15 the extra colour never exceeds .0525. */
    colour=audio_maximum(s->colour,(.35f/.22f)*audio_absolute(s->load_bias));
    colour=audio_maximum(colour,.70f*s->load_supply);
    return input+colour*(wet*s->output_gain-input);
}

void xenolith_folder_defaults(XenolithFolderConfig *c){
    if(c){c->amount=0.0f;c->bias=0.0f;c->mix=1.0f;c->stages=1u;}
}
int xenolith_folder_config_valid(const XenolithFolderConfig *c){
    return c && bounded(c->amount,0.0f,1.0f) && bounded(c->bias,-1.0f,1.0f)
        && bounded(c->mix,0.0f,1.0f) && c->stages<4u;
}
int xenolith_folder_prepare(XenolithFolderPrepared *out,const XenolithFolderConfig *c,float rate){
    XenolithFolderPrepared p;float duration;
    if(!out || !xenolith_folder_config_valid(c) || !bounded(rate,1000.0f,192000.0f))return 0;
    p.config=*c;p.dc_pole=fm_expf(-FM_TWO_PI*10.0f/rate);p.dc_scale=.5f*(1.0f+p.dc_pole);
    duration=.005f*rate;p.ramp_samples=(uint32_t)duration;
    if((float)p.ramp_samples<duration)p.ramp_samples++;
    *out=p;return 1;
}
void xenolith_amplifier_apply_folder(XenolithAmplifier *amp,const XenolithFolderPrepared *p){
    XenolithFolder *s;float inverse;
    if(!amp || !p)return;s=&amp->folder;s->configured=1u;
    /* Once the dedicated stage is configured FOLD belongs to it exclusively.
     * Old static curves remain unchanged; do not modulate two stages at once. */
    amp->fold_offset=amp->fold_target=amp->fold_step=0.0f;amp->fold_remaining=0u;
    if(s->target.config.amount==p->config.amount && s->target.config.bias==p->config.bias
       && s->target.config.mix==p->config.mix && s->target.config.stages==p->config.stages
       && s->target.dc_pole==p->dc_pole && s->target.ramp_samples==p->ramp_samples)return;
    s->target=*p;s->remaining=p->ramp_samples;inverse=1.0f/(float)s->remaining;
    s->amount_step=(p->config.amount-s->amount)*inverse;
    s->bias_step=(p->config.bias-s->bias)*inverse;s->mix_step=(p->config.mix-s->mix)*inverse;
    s->stage_step=((float)p->config.stages-s->stage)*inverse;
}
/* The antiderivative is integrated from the SAME piecewise-linear curve,
 * rather than independently interpolated. Odd curve, even primitive. */
XAMP_AUDIO_INLINE void folder_lookup(uint32_t stage,float x,float *value,float *integral){
    float a=audio_absolute(x),at,part,f0,f1;uint32_t i;
    at=a*(256.0f/6.0f);i=(uint32_t)at;
    if(i>=256u){i=255u;part=1.0f;}else part=at-(float)i;
    f0=FOLDER_CURVE[stage][i];f1=FOLDER_CURVE[stage][i+1u];
    *value=f0+part*(f1-f0);if(x<0.0f)*value=-*value;
    *integral=FOLDER_INTEGRAL[stage][i]+(6.0f/256.0f)*part*(f0+.5f*part*(f1-f0));
}
#if defined(XENOLITH_FOLDER_TEST_REFERENCE)
static float folder_adaa(uint32_t stage,float x,float previous){
    float f0,f1,a0,a1,delta=x-previous;
    /* Midpoint fallback avoids cancellation at slow motion. It is exact when
     * both samples share a linear table segment; crossing a knot is approximate. */
    folder_lookup(stage,x,&f1,&a1);folder_lookup(stage,previous,&f0,&a0);
    if(audio_absolute(delta)<.01f){
        float unused;folder_lookup(stage,.5f*(x+previous),&f1,&unused);return f1;
    }
    return (a1-a0)/delta;
}
#endif

/* Cache only primitives actually evaluated on the immediately preceding
 * sample. Small-delta midpoint evaluation needs no endpoint table reads.
 * A changing stage invalidates missing families through the per-frame mask. */
XAMP_RAM_AUDIO static float folder_adaa_cached(XenolithFolder *s,uint32_t stage,float x,float previous,uint32_t *next_mask){
    float f0,f1,a0,a1,delta=x-previous;
    if(audio_absolute(delta)<.01f){folder_lookup(stage,.5f*(x+previous),&f1,&a1);return f1;}
    folder_lookup(stage,x,&f1,&a1);
    if(s->primitive_mask&(1u<<stage))a0=s->previous_integral[stage];
    else folder_lookup(stage,previous,&f0,&a0);
    s->previous_integral[stage]=a1;*next_mask|=1u<<stage;
    return (a1-a0)/delta;
}
XAMP_AUDIO_INLINE float folder_curve_only(uint32_t stage,float x){
    float at=audio_absolute(x)*(256.0f/6.0f),part,f0,f1;uint32_t i=(uint32_t)at;
    if(i>=256u){i=255u;part=1.0f;}else part=at-(float)i;
    f0=FOLDER_CURVE[stage][i];f1=FOLDER_CURVE[stage][i+1u];
    f0+=part*(f1-f0);return x<0.0f?-f0:f0;
}
XAMP_AUDIO_INLINE float folder_bias_cached(XenolithFolder *s,uint32_t stage){
    if(s->cached_bias!=s->bias){s->bias_mask=0;s->cached_bias=s->bias;}
    if(!(s->bias_mask&(1u<<stage))){s->bias_value[stage]=folder_curve_only(stage,s->bias);s->bias_mask|=1u<<stage;}
    return s->bias_value[stage];
}
XAMP_AUDIO_INLINE float process_folder(XenolithFolder *s,float input){
    static const float family_level[4]={.25f,.5f,1.0f,1.0f};
    float amount,u,wet,dc,dry,weight,frac,offset,lift,balance;uint32_t stage,next_mask=0u;fm_bits_t bits;
    if(!s->configured)return input;
    if(s->remaining){
        if(!--s->remaining){s->amount=s->target.config.amount;s->bias=s->target.config.bias;
            s->mix=s->target.config.mix;s->stage=(float)s->target.config.stages;}
        else{s->amount=audio_clamp(s->amount+s->amount_step,0.0f,1.0f);
            s->bias=audio_clamp(s->bias+s->bias_step,-1.0f,1.0f);
            s->mix=audio_clamp(s->mix+s->mix_step,0.0f,1.0f);
            s->stage=audio_clamp(s->stage+s->stage_step,0.0f,3.0f);}
    }
    if(s->offset_remaining){
        if(!--s->offset_remaining)s->offset=s->offset_target;
        else s->offset=audio_clamp(s->offset+s->offset_step,-1.0f,1.0f);
    }
    amount=audio_clamp(s->amount+s->offset,0.0f,1.0f);
    if(amount==0.0f || s->mix==0.0f){
        s->ready=0u;s->previous_wet=0.0f;s->dc_state=0.0f;return input;
    }
    bits.f=input;
    if((bits.u&UINT32_C(0x7f800000))==UINT32_C(0x7f800000)){
        s->ready=0u;s->previous_wet=0.0f;s->dc_state=0.0f;return 0.0f;
    }
    input=audio_clamp(input,-8.0f,8.0f);
    /* LOAD colours the preceding VCA; it does not move the fold lobes. */
    u=audio_clamp((1.0f+31.0f*amount)*input+s->bias,-6.0f,6.0f);
    if(!s->ready){s->previous_u=u;s->previous_input=input;s->ready=1u;s->primitive_mask=0u;}
    /* The one/two-cell families were markedly quieter at low FOLD, but
     * already louder than six cells at high drive. Add at most6.02/3.52dB
     * below25%, smoothly removing it by50%. This is bounded level trim,
     * not signal-dependent normalization; hot inputs still need headroom. */
    stage=(uint32_t)s->stage;frac=s->stage-(float)stage;
    balance=0.0f;
    if(stage<2u&&amount<.5f){
        balance=audio_minimum((.5f-amount)*4.0f,1.0f);
        balance=balance*balance*(3.0f-2.0f*balance);
    }
    wet=folder_adaa_cached(s,stage,u,s->previous_u,&next_mask);
    offset=folder_bias_cached(s,stage);wet=(wet-offset)*(family_level[stage]+(stage<2u?.25f*balance:0.0f));
    if(frac>0.0f && stage<3u){
        float next=folder_adaa_cached(s,stage+1u,u,s->previous_u,&next_mask),next_offset;
        next_offset=folder_bias_cached(s,stage+1u);
        next=(next-next_offset)*(family_level[stage+1u]+(stage+1u<2u?.25f*balance:0.0f));wet+=frac*(next-wet);
    }
    dc=wet-s->previous_wet+s->target.dc_pole*s->dc_state;
    s->previous_wet=wet;s->dc_state=dc;
    /* Measured high-drive trim: retain the first half of the sweep and both
     * low-stage families. Four/six stages gain at most3.52/6.02dB above it.
     * This is a fixed bounded trim, not a signal-dependent loudness servo. */
    lift=audio_maximum(2.0f*amount-1.0f,0.0f);lift=lift*lift*(3.0f-2.0f*lift);
    wet=s->target.dc_scale*dc*(1.0f-.75f*amount)
        *(1.0f+.5f*audio_maximum(s->stage-1.0f,0.0f)*lift);
    dry=.5f*(input+s->previous_input);s->previous_input=input;s->previous_u=u;s->primitive_mask=next_mask;
    weight=s->mix*audio_minimum(16.0f*amount,1.0f);
    /* Crossfade the half-sample dry alignment with activation, retaining exact
     * zero-amount bypass rather than permanently delaying the whole synth. */
    return input+weight*(wet-input)+weight*(1.0f-s->mix)*(dry-input);
}
XAMP_RAM_AUDIO static float amplifier_process_audio(XenolithAmplifier *s,float input){
    advance_feedback(&s->feedback);
    return process_folder(&s->folder,process_colour(s,input));
}
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
static float (*volatile amplifier_process_entry)(XenolithAmplifier*,float)=amplifier_process_audio;
float xenolith_amplifier_process(XenolithAmplifier *s,float input){return amplifier_process_entry(s,input);}
#else
float xenolith_amplifier_process(XenolithAmplifier *s,float input){return amplifier_process_audio(s,input);}
#endif
