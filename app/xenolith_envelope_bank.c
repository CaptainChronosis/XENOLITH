/* SPDX-License-Identifier: MPL-2.0 */
#include "xenolith_envelope_bank.h"
static int within(float x,float a,float b){return x>=a&&x<=b;}
int xenolith_envelope_route_destination(uint32_t d){
    return d==XM_DST_NONE||d==XM_DST_PITCH||d==XM_DST_INDEX||d==XM_DST_RATIO||d==XM_DST_DRIFT
        ||d==XM_DST_WET_MIX||d==XM_DST_AMPLITUDE||d==XM_DST_BITS
        ||d==XM_DST_FILTER_CUTOFF||d==XM_DST_FILTER_RESONANCE||d==XM_DST_FOLD;
}
void xenolith_envelope_bank_defaults(XenolithEnvelopeBankConfig *c){
    uint32_t i;static const uint32_t dest[3]={XM_DST_FILTER_CUTOFF,XM_DST_INDEX,XM_DST_PITCH};
    if(!c)return;
    for(i=0;i<3u;i++){XenolithEnvelopeRoute *e=&c->envelope[i];
        e->attack_ms=10.0f;e->decay_ms=250.0f;e->sustain=0.8f;e->release_ms=400.0f;
        e->depth=0.0f;e->destination=dest[i];e->trigger=XENV_BOTH;e->enabled=0u;}
}
int xenolith_envelope_bank_valid(const XenolithEnvelopeBankConfig *c){
    uint32_t i;if(!c)return 0;
    for(i=0;i<3u;i++){const XenolithEnvelopeRoute *e=&c->envelope[i];
        XenolithModDestination d=xenolith_mod_destination(e->destination);
        if(!within(e->attack_ms,0,10000)||!within(e->decay_ms,0,10000)||!within(e->sustain,0,1)
           ||!within(e->release_ms,0,10000)||!xenolith_envelope_route_destination(e->destination)
           ||!within(e->depth,-d.offset_limit,d.offset_limit)||e->trigger>=XENV_TRIGGER_COUNT||e->enabled>1u)return 0;
    }return 1;
}
void xenolith_envelope_bank_clear(XenolithEnvelopeBank *s){
    uint32_t i;for(i=0;i<3u;i++){xenolith_envelope_clear(&s->envelope[i]);s->level[i]=0.0f;}
    for(i=0;i<XM_DESTINATION_COUNT;i++)s->offset[i]=0.0f;
    s->gate_mask=0u;s->position=0u;
}
void xenolith_envelope_bank_init(XenolithEnvelopeBank *s){
    uint32_t i;xenolith_envelope_bank_defaults(&s->config);s->enabled_mask=0u;
    for(i=0;i<3u;i++)xenolith_envelope_init(&s->envelope[i],44100.0f);
    xenolith_envelope_bank_clear(s);
}
int xenolith_envelope_bank_apply(XenolithEnvelopeBank *s,const XenolithEnvelopeBankConfig *c){
    uint32_t i;if(!s||!xenolith_envelope_bank_valid(c))return 0;
    s->enabled_mask=0u;
    for(i=0;i<3u;i++){const XenolithEnvelopeRoute *e=&c->envelope[i];
        xenolith_envelope_set(&s->envelope[i],e->attack_ms,e->decay_ms,e->sustain,e->release_ms);
        if(e->enabled){s->enabled_mask|=1u<<i;}
        else {xenolith_envelope_clear(&s->envelope[i]);s->level[i]=0.0f;}
    }
    s->config=*c;s->position=0u;
    for(i=0;i<XM_DESTINATION_COUNT;i++)s->offset[i]=0.0f;
    return 1;
}
void xenolith_envelope_bank_sample(XenolithEnvelopeBank *s,uint32_t edges,uint32_t gates){
    uint32_t i;if(!s->enabled_mask)return;
    for(i=0;i<3u;i++)if(s->enabled_mask&(1u<<i)){
        const XenolithEnvelopeRoute *e=&s->config.envelope[i];
        uint32_t mask=e->trigger==XENV_KEY?1u:e->trigger==XENV_SEQUENCE?2u:3u;
        if(edges&mask)xenolith_envelope_note_on(&s->envelope[i]);
        if(!(gates&mask)&&s->envelope[i].stage!=XENOLITH_ENV_IDLE
           &&s->envelope[i].stage!=XENOLITH_ENV_RELEASE)xenolith_envelope_note_off(&s->envelope[i]);
        s->level[i]=xenolith_envelope_sample(&s->envelope[i]);
    }
    s->gate_mask=gates&3u;
    if(!s->position){
        for(i=0;i<XM_DESTINATION_COUNT;i++)s->offset[i]=0.0f;
        for(i=0;i<3u;i++)if(s->enabled_mask&(1u<<i)){
            const XenolithEnvelopeRoute *e=&s->config.envelope[i];
            s->offset[e->destination]+=s->level[i]*e->depth;
        }
        for(i=0;i<XM_DESTINATION_COUNT;i++){
            float limit=xenolith_mod_destination(i).offset_limit;
            if(s->offset[i]>limit)s->offset[i]=limit;
            if(s->offset[i]<-limit)s->offset[i]=-limit;
        }
    }
    s->position=(s->position+1u)&31u;
}
