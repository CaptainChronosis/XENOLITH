/* SPDX-License-Identifier: MPL-2.0
 * XX29 independent64-note/64+16+16+16 CV sequencer,2026. */
#include "xenolith_sequence.h"
#include "fastmath.h"

static void zero_bytes(void *memory,uint32_t bytes) {
    uint8_t *p=(uint8_t*)memory;
    while(bytes--)*p++=0;
}
static int within(float x,float lo,float hi) { return x>=lo && x<=hi; }
static int step_valid(const XenolithSequenceStep *p) {
    return p && p->note<=127u && p->kind<=XSEQ_TIE
        && p->gate_percent>=1u && p->gate_percent<=99u
        && p->triggers<=7u;
}
static int lane_valid(const XenolithSequenceLane *p) {
    uint32_t i;
    if(!p || p->destination>=XSEQ_DEST_COUNT || p->enabled>1u
       || (p->step_mask&0xffff0000u) || p->reserved
       || !within(p->rise_seconds,0.0f,60.0f) || !within(p->fall_seconds,0.0f,60.0f))return 0;
    for(i=0;i<16u;i++)if(!within(p->value[i],0.0f,1.0f))return 0;
    return 1;
}
int xenolith_sequence_config_valid(const XenolithSequenceConfig *p) {
    return p && p->enabled<=1u && p->length>=1u && p->length<=64u
        && p->division<=XSEQ_SIXTYFOURTH && p->note_mode<=XSEQ_CONTINUOUS
        && within(p->bpm,20.0f,300.0f) && p->quantize<=XSEQ_QUANT_FOUR_STEPS;
}
int xenolith_sequence_pattern_valid(const XenolithSequencePattern *p) {
    uint32_t i,j;
    if(!p)return 0;
    for(i=0;i<64u;i++)if(!step_valid(&p->step[i]))return 0;
    if(p->cv0_extra_mask[1]&0xffff0000u)return 0;
    for(i=0;i<48u;i++)if(!within(p->cv0_extra[i],0.0f,1.0f))return 0;
    for(i=0;i<4u;i++){
        if(!lane_valid(&p->lane[i]))return 0;
        if(p->lane[i].enabled && p->lane[i].destination!=XSEQ_DEST_NONE)
            for(j=0;j<i;j++)if(p->lane[j].enabled && p->lane[j].destination==p->lane[i].destination)return 0;
    }
    return 1;
}
int xenolith_sequence_image_valid(const XenolithSequenceImage *p,uint32_t bytes) {
    return p && bytes==sizeof *p && p->magic==XENOLITH_SEQUENCE_MAGIC
        && p->version==XENOLITH_SEQUENCE_VERSION && p->bytes==sizeof *p && p->reserved==0u
        && xenolith_sequence_config_valid(&p->config) && xenolith_sequence_pattern_valid(&p->pattern);
}
static int legacy_image_valid(const XenolithSequenceImageV4 *p,uint32_t bytes,uint32_t version) {
    uint32_t i,j;
    if(!(p && bytes==sizeof *p && p->magic==XENOLITH_SEQUENCE_MAGIC
        && p->version==version && p->bytes==sizeof *p && p->reserved==0u
        && (version!=1u||p->config.reserved==0u) && p->config.enabled<=1u
        && p->config.length>=1u && p->config.length<=16u && p->config.division<=XSEQ_EIGHTH_TRIPLET
        && p->config.note_mode<=XSEQ_CONTINUOUS && within(p->config.bpm,20.0f,300.0f)))return 0;
    for(i=0;i<16u;i++){
        const XenolithSequenceStepV4 *v=&p->pattern.step[i];
        if(v->note>127u||v->kind>XSEQ_TIE||v->gate_percent<1u||v->gate_percent>99u||v->triggers>7u||v->reserved)return 0;
    }
    for(i=0;i<4u;i++){
        const XenolithSequenceLane *v=&p->pattern.lane[i];
        if(!lane_valid(v)||v->destination>(version>=4u?XSEQ_DEST_COUNT-1u:version>=3u?XSEQ_DEST_EXP_DEPTH:XSEQ_DEST_BITS))return 0;
        if(v->enabled&&v->destination!=XSEQ_DEST_NONE)
            for(j=0;j<i;j++)if(p->pattern.lane[j].enabled&&p->pattern.lane[j].destination==v->destination)return 0;
    }
    return 1;
}
static int migrate_legacy(XenolithSequenceImage *out,const XenolithSequenceImageV4 *p,uint32_t bytes,uint32_t version) {
    XenolithSequenceImageV4 saved;uint32_t i;
    if(!out||!legacy_image_valid(p,bytes,version))return 0;
    saved=*p; /* the caller may expand an old image in-place */
    xenolith_sequence_image_defaults(out);
    out->config.enabled=saved.config.enabled;out->config.length=saved.config.length;
    out->config.division=saved.config.division;out->config.note_mode=saved.config.note_mode;out->config.bpm=saved.config.bpm;
    for(i=0;i<16u;i++){
        out->pattern.step[i].note=saved.pattern.step[i].note;
        out->pattern.step[i].kind=saved.pattern.step[i].kind;
        out->pattern.step[i].gate_percent=saved.pattern.step[i].gate_percent;
        out->pattern.step[i].triggers=saved.pattern.step[i].triggers;
    }
    for(i=0;i<4u;i++)out->pattern.lane[i]=saved.pattern.lane[i];
    return 1;
}
int xenolith_sequence_image_v1_valid(const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return legacy_image_valid(p,bytes,1u);
}
int xenolith_sequence_image_v2_valid(const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return legacy_image_valid(p,bytes,2u);
}
int xenolith_sequence_image_from_v2(XenolithSequenceImage *out,const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return migrate_legacy(out,p,bytes,2u);
}
int xenolith_sequence_image_v3_valid(const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return legacy_image_valid(p,bytes,3u);
}
int xenolith_sequence_image_from_v3(XenolithSequenceImage *out,const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return migrate_legacy(out,p,bytes,3u);
}
int xenolith_sequence_image_v4_valid(const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return legacy_image_valid(p,bytes,4u);
}
int xenolith_sequence_image_from_v4(XenolithSequenceImage *out,const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return migrate_legacy(out,p,bytes,4u);
}
uint32_t xenolith_sequence_cv_length(const XenolithSequenceConfig *c) {
    uint32_t n;
    if(!c||!c->length)return 0u;
    n=c->division==XSEQ_SIXTYFOURTH?(c->length+3u)/4u:c->length;
    return n<16u?n:16u;
}
uint32_t xenolith_sequence_lane_length(const XenolithSequenceConfig *c,uint32_t lane) {
    return !c||lane>=4u?0u:lane==0u?c->length:xenolith_sequence_cv_length(c);
}
uint32_t xenolith_sequence_lane_position(const XenolithSequenceConfig *c,uint32_t lane,uint32_t note_position) {
    uint32_t length=xenolith_sequence_lane_length(c,lane);
    if(!length)return 0u;
    if(lane&&c->division==XSEQ_SIXTYFOURTH)note_position/=4u;
    return note_position%length;
}
int xenolith_sequence_get_lane_step(const XenolithSequencePattern *p,uint32_t lane,uint32_t step,float *value,uint32_t *enabled) {
    if(!p||!value||!enabled||lane>=4u||step>=(lane?16u:64u))return 0;
    if(step<16u){*value=p->lane[lane].value[step];*enabled=(p->lane[lane].step_mask>>step)&1u;}
    else{uint32_t n=step-16u;*value=p->cv0_extra[n];*enabled=(p->cv0_extra_mask[n>>5]>>(n&31u))&1u;}
    return 1;
}
static void set_lane_step(XenolithSequencePattern *p,uint32_t lane,uint32_t step,float value,uint32_t enabled) {
    uint32_t *mask,bit;
    if(step<16u){p->lane[lane].value[step]=value;mask=&p->lane[lane].step_mask;bit=1u<<step;}
    else{uint32_t n=step-16u;p->cv0_extra[n]=value;mask=&p->cv0_extra_mask[n>>5];bit=1u<<(n&31u);}
    if(enabled)*mask|=bit;else *mask&=~bit;
}
int xenolith_sequence_value_bounds(uint32_t destination,float *low,float *high) {
    float a=0.0f,b=1.0f;
    if(!low||!high)return 0;
    switch(destination){
    case XSEQ_DEST_PITCH:a=20.0f;b=800.0f;break;
    case XSEQ_DEST_INDEX:b=64.0f;break;
    case XSEQ_DEST_EXP_DEPTH:b=120.0f;break;
    case XSEQ_DEST_RATIO:a=.001f;b=32.0f;break;
    case XSEQ_DEST_CORE_RATE:a=1000.0f;b=44100.0f;break;
    case XSEQ_DEST_WET_RATE:a=1378.125f;b=44100.0f;break;
    case XSEQ_DEST_BITS:a=1.0f;b=24.0f;break;
    case XSEQ_DEST_FILTER_CUTOFF:a=20.0f;b=20000.0f;break;
    case XSEQ_DEST_ANS_CELL:b=26.0f;break;
    case XSEQ_DEST_DRIFT:case XSEQ_DEST_WET_MIX:case XSEQ_DEST_AMPLITUDE:case XSEQ_DEST_FILTER_RESONANCE:break;
    default:return 0;
    }
    *low=a;*high=b;return 1;
}
int xenolith_sequence_value_from_normalized(uint32_t destination,float value,float *physical) {
    float low,high,v;
    if(!physical||!within(value,0.0f,1.0f)||!xenolith_sequence_value_bounds(destination,&low,&high))return 0;
    if(destination==XSEQ_DEST_PITCH)v=20.0f*fm_exp2f(value*5.321928095f);
    else if(destination==XSEQ_DEST_FILTER_CUTOFF)v=20.0f*fm_exp2f(value*9.965784285f);
    else if(destination==XSEQ_DEST_RATIO)v=.001f*fm_exp2f(value*14.96578428f);
    else if(destination==XSEQ_DEST_CORE_RATE)v=1000.0f*fm_exp2f(value*5.46270675f);
    else if(destination==XSEQ_DEST_WET_RATE)v=1378.125f*fm_exp2f(value*5.0f);
    else v=low+(high-low)*value;
    *physical=fm_clampf(v,low,high);return 1;
}
int xenolith_sequence_value_to_normalized(uint32_t destination,float physical,float *value) {
    float low,high,v;
    if(!value||!xenolith_sequence_value_bounds(destination,&low,&high)||!within(physical,low,high))return 0;
    if(destination==XSEQ_DEST_PITCH)v=fm_log2f(physical/20.0f)/5.321928095f;
    else if(destination==XSEQ_DEST_FILTER_CUTOFF)v=fm_log2f(physical/20.0f)/9.965784285f;
    else if(destination==XSEQ_DEST_RATIO)v=fm_log2f(physical/.001f)/14.96578428f;
    else if(destination==XSEQ_DEST_CORE_RATE)v=fm_log2f(physical/1000.0f)/5.46270675f;
    else if(destination==XSEQ_DEST_WET_RATE)v=fm_log2f(physical/1378.125f)/5.0f;
    else v=(physical-low)/(high-low);
    *value=fm_clampf(v,0.0f,1.0f);return 1;
}
int xenolith_sequence_image_from_v1(XenolithSequenceImage *out,const XenolithSequenceImageV4 *p,uint32_t bytes) {
    return migrate_legacy(out,p,bytes,1u);
}
void xenolith_sequence_image_defaults(XenolithSequenceImage *p) {
    uint32_t i;
    if(!p)return;
    zero_bytes(p,sizeof *p);
    p->magic=XENOLITH_SEQUENCE_MAGIC;p->version=XENOLITH_SEQUENCE_VERSION;p->bytes=sizeof *p;
    p->config.length=64u;p->config.division=XSEQ_SIXTYFOURTH;p->config.bpm=120.0f;
    for(i=0;i<64u;i++)p->pattern.step[i].gate_percent=50u;
    for(i=0;i<4u;i++)p->pattern.lane[i].destination=XSEQ_DEST_NONE;
}
uint32_t xenolith_sequence_steps_per_beat(uint32_t division) {
    static const uint32_t per_beat[5]={1u,2u,4u,3u,4u};
    return division<=XSEQ_SIXTYFOURTH?per_beat[division]:0u;
}
static uint32_t prepare_phase(const XenolithSequenceConfig *c) {
    return (uint32_t)(c->bpm*(float)xenolith_sequence_steps_per_beat(c->division)*(4294967296.0f/(60.0f*44100.0f))+0.5f);
}
static float prepare_slew(float seconds) {
    return seconds<=1.0f/44100.0f ? 1.0f : 1.0f/(44100.0f*seconds);
}
static void prepare_lanes(XenolithSequence *s) {
    uint32_t i;
    for(i=0;i<4u;i++){
        s->rise_step[i]=prepare_slew(s->image.pattern.lane[i].rise_seconds);
        s->fall_step[i]=prepare_slew(s->image.pattern.lane[i].fall_seconds);
    }
}
static void set_lane_target(XenolithSequence *s,uint32_t lane,float value) {
    if(s->lane_target[lane]!=value)s->lane_error[lane]=0.0f;
    s->lane_target[lane]=value;
}
void xenolith_sequence_init(XenolithSequence *s) {
    if(!s)return;
    zero_bytes(s,sizeof *s);xenolith_sequence_image_defaults(&s->image);
    s->phase_increment=prepare_phase(&s->image.config);prepare_lanes(s);
}
void xenolith_sequence_stop(XenolithSequence *s) {
    if(!s)return;
    s->pending_off|=s->gate;s->gate=0u;s->running=0u;s->pending_start=0u;
    s->record_mode=XSEQ_RECORD_OFF;s->record_state=XSEQ_REC_IDLE;
    s->record_notes=0u;s->record_lanes=0u;s->capture_valid=0u;s->capture_future=0u;
    zero_bytes(s->keys_down,sizeof s->keys_down);
}
void xenolith_sequence_panic(XenolithSequence *s) { xenolith_sequence_stop(s); }
int xenolith_sequence_configure(XenolithSequence *s,const XenolithSequenceConfig *c) {
    if(!s || !xenolith_sequence_config_valid(c))return 0;
    if(!c->enabled)xenolith_sequence_stop(s);
    if((c->length!=s->image.config.length||c->division!=s->image.config.division)&&s->record_state!=XSEQ_REC_IDLE){
        s->record_mode=XSEQ_RECORD_OFF;s->record_state=XSEQ_REC_IDLE;
        s->record_notes=0u;s->record_lanes=0u;s->capture_valid=0u;s->capture_future=0u;
    }
    s->image.config=*c;s->phase_increment=prepare_phase(c);
    if(s->edit_step>=c->length)s->edit_step=c->length-1u;
    if(s->running && s->playhead>=c->length){
        s->pending_off|=s->gate;s->gate=0u;s->playhead=0u;s->phase=0u;s->pending_start=1u;
    }
    return 1;
}
int xenolith_sequence_select_step(XenolithSequence *s,uint32_t step) {
    if(!s || step>=s->image.config.length)return 0;
    s->edit_step=step;return 1;
}
int xenolith_sequence_edit_step(XenolithSequence *s,uint32_t step,const XenolithSequenceStep *value) {
    if(!s || step>=64u || !step_valid(value) || (s->record_state!=XSEQ_REC_IDLE
       && !(s->record_mode==XSEQ_RECORD_STEP && !s->running)))return 0;
    s->image.pattern.step[step]=*value;return 1;
}
int xenolith_sequence_edit_lane(XenolithSequence *s,uint32_t lane,const XenolithSequenceLane *value) {
    uint32_t i;
    if(!s || lane>=4u || !lane_valid(value) || (s->record_state!=XSEQ_REC_IDLE
       &&!(s->record_mode==XSEQ_RECORD_STEP&&!s->running)))return 0;
    if(value->enabled && value->destination!=XSEQ_DEST_NONE)
        for(i=0;i<4u;i++)if(i!=lane && s->image.pattern.lane[i].enabled
            && s->image.pattern.lane[i].destination==value->destination)return 0;
    s->image.pattern.lane[lane]=*value;
    s->rise_step[lane]=prepare_slew(value->rise_seconds);s->fall_step[lane]=prepare_slew(value->fall_seconds);
    s->lane_valid_mask&=~(1u<<lane);
    if(s->running && value->enabled){
        float v;uint32_t enabled;
        xenolith_sequence_get_lane_step(&s->image.pattern,lane,xenolith_sequence_lane_position(&s->image.config,lane,s->playhead),&v,&enabled);
        if(enabled){set_lane_target(s,lane,v);s->lane_valid_mask|=1u<<lane;}
    }
    return 1;
}
int xenolith_sequence_edit_lane_step(XenolithSequence *s,uint32_t lane,uint32_t step,float value,uint32_t enabled) {
    if(!s||lane>=4u||step>=(lane?16u:64u)||!within(value,0.0f,1.0f)||enabled>1u
       ||(s->record_state!=XSEQ_REC_IDLE&&!(s->record_mode==XSEQ_RECORD_STEP&&!s->running)))return 0;
    set_lane_step(&s->image.pattern,lane,step,value,enabled);
    if(s->running&&s->image.pattern.lane[lane].enabled
       &&step==xenolith_sequence_lane_position(&s->image.config,lane,s->playhead)){
        if(enabled){set_lane_target(s,lane,value);s->lane_valid_mask|=1u<<lane;}
        else s->lane_valid_mask&=~(1u<<lane);
    }
    return 1;
}
int xenolith_sequence_apply_pattern(XenolithSequence *s,const XenolithSequencePattern *p) {
    if(!s || s->running || s->record_state!=XSEQ_REC_IDLE || !xenolith_sequence_pattern_valid(p))return 0;
    s->image.pattern=*p;s->lane_valid_mask=0u;prepare_lanes(s);return 1;
}
int xenolith_sequence_play_toggle(XenolithSequence *s) {
    if(!s || !s->image.config.enabled)return 0;
    if(s->running){xenolith_sequence_stop(s);return 1;}
    if(s->record_mode==XSEQ_RECORD_STEP){
        /* PLAY leaves stopped step entry and auditions the preserved take. */
        s->record_mode=XSEQ_RECORD_OFF;s->record_state=XSEQ_REC_IDLE;
        s->record_notes=0u;s->record_lanes=0u;s->capture_valid=0u;
    }
    s->running=1u;s->phase=0u;s->playhead=0u;s->pending_start=1u;s->lane_valid_mask=0u;
    return 1;
}
int xenolith_sequence_record_arm(XenolithSequence *s,uint32_t mode,uint32_t notes,uint32_t lanes) {
    if(!s || mode>XSEQ_RECORD_OVERDUB || notes>1u || (lanes&~15u))return 0;
    if(mode!=XSEQ_RECORD_OFF && (!s->image.config.enabled || (!notes&&!lanes)
       || (mode==XSEQ_RECORD_STEP&&s->running) || s->record_state==XSEQ_REC_ACTIVE))return 0;
    s->record_mode=mode;s->record_notes=mode?notes:0u;s->record_lanes=mode?lanes:0u;
    s->record_state=mode?XSEQ_REC_ARMED:XSEQ_REC_IDLE;s->capture_valid=0u;s->capture_future=0u;
    s->record_length=(notes||(lanes&1u)||s->image.config.division==XSEQ_SIXTYFOURTH)?s->image.config.length:xenolith_sequence_cv_length(&s->image.config);
    if(mode==XSEQ_RECORD_STEP){
        uint32_t length=(notes||(lanes&1u))?s->image.config.length:xenolith_sequence_cv_length(&s->image.config);
        if(s->edit_step>=length)s->edit_step=length-1u;
    }
    return 1;
}
static void begin_recording(XenolithSequence *s);
static void start_key_take(XenolithSequence *s) {
    if(s->record_state!=XSEQ_REC_ARMED || s->record_mode<XSEQ_RECORD_REPLACE || s->running)return;
    s->running=1u;s->phase=0u;s->playhead=0u;s->pending_start=1u;s->lane_valid_mask=0u;
    begin_recording(s);
}
static int capture_allowed(const XenolithSequence *s) {
    return (s->record_mode==XSEQ_RECORD_STEP && s->record_state==XSEQ_REC_ARMED && !s->running)
        || (s->record_state==XSEQ_REC_ACTIVE && s->running);
}
static uint32_t release_gate(uint32_t phase) {
    uint32_t percent=(uint32_t)((float)phase*(100.0f/4294967296.0f)+0.5f);
    return percent<1u?1u:percent>99u?99u:percent;
}
static uint32_t capture_grid_step(const XenolithSequence *s) {
    uint32_t span=1u<<s->image.config.quantize,position=s->record_elapsed;
    uint32_t lower=(position/span)*span,remainder=position-lower,rounded=lower;
    uint32_t last=((s->record_length-1u)/span)*span;
    /* Compare a Q32 fraction with half the grid; no64-bit division on target. */
    if(span==1u?s->phase>=UINT32_C(0x80000000):remainder>=span/2u)rounded+=span;
    return rounded>last?last:rounded;
}
static uint32_t step_entry_length(const XenolithSequence *s) {
    return s->record_notes||(s->record_lanes&1u)?s->image.config.length:xenolith_sequence_cv_length(&s->image.config);
}
int xenolith_sequence_capture_key(XenolithSequence *s,uint32_t note,uint32_t down) {
    uint32_t bit,index,was;
    XenolithSequenceStep *step;
    if(!s || note>127u || down>1u)return 0;
    index=note>>5;bit=1u<<(note&31u);was=s->keys_down[index]&bit;
    if(down){
        if(was)return 0;
        if(s->record_notes)start_key_take(s);
        s->keys_down[index]|=bit;
        if(!s->record_notes || !capture_allowed(s))return 0;
        index=s->record_mode==XSEQ_RECORD_STEP?s->edit_step:capture_grid_step(s);
        step=&s->image.pattern.step[index];step->note=(uint8_t)note;step->kind=XSEQ_NOTE;
        if(s->record_mode==XSEQ_RECORD_STEP)s->edit_step=(index+1u)%s->image.config.length;
        else{
            uint32_t n;
            step->gate_percent=99u;s->capture_note=note;s->capture_step=index;s->capture_valid=1u;
            s->capture_future=index>s->record_elapsed;
            for(n=index+1u;n<=s->record_elapsed;n++){
                s->image.pattern.step[n].kind=XSEQ_TIE;s->image.pattern.step[n].note=0u;
                s->image.pattern.step[n].gate_percent=99u;
            }
            if(index<s->record_elapsed)s->capture_step=s->record_elapsed;
        }
        return 1;
    }
    s->keys_down[index]&=~bit;
    if(!was || !s->capture_valid || s->capture_note!=note || !s->record_notes
       || !capture_allowed(s) || s->record_mode==XSEQ_RECORD_STEP)return 0;
    s->image.pattern.step[s->capture_future?s->capture_step:s->playhead].gate_percent=
        (uint8_t)(s->capture_future?1u:release_gate(s->phase));
    if(!s->capture_future && s->image.config.note_mode==XSEQ_ADSR && s->gate && s->note==note){
        s->pending_off=1u;s->gate=0u;
    }
    s->capture_valid=0u;return 1;
}
int xenolith_sequence_capture_parameter(XenolithSequence *s,uint32_t lane,float base) {
    XenolithSequenceLane *p;uint32_t step,length,position,aligned;
    if(!s || lane>=4u || !within(base,0.0f,1.0f) || !(s->record_lanes&(1u<<lane)) || !capture_allowed(s))return 0;
    length=xenolith_sequence_lane_length(&s->image.config,lane);
    aligned=lane&&s->image.config.division==XSEQ_SIXTYFOURTH;
    position=s->record_mode==XSEQ_RECORD_STEP?s->edit_step:s->record_elapsed;
    step=s->record_mode==XSEQ_RECORD_STEP?s->edit_step:capture_grid_step(s);
    /* A short-lane-only stopped take edits CV positions directly. Mixed takes
     * and running captures use note coordinates, projected onto the CV grid. */
    if(aligned&&(s->record_mode!=XSEQ_RECORD_STEP||s->record_notes||(s->record_lanes&1u))){
        position/=4u;step/=4u;
    }
    if(position>=length)return 0;
    if(step>=length)return 0;
    p=&s->image.pattern.lane[lane];set_lane_step(&s->image.pattern,lane,step,base,1u);
    if(s->running && p->enabled&&step<=position){set_lane_target(s,lane,base);s->lane_valid_mask|=1u<<lane;}
    return 1;
}
int xenolith_sequence_capture_key_values(XenolithSequence *s,uint32_t note,uint32_t down,
                                         uint32_t lanes,const float value[4]) {
    uint32_t i,mask=0u,written=0u,old_step;
    int note_written;
    if(!s||note>127u||down>1u||(lanes&~15u))return 0;
    if(!down)return xenolith_sequence_capture_key(s,note,down);
    if(s->keys_down[note>>5]&(1u<<(note&31u)))return 0;
    if(capture_allowed(s) || (!s->running && s->record_state==XSEQ_REC_ARMED
       && s->record_mode>=XSEQ_RECORD_REPLACE))mask=lanes&s->record_lanes;
    for(i=0;i<4u;i++)if((mask&(1u<<i))&&(!value||!within(value[i],0.0f,1.0f)))return 0;
    if(s->record_notes||mask)start_key_take(s);
    old_step=s->edit_step;
    for(i=0;i<4u;i++)if(mask&(1u<<i))written|=(uint32_t)xenolith_sequence_capture_parameter(s,i,value[i]);
    note_written=xenolith_sequence_capture_key(s,note,down);
    if(written&&!s->record_notes&&s->record_mode==XSEQ_RECORD_STEP&&!s->running)
        s->edit_step=(old_step+1u)%step_entry_length(s);
    return written||note_written;
}
static void begin_recording(XenolithSequence *s) {
    uint32_t i,j;
    if(s->record_mode==XSEQ_RECORD_REPLACE){
        if(s->record_notes)for(i=0;i<64u;i++){
            s->image.pattern.step[i].kind=XSEQ_REST;s->image.pattern.step[i].note=0u;
            s->image.pattern.step[i].gate_percent=50u;
        }
        for(i=0;i<4u;i++)if(s->record_lanes&(1u<<i)){
            s->image.pattern.lane[i].step_mask=0u;
            for(j=0;j<16u;j++)s->image.pattern.lane[i].value[j]=0.0f;
            s->lane_valid_mask&=~(1u<<i);
        }
        if(s->record_lanes&1u){zero_bytes(s->image.pattern.cv0_extra,sizeof s->image.pattern.cv0_extra);
            zero_bytes(s->image.pattern.cv0_extra_mask,sizeof s->image.pattern.cv0_extra_mask);}
    }
    s->record_state=XSEQ_REC_ACTIVE;s->capture_valid=0u;s->capture_future=0u;s->record_elapsed=0u;
}
/* At most six raw positions fall onto one coarse grid point (including the
 * clamped final point). The image stays untouched so a finer grid restores it. */
