/* SPDX-License-Identifier: GPL-2.0-or-later
 * XX29 Xenolith I dry-core probe, 2026.
 * The LFNoise2 state recurrence and constructor behavior below are adapted from
 * SuperCollider 3.14.1, server/plugins/NoiseUGens.cpp, Copyright (c) 2002
 * James McCartney. GNU GPL version 2 or (at your option) any later version.
 * https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/NoiseUGens.cpp
 * Distributed WITHOUT ANY WARRANTY; see the accompanying GPL license text.
 *
 * The FM equations implement the operator's DSP_v1.46 Xenolith source. Sine
 * lookup and phase accumulation are independently written from the documented
 * SinOsc model. The sine lookup uses a2048-point full-period table. These use
 * float arithmetic and are not a claim of SC bit identity or a fixed-point port.
 *
 * Default reference boundary: original eight mono cells, no Ndef fades,
 * no X_BUS gain/CombL/LPF/GVerb/Limiter. Random draws use xorshift32 rather than
 * SC's per-synth Taus88 state; drift interpolation matches, seed streams do not.
 * The optional 1..64-cell extension retains that default signal path. Extra
 * cells have their own shared RNG stream. PARALLEL retains counts1..16;
 *17..32 thicken to0.125octaves, then a cubic opens to6octaves at64.
 * Legacy non-PARALLEL graphs retain compact tuning. Mixed ADD graphs retain
 * the tuning family of each cell's birth kind; later edges do not retune it.
 * sqrt(8/N) gain remains an incoherent-power reference, not equal loudness.
 */
#include "xenolith_core.h"
#include "xenolith_sine_table.h"

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define XENOLITH_RAM_AUDIO __attribute__((section(".audio_text.xenolith_core"),noinline))
#define XENOLITH_AUDIO_INLINE static inline __attribute__((always_inline))
#else
#define XENOLITH_RAM_AUDIO
#define XENOLITH_AUDIO_INLINE static inline
#endif

/* The device's XIP instruction/data traffic shares its flash/cache machinery.
 * Keep the existing2048float values bit-for-bit, but read them from the same
 * NOLOAD .pool RAM region as PLAT_POOL after the first stopped initialization.
 * Ordinary portable host builds retain the immutable table with no extra RAM.
 * The test define exercises this exact cache path without target dependencies. */
#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_SINE_CACHE)
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
#define XENOLITH_SINE_STORAGE __attribute__((section(".pool"),aligned(32)))
#else
#define XENOLITH_SINE_STORAGE __attribute__((aligned(32)))
#endif
static float xenolith_sine_ram[2048] XENOLITH_SINE_STORAGE;
static float xenolith_sine_delta_ram[2048] XENOLITH_SINE_STORAGE;
static uint32_t sine_cache_ready;
#define CORE_SINE xenolith_sine_ram
static void prepare_sine_cache(void) {
    uint32_t i;if(sine_cache_ready)return;
    for(i=0;i<2048u;i++){
        xenolith_sine_ram[i]=xenolith_sine[i];
        xenolith_sine_delta_ram[i]=xenolith_sine[(i+1u)&2047u]-xenolith_sine[i];
    }
    sine_cache_ready=1u;
}
#else
#define CORE_SINE xenolith_sine
static void prepare_sine_cache(void) {}
#endif

#define PHASE_MASK UINT32_C(0x1fffffff)
#define PHASE_PER_RADIAN 85445659.44705395f

/* Generated as float32(2^(i/64)), i=0..64. Fractional-octave interpolation
 * has <=0.026cent error. The control contract bounds the input to+/-14.95,
 * so exact IEEE754 powers of two are all normal and their exponents fit.
 * A repeated upper endpoint handles float rounding of(negative tiny)-(-1)
 * to1 exactly, without an out-of-bounds interpolation fetch or DSP clamp.
 * No clipping, division, libm call, allocation or variable-length loop here. */
static const float exponential_fraction[66]={
    1.0f,1.01088929f,1.0218972f,1.03302491f,1.04427373f,1.05564523f,1.06714046f,1.07876074f,
    1.09050775f,1.10238254f,1.1143868f,1.12652159f,1.13878858f,1.15118921f,1.1637249f,1.17639697f,
    1.18920708f,1.20215678f,1.21524739f,1.22848058f,1.24185777f,1.25538075f,1.26905096f,1.28287005f,
    1.29683959f,1.31096125f,1.32523668f,1.33966756f,1.35425556f,1.36900246f,1.38390994f,1.39897966f,
    1.41421354f,1.42961335f,1.44518077f,1.46091783f,1.47682619f,1.49290776f,1.50916445f,1.52559817f,
    1.54221082f,1.55900443f,1.5759809f,1.59314215f,1.61049032f,1.62802744f,1.64575553f,1.66367662f,
    1.68179286f,1.70010638f,1.71861935f,1.73733389f,1.75625217f,1.77537644f,1.79470909f,1.81425214f,
    1.8340081f,1.85397911f,1.87416768f,1.89457595f,1.91520655f,1.93606174f,1.95714414f,1.97845602f,
    2.0f,2.0f
};
XENOLITH_AUDIO_INLINE float exponential_ratio(float octaves) {
    int whole=(int)octaves;
    float position,fraction,value;unsigned index;
    union {uint32_t bits;float value;} scale;
    if(octaves<(float)whole)--whole;
    position=(octaves-(float)whole)*64.0f;index=(unsigned)position;
    fraction=position-(float)index;
    value=exponential_fraction[index]+(exponential_fraction[index+1u]-exponential_fraction[index])*fraction;
    scale.bits=(uint32_t)(127+whole)<<23;
    return value*scale.value;
}
/* Legacy non-PARALLEL arrays use compact tuning, including SELF. In ADD,
 * PARALLEL births retain swarm dispersion while linked births stay compact. */
XENOLITH_AUDIO_INLINE int linked_growth(uint32_t growth) {return growth!=XTOPO_PARALLEL;}
XENOLITH_AUDIO_INLINE uint32_t core_growth(const XenolithCore *s) {return s->graph.active_growth;}
XENOLITH_AUDIO_INLINE int linked_cell(const XenolithCore *s,uint32_t i) {
    return s->graph_config.scope==XGRAPH_ADD?s->graph_config.kind[i]!=XTOPO_PARALLEL:linked_growth(s->topology.growth);
}
static void prepare_flavour(XenolithCore *s,uint32_t extent) {
    uint32_t i,choice=s->flavour.flavour[s->topology.mode];
    s->flavour_active=XFLAVOUR_NORMAL;
    s->flavour_tap_loss=1.0f-1.0f/(1.0f+.5f*s->topology.link);
    for(i=0;i<XENOLITH_MAX_CELLS;i++)s->flavour_series_child[i]=XGRAPH_NONE;
    if(choice==XFLAVOUR_RETURN_NEGATIVE||choice==XFLAVOUR_RETURN_ALTERNATING) {
        if(s->graph.feedback_count&&s->topology.feedback>0.0f)s->flavour_active=choice;
    } else if(choice==XFLAVOUR_CASCADE) {
        for(i=1u;i<XENOLITH_MAX_CELLS;i++) {
            uint32_t parent=s->graph.parent[i];
            uint32_t series=s->graph_config.scope==XGRAPH_ADD?
                s->graph_config.kind[i]==XTOPO_CHAIN4:
                ((s->topology.growth==XTOPO_CHAIN4&&(i&3u))||
                 (s->topology.growth==XTOPO_LOOP4&&((i&3u)==1u||(i&3u)==2u)));
            if(series&&parent<i&&s->flavour_series_child[parent]==XGRAPH_NONE) {
                s->flavour_series_child[parent]=(uint8_t)i;
                if(i<extent&&s->flavour_tap_loss>0.0f)s->flavour_active=choice;
            }
        }
    }
}
static void classify_graph_xip(XenolithCore *s,uint32_t extent) {
    xenolith_graph_classify(&s->graph_config,s->topology.growth,extent,&s->graph);
    prepare_flavour(s,extent);
}
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
/* Runs only on topology/count transactions and once when a suffix retires.
 * Keep its64-entry scan out of the per-sample RAM kernels. The indirect call
 * crosses the pi32v2 RAM/XIP jump range without changing the RAM partition. */
static void (*volatile graph_classifier)(XenolithCore *,uint32_t)=classify_graph_xip;
XENOLITH_AUDIO_INLINE void classify_graph(XenolithCore *s,uint32_t extent) {graph_classifier(s,extent);}
#else
static void classify_graph(XenolithCore *s,uint32_t extent) {classify_graph_xip(s,extent);}
#endif

int xenolith_set_flavour(XenolithCore *s,const XenolithFlavourConfig *c) {
    if(!s||!xenolith_flavour_valid(c))return 0;
    s->flavour=*c;
    prepare_flavour(s,s->count_smoothing?s->render_cells:s->cells);
    return 1;
}

/* Original half-wave-symmetric phase-address warp. A depth-zero request must
 * bypass float phase conversion so even low phase bits retain exact identity.
 * Both segments meet at h=.1, and endpoints0/1 close each half-cycle. */
XENOLITH_AUDIO_INLINE uint32_t warped_phase(uint32_t phase,float amount) {
    uint32_t half=phase&UINT32_C(0x10000000);
    float h,warped,position;
    if(!(amount>0.0f))return phase;
    h=(float)(phase&UINT32_C(0x0fffffff))*(1.0f/268435456.0f);
    warped=h<=0.1f?5.0f*h:0.5f+(h-0.1f)*(5.0f/9.0f);
    position=h+amount*(warped-h);
    return (half+(uint32_t)(position*268435456.0f))&PHASE_MASK;
}

void xenolith_topology_defaults(XenolithTopologyConfig *c) {
    if (!c)return;
    c->growth=XTOPO_PARALLEL;c->mode=XTOPO_LINEAR;c->link=0.5f;c->feedback=0.0f;
}
int xenolith_topology_valid(const XenolithTopologyConfig *c) {
    return c && c->growth<XTOPO_GROWTH_COUNT && c->mode<XTOPO_MODE_COUNT
        && c->link>=0.0f && c->link<=4.0f && c->feedback>=0.0f && c->feedback<=0.95f;
}

XENOLITH_AUDIO_INLINE int32_t phase_increment(float scaled) {
    /* Keep the original conversion exactly for every in-range increment.
     * At low sample rates / wide FM, remove complete 29-bit phase periods
     * before converting: a float-to-int cast outside int32 is undefined.
     * The bounded controls keep the period quotient small and representable.
     * Negative increments remain negative; this is wrapping, not rectifying.
     */
    if (scaled >= 2147483648.0f || scaled < -2147483648.0f) {
        int32_t periods = (int32_t)(scaled * (1.0f / 536870912.0f));
        scaled -= (float)periods * 536870912.0f;
    }
    return (int32_t)scaled;
}

XENOLITH_AUDIO_INLINE float bounded(float value, float low, float high) {
    /* Also maps NaN to low; valid controls need no special sample-loop checks. */
    if (!(value >= low)) return low;
    return value > high ? high : value;
}

/* Optional modulation-input processing stays in XIP. The first-order allpass
 * recurrence follows Smith, Physical Audio Signal Processing, sec.4.3;
 * the bounded cubic and delayed collective bus are this instrument's design.
 * The final clamp protects the FM input contract: it is not part of a lossless
 * allpass and can colour large transients or an intentionally biased input. */
