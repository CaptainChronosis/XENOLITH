/* SPDX-License-Identifier: GPL-3.0-or-later
 * XX29 independently written modulation equations, 2026-11-01.
 * Conceptual references/origin: docs/MODULATION_ENGINE_2026-11-01.md.
 * Uses existing MPL-2.0 fm1-platform fastmath at the 32-sample control tick.
 */
#include "xenolith_modulation.h"
#include "fastmath.h"

enum { IDLE, RISING, FALLING, SUSTAIN };
static void clear_bytes(void *memory,uint32_t bytes) {
    uint8_t *p=(uint8_t *)memory;
    while(bytes--)*p++=0;
}
static int finite(float x) {
    fm_bits_t bits;bits.f=x;
    return (bits.u&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);
}
static int range(float x,float lo,float hi) { return finite(x) && x>=lo && x<=hi; }
static float bound(float x,float lo,float hi) {
    if (!finite(x)) return 0.0f<lo?lo:0.0f>hi?hi:0.0f;
    return x<lo?lo:x>hi?hi:x;
}
static float bipolar(float x) { return bound(x,-1.0f,1.0f); }
static float unipolar(float x) { return bound(x,0.0f,1.0f); }
static uint32_t phase(float x) {
    x=unipolar(x);
    return x>=1.0f?UINT32_MAX:(uint32_t)(x*4294967296.0f);
}
static float value(uint32_t x) { return (float)x*(1.0f/4294967296.0f); }
static float source(const XenolithMod *s,unsigned id) {
    if(id>=XM_SOURCE_COUNT)return 0.0f;
    return id>=XM_SRC_SEQ_1?unipolar(s->source[id]):bipolar(s->source[id]);
}
float xenolith_mod_route_value(unsigned id,float raw) {
    if(id==XM_SRC_NONE || id>=XM_SOURCE_COUNT || !finite(raw))return 0.0f;
    if(id==XM_SRC_FUNCTION_A || id==XM_SRC_FUNCTION_B)return 2.0f*unipolar(raw)-1.0f;
    return id>=XM_SRC_SEQ_1?unipolar(raw):bipolar(raw);
}
static float routed_source(const XenolithMod *s,unsigned id) {
    if(id>=XM_SOURCE_COUNT)return 0.0f;
    if((id==XM_SRC_FUNCTION_A || id==XM_SRC_FUNCTION_B)
        && !s->config.function[id-XM_SRC_FUNCTION_A].enabled)return 0.0f;
    return xenolith_mod_route_value(id,s->source[id]);
}
static int boolean(unsigned x) { return x<=1u; }

XenolithModDestination xenolith_mod_destination(unsigned d) {
    XenolithModDestination result={0.0f,XM_UNIT_NONE,{0,0,0}};
    if (d==XM_DST_PITCH) { result.offset_limit=48.0f; result.unit=XM_UNIT_SEMITONES; }
    else if (d==XM_DST_FILTER_CUTOFF) { result.offset_limit=8.0f; result.unit=XM_UNIT_OCTAVES; }
    else if (d==XM_DST_CORE_RATE||d==XM_DST_WET_RATE||d==XM_DST_ECHO_TIME) { result.offset_limit=16.0f; result.unit=XM_UNIT_OCTAVES; }
    else if (d==XM_DST_CELLS) { result.offset_limit=63.0f; result.unit=XM_UNIT_CELLS; }
    else if (d==XM_DST_BITS) { result.offset_limit=23.0f; result.unit=XM_UNIT_BITS; }
    else if (d==XM_DST_RATIO || (d>=XM_DST_LFO_A_RATE && d<=XM_DST_RANDOM_RATE)
             || (d>=XM_DST_RANDOM_RISE && d<=XM_DST_B_FALL)) {
        result.offset_limit=d==XM_DST_RATIO?8.0f:16.0f; result.unit=XM_UNIT_OCTAVES;
    } else if (d>XM_DST_NONE && d<XM_DESTINATION_COUNT) {
        result.offset_limit=1.0f; result.unit=XM_UNIT_NORMALIZED;
    }
    return result;
}