static int playback_window(const XenolithSequence *s,uint32_t *first,uint32_t *end) {
    uint32_t span=1u<<s->play_quantize,at=s->playhead,length=s->image.config.length;
    uint32_t last=((length-1u)/span)*span;
    if(at%span)return 0;
    *first=at>=span/2u?at-span/2u:0u;
    *end=at==last?length:at+span/2u;
    return 1;
}
static void quantized_note_duration(XenolithSequence *s,uint32_t source) {
    uint32_t count=0u,index=source,length=s->image.config.length;
    const XenolithSequenceStep *p=&s->image.pattern.step[source];
    while(count+1u<length){
        uint32_t next=(index+1u)%length;
        const XenolithSequenceStep *following=&s->image.pattern.step[next];
        if(s->image.config.note_mode==XSEQ_CONTINUOUS?following->kind==XSEQ_REST:following->kind!=XSEQ_TIE)break;
        index=next;p=following;count++;
    }
    if(s->image.config.note_mode==XSEQ_CONTINUOUS){
        uint32_t span=1u<<s->play_quantize;
        uint32_t end=((source+count+1u+span/2u)/span)*span;
        s->quant_gate_slots=end>s->playhead?end-s->playhead-1u:0u;
        s->gate_phase=UINT32_MAX;
    }else{
        s->quant_gate_slots=count;
        s->gate_phase=(uint32_t)((float)p->gate_percent*(4294967296.0f/100.0f));
    }
}
static void enter_step(XenolithSequence *s,uint32_t cycle_start,uint32_t initial) {
    uint32_t i,first=0u,end=0u;
    XenolithSequenceStep *p;
    if(s->record_state==XSEQ_REC_ACTIVE){
        if(!initial && ++s->record_elapsed>=s->record_length){
            s->record_state=XSEQ_REC_IDLE;s->record_mode=XSEQ_RECORD_OFF;
            s->record_notes=0u;s->record_lanes=0u;s->capture_valid=0u;
        }
    }else if(s->record_state==XSEQ_REC_ARMED && s->record_mode>=XSEQ_RECORD_REPLACE){
        uint32_t start=s->record_notes||(s->record_lanes&1u)||s->image.config.division==XSEQ_SIXTYFOURTH?cycle_start:
            s->playhead%xenolith_sequence_cv_length(&s->image.config)==0u;
        if(start)begin_recording(s);
    }
    p=&s->image.pattern.step[s->playhead];
    if(s->capture_future&&s->playhead==s->capture_step)s->capture_future=0u;
    if(s->record_state==XSEQ_REC_ACTIVE && s->record_notes && s->capture_valid
       && !s->capture_future && (s->keys_down[s->capture_note>>5]&(1u<<(s->capture_note&31u))) && s->playhead!=s->capture_step){
        p->kind=XSEQ_TIE;p->note=0u;p->gate_percent=99u;s->capture_step=s->playhead;
    }
    s->frame.step_event=1u;s->play_quantize=s->image.config.quantize;
    if(s->play_quantize){
        uint32_t source=UINT32_MAX,expire=0u;
        if(!initial){
            if(s->quant_gate_slots)s->quant_gate_slots--;
            else if(s->gate)expire=1u;
        }
        if(playback_window(s,&first,&end))for(i=first;i<end;i++){
            s->frame.triggers|=s->image.pattern.step[i].triggers;
            if(s->image.pattern.step[i].kind==XSEQ_NOTE)source=i;
        }
        if(expire && source==UINT32_MAX && !(s->record_state==XSEQ_REC_ACTIVE && s->record_notes
           && s->capture_valid && !s->capture_future && (s->keys_down[s->capture_note>>5]&(1u<<(s->capture_note&31u))))){
            s->frame.note_off=1u;s->gate=0u;
        }
        if(source!=UINT32_MAX){
            s->frame.envelope_on=s->image.config.note_mode==XSEQ_ADSR || !s->gate;
            s->note=s->image.pattern.step[source].note;s->gate=1u;s->frame.note_on=1u;
            quantized_note_duration(s,source);
        }
    }else{
        s->frame.triggers=p->triggers;
        s->gate_phase=(uint32_t)((float)p->gate_percent*(4294967296.0f/100.0f));
        if(p->kind==XSEQ_NOTE){
            s->frame.envelope_on=s->image.config.note_mode==XSEQ_ADSR || !s->gate;
            s->note=p->note;s->gate=1u;s->frame.note_on=1u;
        }
        else if(p->kind==XSEQ_REST){s->frame.note_off|=s->gate;s->gate=0u;}
    }
    for(i=0;i<4u;i++)if(s->image.pattern.lane[i].enabled){
        float value=0.0f;uint32_t enabled;
        if(!i&&s->play_quantize){
            uint32_t raw,found=0u;float candidate;uint32_t active;
            if(end)for(raw=first;raw<end;raw++){
                xenolith_sequence_get_lane_step(&s->image.pattern,0u,raw,&candidate,&active);
                if(active){value=candidate;found=1u;}
            }
            enabled=found;
        }else xenolith_sequence_get_lane_step(&s->image.pattern,i,xenolith_sequence_lane_position(&s->image.config,i,s->playhead),&value,&enabled);
        if(enabled){set_lane_target(s,i,value);s->lane_valid_mask|=1u<<i;}
    }
}
const XenolithSequenceFrame *xenolith_sequence_sample(XenolithSequence *s) {
    uint32_t i;
    if(!s)return 0;
    s->frame.sample=s->sample_count++;
    s->frame.step_event=0u;s->frame.triggers=0u;s->frame.note_on=0u;s->frame.note_off=s->pending_off;s->pending_off=0u;
    s->frame.envelope_on=0u;
    s->frame.lane_mask=0u;
    if(s->running && s->image.config.enabled){
        if(s->pending_start){s->pending_start=0u;enter_step(s,1u,1u);}
        else{
            uint32_t previous=s->phase;
            s->phase+=s->phase_increment;
            if(s->phase<previous){
                s->playhead=(s->playhead+1u)%s->image.config.length;enter_step(s,s->playhead==0u,0u);
            }
        }
        if(s->gate && s->phase>=s->gate_phase
           && !(s->record_state==XSEQ_REC_ACTIVE && s->record_notes && s->capture_valid
                && !s->capture_future && (s->keys_down[s->capture_note>>5]&(1u<<(s->capture_note&31u))))
           && (s->play_quantize?!s->quant_gate_slots:
               s->image.config.note_mode==XSEQ_ADSR && s->image.pattern.step[(s->playhead+1u)%s->image.config.length].kind!=XSEQ_TIE)){
            s->gate=0u;s->frame.note_off=1u;
        }
        for(i=0;i<4u;i++)if((s->lane_valid_mask&(1u<<i)) && s->image.pattern.lane[i].enabled
            && s->image.pattern.lane[i].destination!=XSEQ_DEST_NONE){
            float delta=s->lane_target[i]-s->lane_value[i];
            if(delta<=s->rise_step[i] && delta>=-s->fall_step[i]){
                s->lane_value[i]=s->lane_target[i];s->lane_error[i]=0.0f;
            }else{
                float adjusted,next;
                if(delta>s->rise_step[i])delta=s->rise_step[i];
                if(delta< -s->fall_step[i])delta= -s->fall_step[i];
                adjusted=delta-s->lane_error[i];next=s->lane_value[i]+adjusted;
                s->lane_error[i]=(next-s->lane_value[i])-adjusted;
                if((delta>0.0f && next>s->lane_target[i]) || (delta<0.0f && next<s->lane_target[i])){
                    next=s->lane_target[i];s->lane_error[i]=0.0f;
                }
                s->lane_value[i]=next;
            }
            s->frame.lane_mask|=1u<<i;
        }
    }
    s->frame.playhead=s->playhead;s->frame.running=s->running;s->frame.note=s->note;s->frame.gate=s->gate;
    s->frame.record_state=s->record_state;s->frame.record_mode=s->record_mode;
    for(i=0;i<4u;i++)s->frame.lane[i]=s->lane_value[i];
    return &s->frame;
}
int xenolith_sequence_snapshot(const XenolithSequence *s,XenolithSequenceImage *image) {
    if(!s || !image)return 0;
    *image=s->image;return 1;
}
int xenolith_sequence_load(XenolithSequence *s,const void *raw,uint32_t bytes) {
    const XenolithSequenceImage *image=(const XenolithSequenceImage*)raw;
    const XenolithSequenceImageV4 *legacy=(const XenolithSequenceImageV4*)raw;
    XenolithSequenceImage saved;uint32_t off,note,sample;
    if(!s)return 0;
    if(xenolith_sequence_image_valid(image,bytes))saved=*image;
    else if(!xenolith_sequence_image_from_v4(&saved,legacy,bytes)&&!xenolith_sequence_image_from_v3(&saved,legacy,bytes)
        &&!xenolith_sequence_image_from_v2(&saved,legacy,bytes)&&!xenolith_sequence_image_from_v1(&saved,legacy,bytes))return 0;
    off=s->pending_off|s->gate;note=s->note;sample=s->sample_count;
    zero_bytes(s,sizeof *s);s->image=saved;s->pending_off=off;s->note=note;s->sample_count=sample;
    s->phase_increment=prepare_phase(&s->image.config);prepare_lanes(s);return 1;
}