static void prepare_coupling(XenolithCore *s) {
    float delay=xenolith_coupling_delay(s->coupling.parameter);
    s->coupling_active=(uint32_t)xenolith_coupling_active(&s->coupling);
    s->coupling_drive=xenolith_coupling_drive(s->coupling.parameter);
    if(s->coupling.method==XCOUPLING_ALLPASS)
        s->coupling_coefficient=(1.0f-delay)/(1.0f+delay);
    else s->coupling_coefficient=1.0f/(1.0f+
        .001f*xenolith_coupling_time_ms(s->coupling.parameter)*s->sample_rate);
    s->coupling_depth=s->coupling.depth;s->coupling_bias=s->coupling.bias;
    s->coupling_coefficient_target=s->coupling_coefficient;s->coupling_drive_target=s->coupling_drive;
    s->coupling_remaining=0u;
    s->coupling_depth_step=s->coupling_bias_step=s->coupling_coefficient_step=s->coupling_drive_step=0.0f;
}
static void ramp_coupling(XenolithCore *s) {
    float delay=xenolith_coupling_delay(s->coupling.parameter),inverse;
    s->coupling_coefficient_target=s->coupling.method==XCOUPLING_ALLPASS?
        (1.0f-delay)/(1.0f+delay):1.0f/(1.0f+
            .001f*xenolith_coupling_time_ms(s->coupling.parameter)*s->sample_rate);
    s->coupling_drive_target=xenolith_coupling_drive(s->coupling.parameter);
    s->coupling_remaining=(uint32_t)(.02f*s->sample_rate);
    if(!s->coupling_remaining)s->coupling_remaining=1u;
    inverse=1.0f/(float)s->coupling_remaining;
    s->coupling_depth_step=(s->coupling.depth-s->coupling_depth)*inverse;
    s->coupling_bias_step=(s->coupling.bias-s->coupling_bias)*inverse;
    s->coupling_coefficient_step=(s->coupling_coefficient_target-s->coupling_coefficient)*inverse;
    s->coupling_drive_step=(s->coupling_drive_target-s->coupling_drive)*inverse;
}
static inline void coupling_tick(XenolithCore *s) {
    if(!s->coupling_remaining)return;
    s->coupling_depth+=s->coupling_depth_step;s->coupling_bias+=s->coupling_bias_step;
    s->coupling_coefficient+=s->coupling_coefficient_step;s->coupling_drive+=s->coupling_drive_step;
    if(!--s->coupling_remaining){
        s->coupling_depth=s->coupling.depth;s->coupling_bias=s->coupling.bias;
        s->coupling_coefficient=s->coupling_coefficient_target;s->coupling_drive=s->coupling_drive_target;
    }
}
int xenolith_set_coupling(XenolithCore *s,const XenolithCouplingConfig *c) {
    uint32_t i,reset;
    if(!s||!xenolith_coupling_valid(c))return 0;
    if(c->method==s->coupling.method&&c->depth==s->coupling.depth&&
       c->parameter==s->coupling.parameter&&c->bias==s->coupling.bias)return 1;
    reset=s->coupling.method!=c->method||(!s->coupling_active&&xenolith_coupling_active(c));
    s->coupling=*c;
    if(reset||!xenolith_coupling_active(c))prepare_coupling(s);else ramp_coupling(s);
    if(reset){
        for(i=0;i<XENOLITH_MAX_CELLS;i++)s->coupling_x[i]=s->coupling_y[i]=0.0f;
        s->coupling_mean=s->coupling_bus=0.0f;
    }
    return 1;
}
static inline float coupling_modulator(XenolithCore *s,uint32_t i,float mod) {
    float x=bounded(mod+s->coupling_bias,-1.0f,1.0f),y=x;
    if(s->coupling.method==XCOUPLING_BIAS){
        x=bounded(x*s->coupling_drive,-1.0f,1.0f);
        y=x*(1.5f-.5f*x*x);
    }else if(s->coupling.method==XCOUPLING_LAG){
        y=s->coupling_y[i]+s->coupling_coefficient*(x-s->coupling_y[i]);
        s->coupling_y[i]=y;
    }else if(s->coupling.method==XCOUPLING_ALLPASS){
        y=s->coupling_coefficient*(x-s->coupling_y[i])+s->coupling_x[i];
        s->coupling_x[i]=x;s->coupling_y[i]=y;
    }else if(s->coupling.method==XCOUPLING_COMMON)y=x-s->coupling_bus;
    y=bounded(y,-1.0f,1.0f);
    return mod+s->coupling_depth*(y-mod);
}

/* The table interpolation is bounded by +/-1. Capture actual tonic magnitudes
 * while preparing them; index and clock may change between those ticks, so
 * recompute their cheap bound once per sample. A512-unit margin covers float
 * bound rounding. Extreme/negative FM retains the original modulo slow path. */
XENOLITH_AUDIO_INLINE void observe_tonic(XenolithCore *s,float tonic) {
    float magnitude=tonic<0.0f?-tonic:tonic;
    if(magnitude>s->linear_tonic_bound)s->linear_tonic_bound=magnitude;
}
XENOLITH_AUDIO_INLINE uint32_t direct_linear_phase(const XenolithCore *s) {
    return (s->linear_tonic_bound*(1.0f+s->index))*s->phase_scale<2147483000.0f;
}
XENOLITH_AUDIO_INLINE int32_t linear_increment(float scaled,uint32_t direct) {
    return direct?(int32_t)scaled:phase_increment(scaled);
}

/* Start and delta are immutable within one fade. A shared bounded fraction
 * removes per-cell clamp/store work; retargeting materializes current gains. */
XENOLITH_AUDIO_INLINE float count_gain_value(const XenolithCore *s,uint32_t i) {
    return s->count_cell_gain[i]+s->count_fraction*s->count_cell_step[i];
}

XENOLITH_AUDIO_INLINE float random_bipolar(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return (float)(x >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

XENOLITH_AUDIO_INLINE float noise_tick(XenolithCore *s, XenolithNoise *n, uint32_t *random_state) {
    float result;
    if (n->remaining <= 0) {
        float previous = n->next_value;
        float length, carry = (float)n->remaining + n->remaining_fraction;
        n->next_value = random_bipolar(random_state);
        n->level = n->next_midpoint;
        n->next_midpoint = (n->next_value + previous) * 0.5f;
        n->remaining = (int32_t)((s->sample_rate / (float)s->control_block) / 0.2f);
        if (n->remaining < 2) n->remaining = 2;
        length = (float)n->remaining;
        /* Keep the sub-tick lateness of a retimed boundary in the next
         * segment. Zero carry takes the unchanged legacy recurrence. */
        if (carry != 0.0f) {
            length += carry;
            n->remaining = (int32_t)length;
            if ((float)n->remaining < length) ++n->remaining;
            n->remaining_fraction = length - (float)n->remaining;
        }
        n->curve = 2.0f * (n->next_midpoint - n->level - length * n->slope)
                 / (length * length + length);
    }
    result = n->level;
    n->slope += n->curve;
    n->level += n->slope;
    --n->remaining;
    return result;
}

XENOLITH_AUDIO_INLINE uint32_t *cell_random(XenolithCore *s, unsigned i) {
    return i < XENOLITH_CELLS ? &s->random_state : &s->extra_random_state;
}

static void reset_noise(XenolithCore *s, unsigned i) {
    XenolithNoise *n = &s->noise[i];
    uint32_t *random_state = cell_random(s, i);
    n->level = 0;
    n->slope = 0;
    n->curve = 0;
    n->remaining = 0;
    n->remaining_fraction = 0;
    n->next_value = random_bipolar(random_state);
    n->next_midpoint = n->next_value * 0.5f;
    /* SC's constructor calculates one value and retains advanced noise state. */
    (void)noise_tick(s, n, random_state);
}

static void reset_cell(XenolithCore *s, unsigned i) {
    s->mod_phase[i] = 0;
    s->car_phase[i] = 0;
    s->mod_increment[i] = 0;
    s->tonic[i] = linked_cell(s,i) || s->cells <= 8u ? s->base + ((float)i * 0.46f) * s->spread_current
                                : s->base * s->spread_scale[i];
    observe_tonic(s,s->tonic[i]);
    s->drift_noise[i] = 0;
    reset_noise(s, i);
}

XENOLITH_AUDIO_INLINE float sine_at(uint32_t phase) {
    uint32_t index = (phase >> 18) & 2047u;
    float first = CORE_SINE[index];
    float fraction = (float)(phase & 262143u) * (1.0f / 262144.0f);
#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_SINE_CACHE)
    /* The same float32 subtraction, performed once at boot. This removes
     * the wrapped neighbor address and subtraction from every oscillator. */
    return first + xenolith_sine_delta_ram[index] * fraction;
#else
    return first + ((CORE_SINE[(index + 1u) & 2047u] - first) * fraction);
#endif
}

void xenolith_calculation_defaults(XenolithCalculationConfig *c) {
    if(!c)return;c->table_bits=11u;c->interpolation=XCALC_LINEAR;
    c->target=XCALC_BOTH;c->harmonic=0.0f;
}
int xenolith_calculation_valid(const XenolithCalculationConfig *c) {
    return c&&c->table_bits>=2u&&c->table_bits<=11u
        &&c->interpolation<XCALC_INTERPOLATION_COUNT&&c->target<XCALC_TARGET_COUNT
        &&c->harmonic>=0.0f&&c->harmonic<=1.0f;
}
int xenolith_calculation_active(const XenolithCalculationConfig *c) {
    return xenolith_calculation_valid(c)&&(c->table_bits!=11u||c->interpolation!=XCALC_LINEAR||c->harmonic!=0.0f);
}
void xenolith_set_interpolation_weight(XenolithCore *s,float weight) {
    if(s)s->interpolation_weight=bounded(weight,0.0f,1.0f);
}
static void prepare_load_erosion(XenolithCore *s) {
    unsigned oscillator;
    for(oscillator=0;oscillator<2u;oscillator++) {
        uint32_t selected=s->calculation.target==XCALC_BOTH||s->calculation.target==oscillator;
        float bits=(float)(selected?s->calculation.table_bits:11u)-s->load_table_loss;
        uint32_t ceiling;union {uint32_t bits;float value;} step;
        if(bits<2.0f)bits=2.0f;
        ceiling=(uint32_t)bits;if((float)ceiling<bits)ceiling++;
        s->load_shift[oscillator]=29u-ceiling;
        s->load_chosen_shift[oscillator]=29u-ceiling;
        s->load_fraction[oscillator]=(float)ceiling-bits;
        step.bits=(127u+s->load_shift[oscillator])<<23;
        s->load_phase_step[oscillator]=step.value;
    }
}
void xenolith_set_load_erosion(XenolithCore *s,float loss,float dither,float ratio,float link) {
    if(!s)return;
    loss=bounded(loss,0.0f,5.0f);dither=bounded(dither,0.0f,1.0f);
    ratio=bounded(ratio,.375f,1.0f);link=bounded(link,0.0f,1.0f);
    if(loss!=s->load_table_loss){s->load_table_loss=loss;prepare_load_erosion(s);}
    s->load_dither=dither;s->load_cell_ratio=ratio;s->load_link_gain=link;
    s->load_erosion_active=((loss>0.0f||dither>0.0f)?1u:0u)
        |(ratio<1.0f?2u:0u)|(link<1.0f?4u:0u);
}
int xenolith_set_calculation(XenolithCore *s,const XenolithCalculationConfig *c) {
    union {uint32_t bits;float value;} fraction;
    if(!s||!xenolith_calculation_valid(c))return 0;
    s->calculation=*c;s->calculation_active=(uint32_t)xenolith_calculation_active(c);
    s->calculation_shift=29u-c->table_bits;s->calculation_stride=1u<<(11u-c->table_bits);
    s->calculation_mask=(1u<<s->calculation_shift)-1u;
    fraction.bits=(127u-s->calculation_shift)<<23;s->calculation_fraction=fraction.value;
    prepare_load_erosion(s);
    return 1;
}
/* Read a coarser subset of the same immutable sine data, then optionally blend
 * the signed third Chebyshev harmonic:3y-4y^3 = sin(3p) for y=sin(p).
 * No generated table, division, transcendental or amplitude-bit quantizer. */
XENOLITH_AUDIO_INLINE float sine_with_weight(uint32_t phase,float weight) {
    uint32_t index=(phase>>18)&2047u;float first=CORE_SINE[index];
    float fraction=(float)(phase&262143u)*(1.0f/262144.0f);
#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_SINE_CACHE)
    return first+(fraction*weight)*xenolith_sine_delta_ram[index];
#else
    return first+(fraction*weight)*(CORE_SINE[(index+1u)&2047u]-first);
#endif
}
XENOLITH_AUDIO_INLINE float weighted_sine(const XenolithCore *s,uint32_t phase) {
    if(s->interpolation_weight==1.0f)return sine_at(phase);
    if(s->interpolation_weight==0.0f)return CORE_SINE[(phase>>18)&2047u];
    return sine_with_weight(phase,s->interpolation_weight);
}
XENOLITH_AUDIO_INLINE float calculated_sine(const XenolithCore *s,uint32_t phase,uint32_t oscillator) {
    const XenolithCalculationConfig *c=&s->calculation;float value;
    uint32_t index;
    /* Runtime interpolation relief with the default table needs no variable
     * address grid or harmonic dispatch. The cached delta is the same float
     * subtraction as the generic path, preserving its result exactly. */
    if(!s->calculation_active)return weighted_sine(s,phase);
    if(c->target!=XCALC_BOTH&&c->target!=oscillator)return weighted_sine(s,phase);
    index=((phase>>s->calculation_shift)*s->calculation_stride)&2047u;
    value=CORE_SINE[index];
    if(c->interpolation==XCALC_LINEAR&&s->interpolation_weight!=0.0f){
        float fraction=(float)(phase&s->calculation_mask)*s->calculation_fraction;
        fraction*=s->interpolation_weight;
        value+=(CORE_SINE[(index+s->calculation_stride)&2047u]-value)*fraction;
    }
    if(c->harmonic!=0.0f)value+=c->harmonic*(2.0f*value-4.0f*value*value*value);
    return value;
}

XENOLITH_AUDIO_INLINE uint32_t erosion_in_use(const XenolithCore *s) {
    return (s->load_erosion_active&1u)
        |(s->load_erosion_active&(core_growth(s)?4u:2u));
}
XENOLITH_AUDIO_INLINE uint32_t erosion_random(XenolithCore *s) {
    uint32_t x=s->load_random_state;x^=x<<13;x^=x>>17;x^=x<<5;
    s->load_random_state=x;return x;
}
/* One independent grain per existing control tick. Fractional address depth
 * is a temporally coloured grid selection, not a white-noise/dither theorem. */
XENOLITH_AUDIO_INLINE void erosion_grain(XenolithCore *s) {
    unsigned o;uint32_t random=0u;
    if(s->load_dither>0.0f||s->load_fraction[0]>0.0f||s->load_fraction[1]>0.0f)
        random=erosion_random(s);
    s->load_noise_word=random;
    for(o=0;o<2u;o++){
        uint32_t r=o?((random<<16)|(random>>16)):random;
        uint32_t threshold=(uint32_t)(s->load_fraction[o]*16777216.0f);
        s->load_chosen_shift[o]=s->load_shift[o]+((r>>8)<threshold?1u:0u);
    }
}
XENOLITH_AUDIO_INLINE float load_lookup(uint32_t phase,uint32_t shift,float weight,float harmonic,int32_t offset) {
    uint32_t stride=1u<<(shift-18u),index;
    float value;phase=(phase+(uint32_t)offset)&PHASE_MASK;
    index=((phase>>shift)*stride)&2047u;value=CORE_SINE[index];
    if(weight!=0.0f){
        union {uint32_t bits;float value;} reciprocal;float fraction;
        reciprocal.bits=(127u-shift)<<23;
        fraction=(float)(phase&((1u<<shift)-1u))*reciprocal.value;
        value+=(CORE_SINE[(index+stride)&2047u]-value)*(fraction*weight);
    }
    if(harmonic!=0.0f)value+=harmonic*(2.0f*value-4.0f*value*value*value);
    return value;
}
XENOLITH_AUDIO_INLINE int32_t load_offset(uint32_t seed,uint32_t cell,uint32_t oscillator,float scale) {
    uint32_t hash=seed+(cell+1u)*UINT32_C(0x9e3779b9)+(oscillator?UINT32_C(0x85ebca6b):0u);
    hash^=hash>>16;hash*=UINT32_C(0x7feb352d);hash^=hash>>15;
    return (int32_t)(((float)(hash>>8)*(1.0f/16777216.0f)-.5f)*scale);
}
XENOLITH_AUDIO_INLINE float eroded_sine_at(XenolithCore *s,uint32_t phase,uint32_t oscillator,uint32_t cell) {
    const XenolithCalculationConfig *c=&s->calculation;
    uint32_t selected=c->target==XCALC_BOTH||c->target==oscillator;
    uint32_t shift=s->load_chosen_shift[oscillator];int32_t offset=0;
    float weight=(!selected||c->interpolation==XCALC_LINEAR)?s->interpolation_weight:0.0f;
    float harmonic=selected?c->harmonic:0.0f;
    if(!(s->load_erosion_active&1u))return calculated_sine(s,phase,oscillator);
    if(s->load_dither>0.0f){union {uint32_t bits;float value;} step;step.bits=(127u+shift)<<23;
        offset=load_offset(s->load_noise_word,cell,oscillator,s->load_dither*step.value);}
    return load_lookup(phase,shift,weight,harmonic,offset);
}

XENOLITH_AUDIO_INLINE float aperture_gain(const XenolithCore *s,uint32_t i) {
    /* A disabled aperture must also leave outgoing count-fade cells intact. */
    if(s->load_cell_ratio>=1.0f)return 1.0f;
    float gain=1.0f+((float)s->cells*s->load_cell_ratio-(float)(i+1u))*.25f;
    return gain<0.0f?0.0f:gain>1.0f?1.0f:gain;
}

/* The immutable rows reproduce the existing exp2/jitter formula exactly.
 * Count changes copy one contiguous flash row; no per-detent polynomial,
 * division or hash loop runs inside the audio callback. No extra SRAM. */
#include "xenolith_count_spread.inc"
static float density_gain(uint32_t count) {
    return count_density_target[count-1u];
}
/* Only optional width edits evaluate this bounded polynomial. Width1 takes
 * the frozen exact row; no formula is introduced into sample processing. */
static float width_exp2(float octaves) {
    int whole=(int)octaves;
    float fraction=octaves-(float)whole;
    float x=fraction*0.6931471805599453f;
    float value=1.0f+x*(1.0f+x*(0.5f+x*(0.1666666666666667f
        +x*(0.0416666666666667f+x*(0.0083333333333333f
        +x*(0.0013888888888889f+x*0.0001984126984127f))))));
    union {uint32_t bits;float value;} scale;
    scale.bits=(uint32_t)(127+whole)<<23;
    return value*scale.value;
}
static void prepare_spread_to(uint32_t count,float amount,float *target) {
    uint32_t i=0u;
    if(count>8u&&amount==1.0f){
        uint32_t offset=count*(count-1u)/2u-36u;
        for(;i<count;i++)target[i]=count_spread_table[offset+i];
    }else if(count>8u&&amount>0.0f){
        float span,inverse=1.0f/(float)(count-1u);
        if(count>32u){float opening=(float)(count-32u)*(1.0f/32.0f);span=.125f+5.875f*opening*opening*opening;}
        else if(count>16u){float opening=(float)(count-16u)*(1.0f/16.0f),ease=opening*opening*(3.0f-2.0f*opening);
            float start=.025f+.025f*7.0f/23.0f;span=start+(.125f-start)*ease;}
        else span=.025f+.025f*(float)(count-9u)/23.0f;
        for(;i<count;i++){
            uint32_t mirror=count-1u-i,pair=i<mirror?i:mirror;
            float position=count>16u?2.0f*(float)i*inverse-1.0f:2.0f*(float)i/(float)(count-1u)-1.0f;
            if(pair&&i!=mirror){
                uint32_t hash=pair*UINT32_C(0x9e3779b9)+UINT32_C(0x58584e31);float jitter;
                hash^=hash>>16;hash*=UINT32_C(0x7feb352d);hash^=hash>>15;
                jitter=((float)(hash>>8)*(1.0f/8388608.0f)-1.0f)*(count>16u?.7f*inverse:.7f/(float)(count-1u));
                position+=i<mirror?jitter:-jitter;
            }
            target[i]=width_exp2(position*(.5f*span)*amount);
        }
    }
    for(;i<XENOLITH_MAX_CELLS;i++)target[i]=1.0f;
}
static void prepare_spread(XenolithCore *s) {
    prepare_spread_to(s->cells,s->spread_amount,s->spread_scale);
}

/* Optional layout preparation reads the existing drift value. A key-mask or
 * spectral edit must not insert a noise/control tick or consume RNG draws. */
XENOLITH_AUDIO_INLINE void prepare_gated_pitch(XenolithCore *s,unsigned i) {
    if (s->cell_gates_absolute) {
        s->tonic[i] = s->cell_gate_frequency[i]
            * (1.0f + (s->drift_noise[i] * 0.10f * s->drift));
    } else if (linked_cell(s,i)) {
        s->tonic[i] = s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)) + (float)i*0.46f*s->spread_current;
    } else {
        s->tonic[i] = (s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)))
                    * (s->count_smoothing?s->spread_scale[i]:(s->cells <= 8u ? 1.0f : s->spread_scale[i]));
        if(s->count_smoothing){if(s->layout_offset!=0.0f)s->tonic[i]+=(float)i*0.46f*s->layout_offset*s->spread_current;}
        else if (s->cells <= 8u) s->tonic[i] += ((float)i * 0.46f)*s->spread_current;
    }
    observe_tonic(s,s->tonic[i]);
    s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
}