void xenolith_mod_defaults(XenolithModConfig *c) {
    unsigned i;
    if (!c) return;
    clear_bytes(c,sizeof(*c));
    for (i=0;i<2;++i) {
        c->lfo[i].rate_hz=1.0f;
        c->function[i].rise_s=c->function[i].fall_s=0.1f;
        c->mix[i].gain_a=c->mix[i].gain_b=0.5f;
    }
    c->random.rate_hz=2.0f; c->random.spread=1.0f;
    c->random.rise_s=c->random.fall_s=0.1f; c->random.walk_spread=0.25f;
}

int xenolith_mod_validate(const XenolithModConfig *c) {
    unsigned i;
    if (!c || !boolean(c->enabled) || c->reserved[0] || c->reserved[1] || c->reserved[2]) return 0;
    for (i=0;i<2;++i) {
        const XenolithModLfo *l=&c->lfo[i];
        const XenolithModFunction *f=&c->function[i];
        const XenolithModMix *m=&c->mix[i];
        if (!range(l->rate_hz,0.01f,20.0f) || !range(l->phase,0.0f,1.0f)
            || l->shape>=XM_SHAPE_COUNT || !boolean(l->enabled)
            || l->reset_event>=XM_EVENT_COUNT || l->reserved) return 0;
        if (!range(f->rise_s,f->mode==XM_SLEW?0.0f:0.001f,60.0f)
            || !range(f->fall_s,f->mode==XM_SLEW?0.0f:0.001f,60.0f)
            || !range(f->curve,-1.0f,1.0f) || f->mode>=XM_FUNCTION_MODE_COUNT
            || f->input>=XM_SOURCE_COUNT || f->trigger_event>=XM_EVENT_COUNT
            || !boolean(f->enabled)) return 0;
        if (!range(m->gain_a,-1.0f,1.0f) || !range(m->gain_b,-1.0f,1.0f)
            || !range(m->offset,-1.0f,1.0f) || m->input_a>=XM_SOURCE_COUNT
            || m->input_b>=XM_SOURCE_COUNT || m->mode>=XM_MIX_MODE_COUNT
            || !boolean(m->enabled)) return 0;
    }
    {
        const XenolithModRandom *r=&c->random;
        if (!range(r->rate_hz,0.01f,50.0f) || !range(r->centre,-1.0f,1.0f)
            || !range(r->spread,0.0f,1.0f) || !range(r->correlate,0.0f,0.98f)
            || !range(r->bias,-0.98f,0.98f) || !range(r->weight,0.0f,1.0f)
            || !range(r->rise_s,0.0f,60.0f) || !range(r->fall_s,0.0f,60.0f)
            || !range(r->walk_spread,0.0f,1.0f)
            || !(r->levels==0 || (r->levels>=2 && r->levels<=24))
            || r->clock>=XM_CLOCK_COUNT || !boolean(r->hold) || !boolean(r->enabled)) return 0;
    }
    for (i=0;i<XENOLITH_MOD_ROUTES;++i) {
        const XenolithModRoute *r=&c->route[i];
        XenolithModDestination d=xenolith_mod_destination(r->destination);
        if (!boolean(r->enabled) || r->reserved || r->source>=XM_SOURCE_COUNT
            || r->destination>=XM_DESTINATION_COUNT
            || !range(r->depth,-d.offset_limit,d.offset_limit)) return 0;
    }
    for (i=0;i<XENOLITH_MOD_LINKS;++i) {
        const XenolithModLink *r=&c->link[i];
        if (!boolean(r->enabled) || r->reserved || r->event>=XM_EVENT_COUNT
            || r->target>=XM_LINK_COUNT) return 0;
    }
    return 1;
}

