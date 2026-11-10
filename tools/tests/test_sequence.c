/* Host tests for the independently clocked control sequencer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "xenolith_sequence.h"
#include "xenolith_envelope.h"
static uint32_t assertions,scenarios;
#define CHECK(x) do { assertions++; if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);} }while(0)
static void enable(XenolithSequence *s,float bpm,uint32_t division,uint32_t length) {
    XenolithSequenceConfig c=s->image.config;c.enabled=1;c.bpm=bpm;c.division=division;c.length=length;
    CHECK(xenolith_sequence_configure(s,&c));
}
static void step(XenolithSequence *s,unsigned index,unsigned kind,unsigned note,unsigned gate,unsigned triggers) {
    XenolithSequenceStep p={0};p.kind=(uint8_t)kind;p.note=(uint8_t)note;p.gate_percent=(uint8_t)gate;p.triggers=(uint8_t)triggers;
    CHECK(xenolith_sequence_edit_step(s,index,&p));
}
static const XenolithSequenceFrame *advance(XenolithSequence *s,uint32_t count) {
    const XenolithSequenceFrame *f=0;while(count--)f=xenolith_sequence_sample(s);return f;
}
static const XenolithSequenceFrame *next_step(XenolithSequence *s) {
    const XenolithSequenceFrame *f;uint32_t guard=0;
    do{f=xenolith_sequence_sample(s);CHECK(++guard<200000u);}while(!f->step_event);
    return f;
}
static XenolithSequenceImageV4 legacy_image(const XenolithSequenceImage *p,uint32_t version) {
    XenolithSequenceImageV4 old={0};unsigned i;
    old.magic=p->magic;old.version=version;old.bytes=sizeof old;
    old.config.enabled=p->config.enabled;old.config.length=p->config.length<16u?p->config.length:16u;
    old.config.division=p->config.division<=XSEQ_EIGHTH_TRIPLET?p->config.division:XSEQ_SIXTEENTH;
    old.config.note_mode=p->config.note_mode;old.config.bpm=p->config.bpm;
    for(i=0;i<16;i++){
        old.pattern.step[i].note=p->pattern.step[i].note;old.pattern.step[i].kind=p->pattern.step[i].kind;
        old.pattern.step[i].gate_percent=p->pattern.step[i].gate_percent;old.pattern.step[i].triggers=p->pattern.step[i].triggers;
    }
    memcpy(old.pattern.lane,p->pattern.lane,sizeof old.pattern.lane);return old;
}
static void clock_tests(void) {
    XenolithSequence s;XenolithSequenceImage image;unsigned d,b,i;
    const float tempos[3]={20,120,300};
    xenolith_sequence_init(&s);CHECK(xenolith_sequence_image_valid(&s.image,sizeof s.image));
    CHECK(s.image.bytes==848 && !s.image.config.enabled && s.image.config.bpm==120);
    CHECK(!xenolith_sequence_play_toggle(&s));
    for(i=0;i<1000;i++){
        const XenolithSequenceFrame *f=xenolith_sequence_sample(&s);
        CHECK(f->sample==i && !f->running && !f->gate && !f->step_event && !f->triggers && !f->lane_mask);
    }
    CHECK(xenolith_sequence_snapshot(&s,&image));CHECK(!image.config.enabled);++scenarios;
    for(d=0;d<4;d++)for(b=0;b<3;b++){
        uint64_t phase=0;uint32_t events=0;
        static const double steps_per_beat[4]={1,2,4,3};
        xenolith_sequence_init(&s);enable(&s,tempos[b],d,7);
        CHECK(fabs((double)s.phase_increment-(double)tempos[b]*steps_per_beat[d]*4294967296.0/(60.0*44100.0))<1.0);
        for(i=0;i<16;i++)step(&s,i,XSEQ_REST,0,50,i&7u);
        CHECK(xenolith_sequence_play_toggle(&s));
        for(i=0;i<44100u*12u;i++){
            const XenolithSequenceFrame *f=xenolith_sequence_sample(&s);
            uint32_t event=i==0;
            if(i){phase+=s.phase_increment;if(phase>=UINT64_C(4294967296)){phase-=UINT64_C(4294967296);event=1;}}
            if(event)events++;
            CHECK(s.phase==(uint32_t)phase && f->step_event==event && f->playhead==(events-1u)%7u);
            CHECK(f->triggers==(event?f->playhead&7u:0u));CHECK(!f->gate && !f->note_on);
        }
        CHECK(events==1u+(uint32_t)(((uint64_t)(44100u*12u-1u)*s.phase_increment)>>32));++scenarios;
    }
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);s.sample_count=UINT32_MAX-2u;
    CHECK(xenolith_sequence_play_toggle(&s));
    for(i=0;i<5;i++)CHECK(xenolith_sequence_sample(&s)->sample==(uint32_t)(UINT32_MAX-2u+i));
    { uint32_t phase=s.phase;XenolithSequenceConfig c=s.image.config;c.bpm=137.5f;
      CHECK(xenolith_sequence_configure(&s,&c));CHECK(s.phase==phase && s.running); }
    ++scenarios;
}
static void gate_tests(void) {
    XenolithSequence s;const XenolithSequenceFrame *f;uint32_t count;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,2);
    step(&s,0,XSEQ_NOTE,60,25,7);step(&s,1,XSEQ_REST,0,50,4);
    CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);
    CHECK(f->note_on && !f->note_off && f->gate && f->note==60 && f->triggers==7);
    count=0;do{f=xenolith_sequence_sample(&s);count++;CHECK(count<2205);}while(!f->note_off);
    CHECK(!f->gate && count==(s.gate_phase+s.phase_increment-1u)/s.phase_increment);
    f=xenolith_sequence_sample(&s);CHECK(!f->note_off);
    f=next_step(&s);CHECK(f->playhead==1 && !f->note_on && !f->gate && f->triggers==4);
    f=next_step(&s);CHECK(f->playhead==0 && f->note_on && f->gate);++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,3);
    step(&s,0,XSEQ_NOTE,127,1,1);step(&s,1,XSEQ_TIE,0,50,2);step(&s,2,XSEQ_REST,0,50,4);
    CHECK(xenolith_sequence_play_toggle(&s));xenolith_sequence_sample(&s);
    do{f=xenolith_sequence_sample(&s);CHECK(f->gate && !f->note_off && !f->note_on);}while(!f->step_event);
    CHECK(f->playhead==1 && f->note==127 && f->triggers==2);
    do{f=xenolith_sequence_sample(&s);}while(!f->note_off);
    CHECK(f->playhead==1 && !f->gate && f->note==127);
    f=next_step(&s);CHECK(f->playhead==2 && !f->gate && f->triggers==4);++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);step(&s,0,XSEQ_TIE,0,99,7);
    CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);
    CHECK(!f->gate && !f->note_on && f->triggers==7);
    xenolith_sequence_stop(&s);CHECK(!xenolith_sequence_sample(&s)->note_off);++scenarios;
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_QUARTER,1);step(&s,0,XSEQ_NOTE,0,99,0);
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_sample(&s)->gate);
    xenolith_sequence_stop(&s);f=xenolith_sequence_sample(&s);CHECK(f->note_off && !f->gate && !f->running);
    CHECK(!xenolith_sequence_sample(&s)->note_off);CHECK(xenolith_sequence_play_toggle(&s));
    f=xenolith_sequence_sample(&s);CHECK(f->step_event && f->playhead==0 && f->note_on);++scenarios;
}
static void lane_tests(void) {
    XenolithSequence s;XenolithSequenceLane lane;const XenolithSequenceFrame *f;unsigned i;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,3);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.destination=XSEQ_DEST_INDEX;lane.step_mask=3;
    lane.value[0]=1;lane.value[1]=0;lane.rise_seconds=.01f;lane.fall_seconds=.02f;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    lane.destination=XSEQ_DEST_RATIO;lane.rise_seconds=.02f;lane.fall_seconds=.01f;
    CHECK(xenolith_sequence_edit_lane(&s,1,&lane));
    CHECK(xenolith_sequence_play_toggle(&s));f=advance(&s,100);
    CHECK(f->lane_mask==3 && f->lane[0]>f->lane[1] && fabsf(f->lane[0]-2*f->lane[1])<.00001f);
    f=advance(&s,800);CHECK(f->lane[0]==1 && f->lane[1]==1);
    f=next_step(&s);CHECK(f->playhead==1);f=advance(&s,500);CHECK(f->lane[0]>0 && f->lane[1]==0);
    f=advance(&s,500);CHECK(f->lane[0]==0 && f->lane[1]==0);
    f=next_step(&s);CHECK(f->playhead==2 && f->lane_mask==3 && f->lane[0]==0);++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,2);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.destination=XSEQ_DEST_PITCH;lane.step_mask=2;lane.value[1]=.75f;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));CHECK(xenolith_sequence_play_toggle(&s));
    CHECK(!xenolith_sequence_sample(&s)->lane_mask);f=next_step(&s);CHECK(f->lane_mask==1 && f->lane[0]==.75f);
    f=next_step(&s);CHECK(f->lane_mask==1 && f->lane[0]==.75f);
    lane.enabled=0;CHECK(xenolith_sequence_edit_lane(&s,0,&lane));CHECK(!xenolith_sequence_sample(&s)->lane_mask);
    for(i=0;i<1000;i++)CHECK(isfinite(xenolith_sequence_sample(&s)->lane[0]));++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.destination=XSEQ_DEST_INDEX;lane.step_mask=1;
    lane.value[0]=1;lane.rise_seconds=60;lane.fall_seconds=60;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));CHECK(xenolith_sequence_play_toggle(&s));
    f=advance(&s,44100u*30u);CHECK(fabsf(f->lane[0]-.5f)<.00001f);
    f=advance(&s,44100u*30u+2u);CHECK(f->lane[0]==1.0f);
    lane.value[0]=0;CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    f=advance(&s,44100u*30u);CHECK(fabsf(f->lane[0]-.5f)<.00001f);
    f=advance(&s,44100u*30u+2u);CHECK(f->lane[0]==0.0f);++scenarios;
}
static void record_tests(void) {
    XenolithSequence s;XenolithSequenceImage before,take;XenolithSequenceLane lane;const XenolithSequenceFrame *f;
    unsigned guard;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,4);
    step(&s,2,XSEQ_REST,0,33,4);lane=s.image.pattern.lane[0];lane.destination=XSEQ_DEST_INDEX;lane.enabled=1;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));CHECK(xenolith_sequence_select_step(&s,2));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,1,1));
    CHECK(xenolith_sequence_capture_parameter(&s,0,.37f));CHECK(s.edit_step==2 && s.image.pattern.lane[0].value[2]==.37f);
    CHECK(xenolith_sequence_capture_key(&s,60,1));CHECK(s.edit_step==3 && s.image.pattern.step[2].kind==XSEQ_NOTE);
    CHECK(s.image.pattern.step[2].gate_percent==33 && s.image.pattern.step[2].triggers==4);
    CHECK(!xenolith_sequence_capture_key(&s,60,1) && s.edit_step==3);
    CHECK(!xenolith_sequence_capture_key(&s,60,0));
    CHECK(xenolith_sequence_capture_key(&s,61,1));CHECK(s.edit_step==0);
    step(&s,0,XSEQ_TIE,0,50,0);CHECK(s.edit_step==0); /* serialized rest/tie edits during STEP entry */
    lane=s.image.pattern.lane[0];CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    CHECK(!xenolith_sequence_apply_pattern(&s,&s.image.pattern));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OFF,0,0));CHECK(xenolith_sequence_snapshot(&s,&take));
    CHECK(take.pattern.step[2].note==60 && take.pattern.step[3].note==61 && take.config.enabled);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,1,1));CHECK(xenolith_sequence_play_toggle(&s));
    CHECK(s.running && s.record_state==XSEQ_REC_IDLE && s.record_mode==XSEQ_RECORD_OFF);
    CHECK(memcmp(&take,&s.image,sizeof take)==0);++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,2);
    lane=s.image.pattern.lane[0];lane.destination=XSEQ_DEST_INDEX;lane.enabled=1;lane.step_mask=3;lane.value[0]=.1f;lane.value[1]=.9f;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,1));CHECK(xenolith_sequence_play_toggle(&s));
    f=xenolith_sequence_sample(&s);CHECK(f->record_state==XSEQ_REC_ACTIVE && s.image.pattern.lane[0].step_mask==0);
    CHECK(!xenolith_sequence_edit_step(&s,0,&s.image.pattern.step[0]));
    advance(&s,500);CHECK(xenolith_sequence_capture_key(&s,64,1));CHECK(xenolith_sequence_capture_parameter(&s,0,.2f));
    f=next_step(&s);CHECK(f->playhead==1 && s.image.pattern.step[1].kind==XSEQ_TIE);
    advance(&s,880);CHECK(xenolith_sequence_capture_key(&s,64,0));
    CHECK(s.image.pattern.step[1].gate_percent>=39 && s.image.pattern.step[1].gate_percent<=41);
    CHECK(xenolith_sequence_capture_parameter(&s,0,.8f));f=next_step(&s);
    CHECK(f->record_state==XSEQ_REC_IDLE && f->record_mode==XSEQ_RECORD_OFF && f->note_on && f->note==64);
    CHECK(s.image.pattern.lane[0].step_mask==3 && s.image.pattern.lane[0].value[1]==.8f);
    CHECK(xenolith_sequence_snapshot(&s,&before));CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,1,1));
    CHECK(s.record_state==XSEQ_REC_ARMED);guard=0;
    while(s.record_state!=XSEQ_REC_ACTIVE){xenolith_sequence_sample(&s);CHECK(++guard<10000);}
    while(s.record_state!=XSEQ_REC_IDLE){xenolith_sequence_sample(&s);CHECK(++guard<20000);}
    CHECK(memcmp(&before,&s.image,sizeof before)==0);++scenarios;
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,1,1));
    while(s.record_state!=XSEQ_REC_ACTIVE)xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_capture_key(&s,72,1));CHECK(xenolith_sequence_capture_parameter(&s,0,.61f));
    take=s.image;xenolith_sequence_panic(&s);f=xenolith_sequence_sample(&s);
    CHECK(!f->running && !f->gate && f->record_state==XSEQ_REC_IDLE && memcmp(&take,&s.image,sizeof take)==0);++scenarios;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);step(&s,0,XSEQ_NOTE,60,99,7);
    before=s.image;CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,0,1));CHECK(xenolith_sequence_play_toggle(&s));
    f=xenolith_sequence_sample(&s);CHECK(f->note_on && f->triggers==7 && s.image.pattern.step[0].note==60);
    CHECK(memcmp(before.pattern.step,s.image.pattern.step,sizeof before.pattern.step)==0);++scenarios;
}
static void invalid_image(XenolithSequence *s,const XenolithSequenceImage *bad,uint32_t bytes) {
    XenolithSequence before=*s;CHECK(!xenolith_sequence_image_valid(bad,bytes));
    CHECK(!xenolith_sequence_load(s,bad,bytes));CHECK(memcmp(s,&before,sizeof before)==0);
}
static void validation_tests(void) {
    XenolithSequence s,other,before;XenolithSequenceImage good,bad;XenolithSequenceConfig c;unsigned i;
    xenolith_sequence_init(&s);good=s.image;
    for(i=0;i<sizeof good;i++)invalid_image(&s,&good,i);
    invalid_image(&s,&good,sizeof good+1u);
#define BAD(expr) do { bad=good;expr;invalid_image(&s,&bad,sizeof bad); }while(0)
    BAD(bad.magic=0);BAD(bad.version=XENOLITH_SEQUENCE_VERSION+1u);BAD(bad.bytes--);BAD(bad.reserved=1);
    BAD(bad.config.enabled=2);BAD(bad.config.length=0);BAD(bad.config.length=65);BAD(bad.config.division=5);BAD(bad.config.quantize=3);
    BAD(bad.config.note_mode=2);BAD(bad.config.bpm=NAN);BAD(bad.config.bpm=INFINITY);BAD(bad.config.bpm=19);BAD(bad.config.bpm=301);
    BAD(bad.pattern.step[15].note=128);BAD(bad.pattern.step[15].kind=3);BAD(bad.pattern.step[15].gate_percent=0);
    BAD(bad.pattern.step[15].gate_percent=100);BAD(bad.pattern.step[15].triggers=8);BAD(bad.pattern.step[63].kind=3);
    BAD(bad.pattern.cv0_extra[47]=NAN);BAD(bad.pattern.cv0_extra_mask[1]=0x10000u);
    BAD(bad.pattern.lane[3].destination=XSEQ_DEST_COUNT);BAD(bad.pattern.lane[3].enabled=2);BAD(bad.pattern.lane[3].step_mask=0x10000);
    BAD(bad.pattern.lane[3].reserved=1);BAD(bad.pattern.lane[3].rise_seconds=-1);BAD(bad.pattern.lane[3].fall_seconds=61);
    BAD(bad.pattern.lane[3].rise_seconds=NAN);BAD(bad.pattern.lane[3].value[15]=INFINITY);BAD(bad.pattern.lane[3].value[15]=1.01f);
    BAD(bad.pattern.lane[0].enabled=bad.pattern.lane[1].enabled=1;bad.pattern.lane[0].destination=bad.pattern.lane[1].destination=0);
#undef BAD
    c=s.image.config;c.bpm=NAN;before=s;CHECK(!xenolith_sequence_configure(&s,&c));CHECK(memcmp(&s,&before,sizeof s)==0);
    CHECK(!xenolith_sequence_capture_parameter(&s,0,NAN));CHECK(!xenolith_sequence_capture_key(&s,128,1));
    CHECK(!xenolith_sequence_record_arm(&s,4,0,0));CHECK(!xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,1,0));
    CHECK(!xenolith_sequence_sample(0));CHECK(!xenolith_sequence_image_valid(0,sizeof good));
    CHECK(!xenolith_sequence_load(&s,0,sizeof good));CHECK(!xenolith_sequence_snapshot(&s,0));++scenarios;
    enable(&s,137.5f,XSEQ_EIGHTH_TRIPLET,8);step(&s,0,XSEQ_NOTE,60,99,0);
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_sample(&s)->note_on);
    CHECK(xenolith_sequence_snapshot(&s,&good));xenolith_sequence_init(&other);
    CHECK(xenolith_sequence_load(&other,&good,sizeof good));CHECK(other.image.config.enabled && other.image.config.bpm==137.5f);
    for(i=0;i<1000;i++){const XenolithSequenceFrame *f=xenolith_sequence_sample(&other);CHECK(!f->note_on && !f->gate && !f->running && !f->lane_mask);}
    CHECK(memcmp(&other.image,&good,sizeof good)==0);
    CHECK(xenolith_sequence_load(&s,&s.image,sizeof s.image));
    CHECK(xenolith_sequence_sample(&s)->note_off);CHECK(!xenolith_sequence_sample(&s)->note_on);
    CHECK(xenolith_sequence_play_toggle(&other));CHECK(xenolith_sequence_sample(&other)->note_on);++scenarios;
}
static void amplitude_tests(void) {
    XenolithSequence s,loaded;XenolithSequenceLane lane,duplicate;XenolithSequenceImage image;
    const XenolithSequenceFrame *f;unsigned i;
    CHECK(XSEQ_DEST_PITCH==0 && XSEQ_DEST_INDEX==1 && XSEQ_DEST_RATIO==2 && XSEQ_DEST_DRIFT==3
        && XSEQ_DEST_WET_MIX==4 && XSEQ_DEST_CORE_RATE==5 && XSEQ_DEST_WET_RATE==6 && XSEQ_DEST_NONE==7
        && XSEQ_DEST_AMPLITUDE==8 && XSEQ_DEST_ANS_CELL==9 && XSEQ_DEST_BITS==10 && XSEQ_DEST_EXP_DEPTH==11 && XSEQ_DEST_COUNT==14);
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,2);
    for(i=0;i<4;i++)CHECK(s.image.pattern.lane[i].destination==7 && !s.image.pattern.lane[i].enabled);
    lane=s.image.pattern.lane[0];lane.destination=XSEQ_DEST_AMPLITUDE;lane.enabled=1;lane.step_mask=3;
    lane.value[0]=0;lane.value[1]=1;CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    duplicate=lane;CHECK(!xenolith_sequence_edit_lane(&s,1,&duplicate));
    CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);CHECK(f->lane_mask==1 && f->lane[0]==0);
    f=next_step(&s);CHECK(f->lane_mask==1 && f->lane[0]==1);++scenarios;
    xenolith_sequence_stop(&s);CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,1));
    CHECK(xenolith_sequence_select_step(&s,0));CHECK(xenolith_sequence_capture_parameter(&s,0,.375f));
    CHECK(s.image.pattern.lane[0].value[0]==.375f && s.image.pattern.lane[0].destination==XSEQ_DEST_AMPLITUDE);
    CHECK(xenolith_sequence_snapshot(&s,&image));CHECK(image.bytes==848 && image.version==XENOLITH_SEQUENCE_VERSION);
    xenolith_sequence_init(&loaded);CHECK(xenolith_sequence_load(&loaded,&image,sizeof image));
    CHECK(!memcmp(&loaded.image,&image,sizeof image));CHECK(xenolith_sequence_play_toggle(&loaded));
    f=xenolith_sequence_sample(&loaded);CHECK(f->lane_mask==1 && f->lane[0]==.375f);++scenarios;
}
static void ans_bits_destination_tests(void) {
    XenolithSequence s,loaded;XenolithSequenceLane lane,duplicate;XenolithSequenceImage image;
    const XenolithSequenceFrame *f;unsigned destination,k;
    for(destination=XSEQ_DEST_ANS_CELL;destination<=XSEQ_DEST_BITS;destination++){
        xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);
        lane=s.image.pattern.lane[0];lane.destination=destination;lane.enabled=1;lane.step_mask=1;
    CHECK(sizeof image==848 && sizeof s==1144);
        for(k=0;k<27;k++){
            xenolith_sequence_stop(&s);lane.value[0]=(float)k/26.0f;
            CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
            duplicate=lane;CHECK(!xenolith_sequence_edit_lane(&s,1,&duplicate));
            CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);
            CHECK(f->lane_mask==1 && f->lane[0]==lane.value[0] && f->lane[0]>=0 && f->lane[0]<=1);
            CHECK(xenolith_sequence_snapshot(&s,&image));CHECK(image.pattern.lane[0].destination==destination);
            xenolith_sequence_init(&loaded);CHECK(xenolith_sequence_load(&loaded,&image,sizeof image));
            CHECK(!memcmp(&loaded.image,&image,sizeof image));
        }++scenarios;
    }
}
static void note_mode_tests(void) {
    XenolithSequence s;XenolithSequenceConfig c;XenolithSequenceImage image,upgraded,before;XenolithSequenceImageV4 old;
    const XenolithSequenceFrame *f;unsigned i,mode;
    for(mode=0;mode<2;mode++){
        xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,6);
        c=s.image.config;c.note_mode=mode;CHECK(xenolith_sequence_configure(&s,&c));
        step(&s,0,XSEQ_NOTE,60,1,1);step(&s,1,XSEQ_NOTE,60,1,2);
        step(&s,2,XSEQ_NOTE,67,1,4);step(&s,3,XSEQ_TIE,0,1,7);
        step(&s,4,XSEQ_REST,0,1,3);step(&s,5,XSEQ_TIE,0,1,5);
        CHECK(xenolith_sequence_play_toggle(&s));
        for(i=0;i<13;i++){
            unsigned which=i%6;f=next_step(&s);
            CHECK(f->playhead==which && f->step_event);
            CHECK(f->note_on==(which<=2));
            CHECK(f->envelope_on==(which<=2 && (mode==XSEQ_ADSR||which==0)));
            if(which==1)CHECK(f->note==60);if(which==2||which==3)CHECK(f->note==67);
            if(mode==XSEQ_CONTINUOUS){
                CHECK(f->gate==(which<4));CHECK(f->note_off==(which==4));
                /* Check the entire step, including far beyond GATE1%. */
                while(s.phase+s.phase_increment>=s.phase){
                    f=xenolith_sequence_sample(&s);CHECK(!f->step_event);
                    CHECK(f->gate==(which<4) && !f->envelope_on && !f->note_on && !f->note_off);
                }
            }
        }
        CHECK(xenolith_sequence_snapshot(&s,&image));xenolith_sequence_stop(&s);
        f=xenolith_sequence_sample(&s);CHECK(f->note_off && !f->gate && !f->running && !f->envelope_on);
        f=xenolith_sequence_sample(&s);CHECK(!f->note_off && !f->lane_mask);
        CHECK(!memcmp(&image,&s.image,sizeof image));
        CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_sample(&s)->envelope_on);
        xenolith_sequence_panic(&s);CHECK(xenolith_sequence_sample(&s)->note_off);
        CHECK(!xenolith_sequence_sample(&s)->note_off);++scenarios;
    }
    /* A live mode change preserves phase and never manufactures a NOTE. */
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,1);step(&s,0,XSEQ_NOTE,60,25,0);
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_sample(&s)->envelope_on);
    c=s.image.config;c.note_mode=XSEQ_CONTINUOUS;CHECK(xenolith_sequence_configure(&s,&c));
    f=advance(&s,1200);CHECK(f->gate&&!f->envelope_on);
    c.note_mode=XSEQ_ADSR;CHECK(xenolith_sequence_configure(&s,&c));f=xenolith_sequence_sample(&s);
    CHECK(f->note_off&&!f->gate&&!f->note_on);c.note_mode=XSEQ_CONTINUOUS;
    CHECK(xenolith_sequence_configure(&s,&c));f=xenolith_sequence_sample(&s);CHECK(!f->gate&&!f->envelope_on);
    CHECK(next_step(&s)->envelope_on);++scenarios;
    /* Legacy image migration is explicit and cannot reuse its reserved word. */
    xenolith_sequence_init(&s);image=s.image;image.config.length=16u;image.config.division=XSEQ_SIXTEENTH;old=legacy_image(&image,1);
    CHECK(!xenolith_sequence_image_valid((const XenolithSequenceImage*)&old,sizeof old));CHECK(xenolith_sequence_image_v1_valid(&old,sizeof old));
    CHECK(xenolith_sequence_image_from_v1(&upgraded,&old,sizeof old));CHECK(!memcmp(&image,&upgraded,sizeof image));
    CHECK(xenolith_sequence_load(&s,&old,sizeof old));CHECK(s.image.version==XENOLITH_SEQUENCE_VERSION&&s.image.config.note_mode==XSEQ_ADSR);
    before=upgraded;old.config.reserved=1;
    CHECK(!xenolith_sequence_image_v1_valid(&old,sizeof old));CHECK(!xenolith_sequence_image_from_v1(&upgraded,&old,sizeof old));
    CHECK(!memcmp(&before,&upgraded,sizeof before));CHECK(!xenolith_sequence_load(&s,&old,sizeof old));
    old.config.reserved=0;
    { union {XenolithSequenceImage now;XenolithSequenceImageV4 old;} alias;
      alias.old=old;CHECK(xenolith_sequence_image_from_v1(&alias.now,&alias.old,sizeof alias.old));
      CHECK(!memcmp(&alias.now,&image,sizeof image)); }
    old.version=5;CHECK(!xenolith_sequence_image_from_v1(&upgraded,&old,sizeof old));++scenarios;
}
static void envelope_mode_tests(void) {
    XenolithSequence s;XenolithEnvelope env;XenolithSequenceConfig c;XenolithSequenceLane lane;
    const XenolithSequenceFrame *f;unsigned mode,i,notes,attacks,offs;
    for(mode=0;mode<2;mode++){
        xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,4);
        c=s.image.config;c.note_mode=mode;CHECK(xenolith_sequence_configure(&s,&c));
        step(&s,0,XSEQ_NOTE,60,25,0);step(&s,1,XSEQ_NOTE,60,25,0);
        step(&s,2,XSEQ_TIE,0,25,0);step(&s,3,XSEQ_REST,0,25,0);
        xenolith_envelope_init(&env,44100);xenolith_envelope_set(&env,1,10,.25f,5);
        CHECK(xenolith_sequence_play_toggle(&s));notes=attacks=offs=0;
        for(i=0;i<8000;i++){
            f=xenolith_sequence_sample(&s);notes+=f->note_on;attacks+=f->envelope_on;offs+=f->note_off;
            if(f->note_off)xenolith_envelope_note_off(&env);
            if(f->envelope_on)xenolith_envelope_note_on(&env);
            if(f->step_event&&f->playhead==1)CHECK(env.stage==(mode==XSEQ_ADSR?XENOLITH_ENV_ATTACK:XENOLITH_ENV_SUSTAIN));
            if(mode==XSEQ_CONTINUOUS&&i>500&&f->playhead<3)CHECK(env.stage==XENOLITH_ENV_SUSTAIN&&env.level==.25f);
            CHECK(xenolith_envelope_sample(&env)>=0 && env.level<=1);
        }
        CHECK(notes==2 && attacks==(mode==XSEQ_ADSR?2:1));CHECK(offs==(mode==XSEQ_ADSR?2:1));
        CHECK(env.stage==XENOLITH_ENV_IDLE&&env.level==0);++scenarios;
    }
    /* CV/ANS-only REST steps retain lanes/triggers without claiming a note gate. */
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,2);c=s.image.config;c.note_mode=XSEQ_CONTINUOUS;
    CHECK(xenolith_sequence_configure(&s,&c));step(&s,0,XSEQ_REST,0,1,7);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.step_mask=3;lane.destination=XSEQ_DEST_AMPLITUDE;
    lane.value[0]=.25f;lane.value[1]=.75f;CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    lane.destination=XSEQ_DEST_ANS_CELL;CHECK(xenolith_sequence_edit_lane(&s,1,&lane));
    CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);
    CHECK(f->lane_mask==3&&f->lane[0]==.25f&&f->triggers==7&&!f->gate&&!f->envelope_on&&!f->note_on);
    f=next_step(&s);CHECK(f->lane_mask==3&&f->lane[0]==.75f&&!f->gate&&!f->envelope_on);
    xenolith_sequence_stop(&s);f=xenolith_sequence_sample(&s);CHECK(!f->lane_mask&&!f->gate);++scenarios;
}
static void physical_values_and_v3_tests(void) {
    XenolithSequence s;XenolithSequenceImage image,upgraded,before;XenolithSequenceImageV4 old;
    float low,high,physical,value,old_value;unsigned d,i;
    for(d=0;d<XSEQ_DEST_COUNT;d++)if(d!=XSEQ_DEST_NONE){
        CHECK(xenolith_sequence_value_bounds(d,&low,&high));
        for(i=0;i<=32;i++){
            float n=(float)i/32.0f;
            CHECK(xenolith_sequence_value_from_normalized(d,n,&physical));CHECK(physical>=low&&physical<=high);
            CHECK(xenolith_sequence_value_to_normalized(d,physical,&value));CHECK(fabsf(value-n)<.00001f);
        }
        value=123;CHECK(!xenolith_sequence_value_to_normalized(d,NAN,&value)&&value==123);
        physical=456;CHECK(!xenolith_sequence_value_from_normalized(d,INFINITY,&physical)&&physical==456);
    }
    value=123;CHECK(!xenolith_sequence_value_to_normalized(XSEQ_DEST_INDEX,120,&value)&&value==123);
    CHECK(xenolith_sequence_value_to_normalized(XSEQ_DEST_EXP_DEPTH,120,&value)&&value==1);
    old_value=value;CHECK(!xenolith_sequence_value_from_normalized(XSEQ_DEST_NONE,.5f,&value)&&value==old_value);++scenarios;
    xenolith_sequence_init(&s);image=s.image;old=legacy_image(&image,2);old.config.note_mode=XSEQ_CONTINUOUS;
    CHECK(xenolith_sequence_image_v2_valid(&old,sizeof old));CHECK(!xenolith_sequence_image_valid((const XenolithSequenceImage*)&old,sizeof old));
    CHECK(xenolith_sequence_image_from_v2(&upgraded,&old,sizeof old));CHECK(upgraded.version==XENOLITH_SEQUENCE_VERSION&&upgraded.config.note_mode==XSEQ_CONTINUOUS);
    CHECK(xenolith_sequence_load(&s,&old,sizeof old)&&s.image.version==XENOLITH_SEQUENCE_VERSION&&s.image.config.note_mode==XSEQ_CONTINUOUS);
    before=upgraded;old.pattern.lane[0].destination=XSEQ_DEST_EXP_DEPTH;
    CHECK(!xenolith_sequence_image_v2_valid(&old,sizeof old));CHECK(!xenolith_sequence_image_from_v2(&upgraded,&old,sizeof old));
    CHECK(!memcmp(&before,&upgraded,sizeof before));old.version=1;old.config.reserved=0;
    CHECK(!xenolith_sequence_image_v1_valid(&old,sizeof old));old.version=3;
    CHECK(xenolith_sequence_image_v3_valid(&old,sizeof old));CHECK(xenolith_sequence_image_from_v3(&upgraded,&old,sizeof old));CHECK(upgraded.version==XENOLITH_SEQUENCE_VERSION);
    old.pattern.lane[0].destination=XSEQ_DEST_FILTER_CUTOFF;CHECK(!xenolith_sequence_image_v3_valid(&old,sizeof old));
    old.version=4;CHECK(xenolith_sequence_image_v4_valid(&old,sizeof old));CHECK(xenolith_sequence_image_from_v4(&upgraded,&old,sizeof old));
    CHECK(xenolith_sequence_image_valid(&upgraded,sizeof upgraded));++scenarios;
}
static void atomic_keyboard_cv_tests(void) {
    XenolithSequence s,before;XenolithSequenceLane lane;float values[4]={.125f,.75f,0,0};
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_SIXTEENTH,4);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.destination=XSEQ_DEST_INDEX;CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    lane.destination=XSEQ_DEST_EXP_DEPTH;CHECK(xenolith_sequence_edit_lane(&s,1,&lane));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,3));
    CHECK(xenolith_sequence_capture_key_values(&s,60,1,3,values));CHECK(s.edit_step==1);
    CHECK(s.image.pattern.lane[0].value[0]==.125f&&s.image.pattern.lane[1].value[0]==.75f);
    CHECK(s.image.pattern.lane[0].step_mask==1&&s.image.pattern.lane[1].step_mask==1);
    CHECK(s.image.pattern.step[0].kind==XSEQ_REST);
    before=s;CHECK(!xenolith_sequence_capture_key_values(&s,60,1,3,values));CHECK(!memcmp(&before,&s,sizeof s));
    xenolith_sequence_capture_key_values(&s,60,0,0,0);values[0]=.25f;values[1]=1;
    CHECK(xenolith_sequence_capture_key_values(&s,60,1,3,values));CHECK(s.edit_step==2);
    CHECK(s.image.pattern.lane[0].value[1]==.25f&&s.image.pattern.lane[1].value[1]==1);
    CHECK(xenolith_sequence_capture_parameter(&s,0,.33f));CHECK(s.edit_step==2&&s.image.pattern.lane[0].value[2]==.33f);++scenarios;
    xenolith_sequence_stop(&s);CHECK(xenolith_sequence_select_step(&s,0));CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,1,3));
    CHECK(xenolith_sequence_capture_key_values(&s,62,1,3,values));CHECK(s.edit_step==1);
    CHECK(s.image.pattern.step[0].kind==XSEQ_NOTE&&s.image.pattern.step[0].note==62);
    CHECK(s.image.pattern.lane[0].value[0]==.25f&&s.image.pattern.lane[1].value[0]==1);++scenarios;
    xenolith_sequence_capture_key_values(&s,62,0,0,0);values[1]=NAN;before=s;
    CHECK(!xenolith_sequence_capture_key_values(&s,63,1,3,values));CHECK(!memcmp(&before,&s,sizeof s));
    CHECK(!xenolith_sequence_capture_key_values(&s,63,1,3,0));CHECK(!memcmp(&before,&s,sizeof s));
    CHECK(!xenolith_sequence_capture_key_values(&s,63,1,16,values));CHECK(!memcmp(&before,&s,sizeof s));++scenarios;
}
static void extended_pattern_tests(void) {
    XenolithSequence s;XenolithSequenceLane lane;const XenolithSequenceFrame *f;
    float value,values[4];uint32_t enabled;unsigned i,j;
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,64);
    CHECK(xenolith_sequence_lane_length(&s.image.config,0)==64);
    CHECK(xenolith_sequence_lane_length(&s.image.config,1)==16);
    for(j=0;j<4;j++){
        lane=s.image.pattern.lane[j];lane.enabled=1;lane.destination=j;
        CHECK(xenolith_sequence_edit_lane(&s,j,&lane));
        for(i=0;i<(j?16u:64u);i++)CHECK(xenolith_sequence_edit_lane_step(&s,j,i,(float)i/(j?15.0f:63.0f),1));
    }
    for(i=0;i<64;i++)step(&s,i,XSEQ_NOTE,i+32,50,i&7u);
    CHECK(xenolith_sequence_image_valid(&s.image,sizeof s.image));CHECK(xenolith_sequence_play_toggle(&s));
    for(i=0;i<130;i++){
        unsigned pos=i%64;f=next_step(&s);
        CHECK(f->playhead==pos&&f->note==pos+32&&f->note_on&&f->lane_mask==15u);
        CHECK(fabsf(f->lane[0]-(float)pos/63.0f)<.00001f);
        for(j=1;j<4;j++)CHECK(fabsf(f->lane[j]-(float)(pos%16)/15.0f)<.00001f);
    }
    CHECK(!xenolith_sequence_get_lane_step(&s.image.pattern,1,16,&value,&enabled));
    CHECK(!xenolith_sequence_edit_lane_step(&s,0,64,.5f,1));
    CHECK(!xenolith_sequence_edit_lane_step(&s,0,63,NAN,1));
    CHECK(!xenolith_sequence_edit_lane_step(&s,1,16,.5f,1));++scenarios;
    /* A64-step take fills CV1 once; short lanes retain the first16 captures. */
    xenolith_sequence_stop(&s);CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,3));
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_sample(&s)->record_state==XSEQ_REC_ACTIVE);
    for(i=0;i<64;i++){
        values[0]=(float)i/63.0f;values[1]=i<16?(float)i/15.0f:.99f;values[2]=values[3]=0;
        CHECK(xenolith_sequence_capture_key_values(&s,40+i,1,3,values));
        CHECK(xenolith_sequence_capture_key_values(&s,40+i,0,0,0));
        CHECK(s.image.pattern.step[i].note==40+i);
        f=next_step(&s);
    }
    CHECK(f->record_state==XSEQ_REC_IDLE&&f->playhead==0);
    for(i=0;i<64;i++){
        CHECK(xenolith_sequence_get_lane_step(&s.image.pattern,0,i,&value,&enabled));
        CHECK(enabled&&fabsf(value-(float)i/63.0f)<.00001f);
    }
    for(i=0;i<16;i++)CHECK(s.image.pattern.lane[1].value[i]==(float)i/15.0f);
    CHECK(s.image.pattern.cv0_extra_mask[0]==UINT32_MAX&&s.image.pattern.cv0_extra_mask[1]==65535u);++scenarios;
    /* A short CV-only take arms at its next local zero, not note step0. */
    while(s.playhead!=10u)next_step(&s);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,0,2));
    while(s.record_state!=XSEQ_REC_ACTIVE)next_step(&s);
    CHECK(s.playhead==16u&&s.record_elapsed==0u&&s.record_length==16u);
    for(i=0;i<16;i++){CHECK(xenolith_sequence_capture_parameter(&s,1,.25f));next_step(&s);}
    CHECK(s.playhead==32u&&s.record_state==XSEQ_REC_IDLE);
    for(i=0;i<16;i++)CHECK(s.image.pattern.lane[1].value[i]==.25f);++scenarios;
    /* Stopped STEP entry ignores QUANT and uses the selected lane capacity. */
    xenolith_sequence_stop(&s);
    { XenolithSequenceConfig c=s.image.config;c.quantize=XSEQ_QUANT_FOUR_STEPS;CHECK(xenolith_sequence_configure(&s,&c)); }
    CHECK(xenolith_sequence_select_step(&s,63));CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,2));
    CHECK(s.edit_step==15u);values[1]=.5f;
    CHECK(xenolith_sequence_capture_key_values(&s,60,1,2,values));CHECK(s.edit_step==0u);
    xenolith_sequence_capture_key_values(&s,60,0,0,0);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OFF,0,0));CHECK(xenolith_sequence_select_step(&s,63));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,1));values[0]=.75f;
    CHECK(xenolith_sequence_capture_key_values(&s,60,1,1,values));CHECK(s.edit_step==0u);
    CHECK(xenolith_sequence_get_lane_step(&s.image.pattern,0,63,&value,&enabled)&&enabled&&value==.75f);++scenarios;
}
static void quantization_tests(void) {
    XenolithSequence s,reference;XenolithSequenceConfig c;const XenolithSequenceFrame *f;
    uint32_t q,i,inc;float value;uint32_t enabled;
    /* QUANT changes event placement only: transport events remain sample-identical. */
    xenolith_sequence_init(&s);enable(&s,137.5f,XSEQ_SIXTEENTH,64);reference=s;
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_play_toggle(&reference));inc=s.phase_increment;
    for(q=0;q<3;q++){
        c=s.image.config;c.quantize=q;CHECK(xenolith_sequence_configure(&s,&c));CHECK(s.phase_increment==inc);
        for(i=0;i<100000;i++){
            const XenolithSequenceFrame *a=xenolith_sequence_sample(&s),*b=xenolith_sequence_sample(&reference);
            CHECK(a->playhead==b->playhead&&a->step_event==b->step_event&&s.phase==reference.phase);
        }
    }++scenarios;
    /* A late NOTE rounds forward. Early release must not damage the old slot. */
    xenolith_sequence_init(&s);enable(&s,300,XSEQ_SIXTEENTH,8);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,1));CHECK(xenolith_sequence_play_toggle(&s));
    xenolith_sequence_sample(&s);while(s.phase<UINT32_C(0x90000000))xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_capture_key(&s,60,1));CHECK(s.image.pattern.step[0].kind==XSEQ_REST);
    CHECK(s.image.pattern.step[1].kind==XSEQ_NOTE&&s.capture_future);
    CHECK(xenolith_sequence_capture_parameter(&s,0,.75f));
    CHECK(xenolith_sequence_capture_key(&s,60,0));CHECK(s.image.pattern.step[0].gate_percent==50);
    CHECK(s.image.pattern.step[1].gate_percent==1);f=next_step(&s);CHECK(f->playhead==1&&f->note_on&&f->note==60);
    CHECK(xenolith_sequence_get_lane_step(&s.image.pattern,0,1,&value,&enabled)&&enabled&&value==.75f);++scenarios;
    /* A held forward-quantized onset remains NOTE, then extends through TIE. */
    xenolith_sequence_stop(&s);CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,0));
    CHECK(xenolith_sequence_play_toggle(&s));xenolith_sequence_sample(&s);
    while(s.phase<UINT32_C(0x90000000))xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_capture_key(&s,67,1));f=next_step(&s);
    CHECK(f->playhead==1&&f->note_on&&s.image.pattern.step[1].kind==XSEQ_NOTE&&!s.capture_future);
    f=next_step(&s);CHECK(f->playhead==2&&s.image.pattern.step[2].kind==XSEQ_TIE);
    advance(&s,500);CHECK(xenolith_sequence_capture_key(&s,67,0));CHECK(s.image.pattern.step[2].gate_percent>=22&&s.image.pattern.step[2].gate_percent<=24);++scenarios;
    /* Coarse grids round in absolute pattern coordinates, with boundary clamp. */
    xenolith_sequence_stop(&s);c=s.image.config;c.quantize=XSEQ_QUANT_FOUR_STEPS;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,1));CHECK(xenolith_sequence_play_toggle(&s));
    xenolith_sequence_sample(&s);next_step(&s);
    CHECK(xenolith_sequence_capture_key(&s,55,1));CHECK(s.image.pattern.step[0].kind==XSEQ_NOTE&&s.image.pattern.step[1].kind==XSEQ_TIE);
    CHECK(xenolith_sequence_capture_key(&s,55,0));next_step(&s);
    CHECK(xenolith_sequence_capture_key(&s,57,1));CHECK(s.image.pattern.step[4].kind==XSEQ_NOTE&&s.capture_future);
    CHECK(xenolith_sequence_capture_key(&s,57,0));while(s.playhead!=7u)next_step(&s);
    while(s.phase<UINT32_C(0x90000000))xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_capture_key(&s,59,1));CHECK(s.image.pattern.step[4].note==59&&s.image.pattern.step[0].note==55);
    CHECK(xenolith_sequence_capture_parameter(&s,0,.625f));CHECK(s.image.pattern.lane[0].value[4]==.625f);
    f=next_step(&s);CHECK(f->record_state==XSEQ_REC_IDLE&&s.image.pattern.step[0].note==55);++scenarios;
    /* Changing the loop length retires an in-progress take without losing it. */
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,1,0));
    while(s.record_state!=XSEQ_REC_ACTIVE)next_step(&s);
    c=s.image.config;c.length=4;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(s.record_state==XSEQ_REC_IDLE&&s.image.pattern.step[4].note==59&&s.running);++scenarios;
}
static void subdivision_tests(void) {
    XenolithSequence s,reference;XenolithSequenceConfig c;XenolithSequenceLane lane;
    const XenolithSequenceFrame *f;unsigned i,j,changes[4]={0};float last[4]={-1,-1,-1,-1};
    uint32_t start,enabled;float value;
    xenolith_sequence_init(&s);
    CHECK(s.image.config.length==64u&&s.image.config.division==XSEQ_SIXTYFOURTH&&s.image.config.quantize==XSEQ_QUANT_STEP);
    CHECK(xenolith_sequence_steps_per_beat(XSEQ_SIXTYFOURTH)==4u&&xenolith_sequence_steps_per_beat(5u)==0u);
    enable(&s,120,XSEQ_SIXTYFOURTH,64);
    for(j=0;j<4;j++){
        lane=s.image.pattern.lane[j];lane.enabled=1;lane.destination=j;
        CHECK(xenolith_sequence_edit_lane(&s,j,&lane));
        for(i=0;i<(j?16u:64u);i++)CHECK(xenolith_sequence_edit_lane_step(&s,j,i,(float)i/(j?15.0f:63.0f),1));
    }
    for(i=0;i<64;i++)step(&s,i,XSEQ_NOTE,32+i,50,i==16?XSEQ_TRIGGER_A:0);
    CHECK(xenolith_sequence_play_toggle(&s));
    f=next_step(&s);start=f->sample;
    for(i=0;i<64;i++){
        if(i)f=next_step(&s);
        CHECK(f->playhead==i&&f->note==32u+i&&f->note_on&&f->lane_mask==15u);
        CHECK(f->triggers==(i==16u?XSEQ_TRIGGER_A:0u));
        for(j=0;j<4;j++){
            uint32_t pos=j?i/4u:i;
            CHECK(xenolith_sequence_lane_position(&s.image.config,j,i)==pos);
            CHECK(fabsf(f->lane[j]-(float)pos/(j?15.0f:63.0f))<.00001f);
            if(last[j]!=f->lane[j])changes[j]++;last[j]=f->lane[j];
        }
    }
    f=next_step(&s);CHECK(f->playhead==0u&&f->sample-start>=352798u&&f->sample-start<=352802u);
    CHECK(changes[0]==64u&&changes[1]==16u&&changes[2]==16u&&changes[3]==16u);++scenarios;
    /* QUANT moves notes/CV1; the underlying clock and stored image never move. */
    xenolith_sequence_stop(&s);reference=s;
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(xenolith_sequence_play_toggle(&reference));
    for(j=0;j<3;j++){
        c=s.image.config;c.quantize=j;CHECK(xenolith_sequence_configure(&s,&c));
        for(i=0;i<44100u;i++){
            const XenolithSequenceFrame *a=xenolith_sequence_sample(&s),*b=xenolith_sequence_sample(&reference);
            CHECK(a->playhead==b->playhead&&a->step_event==b->step_event&&s.phase==reference.phase);
            CHECK(!memcmp(&s.image.pattern,&reference.image.pattern,sizeof s.image.pattern));
        }
    }++scenarios;
    /* A short CV-only running take spans the complete64-note loop once. */
    xenolith_sequence_stop(&s);c=s.image.config;c.quantize=0;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,0,2));CHECK(s.record_length==64u);
    CHECK(xenolith_sequence_play_toggle(&s));next_step(&s);CHECK(s.record_state==XSEQ_REC_ACTIVE);
    for(i=0;i<64;i++){
        CHECK(s.record_state==XSEQ_REC_ACTIVE);
        if(!(i&3u))CHECK(xenolith_sequence_capture_parameter(&s,1,(float)(i/4u)/15.0f));
        next_step(&s);
    }
    CHECK(s.record_state==XSEQ_REC_IDLE&&s.playhead==0u&&s.image.pattern.lane[1].step_mask==65535u);
    for(i=0;i<16;i++)CHECK(s.image.pattern.lane[1].value[i]==(float)i/15.0f);++scenarios;
    /* Arming during a loop starts at the shared note/CV zero. */
    while(s.playhead!=10u)next_step(&s);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,0,2));
    while(s.record_state!=XSEQ_REC_ACTIVE){next_step(&s);CHECK(s.record_state!=XSEQ_REC_ACTIVE||s.playhead==0u);}
    CHECK(s.record_elapsed==0u&&s.record_length==64u);++scenarios;
    /* Stopped short-CV entry still advances one of16 values per key. */
    xenolith_sequence_stop(&s);CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,2));
    for(i=0;i<16;i++){
        float values[4]={0,(float)i/15.0f,0,0};
        CHECK(xenolith_sequence_capture_key_values(&s,60,1,2,values));
        xenolith_sequence_capture_key_values(&s,60,0,0,0);CHECK(s.edit_step==(i+1u)%16u);
    }
    for(i=0;i<16;i++)CHECK(xenolith_sequence_get_lane_step(&s.image.pattern,1,i,&value,&enabled)&&enabled&&value==(float)i/15.0f);++scenarios;
    /* Last partial group is valid when a loop is shortened. */
    xenolith_sequence_stop(&s);c=s.image.config;c.length=10;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_cv_length(&c)==3u&&xenolith_sequence_lane_position(&c,1,9u)==2u);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,0,2));
    CHECK(s.edit_step<3u);++scenarios;
}
static void first_key_take_tests(void) {
    XenolithSequence s,before;XenolithSequenceConfig c;XenolithSequenceLane lane;
    const XenolithSequenceFrame *f;uint32_t start,i;float values[4]={.375f,0,0,0};
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_SIXTYFOURTH,64);
    step(&s,0,XSEQ_NOTE,99,50,0);
    lane=s.image.pattern.lane[0];lane.destination=XSEQ_DEST_INDEX;lane.enabled=1;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,1));
    advance(&s,44100);CHECK(!s.running&&s.record_state==XSEQ_REC_ARMED&&s.image.pattern.step[0].note==99);
    before=s;values[0]=NAN;CHECK(!xenolith_sequence_capture_key_values(&s,60,1,1,values));
    CHECK(!memcmp(&before,&s,sizeof s));values[0]=.375f;
    CHECK(xenolith_sequence_capture_key_values(&s,60,1,1,values));
    CHECK(s.running&&s.record_state==XSEQ_REC_ACTIVE&&s.record_elapsed==0u&&s.playhead==0u);
    f=xenolith_sequence_sample(&s);start=f->sample;
    CHECK(f->note_on&&f->gate&&f->note==60u&&s.record_elapsed==0u&&f->lane[0]==.375f);
    /* A physically held note must not release at99% before the next tie exists. */
    for(i=0;i<12000u;i++){
        f=xenolith_sequence_sample(&s);CHECK(f->gate&&!f->note_off&&!f->note_on);
    }
    CHECK(s.playhead==2u&&s.image.pattern.step[1].kind==XSEQ_TIE&&s.image.pattern.step[2].kind==XSEQ_TIE);
    CHECK(xenolith_sequence_capture_key(&s,60,0));
    for(i=0;i<40u;i++){f=xenolith_sequence_sample(&s);if(f->note_off)break;}CHECK(i<40u&&!f->gate);
    while(s.playhead<5u)next_step(&s);
    while(s.phase<UINT32_C(0xc0000000))xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_capture_key(&s,64,1));CHECK(s.capture_future&&s.image.pattern.step[6].note==64u);
    CHECK(xenolith_sequence_capture_key(&s,64,0));f=next_step(&s);CHECK(f->playhead==6u&&f->note_on&&f->note==64u);
    while(s.record_state!=XSEQ_REC_IDLE)xenolith_sequence_sample(&s);
    CHECK(s.playhead==0u&&s.running&&s.sample_count-start>=352799u&&s.sample_count-start<=352803u);
    CHECK(s.image.pattern.step[0].note==60u&&s.image.pattern.step[6].note==64u);++scenarios;
    /* The same first-key entry starts a full-loop CV-only take. */
    xenolith_sequence_stop(&s);lane=s.image.pattern.lane[1];lane.destination=XSEQ_DEST_RATIO;lane.enabled=1;
    CHECK(xenolith_sequence_edit_lane(&s,1,&lane));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,0,2));values[1]=.625f;
    CHECK(xenolith_sequence_capture_key_values(&s,62,1,2,values));
    CHECK(s.running&&s.record_length==64u&&s.image.pattern.lane[1].step_mask==1u&&s.image.pattern.lane[1].value[0]==.625f);
    xenolith_sequence_sample(&s);CHECK(s.record_elapsed==0u);
    xenolith_sequence_capture_key(&s,62,0);xenolith_sequence_stop(&s);++scenarios;
    /* Explicit stopped STEP entry remains available and never starts a clock. */
    c=s.image.config;c.quantize=2u;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_STEP,1,0));
    CHECK(xenolith_sequence_capture_key(&s,65,1));CHECK(!s.running&&s.record_state==XSEQ_REC_ARMED&&s.edit_step==1u);++scenarios;
}
static void playback_quantization_tests(void) {
    XenolithSequence s;XenolithSequenceConfig c;XenolithSequencePattern raw;XenolithSequenceLane lane;
    const XenolithSequenceFrame *f;uint32_t q,i,events,start;
    const uint32_t positions[4]={0u,3u,7u,12u},snapped[4]={0u,4u,8u,12u};
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_SIXTYFOURTH,64);
    lane=s.image.pattern.lane[0];lane.enabled=1u;lane.destination=XSEQ_DEST_INDEX;
    CHECK(xenolith_sequence_edit_lane(&s,0u,&lane));
    for(i=0;i<4u;i++){
        step(&s,positions[i],XSEQ_NOTE,60u+i,50u,0u);
        CHECK(xenolith_sequence_edit_lane_step(&s,0u,positions[i],(float)(i+1u)*.2f,1u));
    }
    raw=s.image.pattern;
    for(q=0u;q<4u;q++){
        xenolith_sequence_stop(&s);c=s.image.config;c.quantize=q==3u?0u:q;
        CHECK(xenolith_sequence_configure(&s,&c));CHECK(xenolith_sequence_play_toggle(&s));events=0u;
        f=next_step(&s);start=f->sample;
        for(i=0u;i<64u;i++){
            if(i)f=next_step(&s);
            if(f->note_on){
                CHECK(events<4u&&f->note==60u+events);
                CHECK(f->playhead==(c.quantize?snapped[events]:positions[events]));
                CHECK(fabsf(f->lane[0]-(float)(events+1u)*.2f)<.00001f);events++;
            }
        }
        CHECK(events==4u);f=next_step(&s);
        CHECK(f->sample-start>=352798u&&f->sample-start<=352802u);
        CHECK(!memcmp(&raw,&s.image.pattern,sizeof raw));
    }++scenarios;
    /* Latest raw onset wins a grid collision; late notes clamp to the last grid. */
    xenolith_sequence_stop(&s);step(&s,1u,XSEQ_NOTE,80u,50u,1u);step(&s,63u,XSEQ_NOTE,90u,50u,2u);
    c=s.image.config;c.quantize=2u;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_play_toggle(&s));f=next_step(&s);CHECK(f->note==80u&&f->note_on&&f->triggers==1u);
    while(s.playhead!=60u)f=next_step(&s);
    CHECK(f->note==90u&&f->note_on&&f->triggers==2u);++scenarios;
    /* A moved NOTE preserves its raw tied duration and final release fraction. */
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_SIXTYFOURTH,64);
    step(&s,3u,XSEQ_NOTE,67u,25u,0u);step(&s,4u,XSEQ_TIE,0u,50u,0u);
    c=s.image.config;c.quantize=2u;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_play_toggle(&s));while(s.playhead!=4u)f=next_step(&s);
    CHECK(f->note_on&&f->gate&&f->note==67u);start=f->sample;
    do{f=xenolith_sequence_sample(&s);CHECK(f->sample-start<9000u);}while(!f->note_off);
    CHECK(s.playhead==5u&&f->sample-start>=8267u&&f->sample-start<=8270u);++scenarios;
    /* CONTINUOUS joins adjacent moved notes and still releases at a real rest. */
    xenolith_sequence_init(&s);enable(&s,120,XSEQ_SIXTYFOURTH,64);
    for(i=0u;i<8u;i++)step(&s,i,XSEQ_NOTE,60u+i,1u,0u);
    c=s.image.config;c.quantize=2u;c.note_mode=XSEQ_CONTINUOUS;CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_play_toggle(&s));f=next_step(&s);CHECK(f->envelope_on&&f->gate);
    do{f=xenolith_sequence_sample(&s);CHECK(!f->note_off);if(f->note_on)CHECK(!f->envelope_on);}while(s.playhead<8u);
    do{f=xenolith_sequence_sample(&s);CHECK(s.playhead<12u);}while(!f->note_off);
    CHECK(!f->gate);++scenarios;
}
int main(void) {
    clock_tests();gate_tests();lane_tests();record_tests();validation_tests();amplitude_tests();ans_bits_destination_tests();
    note_mode_tests();envelope_mode_tests();
    physical_values_and_v3_tests();atomic_keyboard_cv_tests();
    extended_pattern_tests();quantization_tests();subdivision_tests();first_key_take_tests();playback_quantization_tests();
    printf("{\"passed\":true,\"scenarios\":%u,\"assertions\":%u,\"state_bytes\":%u,\"image_bytes\":%u}\n",
           scenarios,assertions,(unsigned)sizeof(XenolithSequence),(unsigned)sizeof(XenolithSequenceImage));return 0;
}