int xenolith_set_topology_graph_prepared(XenolithCore *s,const XenolithTopologyConfig *c,
    const XenolithGraphConfig *cfg,const XenolithGraph *prepared) {
    uint32_t i,extent,compatible,topology_changed;
    if(!s||!xenolith_topology_valid(c)||!xenolith_graph_valid(cfg)||!prepared)return 0;
    for(i=0;i<XENOLITH_MAX_CELLS;i++) {
        if(prepared->parent[i]!=XGRAPH_NONE&&prepared->parent[i]>=i)return 0;
        if(prepared->feedback[i]!=XGRAPH_NONE&&(prepared->feedback[i]<i||prepared->feedback[i]>=XENOLITH_MAX_CELLS))return 0;
    }
    extent=s->count_smoothing?s->render_cells:s->cells;
    compatible=xenolith_graph_prefix_compatible(&s->graph,extent,prepared,extent);
    topology_changed=s->topology.growth!=c->growth||s->topology.mode!=c->mode
        ||s->topology.link!=c->link||s->topology.feedback!=c->feedback;
    s->graph_config=*cfg;s->graph=*prepared;s->topology=*c;classify_graph(s,extent);
    s->index=bounded(s->index,0.0f,xenolith_depth_limit(c->mode));
    /* Legacy callers retain their preceding graph-reset semantics. An ADD
     * suffix edit does not clear any current source or feedback history. */
    if(!compatible||(cfg->scope==XGRAPH_ALL&&topology_changed))
        for(i=0;i<XENOLITH_MAX_CELLS;i++)s->graph_carrier[i]=0.0f;
    if(!compatible||topology_changed)for(i=0;i<extent;i++)prepare_gated_pitch(s,i);
    return 1;
}
int xenolith_set_topology_graph(XenolithCore *s,const XenolithTopologyConfig *c,
    const XenolithGraphConfig *cfg) {
    XenolithGraph prepared;
    if(!s||!xenolith_topology_valid(c)||!xenolith_graph_valid(cfg))return 0;
    if(!xenolith_graph_compile(cfg,c->growth,s->cells,&prepared))return 0;
    return xenolith_set_topology_graph_prepared(s,c,cfg,&prepared);
}
int xenolith_set_topology(XenolithCore *s,const XenolithTopologyConfig *c) {
    if(!s)return 0;
    return xenolith_set_topology_graph(s,c,&s->graph_config);
}

int xenolith_set_cell_gates(XenolithCore *s,uint32_t enabled,
    uint32_t mask_lo,uint32_t mask_hi,uint32_t absolute,const float *frequencies) {
    unsigned i;uint32_t changed,pitch_changed,was_absolute;
    if (!s || enabled>1u || absolute>1u) return 0;
    if (!enabled) absolute=0u;
    if (enabled && absolute) {
        if (!frequencies) return 0;
        for (i=0;i<s->cells;++i)
            if (!(frequencies[i]>=20.0f && frequencies[i]<=8000.0f)) return 0;
    }
    /* A disabled, unused feature leaves even its private tail untouched. */
    if (!enabled && !s->cell_gates_processing) return 1;
    changed=enabled!=s->cell_gates_enabled || absolute!=s->cell_gates_absolute
        || mask_lo!=s->cell_gate_mask[0] || mask_hi!=s->cell_gate_mask[1];
    pitch_changed=absolute!=s->cell_gates_absolute;
    was_absolute=s->cell_gates_absolute;
    if (enabled && absolute) for (i=0;i<s->cells;++i)
        if (frequencies[i]!=((s->count_smoothing||s->cell_glide_frames>0.0f)?s->frequency_target[i]:s->cell_gate_frequency[i])) {changed=1u;pitch_changed=1u;}
    if (!changed) return 1;
    if(s->count_smoothing)s->count_mask_free=0u;
    if (!s->cell_gates_processing) {
        /* The ordinary bank was fully open before enabling. Fade exclusions
         * down from its actual unity gain, rather than jumping to a mask. */
        for (i=0;i<XENOLITH_MAX_CELLS;++i) {
            s->cell_gate_gain[i]=i<s->cells?1.0f:0.0f;
        }
        s->cell_gates_processing=1u;
    }
    s->cell_gates_enabled=enabled;s->cell_gates_absolute=absolute;
    s->cell_gates_processing=1u;
    s->cell_gate_mask[0]=mask_lo;s->cell_gate_mask[1]=mask_hi;
    s->cell_gate_step=200.0f/s->sample_rate;
    if (enabled && absolute) for (i=0;i<s->cells;++i){
        if(pitch_changed&&s->glide_ms>0.0f){
            if(!was_absolute)s->cell_gate_frequency[i]=s->base;
            s->cell_glide_start[i]=s->cell_gate_frequency[i];
        }
        s->frequency_target[i]=frequencies[i];
        if(!s->count_smoothing&&s->glide_ms==0.0f)s->cell_gate_frequency[i]=frequencies[i];
    }
    if(pitch_changed&&enabled&&absolute&&s->glide_ms>0.0f){
        s->cell_glide_elapsed=0.0f;s->cell_glide_frames=s->glide_ms*.001f*s->sample_rate;
        if(s->cell_glide_frames<1.0f)s->cell_glide_frames=1.0f;
        s->cell_glide_inverse=1.0f/s->cell_glide_frames;
    }else if(!absolute)s->cell_glide_frames=0.0f;
    if (pitch_changed) for (i=0;i<s->cells;++i)prepare_gated_pitch(s,i);
    return 1;
}