void xenolith_mod_init(XenolithMod *s,uint32_t seed) {
    if (!s) return;
    clear_bytes(s,sizeof(*s)); xenolith_mod_defaults(&s->config);
    s->random_state=seed?seed:UINT32_C(0x786d3239);
    xenolith_random_memory_init(&s->random_memory,s->random_state^UINT32_C(0x524e444d));
    s->smooth_phase=UINT32_C(0x80000000);
}

int xenolith_mod_random_apply(XenolithMod *s,const XenolithRandomMemoryConfig *c) {
    return s?xenolith_random_memory_apply(&s->random_memory,c):0;
}

int xenolith_mod_apply(XenolithMod *s,const XenolithModConfig *c) {
    if (!s || !xenolith_mod_validate(c)) return 0;
    s->config=*c;
    if (!c->enabled) {
        clear_bytes(s->source,sizeof(s->source)); clear_bytes(s->offset,sizeof(s->offset));
        clear_bytes(s->delayed_events,sizeof(s->delayed_events));
        s->quantum_position=0;
    }
    /* Enabled edits must not accelerate or restart the fixed control clock.
     * Derived coefficients/snapshots update at its next regular tick. */
    return 1;
}

/* Original local xorshift recurrence; state is exclusively this constructor's.
 * Three draws/event give uniform and independent two-uniform mean inputs.
 */
static float uniform(XenolithMod *s) {
    uint32_t x=s->random_state;
    if (!x) x=UINT32_C(0x786d3239);
    x^=x<<13; x^=x>>17; x^=x<<5; s->random_state=x;
    return (float)(x>>8)*(1.0f/16777216.0f);
}
static void random_next(XenolithMod *s) {
    const XenolithModRandom *r=&s->config.random;
    float u=uniform(s), a=uniform(s), b=uniform(s), mid, fresh, step, walk;
    u+=(0.5f*(a+b)-u)*r->weight;
    u=xenolith_random_memory_next(&s->random_memory,u,s->random_bias);
    mid=0.5f+0.5f*s->random_bias;
    u=u<0.5f?2.0f*u*mid:mid+(2.0f*u-1.0f)*(1.0f-mid);
    fresh=bipolar(s->random_centre+s->random_spread*(2.0f*u-1.0f));
    step=s->random_correlate*bipolar(s->random_step)+(1.0f-s->random_correlate)*fresh;
    if (r->levels) {
        float n=(float)(r->levels-1u);
        step=2.0f*(float)(unsigned)((unipolar(0.5f*(step+1.0f))*n)+0.5f)/n-1.0f;
    }
    s->random_step=bipolar(step);
    walk=bipolar(s->random_walk)+s->random_step*r->walk_spread;
    if (walk>1.0f) walk=2.0f-walk;
    else if (walk<-1.0f) walk=-2.0f-walk;
    s->random_walk=bipolar(walk);
}

