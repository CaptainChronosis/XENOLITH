/* SPDX-License-Identifier: MPL-2.0
 * XX29 independently implemented approximate 6SL7/parallel-T feedback cell.
 * LP/BP/HP/NOTCH/PHASE are added ideal-buffered taps of the same nonlinear
 * circuit; TWIN T retains the plate-output response. No third-party code.
 * Source circuit, reductions and limits: docs/SHARED_FILTER_BASE_2026-11-02.md. */
#include "xenolith_filter.h"

/* float32(tan(pi * .45 * i/256)), generated offline with Python math.
 * A repeated endpoint is unnecessary: interpolation clamps index to255. */
static const float tangent[257]={
    0.0f,0.0055223871f,0.0110451104f,0.016568508f,0.0220929161f,0.0276186727f,
    0.0331461169f,0.0386755839f,0.0442074127f,0.0497419424f,0.055279512f,0.0608204678f,
    0.0663651451f,0.0719138905f,0.0774670392f,0.0830249414f,0.0885879472f,0.0941563994f,
    0.0997306406f,0.105311029f,0.110897914f,0.116491638f,0.122092567f,0.127701059f,
    0.133317471f,0.138942137f,0.144575462f,0.150217786f,0.155869469f,0.161530897f,
    0.167202443f,0.172884464f,0.178577349f,0.184281468f,0.189997211f,0.195724949f,
    0.2014651f,0.207218036f,0.212984145f,0.218763828f,0.224557504f,0.230365574f,
    0.236188427f,0.242026493f,0.247880191f,0.253749937f,0.259636164f,0.265539259f,
    0.271459728f,0.27739796f,0.283354431f,0.289329559f,0.295323789f,0.30133763f,
    0.307371527f,0.313425928f,0.31950134f,0.32559821f,0.331717044f,0.337858349f,
    0.344022602f,0.350210339f,0.356422037f,0.362658232f,0.368919492f,0.375206292f,
    0.381519198f,0.387858748f,0.394225538f,0.400620103f,0.40704301f,0.413494885f,
    0.419976264f,0.426487803f,0.433030069f,0.439603716f,0.446209341f,0.4528476f,
    0.459519148f,0.46622467f,0.472964764f,0.479740202f,0.486551613f,0.493399709f,
    0.500285268f,0.507208943f,0.514171541f,0.521173775f,0.528216422f,0.535300314f,
    0.542426169f,0.549594879f,0.55680722f,0.564064026f,0.571366191f,0.578714609f,
    0.586110115f,0.593553722f,0.601046205f,0.608588636f,0.61618191f,0.62382704f,
    0.63152504f,0.639276922f,0.647083759f,0.654946566f,0.662866473f,0.670844555f,
    0.678881943f,0.68697983f,0.695139468f,0.703361928f,0.711648524f,0.720000505f,
    0.728419185f,0.736905873f,0.745461881f,0.75408864f,0.76278758f,0.771560073f,
    0.780407667f,0.789331853f,0.798334181f,0.807416201f,0.81657964f,0.825826049f,
    0.835157216f,0.844574809f,0.854080677f,0.863676667f,0.873364627f,0.883146465f,
    0.893024206f,0.902999818f,0.913075447f,0.923253238f,0.933535278f,0.94392395f,
    0.954421461f,0.965030193f,0.975752652f,0.98659128f,0.99754864f,1.00862741f,
    1.01983023f,1.03116f,1.04261959f,1.05421185f,1.0659399f,1.07780683f,
    1.08981586f,1.10197031f,1.11427367f,1.12672937f,1.139341f,1.15211225f,
    1.16504717f,1.17814958f,1.19142365f,1.20487344f,1.21850348f,1.23231828f,
    1.24632227f,1.26052046f,1.2749176f,1.28951895f,1.30432975f,1.31935549f,
    1.33460176f,1.35007441f,1.36577964f,1.3817234f,1.3979125f,1.41435337f,
    1.43105316f,1.44801903f,1.46525836f,1.48277903f,1.50058889f,1.51869655f,
    1.53711045f,1.55583954f,1.57489336f,1.59428144f,1.61401403f,1.63410151f,
    1.65455484f,1.67538536f,1.69660497f,1.71822596f,1.74026108f,1.76272404f,
    1.78562844f,1.80898917f,1.83282137f,1.8571409f,1.88196445f,1.90730929f,
    1.93319392f,1.95963705f,1.98665881f,2.01428008f,2.04252267f,2.0714097f,
    2.10096526f,2.13121486f,2.16218472f,2.19390345f,2.2263999f,2.25970531f,
    2.29385281f,2.32887626f,2.36481237f,2.40169954f,2.43957829f,2.47849178f,
    2.51848507f,2.55960655f,2.60190725f,2.64544129f,2.69026613f,2.73644304f,
    2.78403711f,2.83311725f,2.88375759f,2.93603706f,2.99003983f,3.04585576f,
    3.10358167f,3.16332078f,3.2251842f,3.28929114f,3.35577035f,3.42475986f,
    3.49640942f,3.57088017f,3.64834714f,3.72899985f,3.81304407f,3.90070415f,
    3.99222374f,4.08786964f,4.18793392f,4.29273605f,4.40262794f,4.51799631f,
    4.63926935f,4.76691914f,4.90147161f,5.04351091f,5.19368935f,5.35273647f,
    5.52147293f,5.70082331f,5.89183283f,6.09569073f,6.3137517f
};