void xenolith_set_control_block(XenolithCore *s, uint32_t frames) {
    unsigned i;
    if (frames < 1u) frames = 1u;
    if (frames > 1024u) frames = 1024u;
    if (frames == s->control_block) return;
    s->control_block = frames;
    s->control_remaining = 0;
    s->control_remaining_fraction = 0;
    // A new server control rate changes LFNoise2's segment length. This helper
    // is for configuration before playback; begin fresh interpolation states
    // rather than stretching a segment initialized for a different rate.
    for (i = 0; i < s->cells; ++i) reset_noise(s, i);
}

static void reclock(XenolithCore *s, float sample_rate, uint32_t frames, uint32_t continuous) {
    unsigned i;
    uint32_t old_frames = s->control_block;
    float old_rate = s->sample_rate;
    sample_rate = bounded(sample_rate, 1000.0f, 192000.0f);
    if (frames < 1u) frames = 1u;
    if (frames > 1024u) frames = 1024u;
    if (sample_rate == s->sample_rate && frames == old_frames) return;
    s->sample_rate = sample_rate;
    s->phase_scale = 536870912.0f / sample_rate;
    if(s->coupling_active)ramp_coupling(s);else prepare_coupling(s);
    if(s->base_glide_frames>0.0f){
        s->base_glide_frames*=sample_rate/old_rate;s->base_glide_elapsed*=sample_rate/old_rate;
        s->base_glide_inverse=1.0f/s->base_glide_frames;
    }
    if(s->cell_glide_frames>0.0f){
        s->cell_glide_frames*=sample_rate/old_rate;s->cell_glide_elapsed*=sample_rate/old_rate;
        s->cell_glide_inverse=1.0f/s->cell_glide_frames;
    }
    if(s->spread_current!=s->spread_amount&&s->layout_remaining>0.0f)
        s->layout_remaining*=sample_rate/old_rate;
    if (s->cell_gates_processing) s->cell_gate_step=200.0f/sample_rate;
    if (old_rate / (float)old_frames != sample_rate / (float)frames) {
        float interval = (old_rate / (float)old_frames) / (sample_rate / (float)frames);
        /* Count fades retain outgoing and dormant oscillator states. Every
         * initialized noise clock must follow the same timebase, including
         * cells outside the current prefix; otherwise shrink/rate/grow cycles
         * repeatedly stretch some old clocks and can overflow remaining. */
        for (i = 0; i < XENOLITH_MAX_CELLS; ++i) {
            XenolithNoise *n = &s->noise[i];
            float curve = n->curve * interval * interval;
            float remaining = ((float)n->remaining
                + (continuous ? n->remaining_fraction : 0.0f)) / interval;
            n->slope = (n->slope + 0.5f*n->curve)*interval - 0.5f*curve;
            n->curve = curve;
            /* Valid five-second segments fit below960000 ticks at192k/1.
             * Include one full second of margin before the integer cast. */
            remaining=bounded(remaining,-1152000.0f,1152000.0f);
            n->remaining = (int32_t)remaining;
            if ((float)n->remaining < remaining) ++n->remaining;
            n->remaining_fraction = continuous ? remaining-(float)n->remaining : 0.0f;
        }
        {
            float remaining = ((float)s->control_remaining
                + (continuous ? s->control_remaining_fraction : 0.0f)) * sample_rate / old_rate;
            s->control_remaining = remaining > 0.0f ? (uint32_t)remaining : 0u;
            if ((float)s->control_remaining < remaining) ++s->control_remaining;
            s->control_remaining_fraction = continuous ? remaining-(float)s->control_remaining : 0.0f;
        }
    } else {
    /* Round up a fractional remaining control interval; at output-block
     * boundaries the1/2/4x ratios are exact, so no control tick is inserted. */
        if (continuous) {
            float remaining = ((float)s->control_remaining+s->control_remaining_fraction)
                * (float)frames / (float)old_frames;
            s->control_remaining = remaining > 0.0f ? (uint32_t)remaining : 0u;
            if ((float)s->control_remaining < remaining) ++s->control_remaining;
            s->control_remaining_fraction = remaining-(float)s->control_remaining;
        } else {
            s->control_remaining = (s->control_remaining * frames + old_frames - 1u) / old_frames;
            s->control_remaining_fraction = 0.0f;
        }
    }
    s->control_block = frames;
    for (i = 0; i < XENOLITH_MAX_CELLS; ++i)
        s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
}

void xenolith_reclock(XenolithCore *s, float sample_rate, uint32_t frames) {
    reclock(s,sample_rate,frames,0u);
}

void xenolith_retime(XenolithCore *s, float sample_rate, uint32_t frames) {
    reclock(s,sample_rate,frames,1u);
}

void xenolith_set_cells(XenolithCore *s, uint32_t count) {
    unsigned i;
    uint32_t previous = s->cells;
    if (count < 1u) count = 1u;
    if (count > XENOLITH_MAX_CELLS) count = XENOLITH_MAX_CELLS;
    if (count == s->cells) return;
    s->cells = count;
    s->render_cells=count;s->count_smoothing=0u;classify_graph(s,count);
    s->spread_current=s->spread_amount;
    s->count_layout_fraction=0.0f;
    s->count_unity_prefix=count;
    for(i=0;i<64u;i++)s->count_cell_gain[i]=i<count?1.0f:0.0f;
    prepare_spread(s);
    if (s->cell_gates_processing && count>previous)s->cell_gates_processing=1u;
    for (i = previous; i < count; ++i) {
        reset_cell(s, i);
        s->graph_carrier[i]=0.0f;
        if (s->cell_gates_processing) {
            s->cell_gate_gain[i]=0.0f;
            s->cell_gate_frequency[i]=bounded(s->base,20.0f,8000.0f);
            s->frequency_target[i]=s->cell_glide_start[i]=s->cell_gate_frequency[i];
            prepare_gated_pitch(s,i);
        }
        else if (linked_cell(s,i))prepare_gated_pitch(s,i);
    }
    for (i=count;i<previous;++i)s->graph_carrier[i]=0.0f;
    if (s->cell_gates_processing) for (i=count;i<previous;++i)s->cell_gate_gain[i]=0.0f;
    s->cell_gain = density_gain(count);
    /* Recompute tonic/increments on the next sample for newly active cells. */
    if (!linked_growth(core_growth(s))) {
        s->control_remaining = 0;
        s->control_remaining_fraction = 0;
    }
}

int xenolith_transition_cells(XenolithCore *s,uint32_t count,uint32_t smooth) {
    uint32_t i,previous,extent;float duration;
    if(!s||smooth>1u)return 0;
    if(count<1u)count=1u;if(count>64u)count=64u;
    if(count==s->cells && (smooth||!s->count_smoothing))return 1;
    previous=s->cells;extent=s->count_smoothing?s->render_cells:previous;
    s->count_layout_fraction=0.0f;
    if(s->count_smoothing)for(i=0;i<extent;i++)s->count_cell_gain[i]=count_gain_value(s,i);
    s->count_fraction=0.0f;s->count_elapsed=0u;
    if(!s->count_smoothing)s->count_mask_free=!s->cell_gates_enabled&&(s->cell_gates_processing==0u||s->cell_gates_processing==2u);
    if(!s->count_smoothing){
        s->layout_offset=previous<=8u?1.0f:0.0f;
        for(i=0;i<64u;i++)s->count_cell_gain[i]=i<previous?1.0f:0.0f;
    }
    if(!s->cell_gates_processing)for(i=0;i<64u;i++)s->cell_gate_gain[i]=i<previous?1.0f:0.0f;
    for(i=extent;i<count;i++){
        s->count_cell_gain[i]=0.0f;s->cell_gate_gain[i]=s->cell_gates_enabled?0.0f:1.0f;
        if(s->cell_gate_frequency[i]<20.0f)s->cell_gate_frequency[i]=bounded(s->base,20.0f,8000.0f);
        s->frequency_target[i]=s->cell_gate_frequency[i];
        s->cell_glide_start[i]=s->cell_gate_frequency[i];
    }
    s->cells=count;prepare_spread_to(count,s->spread_amount,s->spread_target);
    s->count_gain_target=density_gain(count);s->layout_offset_target=count<=8u?1.0f:0.0f;
    s->cell_gate_step=200.0f/s->sample_rate;
    if(!smooth){
        s->render_cells=count;s->count_smoothing=0u;classify_graph(s,count);s->count_gain_remaining=0u;s->layout_remaining=0.0f;
        s->count_gain_finalize=0u;s->count_weight=(float)count;s->count_weight_step=0.0f;
        s->count_unity_prefix=count;
        s->cell_gain=s->count_gain_target;s->layout_offset=s->layout_offset_target;
        s->spread_current=s->spread_amount;
        for(i=0;i<64u;i++){
            s->spread_scale[i]=s->spread_target[i];
            s->count_cell_gain[i]=i<count?1.0f:0.0f;
            s->count_cell_step[i]=0.0f;
            if(i<count&&s->cell_gates_absolute&&s->cell_glide_frames==0.0f)s->cell_gate_frequency[i]=s->frequency_target[i];
            if(i>=count)s->cell_gate_gain[i]=0.0f;
            else if(!s->cell_gates_enabled)s->cell_gate_gain[i]=1.0f;
        }
        for(i=0;i<count;i++)prepare_gated_pitch(s,i);
        s->cell_gates_processing=s->cell_gates_enabled?1u:0u;
        return 1;
    }
    s->render_cells=extent>count?extent:count;classify_graph(s,s->render_cells);s->count_smoothing=1u;s->cell_gates_processing=1u;
    duration=.005f*s->sample_rate;s->count_gain_remaining=(uint32_t)duration;
    if((float)s->count_gain_remaining<duration)s->count_gain_remaining++;
    s->count_gain_finalize=0u;s->count_gain_step=1.0f/(float)s->count_gain_remaining;
    s->count_weight=0.0f;
    for(i=0;i<s->render_cells;i++){
        s->count_weight+=s->count_cell_gain[i];
        s->count_cell_step[i]=(i<count?1.0f:0.0f)-s->count_cell_gain[i];
    }
    s->count_weight_step=((float)count-s->count_weight)*s->count_gain_step;
    s->count_unity_prefix=0u;
    while(s->count_unity_prefix<count&&s->count_cell_gain[s->count_unity_prefix]==1.0f
        &&s->count_cell_step[s->count_unity_prefix]==0.0f)s->count_unity_prefix++;
    s->layout_remaining=.020f*s->sample_rate;
    return 1;
}

/* Quarter-cell density grid, used only during live count fades. Linear
 * interpolation stays continuous; exact established coefficients resume at
 * settlement. The device caches these1016bytes before audio starts. */