/* Called only by tick. Bounds are checked again after summing routes. */
static float exponential(float base,float offset,float lo,float hi) {
    return bound(base*fm_exp2f(bound(offset,-16.0f,16.0f)),lo,hi);
}
static uint32_t rate_increment(float hz) { return (uint32_t)(hz*(4294967296.0f/44100.0f)); }
static uint32_t travel_increment(float seconds) {
    return seconds<=0.0f?UINT32_MAX:(uint32_t)(4294967296.0f/(44100.0f*seconds));
}
static float curve(float p,float amount) {
    p=unipolar(p); amount=bipolar(amount);
    return unipolar(amount>=0.0f?p+amount*(p*p-p):p-amount*((2.0f*p-p*p)-p));
}
static void tick(XenolithMod *s,const XenolithModInput *input) {
    float next[XM_SOURCE_COUNT]={0}; unsigned i;
    const XenolithModConfig *c=&s->config;
    clear_bytes(s->offset,sizeof(s->offset));
    for (i=0;i<XENOLITH_MOD_ROUTES;++i) {
        const XenolithModRoute *r=&c->route[i];
        if (r->enabled && r->source<XM_SOURCE_COUNT && r->destination>XM_DST_NONE
            && r->destination<XM_DESTINATION_COUNT) {
            float limit=xenolith_mod_destination(r->destination).offset_limit;
            s->offset[r->destination]+=routed_source(s,r->source)*bound(r->depth,-limit,limit);
        }
    }
    for (i=1;i<XM_DESTINATION_COUNT;++i) {
        float limit=xenolith_mod_destination(i).offset_limit;
        s->offset[i]=bound(s->offset[i],-limit,limit);
    }
    for (i=0;i<2;++i) {
        const XenolithModLfo *l=&c->lfo[i];
        const XenolithModFunction *f=&c->function[i];
        const XenolithModMix *m=&c->mix[i];
        unsigned ld=XM_DST_LFO_A_RATE+i, fd=XM_DST_A_RISE+2u*i, md=XM_DST_MIX_A_GAIN_A+3u*i;
        if (l->enabled) {
            uint32_t p=s->lfo_phase[i]+(l->phase>=1.0f?0u:phase(l->phase));
            float x=value(p), y;
            s->lfo_increment[i]=rate_increment(exponential(l->rate_hz,s->offset[ld],0.01f,20.0f));
            if (l->shape==XM_SINE) y=fm_sin_turns(x);
            else if (l->shape==XM_RAMP) y=2.0f*x-1.0f;
            else if (l->shape==XM_PULSE) y=x<0.5f?1.0f:-1.0f;
            else y=x<0.5f?4.0f*x-1.0f:3.0f-4.0f*x;
            next[XM_SRC_LFO_A+i]=bipolar(y);
        }
        if (f->enabled) {
            float lo=f->mode==XM_SLEW?0.0f:0.001f;
            s->function_input[i]=source(s,f->input);
            s->function_rise[i]=travel_increment(exponential(f->rise_s,s->offset[fd],lo,60.0f));
            s->function_fall[i]=travel_increment(exponential(f->fall_s,s->offset[fd+1],lo,60.0f));
            next[XM_SRC_FUNCTION_A+i]=curve(value(s->function_phase[i]),f->curve);
        }
        if (m->enabled) {
            float a=routed_source(s,m->input_a)*bipolar(m->gain_a+s->offset[md]);
            float b=routed_source(s,m->input_b)*bipolar(m->gain_b+s->offset[md+1]);
            float y=m->mode==XM_PRODUCT?a*b:m->mode==XM_MAX?(a>b?a:b):a+b;
            next[XM_SRC_MIX_A+i]=bipolar(y+bipolar(m->offset+s->offset[md+2]));
        }
    }
    if (c->random.enabled) {
        const XenolithModRandom *r=&c->random;
        s->random_increment=rate_increment(exponential(r->rate_hz,s->offset[XM_DST_RANDOM_RATE],0.01f,50.0f));
        s->random_centre=bipolar(r->centre+s->offset[XM_DST_RANDOM_CENTRE]);
        s->random_spread=unipolar(r->spread+s->offset[XM_DST_RANDOM_SPREAD]);
        s->random_correlate=bound(r->correlate+s->offset[XM_DST_RANDOM_CORRELATE],0.0f,0.98f);
        s->random_bias=bound(r->bias+s->offset[XM_DST_RANDOM_BIAS],-0.98f,0.98f);
        s->smooth_rise=travel_increment(exponential(r->rise_s,s->offset[XM_DST_RANDOM_RISE],0.0f,60.0f));
        s->smooth_fall=travel_increment(exponential(r->fall_s,s->offset[XM_DST_RANDOM_FALL],0.0f,60.0f));
        next[XM_SRC_STEP]=bipolar(s->random_step);
        next[XM_SRC_SMOOTH]=bipolar(2.0f*value(s->smooth_phase)-1.0f);
        next[XM_SRC_WALK]=bipolar(s->random_walk);
    }
    next[XM_SRC_KEY]=bipolar(input->key);
    for(i=0;i<4u;++i)
        next[XM_SRC_SEQ_1+i]=(input->sequence_mask&(1u<<i))?unipolar(input->sequence[i]):0.0f;
    for(i=0;i<4u;i++)next[XM_SRC_ENV_AMP+i]=(input->envelope_mask&(1u<<i))?unipolar(input->envelope[i]):0.0f;
    for(i=0;i<XM_SOURCE_COUNT;++i)s->source[i]=next[i];
}

