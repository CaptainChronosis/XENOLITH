/* SPDX-License-Identifier: MPL-2.0
 * Normalized two-port allpasses, independently implemented from the energy
 * scattering equations discussed by Smith and tank-diffusion role in Dattorro.
 * Neither a Dattorro topology clone nor a physical spring model. */
#include "xenolith_dispersion.h"
#include "fastmath.h"

static const uint16_t lengths[3][8]={
    {3,4,5,7,10,14,19,25},
    {6,8,11,15,21,29,39,50},
    {13,17,23,31,43,59,79,101}
};
static const uint16_t offsets[8]={0,13,30,53,84,127,186,265};
static const uint16_t group_start[2]={0,84},group_end[2]={84,366};
#define XD_RUNNING 0u
#define XD_RAMP_OFF 1u
#define XD_CLEARING 2u
#define XD_OFF 3u
/* Enough margin for float multiplies/adds and the coefficient norm check. */
#define XD_MARGIN 0.999999523162841796875f
#define XD_NORM_LIMIT 0.99999904632568359375f

static int within(float x,float lo,float hi){return x>=lo&&x<=hi;}
static uint32_t ramp_length(float rate){return (uint32_t)(rate*.05f+.5f);}
static float amount(const XenolithDispersionPrepared *p,uint32_t group){return group?p->amount_b:p->amount_a;}
static void coefficient(float value,float *a,float *b){
    if(value==0.0f){*a=1.0f;*b=0.0f;return;}
    *a=1.0f-1.8f*value;
    *b=fm_sqrtf(fm_maxf(0.0f,1.0f-(*a)*(*a)))*XD_MARGIN;
    *a*=XD_MARGIN;
}
void xenolith_dispersion_defaults(XenolithDispersionConfig *c){
    if(!c)return;c->amount_a=c->amount_b=0.0f;c->scale=2u;
}
int xenolith_dispersion_config_valid(const XenolithDispersionConfig *c){
    return c&&within(c->amount_a,0.0f,1.0f)&&within(c->amount_b,0.0f,1.0f)&&c->scale<3u;
}
int xenolith_dispersion_prepare(XenolithDispersionPrepared *out,const XenolithDispersionConfig *c,float rate){
    XenolithDispersionPrepared p;
    if(!out||!xenolith_dispersion_config_valid(c)||!within(rate,1000.0f,192000.0f))return 0;
    p.amount_a=c->amount_a;p.amount_b=c->amount_b;p.scale=c->scale;p.wet_rate=rate;
    coefficient(c->amount_a,&p.a[0],&p.b[0]);coefficient(c->amount_b,&p.a[1],&p.b[1]);
    p.ramp_samples=ramp_length(rate);p.inverse_ramp=1.0f/(float)p.ramp_samples;
    *out=p;return 1;
}
static int prepared_valid(const XenolithDispersionPrepared *p){
    uint32_t g;
    if(!p||!within(p->amount_a,0.0f,1.0f)||!within(p->amount_b,0.0f,1.0f)||p->scale>=3u
       ||!within(p->wet_rate,1000.0f,192000.0f)||p->ramp_samples!=ramp_length(p->wet_rate)
       ||p->inverse_ramp!=1.0f/(float)p->ramp_samples)return 0;
    for(g=0;g<2u;g++){
        float value=amount(p,g),norm=p->a[g]*p->a[g]+p->b[g]*p->b[g];
        if(value==0.0f){if(p->a[g]!=1.0f||p->b[g]!=0.0f)return 0;}
        else if(p->a[g]!=(1.0f-1.8f*value)*XD_MARGIN||!within(p->b[g],0.0f,1.0f)
                ||!within(norm,.999998f,1.0f))return 0;
    }
    return 1;
}
static void begin_ramp(XenolithDispersion *s,uint32_t g,float a,float b,uint32_t phase){
    s->a_goal[g]=a;s->b_goal[g]=b;s->phase[g]=phase;
    s->remaining[g]=s->target.ramp_samples;
    s->a_step[g]=(a-s->a[g])*s->target.inverse_ramp;
    s->b_step[g]=(b-s->b[g])*s->target.inverse_ramp;
}
static void retire(XenolithDispersion *s,uint32_t g){
    if(s->phase[g]==XD_RUNNING)begin_ramp(s,g,1.0f,0.0f,XD_RAMP_OFF);
}
static void resume_ready(XenolithDispersion *s){
    uint32_t g;
    if(s->geometry_transition){
        if(s->phase[0]!=XD_OFF||s->phase[1]!=XD_OFF)return;
        s->active_scale=s->target.scale;s->geometry_transition=0u;
    }
    for(g=0;g<2u;g++)if(s->phase[g]==XD_OFF&&amount(&s->target,g)>0.0f)
        begin_ramp(s,g,s->target.a[g],s->target.b[g],XD_RUNNING);
}
void xenolith_dispersion_clear(XenolithDispersion *s){
    uint32_t i;if(!s)return;
    for(i=0;i<XENOLITH_DISPERSION_SAMPLES;i++)s->delay[i]=0.0f;
    for(i=0;i<8u;i++)s->write[i]=0u;
    s->active_scale=s->target.scale;s->geometry_transition=0u;
    for(i=0;i<2u;i++){
        s->a[i]=s->a_goal[i]=s->target.a[i];s->b[i]=s->b_goal[i]=s->target.b[i];
        s->a_step[i]=s->b_step[i]=0.0f;s->remaining[i]=0u;s->clear_cursor[i]=group_start[i];
        s->phase[i]=amount(&s->target,i)>0.0f?XD_RUNNING:XD_OFF;
    }
}
void xenolith_dispersion_init(XenolithDispersion *s,float rate){
    XenolithDispersionConfig c;if(!s)return;
    xenolith_dispersion_defaults(&c);
    if(!within(rate,1000.0f,192000.0f))rate=44100.0f;
    xenolith_dispersion_prepare(&s->target,&c,rate);s->faults=0u;xenolith_dispersion_clear(s);
}
int xenolith_dispersion_retime(XenolithDispersion *s,float rate){
    uint32_t g;float ratio;
    if(!s||!within(rate,1000.0f,192000.0f))return 0;
    if(rate==s->target.wet_rate)return 1;
    ratio=rate/s->target.wet_rate;s->target.wet_rate=rate;
    s->target.ramp_samples=ramp_length(rate);s->target.inverse_ramp=1.0f/(float)s->target.ramp_samples;
    for(g=0;g<2u;g++)if(s->remaining[g]){
        uint32_t n=(uint32_t)((float)s->remaining[g]*ratio+.5f);float inverse;
        if(!n)n=1u;s->remaining[g]=n;inverse=1.0f/(float)n;
        s->a_step[g]=(s->a_goal[g]-s->a[g])*inverse;
        s->b_step[g]=(s->b_goal[g]-s->b[g])*inverse;
    }
    return 1;
}
int xenolith_dispersion_apply(XenolithDispersion *s,const XenolithDispersionPrepared *p){
    float old[2];uint32_t g;
    if(!s||!prepared_valid(p))return 0;
    old[0]=s->target.amount_a;old[1]=s->target.amount_b;
    xenolith_dispersion_retime(s,p->wet_rate);s->target=*p;
    if(s->geometry_transition||s->active_scale!=p->scale){
        s->geometry_transition=1u;for(g=0;g<2u;g++)retire(s,g);resume_ready(s);return 1;
    }
    for(g=0;g<2u;g++)if(amount(p,g)!=old[g]){
        if(s->phase[g]==XD_CLEARING)continue;
        if(amount(p,g)==0.0f)retire(s,g);
        else begin_ramp(s,g,p->a[g],p->b[g],XD_RUNNING);
    }
    resume_ready(s);return 1;
}
static float input_bound(XenolithDispersion *s,float x){
    if(within(x,-64.0f,64.0f))return x;
    s->faults++;if(x!=x)return 0.0f;return x<0.0f?-64.0f:64.0f;
}
void xenolith_dispersion_process(XenolithDispersion *s,float vector[8]){
    uint32_t group,i;
    if(!s||!vector||!xenolith_dispersion_active(s))return;
    for(group=0;group<2u;group++){
        float a,b;
        if(s->phase[group]==XD_OFF)continue;
        if(s->phase[group]==XD_CLEARING){
            uint32_t end=group_end[group];
            for(i=0;i<4u&&s->clear_cursor[group]<end;i++)s->delay[s->clear_cursor[group]++]=0.0f;
            if(s->clear_cursor[group]==end){
                for(i=group*4u;i<group*4u+4u;i++)s->write[i]=0u;
                s->phase[group]=XD_OFF;
            }
            continue;
        }
        if(s->remaining[group]){
            --s->remaining[group];
            s->a[group]=s->a_goal[group]-s->a_step[group]*(float)s->remaining[group];
            s->b[group]=s->b_goal[group]-s->b_step[group]*(float)s->remaining[group];
            if(!s->remaining[group]){
                s->a[group]=s->a_goal[group];s->b[group]=s->b_goal[group];
                if(s->phase[group]==XD_RAMP_OFF){
                    s->phase[group]=XD_CLEARING;s->clear_cursor[group]=group_start[group];continue;
                }
            }
        }
        a=s->a[group];b=s->b[group];
        /* Linear interpolation is contractive in exact arithmetic. Protect
         * that invariant against accumulated float rounding during ramps. */
        if(a*a+b*b>XD_NORM_LIMIT){a*=XD_MARGIN;b*=XD_MARGIN;s->a[group]=a;s->b[group]=b;}
        for(i=group*4u;i<group*4u+4u;i++){
            uint32_t p=offsets[i]+s->write[i];float x=input_bound(s,vector[i]),q=s->delay[p],y,next;
            if(!within(q,-1024.0f,1024.0f)){q=0.0f;s->faults++;}
            y=a*x+b*q;next=b*x-a*q;
            if(!within(y,-1024.0f,1024.0f)){y=y<0.0f?-1024.0f:1024.0f;s->faults++;}
            if(!within(next,-1024.0f,1024.0f)){next=next<0.0f?-1024.0f:1024.0f;s->faults++;}
            vector[i]=fm_flush(y);s->delay[p]=fm_flush(next);
            if(++s->write[i]==lengths[s->active_scale][i])s->write[i]=0u;
        }
    }
    resume_ready(s);
}