static const float count_density_table[254]={
    8.00000000f,6.40000010f,5.33333349f,4.57142878f,4.00000000f,3.55555558f,
    3.20000005f,2.90909100f,2.66666675f,2.46153855f,2.28571439f,2.13333344f,
    2.00000000f,1.88235295f,1.77777779f,1.68421054f,1.60000002f,1.52380955f,
    1.45454550f,1.39130437f,1.33333337f,1.27999997f,1.23076928f,1.18518519f,
    1.14285719f,1.10344827f,1.06666672f,1.03225803f,1.00000000f,0.984731913f,
    0.970142484f,0.956182897f,0.942809045f,0.929981112f,0.917662919f,0.905821621f,
    0.894427180f,0.883452237f,0.872871578f,0.862662196f,0.852802873f,0.843274057f,
    0.834057629f,0.825137019f,0.816496611f,0.808122039f,0.800000012f,0.792118013f,
    0.784464538f,0.777028680f,0.769800365f,0.762770057f,0.755928934f,0.749268651f,
    0.742781341f,0.736459672f,0.730296731f,0.724285960f,0.718421221f,0.712696671f,
    0.707106769f,0.701646388f,0.696310639f,0.691094756f,0.685994327f,0.681005239f,
    0.676123381f,0.671345115f,0.666666687f,0.662084699f,0.657595932f,0.653197289f,
    0.648885667f,0.644658387f,0.640512586f,0.636445820f,0.632455528f,0.628539383f,
    0.624695063f,0.620920420f,0.617213428f,0.613572001f,0.609994292f,0.606478453f,
    0.603022695f,0.599625349f,0.596284807f,0.592999458f,0.589767814f,0.586588442f,
    0.583459973f,0.580380976f,0.577350259f,0.574366510f,0.571428597f,0.568535268f,
    0.565685451f,0.562878013f,0.560112059f,0.557386398f,0.554700196f,0.552052438f,
    0.549442232f,0.546868742f,0.544331074f,0.541828394f,0.539359868f,0.536924839f,
    0.534522474f,0.532152057f,0.529812932f,0.527504385f,0.525225759f,0.522976339f,
    0.520755649f,0.518562973f,0.516397774f,0.514259458f,0.512147546f,0.510061383f,
    0.508000493f,0.505964398f,0.503952622f,0.501964629f,0.500000000f,0.498058259f,
    0.496138930f,0.494241655f,0.492365956f,0.490511477f,0.488677770f,0.486864507f,
    0.485071242f,0.483297676f,0.481543422f,0.479808122f,0.478091449f,0.476393074f,
    0.474712670f,0.473049909f,0.471404523f,0.469776183f,0.468164593f,0.466569483f,
    0.464990556f,0.463427544f,0.461880207f,0.460348278f,0.458831459f,0.457329571f,
    0.455842316f,0.454369456f,0.452910811f,0.451466113f,0.450035155f,0.448617727f,
    0.447213590f,0.445822567f,0.444444448f,0.443079025f,0.441726118f,0.440385520f,
    0.439057052f,0.437740535f,0.436435789f,0.435142636f,0.433860928f,0.432590455f,
    0.431331098f,0.430082679f,0.428845018f,0.427617997f,0.426401436f,0.425195217f,
    0.423999161f,0.422813147f,0.421637028f,0.420470655f,0.419313937f,0.418166697f,
    0.417028815f,0.415900201f,0.414780676f,0.413670152f,0.412568510f,0.411475599f,
    0.410391331f,0.409315616f,0.408248305f,0.407189280f,0.406138480f,0.405095756f,
    0.404061019f,0.403034180f,0.402015120f,0.401003778f,0.400000006f,0.399003744f,
    0.398014873f,0.397033334f,0.396059006f,0.395091832f,0.394131720f,0.393178552f,
    0.392232269f,0.391292781f,0.390360028f,0.389433920f,0.388514340f,0.387601256f,
    0.386694610f,0.385794252f,0.384900182f,0.384012282f,0.383130521f,0.382254779f,
    0.381385028f,0.380521208f,0.379663199f,0.378810972f,0.377964467f,0.377123624f,
    0.376288354f,0.375458598f,0.374634326f,0.373815447f,0.373001933f,0.372193694f,
    0.371390671f,0.370592833f,0.369800121f,0.369012475f,0.368229836f,0.367452174f,
    0.366679400f,0.365911484f,0.365148365f,0.364390016f,0.363636374f,0.362887383f,
    0.362142980f,0.361403167f,0.360667855f,0.359937012f,0.359210610f,0.358488560f,
    0.357770890f,0.357057482f,0.356348336f,0.355643392f,0.354942590f,0.354245961f,
    0.353553385f,0.353553385f
};
#if defined(__PI32V2__) || defined(__GCC_PI32V2__) || defined(XENOLITH_TEST_SINE_CACHE)
static float count_density_ram[254] XENOLITH_SINE_STORAGE;
static uint32_t count_density_ready;
#define COUNT_DENSITY count_density_ram
static void prepare_count_density(void) {
    uint32_t i;if(count_density_ready)return;
    for(i=0;i<254u;i++)count_density_ram[i]=count_density_table[i];
    count_density_ready=1u;
}
#else
#define COUNT_DENSITY count_density_table
static void prepare_count_density(void) {}
#endif
XENOLITH_AUDIO_INLINE float count_density(float weight) {
    float position,fraction;uint32_t index;
    if(weight<=1.0f)return 8.0f;if(weight>=64.0f)return COUNT_DENSITY[252];
    position=(weight-1.0f)*4.0f;index=(uint32_t)position;fraction=position-(float)index;
    return COUNT_DENSITY[index]+fraction*(COUNT_DENSITY[index+1u]-COUNT_DENSITY[index]);
}

XENOLITH_AUDIO_INLINE void count_transition_tick(XenolithCore *s) {
    s->count_layout_fraction=0.0f;
    s->count_gain_finalize=0u;
    if(s->count_gain_remaining){
        float weight;
        s->count_gain_remaining--;s->count_elapsed++;
        s->count_fraction=s->count_gain_remaining?(float)s->count_elapsed*s->count_gain_step:1.0f;
        if(s->count_fraction>1.0f)s->count_fraction=1.0f;
        s->count_weight+=s->count_weight_step;weight=s->count_weight;
        if(!s->count_gain_remaining){s->count_gain_finalize=1u;s->count_weight=(float)s->cells;s->cell_gain=s->count_gain_target;}
        else s->cell_gain=count_density(weight);
    }
    if(!s->control_remaining && s->layout_remaining>0.0f){
        float frames=(float)s->control_block;
        float fraction=frames>=s->layout_remaining?1.0f:frames/s->layout_remaining;
        s->count_layout_fraction=fraction;
        s->layout_offset+=fraction*(s->layout_offset_target-s->layout_offset);
        s->spread_current=fraction==1.0f?s->spread_amount:s->spread_current+fraction*(s->spread_amount-s->spread_current);
        s->layout_remaining=frames>=s->layout_remaining?0.0f:s->layout_remaining-frames;
    }
    if(s->render_cells>s->cells && count_gain_value(s,s->render_cells-1u)==0.0f) {
        do{s->render_cells--;}while(s->render_cells>s->cells&&count_gain_value(s,s->render_cells-1u)==0.0f);
        classify_graph(s,s->render_cells);
    }
    if(!s->count_gain_remaining&&!s->count_gain_finalize&&s->render_cells==s->cells){
        if(s->count_mask_free)s->cell_gates_processing=2u;
        if(s->layout_remaining==0.0f&&s->count_layout_fraction==0.0f){s->count_smoothing=0u;if(s->count_mask_free)s->cell_gates_processing=0u;}
    }
}

XENOLITH_AUDIO_INLINE void count_layout_cell(XenolithCore *s,uint32_t i) {
    if(s->count_smoothing&&s->count_layout_fraction>0.0f){
        float fraction=s->count_layout_fraction;
        s->spread_scale[i]+=fraction*(s->spread_target[i]-s->spread_scale[i]);
        if(s->cell_gates_absolute&&s->cell_glide_frames==0.0f)s->cell_gate_frequency[i]+=fraction*(s->frequency_target[i]-s->cell_gate_frequency[i]);
    }
}

XENOLITH_AUDIO_INLINE float count_gain_at(XenolithCore *s,uint32_t i) {
    return count_gain_value(s,i);
}

void xenolith_set(XenolithCore *s, float base, float index, float ratio, float drift) {
    base=bounded(base,20.0f,1200.0f);
    if(s->glide_ms==0.0f){s->base=s->base_target=base;s->base_glide_frames=0.0f;}
    else if(base!=s->base_target){
        s->base_target=base;s->base_glide_start=s->base;s->base_glide_elapsed=0.0f;
        s->base_glide_frames=s->glide_ms*.001f*s->sample_rate;
        if(s->base_glide_frames<1.0f)s->base_glide_frames=1.0f;
        s->base_glide_inverse=1.0f/s->base_glide_frames;
    }
    s->index = bounded(index, 0.0f, xenolith_depth_limit(s->topology.mode));
    s->ratio = bounded(ratio, 0.001f, 32.0f);
    s->drift = bounded(drift, 0.0f, 1.0f);
}

void xenolith_set_performance(XenolithCore *s,float glide_ms,float spread) {
    uint32_t i;
    if(!s)return;
    glide_ms=bounded(glide_ms,0.0f,2000.0f);spread=bounded(spread,0.0f,2.0f);
    if(glide_ms!=s->glide_ms){
        s->glide_ms=glide_ms;
        if(glide_ms==0.0f){
            s->base=s->base_target;s->base_glide_frames=0.0f;
            if(s->cell_glide_frames>0.0f){
                for(i=0;i<s->cells;i++)s->cell_gate_frequency[i]=s->frequency_target[i];
                s->cell_glide_frames=0.0f;
            }
        }
    }
    if(spread==s->spread_amount)return;
    s->spread_amount=spread;prepare_spread_to(s->cells,spread,s->spread_target);
    if(!s->count_smoothing){
        s->render_cells=s->cells;s->count_gain_remaining=s->count_gain_finalize=0u;
        s->count_fraction=0.0f;s->count_elapsed=0u;s->count_unity_prefix=s->cells;
        s->count_mask_free=!s->cell_gates_enabled&&(s->cell_gates_processing==0u||s->cell_gates_processing==2u);
        s->layout_offset=s->layout_offset_target=s->cells<=8u?1.0f:0.0f;
        for(i=0;i<64u;i++){
            s->count_cell_gain[i]=i<s->cells?1.0f:0.0f;s->count_cell_step[i]=0.0f;
            if(!s->cell_gates_processing)s->cell_gate_gain[i]=i<s->cells?1.0f:0.0f;
        }
    }
    s->count_smoothing=1u;s->cell_gates_processing=1u;s->cell_gate_step=200.0f/s->sample_rate;
    s->count_layout_fraction=0.0f;s->layout_remaining=.020f*s->sample_rate;
}

/* One optional scalar ramp per actual core sample, with per-cell work only
 * on its existing control tick. Defaults skip this block entirely. Targets
 * repeat without restarting; clock changes preserve elapsed physical time. */
XENOLITH_AUDIO_INLINE void performance_tick(XenolithCore *s) {
    if(s->base_glide_frames>0.0f){
        s->base_glide_elapsed+=1.0f;
        if(s->base_glide_elapsed>=s->base_glide_frames){s->base=s->base_target;s->base_glide_frames=0.0f;}
        else s->base=s->base_glide_start+(s->base_target-s->base_glide_start)*(s->base_glide_elapsed*s->base_glide_inverse);
    }
    if(s->cell_glide_frames>0.0f){
        s->cell_glide_elapsed+=1.0f;
        if(!s->control_remaining){
            uint32_t i;float fraction=s->cell_glide_elapsed*s->cell_glide_inverse;
            if(fraction>=1.0f)fraction=1.0f;
            for(i=0;i<s->cells;i++)s->cell_gate_frequency[i]=fraction==1.0f?s->frequency_target[i]
                :s->cell_glide_start[i]+(s->frequency_target[i]-s->cell_glide_start[i])*fraction;
            if(fraction==1.0f)s->cell_glide_frames=0.0f;
        }
    }
}

void xenolith_init(XenolithCore *s, float sample_rate, uint32_t seed) {
    unsigned i;
    XenolithCalculationConfig calculation;
    prepare_sine_cache();
    prepare_count_density();
    xenolith_flavour_defaults(&s->flavour);
    s->interpolation_weight=1.0f;
    s->glide_ms=0.0f;s->spread_amount=s->spread_current=1.0f;
    s->base_target=s->base_glide_start=s->base_glide_elapsed=s->base_glide_frames=s->base_glide_inverse=0.0f;
    s->cell_glide_elapsed=s->cell_glide_frames=s->cell_glide_inverse=0.0f;
    s->load_table_loss=s->load_dither=0.0f;s->load_cell_ratio=s->load_link_gain=1.0f;
    s->load_erosion_active=0u;s->load_noise_word=0u;
    s->load_random_state=(seed?seed:UINT32_C(0x29f18413))^UINT32_C(0xa511e9b3);
    if(!s->load_random_state)s->load_random_state=UINT32_C(0x6d2b79f5);
    xenolith_calculation_defaults(&calculation);(void)xenolith_set_calculation(s,&calculation);
    s->sample_rate = bounded(sample_rate, 1000.0f, 192000.0f);
    s->phase_scale = 536870912.0f / s->sample_rate;
    xenolith_coupling_defaults(&s->coupling);prepare_coupling(s);
    s->coupling_mean=s->coupling_bus=0.0f;
    s->random_state = seed ? seed : UINT32_C(0x29f18413);
    s->extra_random_state = s->random_state ^ UINT32_C(0x9e3779b9);
    if (s->extra_random_state == 0) s->extra_random_state = UINT32_C(0x6d2b79f5);
    s->control_block = XENOLITH_CONTROL_BLOCK;
    s->control_remaining = 0;
    s->control_remaining_fraction = 0;
    s->linear_tonic_bound=0.0f;s->count_fraction=0.0f;s->count_elapsed=0u;
    s->cells = XENOLITH_CELLS;
    s->render_cells=XENOLITH_CELLS;s->count_smoothing=s->count_gain_remaining=0u;
    s->count_gain_finalize=s->count_mask_free=0u;s->count_weight=(float)XENOLITH_CELLS;s->count_weight_step=0.0f;
    s->count_unity_prefix=XENOLITH_CELLS;
    s->count_gain_target=1.0f;s->count_gain_step=0.0f;s->layout_remaining=0.0f;
    s->layout_offset=s->layout_offset_target=1.0f;
    s->count_layout_fraction=0.0f;
    s->cell_gain = 1.0f;
    s->cell_gates_enabled=s->cell_gates_absolute=s->cell_gates_processing=0u;
    s->cell_gate_mask[0]=s->cell_gate_mask[1]=0u;s->cell_gate_step=0.0f;
    xenolith_topology_defaults(&s->topology);
    xenolith_graph_defaults(&s->graph_config);
    (void)xenolith_graph_compile(&s->graph_config,s->topology.growth,s->cells,&s->graph);
    prepare_flavour(s,s->cells);
    xenolith_set(s, 84.0f, 2.4f, 1.2f, 0.20f);
    prepare_spread(s);
    for (i = 0; i < XENOLITH_MAX_CELLS; ++i) {
        s->cell_gate_gain[i]=s->cell_gate_frequency[i]=0.0f;
        s->cell_glide_start[i]=0.0f;
        s->spread_target[i]=s->spread_scale[i];s->frequency_target[i]=0.0f;
        s->count_cell_gain[i]=i<s->cells?1.0f:0.0f;
        s->count_cell_step[i]=0.0f;
        s->graph_carrier[i]=0.0f;
        s->coupling_x[i]=s->coupling_y[i]=0.0f;
        reset_cell(s, i);
    }
}