static uint32_t move(uint32_t p,uint32_t target,uint32_t rise,uint32_t fall) {
    if (p<target) return target-p<=rise?target:p+rise;
    return p-target<=fall?target:p-fall;
}
static uint32_t function_sample(XenolithMod *s,unsigned i,int trigger,int gate) {
    const XenolithModFunction *f=&s->config.function[i];
    uint32_t old=s->function_phase[i], p=old, event=0;
    uint8_t *stage=&s->function_stage[i];
    if (!f->enabled) return 0;
    if (f->mode==XM_SLEW) {
        uint32_t target=phase(s->function_input[i]);
        p=move(p,target,s->function_rise[i],s->function_fall[i]);
        if (p==target && old!=target)
            event=XM_EVENT_BIT((old<target?XM_EVENT_A_RISE:XM_EVENT_A_FALL)+2u*i);
        *stage=IDLE;
    } else {
        if (f->mode==XM_GATE) {
            int high=f->input==XM_SRC_NONE?gate:s->function_input[i]>0.5f;
            if (high || trigger) { if (*stage!=SUSTAIN) *stage=RISING; }
            else if (p || *stage==RISING || *stage==SUSTAIN) *stage=FALLING;
        } else if (f->mode==XM_CYCLE) {
            if (*stage==IDLE || *stage==SUSTAIN) *stage=RISING;
            if (trigger && *stage!=RISING) *stage=RISING;
        } else {
            if (*stage==SUSTAIN) *stage=FALLING;
            if (trigger && *stage!=RISING) *stage=RISING;
        }
        if (*stage==RISING) {
            p=move(p,UINT32_MAX,s->function_rise[i],s->function_fall[i]);
            if (p==UINT32_MAX) {
                event=XM_EVENT_BIT(XM_EVENT_A_RISE+2u*i);
                *stage=f->mode==XM_GATE?SUSTAIN:FALLING;
            }
        } else if (*stage==FALLING) {
            p=move(p,0,s->function_rise[i],s->function_fall[i]);
            if (!p) {
                event=XM_EVENT_BIT(XM_EVENT_A_FALL+2u*i);
                *stage=f->mode==XM_CYCLE?RISING:IDLE;
            }
        }
    }
    s->function_phase[i]=p;
    return event;
}