static int within(float x,float lo,float hi){return x>=lo && x<=hi;}
static float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static int finite_value(float x){union{float f;uint32_t u;}v;v.f=x;return (v.u&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);}
static uint32_t ramp_length(float rate){return (uint32_t)(rate*.005f+.5f);}
static uint32_t load_ramp_length(float rate){return (uint32_t)(rate*.010f+.5f);}
static float cutoff_bound(float value,float rate){return clamp(value,20.0f,rate*.45f<20000.0f?rate*.45f:20000.0f);}
/* Radiotron Fig 15.15 + nominal Seely 6SL7 parameters, with gm=mu/rp.
 * Plate/cathode local current feedback is retained; T-port loading is not. */
#define TUBE_RHO (1281.0f/220.0f)
#define TUBE_GAIN (1501.0f/220.0f)
#define TUBE_LAMBDA_MAX (398993.0f/58539.0f)
static float table_g(float cutoff,float inverse_rate){
    float position=cutoff*inverse_rate*(256.0f/.45f),fraction;uint32_t i;
    position=clamp(position,0.0f,256.0f);i=(uint32_t)position;
    if(i>255u)i=255u;fraction=position-(float)i;
    return tangent[i]+fraction*(tangent[i+1u]-tangent[i]);
}
static void response(uint32_t mode,float out[3]){
    out[0]=(mode==XFILTER_HP||mode==XFILTER_NOTCH||mode==XFILTER_ALLPASS)?1.0f:0.0f;
    out[1]=mode==XFILTER_BP?1.0f:mode==XFILTER_ALLPASS?-2.0f:(mode==XFILTER_HP||mode==XFILTER_NOTCH)?-1.0f:0.0f;
    out[2]=mode==XFILTER_LP?1.0f:mode==XFILTER_HP?-1.0f:0.0f;
}
void xenolith_filter_defaults(XenolithFilterConfig *c){
    if(!c)return;c->placement=XFILTER_OFF;c->mode=XFILTER_LP;c->slope=XFILTER_TWO_POLE;
    c->cutoff_hz=8000.0f;c->resonance=0.0f;
}
int xenolith_filter_config_valid(const XenolithFilterConfig *c){
    return c&&c->placement<XFILTER_PLACEMENT_COUNT&&c->mode<XFILTER_MODE_COUNT
        &&c->slope<XFILTER_SLOPE_COUNT&&within(c->cutoff_hz,20.0f,20000.0f)
        &&within(c->resonance,0.0f,1.0f);
}
void xenolith_filter_circuit_defaults(XenolithFilterCircuitConfig *c){
    if(!c)return;c->cathode_ohms=2200.0f;c->headroom=.25f;
    c->coupling_hz=3.183098862f;c->balance=1.0f;
}
int xenolith_filter_circuit_valid(const XenolithFilterCircuitConfig *c){
    return c&&within(c->cathode_ohms,470.0f,10000.0f)&&within(c->headroom,.0625f,1.0f)
        &&within(c->coupling_hz,.5f,100.0f)&&within(c->balance,.5f,1.0f);
}
static int same_circuit(const XenolithFilterCircuitConfig *a,const XenolithFilterCircuitConfig *b){
    return a->cathode_ohms==b->cathode_ohms&&a->headroom==b->headroom
        &&a->coupling_hz==b->coupling_hz&&a->balance==b->balance;
}
int xenolith_filter_set_circuit(XenolithFilter *s,const XenolithFilterCircuitConfig *c){
    if(!s||!xenolith_filter_circuit_valid(c))return 0;
    if(same_circuit(&s->circuit_target,c))return 1;
    s->circuit_target=*c;s->circuit_remaining=(uint32_t)(s->sample_rate*.02f+.5f);
    s->control_remaining=0u;return 1;
}
int xenolith_filter_prepare(XenolithFilterPrepared *out,const XenolithFilterConfig *c,float rate){
    XenolithFilterPrepared p;
    if(!out||!xenolith_filter_config_valid(c)||!within(rate,1000.0f,192000.0f))return 0;
    p.config=*c;p.sample_rate=rate;p.effective_cutoff_hz=cutoff_bound(c->cutoff_hz,rate);
    p.ramp_samples=ramp_length(rate);*out=p;return 1;
}
void xenolith_filter_clear(XenolithFilter *s){
    uint32_t channel,stage;if(!s)return;
    for(channel=0u;channel<2u;channel++)for(stage=0u;stage<2u;stage++){
        s->section[channel][stage].band=0.0f;s->section[channel][stage].low=0.0f;
        s->tube_dc[channel][stage]=0.0f;
    }
}
static void coefficients(XenolithFilter *s){
    float lower=s->cutoff_hz*(1.0f-.2928932188f*s->stagger);
    float upper=s->cutoff_hz*(1.0f+.4142135624f*s->stagger);
    float maximum=TUBE_LAMBDA_MAX,b=s->circuit.balance;
    float resonance=s->resonance,headroom=s->circuit.headroom,cathode=s->circuit.cathode_ohms;
    if(s->load_sag>0.0f&&s->load_shape>0.0f){
        float colour=s->load_sag*s->load_shape,loss=.18f*colour;
        /* Bounded movement within the existing circuit's parameter range,
         * not a new supply/rail model. Affect low cutoffs too; preserve the
         * saved and CV bases and exact zero-stress expression below. */
        lower*=1.0f-loss;upper*=1.0f-loss;
        resonance*=1.0f-.12f*colour;
        headroom=clamp(headroom*(1.0f-.10f*colour),.0625f,1.0f);
        cathode=clamp(cathode*(1.0f+.15f*colour),470.0f,10000.0f);
    }
    if(upper>s->sample_rate*.45f)upper=s->sample_rate*.45f;
    s->g=table_g(lower,s->inverse_rate);s->k=b==1.0f?4.0f:1.0f/b+1.0f+2.0f*b;
    s->tube_notch_band=2.0f+2.0f*b;
    s->tube_rho=TUBE_RHO;s->tube_gain=TUBE_GAIN;
    if(cathode!=2200.0f){
        s->tube_rho=(70.0f/44000.0f)*(cathode*(71.0f/70.0f)+100000.0f/70.0f);
        s->tube_gain=1.0f+s->tube_rho;
        maximum=7.0f*(1.0f+s->tube_rho*1.21875f)/(s->tube_gain*1.21875f);
    }
    s->tube_input_scale=.625f/headroom;s->tube_output_scale=headroom/.625f;
    s->inverse_denominator=1.0f/(1.0f+s->g*(s->g+s->k));
    if(s->stagger==0.0f){s->g2=s->g;s->inverse_denominator2=s->inverse_denominator;}
    else{s->g2=table_g(upper,s->inverse_rate);s->inverse_denominator2=1.0f/(1.0f+s->g2*(s->g2+s->k));}
    {
        s->tube_feedback=s->target.config.mode==XFILTER_TWIN_T?maximum*resonance:
            1.0f+(maximum-1.0f)*resonance;
        s->tube_band_scale=s->tube_output_scale*(s->k+s->tube_feedback*(1.0f/b-1.0f));
        s->tube_notch_scale=s->tube_output_scale*s->tube_notch_band;
        s->tube_linear[0]=1.0f/(s->tube_gain*(1.0f+s->tube_feedback*(1.0f-s->tube_notch_band*s->g*s->inverse_denominator)));
        s->tube_linear[1]=1.0f/(s->tube_gain*(1.0f+s->tube_feedback*(1.0f-s->tube_notch_band*s->g2*s->inverse_denominator2)));
        /* These products are constant between coefficient ticks. Retain the
         * sample expression's float operation order for exact PCM. */
        s->tube_feedback_total[0]=s->tube_rho+s->tube_gain*s->tube_feedback*(1.0f-s->tube_notch_band*s->g*s->inverse_denominator);
        s->tube_feedback_total[1]=s->tube_rho+s->tube_gain*s->tube_feedback*(1.0f-s->tube_notch_band*s->g2*s->inverse_denominator2);
        s->tube_g_plus_k[0]=s->g+s->k;s->tube_g_plus_k[1]=s->g2+s->k;
        s->tube_excitation_scale=s->tube_output_scale*(1.0f+s->tube_feedback);
        s->tube_input_feedback=s->tube_notch_band*s->tube_feedback;
        s->tube_output_feedback=s->tube_notch_scale*s->tube_feedback;
        {float g=table_g(s->circuit.coupling_hz,s->inverse_rate);s->tube_dc_coefficient=g/(1.0f+g);}
    }
}
void xenolith_filter_init(XenolithFilter *s){
    uint32_t i;XenolithFilterConfig c;if(!s)return;
    s->load_sag=s->solver_linear_mix=0.0f;s->load_shape=1.0f;
    s->load_sag_target=0.0f;s->load_shape_target=1.0f;s->load_remaining=s->tube_early_solves=0u;
    xenolith_filter_defaults(&c);xenolith_filter_prepare(&s->target,&c,44100.0f);
    s->sample_rate=44100.0f;s->inverse_rate=1.0f/44100.0f;
    s->cutoff_hz=s->cutoff_target=c.cutoff_hz;s->resonance=s->resonance_target=c.resonance;
    response(c.mode,s->mix);for(i=0u;i<3u;i++)s->mix_target[i]=s->mix[i];
    s->slope=s->slope_target=s->stagger=s->stagger_target=s->wet=s->wet_target=s->wet_step=0.0f;
    s->remaining=s->control_remaining=s->wet_remaining=s->faults=0u;
    s->tube_residual_peak=0.0f;s->tube_solver_faults=0u;
    s->tube_feedback=s->tube_dc_coefficient=0.0f;s->tube_linear[0]=s->tube_linear[1]=1.0f;
    xenolith_filter_circuit_defaults(&s->circuit);s->circuit_target=s->circuit;s->circuit_remaining=0u;
    xenolith_filter_clear(s);coefficients(s);
}
static int same_config(const XenolithFilterPrepared *a,const XenolithFilterPrepared *b){
    return a->config.placement==b->config.placement&&a->config.mode==b->config.mode
        &&a->config.slope==b->config.slope&&a->config.cutoff_hz==b->config.cutoff_hz
        &&a->config.resonance==b->config.resonance&&a->sample_rate==b->sample_rate;
}
int xenolith_filter_retime(XenolithFilter *s,float rate){
    float ratio;if(!s||!within(rate,1000.0f,192000.0f))return 0;
    if(rate==s->sample_rate)return 1;ratio=rate/s->sample_rate;
    s->remaining=(uint32_t)((float)s->remaining*ratio+.5f);
    if(s->load_remaining){
        s->load_remaining=(uint32_t)((float)s->load_remaining*ratio+.5f);
        if(!s->load_remaining)s->load_remaining=1u;
    }
    if(s->circuit_remaining){
        s->circuit_remaining=(uint32_t)((float)s->circuit_remaining*ratio+.5f);
        if(!s->circuit_remaining)s->circuit_remaining=1u;
    }
    if(s->wet_remaining){
        s->wet_remaining=(uint32_t)((float)s->wet_remaining*ratio+.5f);
        if(!s->wet_remaining){s->wet=s->wet_target;s->wet_step=0.0f;}
        else s->wet_step=(s->wet_target-s->wet)/(float)s->wet_remaining;
    }
    s->sample_rate=rate;s->inverse_rate=1.0f/rate;
    s->cutoff_hz=cutoff_bound(s->cutoff_hz,rate);
    if(!s->remaining&&s->cutoff_hz!=cutoff_bound(s->cutoff_target,rate))s->remaining=ramp_length(rate);
    s->control_remaining=0u;coefficients(s);return 1;
}
int xenolith_filter_apply(XenolithFilter *s,const XenolithFilterPrepared *p){
    uint32_t i,placement_changed,domain_changed;
    if(!s||!p||!xenolith_filter_config_valid(&p->config)||!within(p->sample_rate,1000.0f,192000.0f)
       ||p->effective_cutoff_hz!=cutoff_bound(p->config.cutoff_hz,p->sample_rate)
       ||p->ramp_samples!=ramp_length(p->sample_rate))return 0;
    if(same_config(&s->target,p))return 1;
    placement_changed=p->config.placement!=s->target.config.placement;
    domain_changed=(p->config.mode==XFILTER_TWIN_T)!=(s->target.config.mode==XFILTER_TWIN_T);
    xenolith_filter_retime(s,p->sample_rate);s->target=*p;
    s->cutoff_target=p->config.cutoff_hz;s->resonance_target=p->config.resonance;
    response(p->config.mode,s->mix_target);s->slope_target=p->config.slope==XFILTER_TWO_POLE?0.0f:1.0f;
    s->stagger_target=p->config.slope==XFILTER_BRIDGE?1.0f:0.0f;
    s->remaining=ramp_length(s->sample_rate);s->control_remaining=0u;
    if(placement_changed||domain_changed){
        /* The parent closes its structural gate before moving this route. */
        xenolith_filter_clear(s);s->cutoff_hz=p->effective_cutoff_hz;s->resonance=p->config.resonance;
        for(i=0u;i<3u;i++)s->mix[i]=s->mix_target[i];s->slope=s->slope_target;s->stagger=s->stagger_target;
        s->remaining=0u;coefficients(s);
        if(p->config.placement!=XFILTER_OFF)s->wet=0.0f;
    }
    s->wet_target=p->config.placement==XFILTER_OFF?0.0f:1.0f;
    s->wet_remaining=ramp_length(s->sample_rate);
    s->wet_step=(s->wet_target-s->wet)/(float)s->wet_remaining;return 1;
}
int xenolith_filter_set_cutoff(XenolithFilter *s,float hz){
    if(!s||!finite_value(hz))return 0;hz=clamp(hz,20.0f,20000.0f);
    if(hz!=s->cutoff_target){s->cutoff_target=hz;s->remaining=ramp_length(s->sample_rate);}
    return 1;
}
int xenolith_filter_set_resonance(XenolithFilter *s,float resonance){
    if(!s||!finite_value(resonance))return 0;resonance=clamp(resonance,0.0f,1.0f);
    if(resonance!=s->resonance_target){s->resonance_target=resonance;s->remaining=ramp_length(s->sample_rate);}
    return 1;
}
uint32_t xenolith_filter_active(const XenolithFilter *s){return s&&(s->wet!=0.0f||s->wet_target!=0.0f);}
static void control_tick(XenolithFilter *s){
    if(!s->control_remaining){
        uint32_t interval=(uint32_t)(s->sample_rate*(16.0f/44100.0f)+.5f);
        uint32_t changed=0u;
        if(interval<1u)interval=1u;if(interval>64u)interval=64u;
        if(s->remaining){
            uint32_t n=s->remaining>interval?interval:s->remaining,i;
            float fraction=(float)n/(float)s->remaining;
            float target=cutoff_bound(s->cutoff_target,s->sample_rate);
            if(n==s->remaining){
                s->cutoff_hz=target;s->resonance=s->resonance_target;s->slope=s->slope_target;s->stagger=s->stagger_target;
                for(i=0u;i<3u;i++)s->mix[i]=s->mix_target[i];
            }else{
                s->cutoff_hz+=fraction*(target-s->cutoff_hz);
                s->resonance+=fraction*(s->resonance_target-s->resonance);
                s->slope+=fraction*(s->slope_target-s->slope);
                s->stagger+=fraction*(s->stagger_target-s->stagger);
                for(i=0u;i<3u;i++)s->mix[i]+=fraction*(s->mix_target[i]-s->mix[i]);
            }
            s->remaining-=n;changed=1u;
        }
        if(s->circuit_remaining){
            uint32_t n=s->circuit_remaining>interval?interval:s->circuit_remaining;
            float f=(float)n/(float)s->circuit_remaining;
            if(n==s->circuit_remaining)s->circuit=s->circuit_target;
            else{
                s->circuit.cathode_ohms+=f*(s->circuit_target.cathode_ohms-s->circuit.cathode_ohms);
                s->circuit.headroom+=f*(s->circuit_target.headroom-s->circuit.headroom);
                s->circuit.coupling_hz+=f*(s->circuit_target.coupling_hz-s->circuit.coupling_hz);
                s->circuit.balance+=f*(s->circuit_target.balance-s->circuit.balance);
            }
            s->circuit_remaining-=n;changed=1u;
        }
        if(s->load_remaining){
            uint32_t n=s->load_remaining>interval?interval:s->load_remaining;
            float fraction=(float)n/(float)s->load_remaining;
            if(n==s->load_remaining){s->load_sag=s->load_sag_target;s->load_shape=s->load_shape_target;}
            else{
                s->load_sag+=fraction*(s->load_sag_target-s->load_sag);
                s->load_shape+=fraction*(s->load_shape_target-s->load_shape);
            }
            s->load_remaining-=n;changed=1u;
        }
        if(changed)coefficients(s);
        s->control_remaining=interval;
    }
    --s->control_remaining;
    if(s->wet_remaining){
        --s->wet_remaining;s->wet+=s->wet_step;
        if(!s->wet_remaining)s->wet=s->wet_target;
    }
}
static float tube_law(float v,float *slope){
    /* Biased 3/2 law: (2/3)((1+v)^(3/2)-1), cubic near the bias point.
     * Tangent tails preserve positive differential conductance globally. */
    if(v<-.5f){*slope=.71875f;return -.4322916667f+.71875f*(v+.5f);}
    if(v>.5f){*slope=1.21875f;return .5572916667f+1.21875f*(v-.5f);}
    {float square=v*v;*slope=1.0f+.5f*v-.125f*square;return v+.25f*square-v*square*(1.0f/24.0f);}
}
void xenolith_filter_set_load_sag(XenolithFilter *s,float stress){
    float value;
    if(!s||!(stress>=0.0f&&stress<=1.0f))return;
    value=clamp(stress/.5f,0.0f,1.0f);
    if(value==s->load_sag_target)return;
    s->load_sag_target=value;s->load_remaining=load_ramp_length(s->sample_rate);
}
void xenolith_filter_set_load_shape(XenolithFilter *s,float shape){
    if(!s||!(shape>=0.0f&&shape<=2.0f)||shape==s->load_shape_target)return;
    s->load_shape_target=shape;s->load_remaining=load_ramp_length(s->sample_rate);
}
static float tube_solve(XenolithFilter *s,float u,float feedback,float linear){
    float v=u*linear,lo=u<0.0f?u:0.0f,hi=u>0.0f?u:0.0f,slope,w,residual;unsigned iteration;
    float tolerance=.00002f*(1.0f+(u<0.0f?-u:u));
    if(feedback==0.0f)return tube_law(u,&slope);
    w=tube_law(v,&slope);
    for(iteration=0u;iteration<2u;iteration++){
        v-=(v+feedback*w-u)/(1.0f+feedback*slope);
        v=clamp(v,lo,hi);
        w=tube_law(v,&slope);
        /* Keep two steps at zero LOAD for exact baseline PCM. Under LOAD,
         * accept a first step only under the same finite residual tolerance
         * used by the final guard. Reuse its law/slope if a second is needed.
         * tube_law has tangent tails: it is not a saturation substitute. */
        if(!iteration&&s->load_sag>0.0f){
            residual=v+feedback*w-u;
            if(finite_value(residual)&&within(residual,-tolerance,tolerance)){
                ++s->tube_early_solves;break;
            }
        }
    }
    residual=v+feedback*w-u;
    if(finite_value(residual)){
        float absolute=residual<0.0f?-residual:residual;
        if(absolute>s->tube_residual_peak)s->tube_residual_peak=absolute;
    }
    if(!finite_value(residual)||!within(residual,-tolerance,tolerance)){
        /* One linear, positive-gain loop for this sample is the bounded
         * fallback; no extra iterations or partially solved nonlinear state. */
        s->tube_solver_faults++;w=u*linear;
    }
    return w;
}
static float tube_sample(XenolithFilter *s,uint32_t channel,uint32_t stage,float x,float g,float inverse){
    XenolithFilterSection *z=&s->section[channel][stage];
    float constant=(z->band-g*z->low)*inverse;
    float feedback=s->tube_feedback_total[stage];
    /* input divider .625; fixed digital headroom .25. Open-loop tube gain
     * A0 is normalized out of these states; feedback stores beta*A0. */
    float u=s->tube_input_scale*x+s->tube_input_feedback*constant;
    /* J=Phi(v-rho*J), w=(1+rho)*J. Gain normalization leaves the
     * physical grid headroom fixed, retaining distortion reduction. */
    float w=s->tube_gain*tube_solve(s,u,feedback,s->tube_linear[stage]);
    float hp=(w-s->tube_g_plus_k[stage]*z->band-z->low)*inverse;
    float db=g*hp,bp=z->band+db,dl=g*bp,lp=z->low+dl;
    float output=s->tube_output_scale*w,dc;
    if(s->target.config.mode!=XFILTER_TWIN_T){
        float excitation=s->tube_excitation_scale*w-s->tube_output_feedback*bp;
        float band=s->tube_band_scale*bp;
        float low=s->tube_excitation_scale*lp;
        output=s->mix[0]*excitation+s->mix[1]*band+s->mix[2]*low;
    }
    dc=(output-s->tube_dc[channel][stage])*s->tube_dc_coefficient;
    z->band+=2.0f*db;z->low+=2.0f*dl;
    output-=s->tube_dc[channel][stage]+dc;s->tube_dc[channel][stage]+=2.0f*dc;
    return output;
}
static float channel_sample(XenolithFilter *s,uint32_t channel,float x){
    float first,filtered;uint32_t i;
    if(!finite_value(x)){
        s->section[channel][0].band=s->section[channel][0].low=0.0f;
        s->section[channel][1].band=s->section[channel][1].low=0.0f;
        s->tube_dc[channel][0]=s->tube_dc[channel][1]=0.0f;s->faults++;return 0.0f;
    }
    x=clamp(x,-8.0f,8.0f);
    first=tube_sample(s,channel,0u,x,s->g,s->inverse_denominator);filtered=first;
    if(s->slope>0.0f||s->slope_target>0.0f){
        float input=s->stagger==0.0f?first:first+s->stagger*(x-first);
        float second=tube_sample(s,channel,1u,input,s->g2,s->inverse_denominator2);
        float network=s->stagger==0.0f?second:second+(.5f*s->stagger)*(first-second);
        filtered=first+s->slope*(network-first);
    }else{
        s->section[channel][1].band=0.0f;s->section[channel][1].low=0.0f;
        s->tube_dc[channel][1]=0.0f;
    }
    for(i=0u;i<2u;i++)if(!within(s->section[channel][i].band,-1024.0f,1024.0f)
       ||!within(s->section[channel][i].low,-1024.0f,1024.0f)
       ||!within(s->tube_dc[channel][i],-1024.0f,1024.0f)){
        s->section[channel][0].band=s->section[channel][0].low=0.0f;
        s->section[channel][1].band=s->section[channel][1].low=0.0f;
        s->tube_dc[channel][0]=s->tube_dc[channel][1]=0.0f;s->faults++;return 0.0f;
    }
    if(!finite_value(filtered)){s->faults++;return 0.0f;}
    filtered=clamp(filtered,-64.0f,64.0f);
    return s->wet==1.0f?filtered:x+s->wet*(filtered-x);
}
float xenolith_filter_process_mono(XenolithFilter *s,float input){
    if(!s||!xenolith_filter_active(s))return input;control_tick(s);return channel_sample(s,0u,input);
}
void xenolith_filter_process_stereo(XenolithFilter *s,float *left,float *right){
    if(!s||!left||!right||!xenolith_filter_active(s))return;
    control_tick(s);*left=channel_sample(s,0u,*left);*right=channel_sample(s,1u,*right);
}