XENOLITH_AUDIO_INLINE float sample_graph(XenolithCore *s) {
    unsigned i;uint32_t all_open=1u;float sum=0.0f;
    uint32_t extent=s->count_smoothing?s->render_cells:s->cells;
    uint32_t fading=s->count_smoothing&&(s->count_gain_remaining||s->count_gain_finalize||extent>s->cells);
    uint32_t growth=core_growth(s),phase=s->topology.mode;
    if (!s->control_remaining) {
        s->linear_tonic_bound=0.0f;
        for (i=0;i<extent;++i) {
            count_layout_cell(s,i);
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            prepare_gated_pitch(s,i);
        }
        s->control_remaining=s->control_block;
    }
    for (i=0;i<extent;++i) {
        float gain=1.0f,parent=0.0f,feedback=0.0f,carrier=0.0f;
        if (s->cell_gates_processing==1u && !(s->count_smoothing&&s->count_mask_free)) {
            uint32_t selected=(s->cell_gate_mask[i>>5]>>(i&31u))&1u;
            float target=s->cell_gates_enabled?(float)selected:1.0f;
            gain=s->cell_gate_gain[i];
            if (gain<target)gain=target-gain<=s->cell_gate_step?target:gain+s->cell_gate_step;
            else if (gain>target)gain=gain-target<=s->cell_gate_step?target:gain-s->cell_gate_step;
            s->cell_gate_gain[i]=gain;if (gain!=1.0f)all_open=0u;
        }
        if(fading&&i>=s->count_unity_prefix)gain*=count_gain_at(s,i);
        if (gain>0.0f) {
            float mod=phase==XTOPO_PULSE?
                ((s->mod_phase[i]&UINT32_C(0x10000000))?-1.0f:1.0f):sine_at(s->mod_phase[i]);
            int32_t increment;
            if (growth==XTOPO_MIXED) {
                uint32_t source=s->graph.parent[i],delayed=s->graph.feedback[i];
                if(source<extent)parent=s->graph_carrier[source];
                if(delayed<extent){feedback=s->topology.feedback*s->graph_carrier[delayed];
                    if(s->count_smoothing&&count_gain_value(s,delayed)!=1.0f)feedback*=count_gain_value(s,delayed);}
            }
            else if (growth==XTOPO_BRANCH) {if (i)parent=s->graph_carrier[(i-1u)>>1];}
            else if (growth==XTOPO_SELF){feedback=s->topology.feedback*s->graph_carrier[i];
                if(s->count_smoothing&&count_gain_value(s,i)!=1.0f)feedback*=count_gain_value(s,i);}
            else if (growth && (i&3u))parent=s->graph_carrier[i-1u];
            else if (growth==XTOPO_LOOP4 && i+3u<extent){feedback=s->topology.feedback*s->graph_carrier[i+3u];
                if(s->count_smoothing&&count_gain_value(s,i+3u)!=1.0f)feedback*=count_gain_value(s,i+3u);}
            if (phase==XTOPO_PHASE) {
                float radians=s->index*mod+s->topology.link*parent+feedback;
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                carrier=sine_at(output_phase)*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_WARP) {
                float radians=s->topology.link*parent+feedback;
                float amount=(s->index*(1.0f/64.0f))*((1.0f+mod)*0.5f);
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                carrier=sine_at(warped_phase(output_phase,amount))*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_EXPONENTIAL) {
                float octaves=(s->index*(1.0f/12.0f))*mod+s->topology.link*parent+feedback;
                float frequency=s->tonic[i]*exponential_ratio(octaves);
                carrier=sine_at(s->car_phase[i])*gain;
                increment=phase_increment(frequency*s->phase_scale);
            } else {
                float frequency=s->tonic[i]+mod*(s->tonic[i]*s->index);
                frequency+=s->tonic[i]*(s->topology.link*parent+feedback);
                carrier=sine_at(s->car_phase[i])*gain;
                increment=phase_increment(frequency*s->phase_scale);
            }
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        s->graph_carrier[i]=carrier;sum+=carrier*0.12f;
    }
    --s->control_remaining;
    if(all_open&&s->count_smoothing&&!s->cell_gates_enabled)s->count_mask_free=1u;
    if (s->cell_gates_processing==1u && all_open&&!s->count_smoothing)s->cell_gates_processing=s->cell_gates_enabled?2u:0u;
    return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
}

/* Optional calculation renderer: retain the original graph/fast loops above
 * and below unchanged when this explicitly selected waveform path is OFF. */
XENOLITH_AUDIO_INLINE float sample_calculation_graph(XenolithCore *s) {
    unsigned i;uint32_t all_open=1u;float sum=0.0f;
    uint32_t extent=s->count_smoothing?s->render_cells:s->cells;
    uint32_t fading=s->count_smoothing&&(s->count_gain_remaining||s->count_gain_finalize||extent>s->cells);
    uint32_t growth=core_growth(s),phase=s->topology.mode;
    if (!s->control_remaining) {
        s->linear_tonic_bound=0.0f;
        for (i=0;i<extent;++i) {
            count_layout_cell(s,i);
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            prepare_gated_pitch(s,i);
        }
        s->control_remaining=s->control_block;
    }
    for (i=0;i<extent;++i) {
        float gain=1.0f,parent=0.0f,feedback=0.0f,carrier=0.0f;
        if (s->cell_gates_processing==1u && !(s->count_smoothing&&s->count_mask_free)) {
            uint32_t selected=(s->cell_gate_mask[i>>5]>>(i&31u))&1u;
            float target=s->cell_gates_enabled?(float)selected:1.0f;
            gain=s->cell_gate_gain[i];
            if (gain<target)gain=target-gain<=s->cell_gate_step?target:gain+s->cell_gate_step;
            else if (gain>target)gain=gain-target<=s->cell_gate_step?target:gain-s->cell_gate_step;
            s->cell_gate_gain[i]=gain;if (gain!=1.0f)all_open=0u;
        }
        if(fading&&i>=s->count_unity_prefix)gain*=count_gain_at(s,i);
        if (gain>0.0f) {
            float mod=phase==XTOPO_PULSE?
                ((s->mod_phase[i]&UINT32_C(0x10000000))?-1.0f:1.0f):calculated_sine(s,s->mod_phase[i],XCALC_MOD);
            int32_t increment;
            if (growth==XTOPO_MIXED) {
                uint32_t source=s->graph.parent[i],delayed=s->graph.feedback[i];
                if(source<extent)parent=s->graph_carrier[source];
                if(delayed<extent){feedback=s->topology.feedback*s->graph_carrier[delayed];
                    if(s->count_smoothing&&count_gain_value(s,delayed)!=1.0f)feedback*=count_gain_value(s,delayed);}
            }
            else if (growth==XTOPO_BRANCH) {if (i)parent=s->graph_carrier[(i-1u)>>1];}
            else if (growth==XTOPO_SELF){feedback=s->topology.feedback*s->graph_carrier[i];
                if(s->count_smoothing&&count_gain_value(s,i)!=1.0f)feedback*=count_gain_value(s,i);}
            else if (growth && (i&3u))parent=s->graph_carrier[i-1u];
            else if (growth==XTOPO_LOOP4 && i+3u<extent){feedback=s->topology.feedback*s->graph_carrier[i+3u];
                if(s->count_smoothing&&count_gain_value(s,i+3u)!=1.0f)feedback*=count_gain_value(s,i+3u);}
            if (phase==XTOPO_PHASE) {
                float radians=s->index*mod+s->topology.link*parent+feedback;
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                carrier=calculated_sine(s,output_phase,XCALC_CARRIER)*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_WARP) {
                float radians=s->topology.link*parent+feedback;
                float amount=(s->index*(1.0f/64.0f))*((1.0f+mod)*0.5f);
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                carrier=calculated_sine(s,warped_phase(output_phase,amount),XCALC_CARRIER)*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_EXPONENTIAL) {
                float octaves=(s->index*(1.0f/12.0f))*mod+s->topology.link*parent+feedback;
                float frequency=s->tonic[i]*exponential_ratio(octaves);
                carrier=calculated_sine(s,s->car_phase[i],XCALC_CARRIER)*gain;
                increment=phase_increment(frequency*s->phase_scale);
            } else {
                float frequency=s->tonic[i]+mod*(s->tonic[i]*s->index);
                frequency+=s->tonic[i]*(s->topology.link*parent+feedback);
                carrier=calculated_sine(s,s->car_phase[i],XCALC_CARRIER)*gain;
                increment=phase_increment(frequency*s->phase_scale);
            }
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        s->graph_carrier[i]=carrier;sum+=carrier*0.12f;
    }
    --s->control_remaining;
    if(all_open&&s->count_smoothing&&!s->cell_gates_enabled)s->count_mask_free=1u;
    if (s->cell_gates_processing==1u && all_open&&!s->count_smoothing)s->cell_gates_processing=s->cell_gates_enabled?2u:0u;
    return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
}

/* Runtime degradation keeps every graph edge and the complete control clock.
 * Only the uncoupled PARALLEL output aperture can suppress a carrier lookup;
 * its modulator and carrier phase integration continue behind the aperture. */
/* Optional graph colour stays in XIP: keeping this duplicate general renderer
 * in the32KB audio island would displace the default and measured cheap paths.
 * Admission/timing must account for the optional graph cost separately. */
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
__attribute__((noinline))
#endif
static float xenolith_erosion_graph_kernel(XenolithCore *s) {
    unsigned i;uint32_t all_open=1u;float sum=0.0f;
    uint32_t common=s->coupling_active&&s->coupling.method==XCOUPLING_COMMON;
    float bus_sum=0.0f,bus_weight=0.0f;
    uint32_t extent=s->count_smoothing?s->render_cells:s->cells;
    uint32_t fading=s->count_smoothing&&(s->count_gain_remaining||s->count_gain_finalize||extent>s->cells);
    uint32_t growth=core_growth(s),phase=s->topology.mode;
    float link=s->topology.link*s->load_link_gain;
    float feedback_gain=s->topology.feedback*s->load_link_gain;
    if(s->coupling_active)coupling_tick(s);
    if(common)s->coupling_bus+=s->coupling_coefficient*(s->coupling_mean-s->coupling_bus);
    if (!s->control_remaining) {
        if(s->load_erosion_active)erosion_grain(s);
        s->linear_tonic_bound=0.0f;
        for (i=0;i<extent;++i) {
            count_layout_cell(s,i);
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            prepare_gated_pitch(s,i);
        }
        s->control_remaining=s->control_block;
    }
    for (i=0;i<extent;++i) {
        float gain=1.0f,parent=0.0f,feedback=0.0f,carrier=0.0f;
        float aperture=growth==XTOPO_PARALLEL?aperture_gain(s,i):1.0f;
        if (s->cell_gates_processing==1u && !(s->count_smoothing&&s->count_mask_free)) {
            uint32_t selected=(s->cell_gate_mask[i>>5]>>(i&31u))&1u;
            float target=s->cell_gates_enabled?(float)selected:1.0f;
            gain=s->cell_gate_gain[i];
            if (gain<target)gain=target-gain<=s->cell_gate_step?target:gain+s->cell_gate_step;
            else if (gain>target)gain=gain-target<=s->cell_gate_step?target:gain-s->cell_gate_step;
            s->cell_gate_gain[i]=gain;if (gain!=1.0f)all_open=0u;
        }
        if(fading&&i>=s->count_unity_prefix)gain*=count_gain_at(s,i);
        if (gain>0.0f) {
            float mod=phase==XTOPO_PULSE?
                ((s->mod_phase[i]&UINT32_C(0x10000000))?-1.0f:1.0f):eroded_sine_at(s,s->mod_phase[i],XCALC_MOD,i);
            int32_t increment;
            if(s->coupling_active)mod=coupling_modulator(s,i,mod);
            if (growth==XTOPO_MIXED) {
                uint32_t source=s->graph.parent[i],delayed=s->graph.feedback[i];
                if(source<extent)parent=s->graph_carrier[source];
                if(delayed<extent){feedback=feedback_gain*s->graph_carrier[delayed];
                    if(s->count_smoothing&&count_gain_value(s,delayed)!=1.0f)feedback*=count_gain_value(s,delayed);}
            }
            else if (growth==XTOPO_BRANCH) {if (i)parent=s->graph_carrier[(i-1u)>>1];}
            else if (growth==XTOPO_SELF){feedback=feedback_gain*s->graph_carrier[i];
                if(s->count_smoothing&&count_gain_value(s,i)!=1.0f)feedback*=count_gain_value(s,i);}
            else if (growth && (i&3u))parent=s->graph_carrier[i-1u];
            else if (growth==XTOPO_LOOP4 && i+3u<extent){feedback=feedback_gain*s->graph_carrier[i+3u];
                if(s->count_smoothing&&count_gain_value(s,i+3u)!=1.0f)feedback*=count_gain_value(s,i+3u);}
            if(s->flavour_active==XFLAVOUR_RETURN_NEGATIVE||
               (s->flavour_active==XFLAVOUR_RETURN_ALTERNATING&&(i&1u)))feedback=-feedback;
            if (phase==XTOPO_PHASE) {
                float radians=s->index*mod+link*parent+feedback;
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                if(aperture>0.0f)carrier=eroded_sine_at(s,output_phase,XCALC_CARRIER,i)*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_WARP) {
                float radians=link*parent+feedback;
                float amount=(s->index*(1.0f/64.0f))*((1.0f+mod)*0.5f);
                uint32_t output_phase=(s->car_phase[i]+(uint32_t)phase_increment(radians*PHASE_PER_RADIAN))&PHASE_MASK;
                if(aperture>0.0f)carrier=eroded_sine_at(s,warped_phase(output_phase,amount),XCALC_CARRIER,i)*gain;
                increment=phase_increment(s->tonic[i]*s->phase_scale);
            } else if (phase==XTOPO_EXPONENTIAL) {
                float octaves=(s->index*(1.0f/12.0f))*mod+link*parent+feedback;
                float frequency=s->tonic[i]*exponential_ratio(octaves);
                if(aperture>0.0f)carrier=eroded_sine_at(s,s->car_phase[i],XCALC_CARRIER,i)*gain;
                increment=phase_increment(frequency*s->phase_scale);
            } else {
                float frequency=s->tonic[i]+mod*(s->tonic[i]*s->index);
                frequency+=s->tonic[i]*(link*parent+feedback);
                if(aperture>0.0f)carrier=eroded_sine_at(s,s->car_phase[i],XCALC_CARRIER,i)*gain;
                increment=phase_increment(frequency*s->phase_scale);
            }
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        s->graph_carrier[i]=carrier;
        if(common){bus_sum+=carrier*aperture;bus_weight+=gain*aperture;}
        if(s->flavour_active==XFLAVOUR_CASCADE&&s->flavour_series_child[i]<extent){
            uint32_t child=s->flavour_series_child[i];
            float child_gain=s->count_smoothing?count_gain_value(s,child):1.0f;
            float tap=1.0f-s->flavour_tap_loss*child_gain;
            sum+=((carrier*0.12f)*aperture)*tap;
        }else sum+=(carrier*0.12f)*aperture;
    }
    if(common)s->coupling_mean=bus_weight>0.00001f?bus_sum/bus_weight:0.0f;
    --s->control_remaining;
    if(all_open&&s->count_smoothing&&!s->cell_gates_enabled)s->count_mask_free=1u;
    if (s->cell_gates_processing==1u && all_open&&!s->count_smoothing)s->cell_gates_processing=s->cell_gates_enabled?2u:0u;
    return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
}

XENOLITH_RAM_AUDIO static float sample_cell_gates(XenolithCore *s) {
    unsigned i;uint32_t all_open=1u;float sum=0.0f;
    uint32_t extent=s->count_smoothing?s->render_cells:s->cells;
    uint32_t fading=s->count_smoothing&&(s->count_gain_remaining||s->count_gain_finalize||extent>s->cells);
    if (s->control_remaining==0u) {
        s->linear_tonic_bound=0.0f;
        for (i=0;i<extent;++i) {
            count_layout_cell(s,i);
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            prepare_gated_pitch(s,i);
        }
        s->control_remaining=s->control_block;
    }
    uint32_t direct=direct_linear_phase(s);
    float depth=s->index,phase_scale=s->phase_scale;
    /* Once every gain is unity, do not pay for per-cell gain/mask work until
     * a layout/count change actually starts another fade. Spectral control
     * preparation still runs on its ordinary tick. */
    if (s->cell_gates_processing==2u) {
        for (i=0;i<s->cells;++i) {
            float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
            float frequency=s->tonic[i]+mod;
            int32_t increment=linear_increment(frequency*phase_scale,direct);
            sum+=sine_at(s->car_phase[i])*0.12f;
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        --s->control_remaining;
        return s->cells==XENOLITH_CELLS?sum:sum*s->cell_gain;
    }
    if(s->count_smoothing&&s->count_mask_free){
        for(i=0;i<s->count_unity_prefix;i++){
            float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
            int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
            sum+=sine_at(s->car_phase[i])*0.12f;
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        for(;i<extent;i++){
            float gain=fading?count_gain_at(s,i):1.0f;
            if(gain>0.0f){
                float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
                int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
                sum+=(sine_at(s->car_phase[i])*0.12f)*gain;
                s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
                s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
            }
        }
        --s->control_remaining;
        return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
    }
    for (i=0;i<extent;++i) {
        uint32_t selected=(s->cell_gate_mask[i>>5]>>(i&31u))&1u;
        float gain=s->cell_gate_gain[i],target=s->cell_gates_enabled?(float)selected:1.0f;
        if (gain<target)gain=target-gain<=s->cell_gate_step?target:gain+s->cell_gate_step;
        else if (gain>target)gain=gain-target<=s->cell_gate_step?target:gain-s->cell_gate_step;
        s->cell_gate_gain[i]=gain;
        if (gain!=1.0f)all_open=0u;
        if(fading&&i>=s->count_unity_prefix)gain*=count_gain_at(s,i);
        if (gain>0.0f) {
            float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
            float frequency=s->tonic[i]+mod;
            int32_t increment=linear_increment(frequency*phase_scale,direct);
            sum+=(sine_at(s->car_phase[i])*0.12f)*gain;
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
    }
    --s->control_remaining;
    if(all_open&&s->count_smoothing&&!s->cell_gates_enabled)s->count_mask_free=1u;
    if (all_open&&!s->count_smoothing)s->cell_gates_processing=s->cell_gates_enabled?2u:0u;
    return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
}

/* Separate register allocation keeps the ordinary parallel bank from paying
 * the general graph renderer's full stack frame on every sample. */
XENOLITH_RAM_AUDIO static float xenolith_parallel_kernel(XenolithCore *s) {
    unsigned i;
    float sum = 0;
    if (s->control_remaining == 0) {
        s->linear_tonic_bound=0.0f;
        for (i = 0; i < s->cells; ++i) {
            s->drift_noise[i] = noise_tick(s, &s->noise[i], cell_random(s, i));
            s->tonic[i] = (s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)))
                        * (s->cells <= 8u ? 1.0f : s->spread_scale[i]);
            if (s->cells <= 8u) s->tonic[i] += ((float)i * 0.46f)*s->spread_current;
            observe_tonic(s,s->tonic[i]);
            // Float-only target arithmetic: SC computes this control-frequency
            // increment in double. At extreme settings the quantized increment
            // can differ by a few phase units; reference tests disclose that.
            s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
        }
        s->control_remaining = s->control_block;
    }
    {   /* Audio ownership fixes these values for the whole cell loop. Explicit
         * locals prevent alias-conservative per-cell scalar/count reloads. */
    uint32_t cells=s->cells,direct=direct_linear_phase(s);float depth=s->index,phase_scale=s->phase_scale;
    for (i = 0; i < cells; ++i) {
        float mod = sine_at(s->mod_phase[i]) * (s->tonic[i] * depth);
        float frequency = s->tonic[i] + mod;
        int32_t increment = linear_increment(frequency * phase_scale,direct);
        /* Output current phase first, then advance; negative FM is supported. */
        sum += sine_at(s->car_phase[i]) * 0.12f;
        s->mod_phase[i] = (s->mod_phase[i] + (uint32_t)s->mod_increment[i]) & PHASE_MASK;
        s->car_phase[i] = (s->car_phase[i] + (uint32_t)increment) & PHASE_MASK;
    }
    }
    --s->control_remaining;
    return s->cells == XENOLITH_CELLS ? sum : sum * s->cell_gain;
}

/* Split the full/fading/silent parallel prefix once per sample. The silent
 * suffix still integrates exact FM phases, but does not fetch carrier audio. */
XENOLITH_RAM_AUDIO static float xenolith_parallel_aperture_kernel(XenolithCore *s) {
    unsigned i;
    float sum = 0;
    if (s->control_remaining == 0) {
        s->linear_tonic_bound=0.0f;
        for (i = 0; i < s->cells; ++i) {
            s->drift_noise[i] = noise_tick(s, &s->noise[i], cell_random(s, i));
            s->tonic[i] = (s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)))
                        * (s->cells <= 8u ? 1.0f : s->spread_scale[i]);
            if (s->cells <= 8u) s->tonic[i] += ((float)i * 0.46f)*s->spread_current;
            observe_tonic(s,s->tonic[i]);
            // Float-only target arithmetic: SC computes this control-frequency
            // increment in double. At extreme settings the quantized increment
            // can differ by a few phase units; reference tests disclose that.
            s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
        }
        s->control_remaining = s->control_block;
    }
    {   /* Audio ownership fixes these values for the whole cell loop. Explicit
         * locals prevent alias-conservative per-cell scalar/count reloads. */
    uint32_t cells=s->cells,direct=direct_linear_phase(s);float depth=s->index,phase_scale=s->phase_scale;
    uint32_t full=(uint32_t)((float)cells*s->load_cell_ratio);
    uint32_t end=full+4u;if(end>cells)end=cells;
    for(i=0;i<full;i++){
        float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=sine_at(s->car_phase[i])*.12f;
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<end;i++){
        float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=(sine_at(s->car_phase[i])*.12f)*aperture_gain(s,i);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<cells;i++){
        float mod=sine_at(s->mod_phase[i])*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    }
    --s->control_remaining;
    return s->cells == XENOLITH_CELLS ? sum : sum * s->cell_gain;
}
XENOLITH_RAM_AUDIO static float xenolith_parallel_hold_erosion_kernel(XenolithCore *s) {
    unsigned i;
    float sum = 0;
    if (s->control_remaining == 0) {
        erosion_grain(s);
        s->linear_tonic_bound=0.0f;
        for (i = 0; i < s->cells; ++i) {
            s->drift_noise[i] = noise_tick(s, &s->noise[i], cell_random(s, i));
            s->tonic[i] = (s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)))
                        * (s->cells <= 8u ? 1.0f : s->spread_scale[i]);
            if (s->cells <= 8u) s->tonic[i] += ((float)i * 0.46f)*s->spread_current;
            observe_tonic(s,s->tonic[i]);
            // Float-only target arithmetic: SC computes this control-frequency
            // increment in double. At extreme settings the quantized increment
            // can differ by a few phase units; reference tests disclose that.
            s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
        }
        s->control_remaining = s->control_block;
    }
    {   /* Audio ownership fixes these values for the whole cell loop. Explicit
         * locals prevent alias-conservative per-cell scalar/count reloads. */
    uint32_t cells=s->cells,direct=direct_linear_phase(s);float depth=s->index,phase_scale=s->phase_scale;
    uint32_t mm=2047u&~((1u<<(s->load_chosen_shift[0]-18u))-1u);
    uint32_t cm=2047u&~((1u<<(s->load_chosen_shift[1]-18u))-1u);
    uint32_t full=(uint32_t)((float)cells*s->load_cell_ratio);
    uint32_t end=full+4u;if(end>cells)end=cells;
    for(i=0;i<full;i++){
        float mod=CORE_SINE[(s->mod_phase[i]>>18)&mm]*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=CORE_SINE[(s->car_phase[i]>>18)&cm]*.12f;
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<end;i++){
        float mod=CORE_SINE[(s->mod_phase[i]>>18)&mm]*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=(CORE_SINE[(s->car_phase[i]>>18)&cm]*.12f)*aperture_gain(s,i);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<cells;i++){
        float mod=CORE_SINE[(s->mod_phase[i]>>18)&mm]*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    }
    --s->control_remaining;
    return s->cells == XENOLITH_CELLS ? sum : sum * s->cell_gain;
}
XENOLITH_RAM_AUDIO static float xenolith_parallel_erosion_kernel(XenolithCore *s) {
    unsigned i;
    float sum = 0;
    if (s->control_remaining == 0) {
        erosion_grain(s);
        s->linear_tonic_bound=0.0f;
        for (i = 0; i < s->cells; ++i) {
            s->drift_noise[i] = noise_tick(s, &s->noise[i], cell_random(s, i));
            s->tonic[i] = (s->base * (1.0f + (s->drift_noise[i] * 0.10f * s->drift)))
                        * (s->cells <= 8u ? 1.0f : s->spread_scale[i]);
            if (s->cells <= 8u) s->tonic[i] += ((float)i * 0.46f)*s->spread_current;
            observe_tonic(s,s->tonic[i]);
            // Float-only target arithmetic: SC computes this control-frequency
            // increment in double. At extreme settings the quantized increment
            // can differ by a few phase units; reference tests disclose that.
            s->mod_increment[i] = phase_increment((s->tonic[i] * s->ratio) * s->phase_scale);
        }
        s->control_remaining = s->control_block;
    }
    {   /* Audio ownership fixes these values for the whole cell loop. Explicit
         * locals prevent alias-conservative per-cell scalar/count reloads. */
    uint32_t cells=s->cells,direct=direct_linear_phase(s);float depth=s->index,phase_scale=s->phase_scale;
    uint32_t ms=s->load_chosen_shift[0],cs=s->load_chosen_shift[1];
    uint32_t mt=s->calculation.target!=XCALC_CARRIER,ct=s->calculation.target!=XCALC_MOD;
    float mw=(!mt||s->calculation.interpolation)?s->interpolation_weight:0.0f;
    float cw=(!ct||s->calculation.interpolation)?s->interpolation_weight:0.0f;
    float mh=mt?s->calculation.harmonic:0.0f,ch=ct?s->calculation.harmonic:0.0f;
    union {uint32_t bits;float value;} mp,cp;float md,cd;
    mp.bits=(127u+ms)<<23;cp.bits=(127u+cs)<<23;
    md=s->load_dither*mp.value;cd=s->load_dither*cp.value;
    uint32_t full=(uint32_t)((float)cells*s->load_cell_ratio);
    uint32_t end=full+4u;if(end>cells)end=cells;
    for(i=0;i<full;i++){
        int32_t mo=md>0?load_offset(s->load_noise_word,i,0,md):0;
        int32_t co=cd>0?load_offset(s->load_noise_word,i,1,cd):0;
        float mod=load_lookup(s->mod_phase[i],ms,mw,mh,mo)*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=load_lookup(s->car_phase[i],cs,cw,ch,co)*.12f;
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<end;i++){
        int32_t mo=md>0?load_offset(s->load_noise_word,i,0,md):0;
        int32_t co=cd>0?load_offset(s->load_noise_word,i,1,cd):0;
        float mod=load_lookup(s->mod_phase[i],ms,mw,mh,mo)*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        sum+=(load_lookup(s->car_phase[i],cs,cw,ch,co)*.12f)*aperture_gain(s,i);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    for(;i<cells;i++){
        int32_t mo=md>0?load_offset(s->load_noise_word,i,0,md):0;
        float mod=load_lookup(s->mod_phase[i],ms,mw,mh,mo)*(s->tonic[i]*depth);
        int32_t increment=linear_increment((s->tonic[i]+mod)*phase_scale,direct);
        s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
        s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
    }
    }
    --s->control_remaining;
    return s->cells == XENOLITH_CELLS ? sum : sum * s->cell_gain;
}

/* Default sine bank under runtime interpolation relief. Keep the ordinary
 * kernel's control cadence and register footprint separate from mask/graph
 * work. The HOLD endpoint has one table read per oscillator. */
XENOLITH_RAM_AUDIO static float xenolith_parallel_relief_kernel(XenolithCore *s) {
    unsigned i;float sum=0.0f;
    if (!s->control_remaining) {
        s->linear_tonic_bound=0.0f;
        for(i=0;i<s->cells;i++){
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            s->tonic[i]=(s->base*(1.0f+(s->drift_noise[i]*0.10f*s->drift)))
                *(s->cells<=8u?1.0f:s->spread_scale[i]);
            if(s->cells<=8u)s->tonic[i]+=(float)i*.46f*s->spread_current;
            observe_tonic(s,s->tonic[i]);
            s->mod_increment[i]=phase_increment((s->tonic[i]*s->ratio)*s->phase_scale);
        }
        s->control_remaining=s->control_block;
    }
    {
        uint32_t cells=s->cells,direct=direct_linear_phase(s);
        float depth=s->index,scale=s->phase_scale,weight=s->interpolation_weight;
        if(weight==0.0f){
            for(i=0;i<cells;i++){
                float mod=CORE_SINE[(s->mod_phase[i]>>18)&2047u]*(s->tonic[i]*depth);
                float carrier=CORE_SINE[(s->car_phase[i]>>18)&2047u];
                int32_t increment=linear_increment((s->tonic[i]+mod)*scale,direct);
                s->graph_carrier[i]=carrier;sum+=carrier*.12f;
                s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
                s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
            }
        }else{
            for(i=0;i<cells;i++){
                float mod=sine_with_weight(s->mod_phase[i],weight)*(s->tonic[i]*depth);
                float carrier=sine_with_weight(s->car_phase[i],weight);
                int32_t increment=linear_increment((s->tonic[i]+mod)*scale,direct);
                s->graph_carrier[i]=carrier;sum+=carrier*.12f;
                s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
                s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
            }
        }
    }
    --s->control_remaining;
    return s->cells==XENOLITH_CELLS?sum:sum*s->cell_gain;
}

/* The optional table renderer needs no graph dispatch for an uncoupled linear
 * bank. Retain its carrier history and exact gain/order so later topology or
 * cell-mask changes observe the same state as the general renderer. */
XENOLITH_RAM_AUDIO static float xenolith_parallel_calculation_kernel(XenolithCore *s) {
    unsigned i;uint32_t all_open=1u;float sum=0.0f;
    uint32_t extent=s->count_smoothing?s->render_cells:s->cells;
    uint32_t fading=s->count_smoothing&&(s->count_gain_remaining||s->count_gain_finalize||extent>s->cells);
    if (!s->control_remaining) {
        s->linear_tonic_bound=0.0f;
        for (i=0;i<extent;++i) {
            count_layout_cell(s,i);
            s->drift_noise[i]=noise_tick(s,&s->noise[i],cell_random(s,i));
            prepare_gated_pitch(s,i);
        }
        s->control_remaining=s->control_block;
    }
    {
    float depth=s->index,phase_scale=s->phase_scale;
    for (i=0;i<extent;++i) {
        float gain=1.0f,carrier=0.0f;
        if (s->cell_gates_processing==1u && !(s->count_smoothing&&s->count_mask_free)) {
            uint32_t selected=(s->cell_gate_mask[i>>5]>>(i&31u))&1u;
            float target=s->cell_gates_enabled?(float)selected:1.0f;
            gain=s->cell_gate_gain[i];
            if (gain<target)gain=target-gain<=s->cell_gate_step?target:gain+s->cell_gate_step;
            else if (gain>target)gain=gain-target<=s->cell_gate_step?target:gain-s->cell_gate_step;
            s->cell_gate_gain[i]=gain;if (gain!=1.0f)all_open=0u;
        }
        if(fading&&i>=s->count_unity_prefix)gain*=count_gain_at(s,i);
        if (gain>0.0f) {
            float mod=calculated_sine(s,s->mod_phase[i],XCALC_MOD);
            float frequency=s->tonic[i]+mod*(s->tonic[i]*depth);
            int32_t increment;
            /* Preserve the graph path's signed-zero addition as well. */
            frequency+=s->tonic[i]*0.0f;
            carrier=calculated_sine(s,s->car_phase[i],XCALC_CARRIER)*gain;
            increment=phase_increment(frequency*phase_scale);
            s->mod_phase[i]=(s->mod_phase[i]+(uint32_t)s->mod_increment[i])&PHASE_MASK;
            s->car_phase[i]=(s->car_phase[i]+(uint32_t)increment)&PHASE_MASK;
        }
        s->graph_carrier[i]=carrier;sum+=carrier*0.12f;
    }
    }
    --s->control_remaining;
    if(all_open&&s->count_smoothing&&!s->cell_gates_enabled)s->count_mask_free=1u;
    if (s->cell_gates_processing==1u && all_open&&!s->count_smoothing)s->cell_gates_processing=s->cell_gates_enabled?2u:0u;
    return s->cells==XENOLITH_CELLS&&!s->count_smoothing?sum:sum*s->cell_gain;
}

/* Parallel count changes retain their established gain/layout ramps without
 * entering the general graph renderer's larger stack/register frame. */
XENOLITH_RAM_AUDIO static float xenolith_parallel_count_kernel(XenolithCore *s) {
    count_transition_tick(s);
    if(s->cell_gates_processing)return sample_cell_gates(s);
    return xenolith_parallel_kernel(s);
}

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
static float (*volatile erosion_graph_kernel)(XenolithCore *)=xenolith_erosion_graph_kernel;
#endif
XENOLITH_RAM_AUDIO static float xenolith_sample_kernel(XenolithCore *s) {
    if(s->count_smoothing)count_transition_tick(s);
    if(erosion_in_use(s)){
        if(s->load_erosion_active==2u&&s->interpolation_weight==1.0f&&
            !(s->count_smoothing|s->calculation_active|core_growth(s)|
              s->topology.mode|s->cell_gates_processing))return xenolith_parallel_aperture_kernel(s);
        if(!(s->count_smoothing|core_growth(s)|s->topology.mode|s->cell_gates_processing)){
            if(s->interpolation_weight==0.0f&&s->load_dither==0.0f&&s->calculation.harmonic==0.0f)
                return xenolith_parallel_hold_erosion_kernel(s);
            return xenolith_parallel_erosion_kernel(s);
        }
#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
        return erosion_graph_kernel(s);
#else
        return xenolith_erosion_graph_kernel(s);
#endif
    }
    if(s->calculation_active||s->interpolation_weight!=1.0f){
        if(!s->calculation_active&&!(s->count_smoothing|core_growth(s)|
            s->topology.mode|s->cell_gates_processing))return xenolith_parallel_relief_kernel(s);
        if(!(core_growth(s)|s->topology.mode))return xenolith_parallel_calculation_kernel(s);
        return sample_calculation_graph(s);
    }
    if(core_growth(s)|s->topology.mode)return sample_graph(s);
    if(s->cell_gates_processing)return sample_cell_gates(s);
    return xenolith_parallel_kernel(s);
}

#if defined(__PI32V2__) || defined(__GCC_PI32V2__)
/* A direct XIP tail jump cannot span the RAMTEXT address gap on pi32v2.
 * One indirect XIP trampoline retains the public API and a call-free RAM body. */
static float (*volatile sample_kernel)(XenolithCore *)=xenolith_sample_kernel;
static float (*volatile parallel_kernel)(XenolithCore *)=xenolith_parallel_kernel;
static float (*volatile parallel_count_kernel)(XenolithCore *)=xenolith_parallel_count_kernel;
static float (*volatile parallel_relief_kernel)(XenolithCore *)=xenolith_parallel_relief_kernel;
float xenolith_sample(XenolithCore *s) {
    if(s->base_glide_frames>0.0f||s->cell_glide_frames>0.0f)performance_tick(s);
    if(s->flavour_active||s->coupling_active){
        if(s->count_smoothing)count_transition_tick(s);
        return xenolith_erosion_graph_kernel(s);
    }
    if(!s->load_erosion_active&&s->interpolation_weight==1.0f&&!(s->count_smoothing|s->calculation_active|core_growth(s)|
         s->topology.mode|s->cell_gates_processing))return parallel_kernel(s);
    if(!s->load_erosion_active&&s->interpolation_weight!=1.0f&&!(s->count_smoothing|s->calculation_active|core_growth(s)|
         s->topology.mode|s->cell_gates_processing))return parallel_relief_kernel(s);
    if(s->count_smoothing&&s->count_mask_free&&!s->cell_gates_enabled&&!s->load_erosion_active&&
       s->interpolation_weight==1.0f&&!(s->calculation_active|core_growth(s)|s->topology.mode))
        return parallel_count_kernel(s);
    return sample_kernel(s);
}
#else
float xenolith_sample(XenolithCore *s) {
    if(s->base_glide_frames>0.0f||s->cell_glide_frames>0.0f)performance_tick(s);
    if(s->flavour_active||s->coupling_active){
        if(s->count_smoothing)count_transition_tick(s);
        return xenolith_erosion_graph_kernel(s);
    }
    if(!s->load_erosion_active&&s->interpolation_weight==1.0f&&!(s->count_smoothing|s->calculation_active|core_growth(s)|
         s->topology.mode|s->cell_gates_processing))return xenolith_parallel_kernel(s);
    if(!s->load_erosion_active&&s->interpolation_weight!=1.0f&&!(s->count_smoothing|s->calculation_active|core_growth(s)|
         s->topology.mode|s->cell_gates_processing))return xenolith_parallel_relief_kernel(s);
    if(s->count_smoothing&&s->count_mask_free&&!s->cell_gates_enabled&&!s->load_erosion_active&&
       s->interpolation_weight==1.0f&&!(s->calculation_active|core_growth(s)|s->topology.mode))
        return xenolith_parallel_count_kernel(s);
    return xenolith_sample_kernel(s);
}
#endif