uint32_t xenolith_mod_advance(XenolithMod *s,const XenolithModInput *input) {
    static const XenolithModInput zero={0};
    const XenolithModInput *in=input?input:&zero;
    uint32_t delayed, events, trigger_events, actions, output=0; unsigned i, slot;
    if (!s || !s->config.enabled) return 0;
    slot=s->sample&(XENOLITH_MOD_QUANTUM-1u);
    delayed=s->delayed_events[slot];
    events=in->events&(XM_EVENT_BIT(XM_EVENT_KEY)|XM_EVENT_BIT(XM_EVENT_STEP));
    /* Direct KEY/STEP selections act once on the external sample. Only END
     * selections read delayed events; the external bits in the ring belong
     * exclusively to trigger links, otherwise an edge would fire twice. */
    trigger_events=events|(delayed&~(XM_EVENT_BIT(XM_EVENT_KEY)|XM_EVENT_BIT(XM_EVENT_STEP)));
    actions=in->actions&(XM_FIRE_A|XM_FIRE_B|XM_NEXT|XM_RESET_A|XM_RESET_B);
    /* Every link reads the same delayed event set once; new events never
     * enter this dispatch, including self links and two-function cycles. */
    for (i=0;i<XENOLITH_MOD_LINKS;++i) {
        const XenolithModLink *r=&s->config.link[i];
        if (r->enabled && r->event>XM_EVENT_NONE && r->event<XM_EVENT_COUNT
            && (delayed&XM_EVENT_BIT(r->event))) {
            if (r->target==XM_LINK_A) actions|=XM_FIRE_A;
            else if (r->target==XM_LINK_B) actions|=XM_FIRE_B;
            else if (r->target==XM_LINK_RANDOM) actions|=XM_NEXT;
        }
    }
    for (i=0;i<2;++i) {
        unsigned e=s->config.lfo[i].reset_event;
        if ((actions&(XM_RESET_A<<i)) || (e>XM_EVENT_NONE && e<XM_EVENT_COUNT
            && (trigger_events&XM_EVENT_BIT(e)))) s->lfo_phase[i]=0;
    }
    if (!s->quantum_position) tick(s,in);
    for (i=0;i<2;++i) {
        unsigned e=s->config.function[i].trigger_event;
        int fire=(actions&(XM_FIRE_A<<i))!=0;
        if (e>XM_EVENT_NONE && e<XM_EVENT_COUNT && (trigger_events&XM_EVENT_BIT(e))) fire=1;
        output|=function_sample(s,i,fire,in->gate!=0);
        if (s->config.lfo[i].enabled) s->lfo_phase[i]+=s->lfo_increment[i];
    }
    if (s->config.random.enabled) {
        const XenolithModRandom *r=&s->config.random;
        uint32_t old=s->random_phase;
        int next=0;
        if (r->clock==XM_CLOCK_FREE) {
            s->random_phase+=s->random_increment;
            next=s->random_phase<old;
        } else if (r->clock==XM_CLOCK_STEP) next=(events&XM_EVENT_BIT(XM_EVENT_STEP))!=0;
        else if (r->clock==XM_CLOCK_KEY) next=(events&XM_EVENT_BIT(XM_EVENT_KEY))!=0;
        else if (r->clock==XM_CLOCK_FUNCTION)
            next=(delayed&(XM_EVENT_BIT(XM_EVENT_A_RISE)|XM_EVENT_BIT(XM_EVENT_A_FALL)
                           |XM_EVENT_BIT(XM_EVENT_B_RISE)|XM_EVENT_BIT(XM_EVENT_B_FALL)))!=0;
        if ((next && !r->hold) || (actions&XM_NEXT)) random_next(s);
        s->smooth_phase=move(s->smooth_phase,phase(0.5f*(bipolar(s->random_step)+1.0f)),s->smooth_rise,s->smooth_fall);
    }
    s->delayed_events[slot]=events|output;
    s->quantum_position=(s->quantum_position+1u)&(XENOLITH_MOD_QUANTUM-1u);
    ++s->sample;
    return output;
}

_Static_assert(sizeof(float)==4,"modulation requires 32-bit float");
_Static_assert(sizeof(XenolithModRoute)==8,"explicit route layout");
_Static_assert(sizeof(XenolithModLink)==4,"explicit trigger layout");
_Static_assert(sizeof(XenolithModConfig)==228,"modulation configuration storage layout changed");
_Static_assert(XM_SRC_KEY==10 && XM_SRC_SEQ_1==11 && XM_SRC_SEQ_4==14,"source IDs must append");
_Static_assert(XM_DST_FOLD==33,"FOLD destination must append");
_Static_assert(XM_DST_MIX_B_OFFSET==24 && XM_DST_AMPLITUDE==25,"destination IDs must append");
_Static_assert(sizeof(XenolithMod)+sizeof(XenolithModConfig)<=3072,"modulation working-set budget");
