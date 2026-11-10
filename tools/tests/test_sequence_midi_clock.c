/* SPDX-License-Identifier: MPL-2.0
 * External clock invariants, independent of MIDI byte parsing and hardware. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xenolith_sequence.h"
static unsigned assertions,scenarios;
#define CHECK(x) do { ++assertions; if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);} }while(0)
static void init(XenolithSequence *s,unsigned division) {
    XenolithSequenceConfig c;unsigned i;
    xenolith_sequence_init(s);c=s->image.config;c.enabled=1;c.division=division;
    CHECK(xenolith_sequence_configure(s,&c));
    for(i=0;i<64;i++){
        XenolithSequenceStep p={(uint8_t)(48+i),XSEQ_NOTE,50,0};
        CHECK(xenolith_sequence_edit_step(s,i,&p));
    }
}
static const XenolithSequenceFrame *advance(XenolithSequence *s,unsigned count) {
    const XenolithSequenceFrame *f=0;
    while(count--)f=xenolith_sequence_sample(s);
    return f;
}
static const XenolithSequenceFrame *tick(XenolithSequence *s,uint32_t ms) {
    CHECK(xenolith_sequence_midi_realtime(s,0xf8u,ms));
    return xenolith_sequence_sample(s);
}
static void acquisition_transport(void) {
    XenolithSequence s,before;XenolithSequenceConfig c;const XenolithSequenceFrame *f;unsigned i,position;
    xenolith_sequence_init(&s);before=s;
    CHECK(!xenolith_sequence_midi_realtime(&s,0xfau,0));CHECK(!memcmp(&before,&s,sizeof s));
    init(&s,XSEQ_SIXTYFOURTH);c=s.image.config;c.note_mode=XSEQ_CONTINUOUS;
    CHECK(xenolith_sequence_configure(&s,&c));before=s;
    CHECK(!xenolith_sequence_midi_realtime(&s,0xf8u,0));
    CHECK(!xenolith_sequence_midi_realtime(&s,0xfcu,0));
    CHECK(!xenolith_sequence_midi_realtime(&s,0xfeu,0));CHECK(!memcmp(&before,&s,sizeof s));
    CHECK(xenolith_sequence_midi_realtime(&s,0xfau,10));
    f=advance(&s,100);CHECK(s.running&&s.external_clock&&!f->gate&&!f->step_event);
    f=tick(&s,20);CHECK(f->step_event&&f->note_on&&f->note==48&&f->playhead==0);
    for(i=1;i<18;i++)tick(&s,20+20*i);
    CHECK(s.playhead==2);position=s.playhead;
    CHECK(xenolith_sequence_midi_realtime(&s,0xfbu,400));CHECK(!s.external_wait&&s.playhead==position);
    CHECK(xenolith_sequence_midi_realtime(&s,0xfcu,410));
    f=xenolith_sequence_sample(&s);CHECK(!f->running&&!f->gate&&f->note_off&&s.external_clock);
    before=s;CHECK(!xenolith_sequence_midi_realtime(&s,0xf8u,420));CHECK(!memcmp(&before,&s,sizeof s));
    CHECK(xenolith_sequence_midi_realtime(&s,0xfbu,430));
    f=tick(&s,440);CHECK(f->playhead==position&&f->note_on);
    f=tick(&s,460);CHECK(f->playhead==position+1&&f->step_event);
    CHECK(xenolith_sequence_midi_realtime(&s,0xfcu,470));xenolith_sequence_sample(&s);
    CHECK(xenolith_sequence_play_toggle(&s));f=xenolith_sequence_sample(&s);
    CHECK(!s.external_clock&&s.running&&f->note_on&&f->playhead==0);
    before=s;CHECK(!xenolith_sequence_midi_realtime(&s,0xfcu,480));CHECK(!memcmp(&before,&s,sizeof s));
    CHECK(xenolith_sequence_midi_realtime(&s,0xfau,500));f=tick(&s,520);
    CHECK(f->playhead==0&&f->step_event);++scenarios;
}
static void pulses_and_quantization(void) {
    unsigned division,q,i;
    for(division=0;division<=XSEQ_SIXTYFOURTH;division++)for(q=0;q<3;q++){
        XenolithSequence s;XenolithSequenceConfig c;XenolithSequencePattern saved;
        const XenolithSequenceFrame *f;unsigned ppstep,events=0;
        init(&s,division);c=s.image.config;c.quantize=q;CHECK(xenolith_sequence_configure(&s,&c));
        saved=s.image.pattern;ppstep=24/xenolith_sequence_steps_per_beat(division);
        CHECK(xenolith_sequence_midi_realtime(&s,0xfau,0));
        for(i=0;i<=384;i++){
            /* Alternate host periods without changing the saved internal BPM. */
            f=tick(&s,i*20u);events+=f->step_event;
            CHECK(f->playhead==(i/ppstep)%64u);
            CHECK(f->step_event==(i%ppstep==0u));
            if(i<384){f=advance(&s,881);CHECK(!f->step_event&&s.running);}
        }
        CHECK(events==1+384/ppstep&&s.image.config.bpm==120);
        CHECK(!memcmp(&saved,&s.image.pattern,sizeof saved));++scenarios;
    }
    /* One grouped sixteen-beat loop is384 input clock intervals, for any BPM. */
    for(i=0;i<3;i++){
        static const unsigned period_ms[3]={10,25,50};
        XenolithSequence s;unsigned j,ms=period_ms[i];
        init(&s,XSEQ_SIXTYFOURTH);CHECK(xenolith_sequence_midi_realtime(&s,0xfau,0));
        for(j=0;j<=384;j++){
            const XenolithSequenceFrame *f=tick(&s,j*ms);
            CHECK(f->playhead==(j/6)%64);
            if(j<384)advance(&s,ms*441/10-1);
        }
        CHECK(s.running&&s.playhead==0);++scenarios;
    }
}
static void clock_loss_and_bursts(void) {
    XenolithSequence s;XenolithSequenceConfig c;const XenolithSequenceFrame *f;unsigned i,off=0;
    init(&s,XSEQ_SIXTYFOURTH);c=s.image.config;c.note_mode=XSEQ_CONTINUOUS;
    CHECK(xenolith_sequence_configure(&s,&c));
    CHECK(xenolith_sequence_midi_realtime(&s,0xfau,UINT32_MAX-50));
    tick(&s,UINT32_MAX-20);advance(&s,881);tick(&s,0);
    CHECK(s.external_period>800&&s.external_period<1000);
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_OVERDUB,1,0));
    for(i=0;i<88201;i++){f=xenolith_sequence_sample(&s);off+=f->note_off;}
    CHECK(!s.running&&!s.gate&&off==1&&s.record_state==XSEQ_REC_IDLE);
    CHECK(!xenolith_sequence_midi_realtime(&s,0xf8u,3000));
    CHECK(xenolith_sequence_play_toggle(&s));CHECK(!s.external_clock&&s.running);++scenarios;
    /* Batched input clocks preserve every crossed step without an unbounded loop. */
    init(&s,XSEQ_SIXTYFOURTH);CHECK(xenolith_sequence_midi_realtime(&s,0xfau,0));
    for(i=0;i<=24;i++)CHECK(xenolith_sequence_midi_realtime(&s,0xf8u,100));
    CHECK(s.external_steps==4);f=xenolith_sequence_sample(&s);CHECK(f->playhead==0&&f->step_event);
    for(i=1;i<=4;i++){f=xenolith_sequence_sample(&s);CHECK(f->playhead==i&&f->step_event);}
    CHECK(!s.external_steps);++scenarios;
    /* Waiting for the very first clock also has bounded lifetime. */
    init(&s,XSEQ_SIXTYFOURTH);CHECK(xenolith_sequence_midi_realtime(&s,0xfau,0));
    advance(&s,88201);CHECK(!s.running&&!s.gate);++scenarios;
}
static void record_and_saved_image(void) {
    XenolithSequence s,restored;XenolithSequenceImage image;XenolithSequenceLane lane;
    unsigned i;float value[4]={.25f,.75f,0,0};
    init(&s,XSEQ_SIXTYFOURTH);
    lane=s.image.pattern.lane[0];lane.enabled=1;lane.destination=XSEQ_DEST_INDEX;
    CHECK(xenolith_sequence_edit_lane(&s,0,&lane));
    lane=s.image.pattern.lane[1];lane.enabled=1;lane.destination=XSEQ_DEST_RATIO;
    CHECK(xenolith_sequence_edit_lane(&s,1,&lane));
    CHECK(xenolith_sequence_record_arm(&s,XSEQ_RECORD_REPLACE,1,3));
    CHECK(xenolith_sequence_midi_realtime(&s,0xfau,0));tick(&s,0);
    CHECK(s.record_state==XSEQ_REC_ACTIVE);
    CHECK(xenolith_sequence_capture_key_values(&s,67,1,3,value));
    CHECK(xenolith_sequence_capture_key(&s,67,0));
    for(i=1;i<=384;i++){advance(&s,881);tick(&s,i*20);}
    CHECK(s.record_state==XSEQ_REC_IDLE&&s.playhead==0&&s.running);
    CHECK(s.image.pattern.step[0].note==67&&s.image.pattern.lane[0].value[0]==.25f&&s.image.pattern.lane[1].value[0]==.75f);
    CHECK(xenolith_sequence_snapshot(&s,&image));xenolith_sequence_init(&restored);
    CHECK(xenolith_sequence_load(&restored,&image,sizeof image));
    CHECK(!restored.external_clock&&!restored.running&&!memcmp(&restored.image,&image,sizeof image));++scenarios;
}
int main(void) {
    acquisition_transport();pulses_and_quantization();clock_loss_and_bursts();record_and_saved_image();
    printf("{\"passed\":true,\"scenarios\":%u,\"assertions\":%u,\"state_bytes\":%u,\"image_bytes\":%u}\n",
           scenarios,assertions,(unsigned)sizeof(XenolithSequence),(unsigned)sizeof(XenolithSequenceImage));
    return 0;
}
