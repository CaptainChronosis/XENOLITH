/* SPDX-License-Identifier: MPL-2.0
 * XX29 foreground64-note/CV1 and16-step CV2..4 editor. Include after xenolith_mod_ui.inc.
 * Commands cross the parent's owned queue. No direct audio-state writes.
 */
#include "xenolith_sequence.h"
#include "xenolith_controls.h"
#include "xenolith_ratio.h"

enum { SEQ_UI_OK,SEQ_UI_FULL,SEQ_UI_BUSY,SEQ_UI_RECORD_LOCK,SEQ_UI_INVALID,SEQ_UI_FILLED,SEQ_UI_RANGE,SEQ_UI_TARGET,SEQ_UI_EXP_RANGE };
static uint32_t seq_ui_object,seq_ui_page,seq_ui_cursor,seq_ui_cursor_ready;
static uint32_t seq_ui_cursor_pending,seq_ui_cursor_serial,seq_ui_status;
static const char *const SEQ_UI_DESTINATIONS[XSEQ_DEST_COUNT]={
    "TONIC","INDEX","RATIO","DRIFT","WET MIX","CORE RATE","WET RATE","NONE","AMPLITUDE","ANS CELL","BITS","EXP DEPTH","FILTER HZ","FILTER RES"
};
static const char *const SEQ_UI_QUANT[5][3]={{"1/4","1/2","1"},{"1/8","1/4","1/2"},{"1/16","1/8","1/4"},{"1/8T","1/4T","1/2T"},{"1/4 BEAT","1/2 BEAT","1 BEAT"}};
static const char *const SEQ_UI_TYPES[3]={"REST","NOTE","TIE"};
static const char *const SEQ_UI_RECORDS[4]={"OFF","STEP","REPLACE","OVERDUB"};

/* Audible, readable time detents: no repeated millisecond-rounded 1 ms
 * values across dozens of turns. Stored asymmetric times remain untouched
 * until this control is edited; the first edit starts from their maximum. */
static const float SEQ_UI_SLEW_TIMES[]={0,.001f,.002f,.005f,.01f,.02f,.05f,.1f,.2f,.5f,1,2,5,10,20,30,60};
static float seq_ui_slew_step(float value,int32_t step)
{
    uint32_t i,count=(uint32_t)(sizeof SEQ_UI_SLEW_TIMES/sizeof *SEQ_UI_SLEW_TIMES);
    int32_t delta=mod_ui_delta(step);
    if(!delta)return value;
    if(delta>0){
        for(i=0u;i+1u<count&&SEQ_UI_SLEW_TIMES[i]<=value;i++);
        return SEQ_UI_SLEW_TIMES[mod_ui_enum(i,delta-1,count)];
    }
    i=count-1u;while(i&&SEQ_UI_SLEW_TIMES[i]>=value)--i;
    return SEQ_UI_SLEW_TIMES[mod_ui_enum(i,delta+1,count)];
}
static void seq_ui_append(char *out,const char *suffix)
{
    uint32_t i=0u,n=0u;while(out[n]&&n<23u)++n;
    while(suffix[i]&&n<23u)out[n++]=suffix[i++];out[n]=0;
}
static void seq_ui_slew_text(const XenolithSequenceLane *lane,char *out)
{
    float value=lane->rise_seconds;float unit;uint32_t integer;
    if(value!=lane->fall_seconds){mod_ui_copy(out,"ASYM");return;}
    if(!(value>0)){mod_ui_copy(out,"OFF");return;}
    unit=value<1.0f?value*1000.0f:value;integer=(uint32_t)(unit+.5f);
    if(fm_fabsf(unit-(float)integer)<.0001f)number_text((int32_t)integer,out);
    else mod_ui_number(unit,out);
    seq_ui_append(out,value<1.0f?" ms":" s");
}
static float seq_ui_lane_value(uint32_t d,float normalized,int32_t step)
{
    float v,low,high;
    if(xenolith_sequence_value_from_normalized(d,normalized,&v)
       &&xenolith_sequence_value_bounds(d,&low,&high)){
        if(d==XSEQ_DEST_INDEX)v=xenolith_index_step(v,step);
        else if(d==XSEQ_DEST_EXP_DEPTH)v=xenolith_exp_depth_step(v,step);
        else if(d==XSEQ_DEST_RATIO)v=xenolith_ratio_step(v,step);
        else if(d==XSEQ_DEST_PITCH||d==XSEQ_DEST_CORE_RATE||d==XSEQ_DEST_WET_RATE||d==XSEQ_DEST_FILTER_CUTOFF)
            v=mod_ui_bound(v*fm_exp2f((float)mod_ui_delta(step)/(d==XSEQ_DEST_PITCH?48.0f:96.0f)),low,high);
        else if(d==XSEQ_DEST_BITS||d==XSEQ_DEST_ANS_CELL)v=mod_ui_linear((float)(uint32_t)(v+.5f),step,1.0f,low,high);
        else v=mod_ui_linear(v,step,.01f,low,high);
        xenolith_sequence_value_to_normalized(d,v,&normalized);
    }else normalized=mod_ui_linear(normalized,step,.01f,0.0f,1.0f);
    return normalized;
}
static void seq_ui_value_text(uint32_t destination,float normalized,const char **label,char *text)
{
    float value;
    static const char *const units[XSEQ_DEST_COUNT]={"TONIC HZ","INDEX","RATIO","DRIFT %","WET MIX %","CORE KHZ","WET KHZ","VALUE 0..1","AMPLITUDE %","ANS KEY","BITS","EXP DEPTH ST","FILTER KHZ","FILTER RES %"};
    *label=units[destination];
    if(!xenolith_sequence_value_from_normalized(destination,normalized,&value))value=normalized;
    if(destination==XSEQ_DEST_CORE_RATE||destination==XSEQ_DEST_WET_RATE||destination==XSEQ_DEST_FILTER_CUTOFF)value*=.001f;
    else if(destination==XSEQ_DEST_DRIFT||destination==XSEQ_DEST_WET_MIX||destination==XSEQ_DEST_AMPLITUDE||destination==XSEQ_DEST_FILTER_RESONANCE)value*=100.0f;
    else if(destination==XSEQ_DEST_ANS_CELL)value=(float)(uint32_t)(value+.5f)+1.0f;
    mod_ui_number(value,text);
}

static uint32_t seq_ui_length(void) {
    return seq_ui_object>=2u?xenolith_sequence_lane_length(&seq_edit.config,seq_ui_object-2u):seq_edit.config.length;
}
static void seq_ui_lane_set(XenolithSequencePattern *p,uint32_t lane,uint32_t step,float value,uint32_t enabled) {
    if(step<16u){p->lane[lane].value[step]=value;
        if(enabled)p->lane[lane].step_mask|=1u<<step;else p->lane[lane].step_mask&=~(1u<<step);
    }else if(!lane&&step<64u){uint32_t i=step-16u,mask=1u<<(i&31u);p->cv0_extra[i]=value;
        if(enabled)p->cv0_extra_mask[i>>5]|=mask;else p->cv0_extra_mask[i>>5]&=~mask;
    }
}
/* Queue depth is bounded64, so signed modular comparison safely recognizes
 * this cursor command's acknowledgement across unsigned counter wrap. */
static void seq_ui_sync_cursor(void)
{
    uint32_t length=seq_ui_length(),actual=seq_cursor_snapshot;
    if(length<1u||length>64u)length=16u;
    if(seq_ui_object>=2u&&actual>=length)actual%=length;
    if(!seq_ui_cursor_ready){seq_ui_cursor_ready=1u;seq_ui_cursor=actual;}
    if(seq_ui_cursor_pending&&(int32_t)(seq_read-seq_ui_cursor_serial)<0){
        if(seq_ui_cursor>=length)seq_ui_cursor=length-1u;return;
    }
    seq_ui_cursor_pending=0u;seq_ui_cursor=actual<length?actual:length-1u;
}
static void seq_ui_move_cursor(int32_t step)
{
    SeqCommand c={0};uint32_t next=mod_ui_enum(seq_ui_cursor,step,seq_ui_length());
    if(next==seq_ui_cursor)return;
    c.type=SQ_CURSOR;c.a=next;
    if(!seq_send(&c)){seq_ui_status=SEQ_UI_FULL;return;}
    seq_ui_cursor=next;seq_ui_cursor_pending=1u;seq_ui_cursor_serial=seq_write;seq_ui_status=SEQ_UI_OK;
}
/* Validate the full proposed image before queueing, and commit the foreground
 * shadow only after acceptance. Recording owns step/lane edits exclusively. */
static int seq_ui_propose(const SeqCommand *command)
{
    XenolithSequenceImage trial=seq_edit;uint32_t i;
    if(command->type==SQ_CONFIG)trial.config=command->data.config;
    else{
        if(seq_recording!=XSEQ_REC_IDLE&&!((command->type==SQ_STEP||command->type==SQ_LANE||command->type==SQ_LANE_STEP||command->type==SQ_LANE_FILL)
            &&seq_record_mode==XSEQ_RECORD_STEP&&!seq_running)){
            seq_ui_status=SEQ_UI_RECORD_LOCK;return 0;
        }
        if(command->type==SQ_STEP&&command->index<XENOLITH_SEQUENCE_STEPS)trial.pattern.step[command->index]=command->data.step;
        else if(command->type==SQ_LANE_STEP&&command->index<4u&&command->a<xenolith_sequence_lane_length(&trial.config,command->index))
            seq_ui_lane_set(&trial.pattern,command->index,command->a,command->data.value,command->b);
        else if(command->type==SQ_LANE_FILL&&command->index<4u){
            uint32_t count=xenolith_sequence_lane_length(&trial.config,command->index);
            for(i=0;i<count;i++){float value;uint32_t enabled;
                xenolith_sequence_get_lane_step(&trial.pattern,command->index,i,&value,&enabled);
                seq_ui_lane_set(&trial.pattern,command->index,i,command->data.value,enabled);
            }
        }
        else if(command->type==SQ_LANE&&command->index<4u){
            const XenolithSequenceLane *lane=&command->data.lane;
            if(lane->enabled&&lane->destination!=XSEQ_DEST_NONE)
                for(i=0;i<4u;i++)if(i!=command->index&&trial.pattern.lane[i].enabled
                    &&trial.pattern.lane[i].destination==lane->destination){seq_ui_status=SEQ_UI_BUSY;return 0;}
            trial.pattern.lane[command->index]=*lane;
        }else{seq_ui_status=SEQ_UI_INVALID;return 0;}
    }
    if(!xenolith_sequence_image_valid(&trial,sizeof trial)){seq_ui_status=SEQ_UI_INVALID;return 0;}
    if(!seq_send(command)){seq_ui_status=SEQ_UI_FULL;return 0;}
    seq_edit=trial;seq_ui_status=SEQ_UI_OK;return 1;
}
static int seq_ui_turn(int role,int32_t step)
{
    uint32_t object=seq_ui_object,page=seq_ui_page,knob;SeqCommand c={0};
    if(!sequence_page||!step)return 0;
    seq_ui_sync_cursor();
    if(role==EN_SELECT){
        seq_ui_object=mod_ui_enum(object,step,6u);seq_ui_page=0u;seq_ui_status=SEQ_UI_OK;redraw=1u;return 1;
    }
    if(role<EN_K1||role>EN_K4)return 0;
    knob=(uint32_t)(role-EN_K1);
    if(object==0u&&page==1u&&knob==0u&&seq_recording!=XSEQ_REC_IDLE){
        seq_ui_status=SEQ_UI_RECORD_LOCK;redraw=1u;return 1;
    }
    if(object==0u){
        if(!page){
            c.type=SQ_CONFIG;c.data.config=seq_edit.config;
            if(knob==0u)c.data.config.enabled=step>0;
            else if(knob==1u)c.data.config.bpm=mod_ui_linear(c.data.config.bpm,step,1.0f,20.0f,300.0f);
            else if(knob==2u){
                if(c.data.config.division==XSEQ_SIXTYFOURTH)
                    c.data.config.length=4u*(1u+mod_ui_enum((c.data.config.length+3u)/4u-1u,step,16u));
                else c.data.config.length=1u+mod_ui_enum(c.data.config.length-1u,step,64u);
            }
            else c.data.config.quantize=mod_ui_enum(c.data.config.quantize,step,3u);
            seq_ui_propose(&c);
        }else{
            /* These are next-REC preferences, not an implicit arm command. */
            if(knob==0u)seq_record_mode=1u+mod_ui_enum(seq_record_mode?seq_record_mode-1u:0u,step,3u);
            else if(knob==1u)seq_record_notes=step>0;
            else if(knob==2u)seq_record_lanes=mod_ui_enum(seq_record_lanes,step,16u);
            else{
                c.type=SQ_CONFIG;c.data.config=seq_edit.config;
                c.data.config.note_mode=mod_ui_enum(c.data.config.note_mode,step,2u);seq_ui_propose(&c);
            }
        }
    }else if(object==1u){
        if(knob==0u)seq_ui_move_cursor(step);
        else{
            c.type=SQ_STEP;c.index=seq_ui_cursor;c.data.step=seq_edit.pattern.step[seq_ui_cursor];
            if(!page){
                if(knob==1u)c.data.step.kind=mod_ui_enum(c.data.step.kind,step,3u);
                else if(knob==2u)c.data.step.note=mod_ui_enum(c.data.step.note,step,128u);
                else c.data.step.gate_percent=1u+mod_ui_enum(c.data.step.gate_percent-1u,step,99u);
            }else{
                uint32_t bit=1u<<(knob-1u);
                if(step>0)c.data.step.triggers|=(uint8_t)bit;else c.data.step.triggers&=(uint8_t)~bit;
            }
            seq_ui_propose(&c);
        }
    }else{
        uint32_t lane=object-2u,enabled;float value;
        xenolith_sequence_get_lane_step(&seq_edit.pattern,lane,seq_ui_cursor,&value,&enabled);
        c.type=SQ_LANE;c.index=lane;c.data.lane=seq_edit.pattern.lane[lane];
        if(!page&&knob<3u){
            if(knob==0u)c.data.lane.destination=mod_ui_enum(c.data.lane.destination,step,XSEQ_DEST_COUNT);
            else if(knob==1u)c.data.lane.enabled=step>0;
            else{
                float slew=c.data.lane.rise_seconds>c.data.lane.fall_seconds?c.data.lane.rise_seconds:c.data.lane.fall_seconds;
                slew=seq_ui_slew_step(slew,step);c.data.lane.rise_seconds=c.data.lane.fall_seconds=slew;
            }
            seq_ui_propose(&c);
        }else if(page&&knob==0u)seq_ui_move_cursor(step);
        else if((!page&&knob==3u)||(page&&knob==1u)){
            c.type=SQ_LANE_STEP;c.a=seq_ui_cursor;c.b=1u;
            c.data.value=seq_ui_lane_value(seq_edit.pattern.lane[lane].destination,value,step);seq_ui_propose(&c);
        }else if(page&&knob==2u){
            c.type=SQ_LANE_STEP;c.a=seq_ui_cursor;c.b=step>0;c.data.value=value;seq_ui_propose(&c);
        }else if(page&&step>0){
            c.type=SQ_LANE_FILL;c.data.value=value;if(seq_ui_propose(&c))seq_ui_status=SEQ_UI_FILLED;
        }
    }
    redraw=1u;return 1;
}
static void seq_ui_select(void)
{
    seq_ui_page=(seq_ui_page+1u)%2u;seq_ui_status=SEQ_UI_OK;redraw=1u;
}
static void seq_ui_field(uint32_t knob,const char **label,char value[24])
{
    uint32_t object=seq_ui_object,page=seq_ui_page;*label="--";mod_ui_copy(value,"--");
    if(object==0u){
        static const char *const labels[2][4]={{"ENABLE","BPM","BEATS","QUANT"},
            {"NEXT REC MODE","ARM NOTES","ARM LANES","NOTE GATE"}};*label=labels[page][knob];
        if(!page){
            if(knob==0u)mod_ui_on(value,seq_edit.config.enabled);
            else if(knob==1u)mod_ui_number(seq_edit.config.bpm,value);
            else if(knob==2u){
                if(seq_edit.config.division==XSEQ_SIXTYFOURTH)mod_ui_number((float)seq_edit.config.length*.25f,value);
                else{*label="SLOTS";number_text((int32_t)seq_edit.config.length,value);}
            }
            else mod_ui_copy(value,SEQ_UI_QUANT[seq_edit.config.division][seq_edit.config.quantize]);
        }else{
            if(knob==0u)mod_ui_copy(value,SEQ_UI_RECORDS[seq_record_mode<=3u?seq_record_mode:0u]);
            else if(knob==1u)mod_ui_on(value,seq_record_notes);
            else if(knob==2u){uint32_t i;for(i=0;i<4u;i++)value[i]=seq_record_lanes&(1u<<i)?(char)('1'+i):'-';value[4]=0;}
            else mod_ui_copy(value,seq_edit.config.note_mode==XSEQ_CONTINUOUS?"CONTINUOUS":"ADSR");
        }
    }else if(object==1u){
        const XenolithSequenceStep *s=&seq_edit.pattern.step[seq_ui_cursor];
        static const char *const labels[2][4]={{"EDIT STEP","TYPE","NOTE MIDI","GATE %"},
            {"EDIT STEP","TRIGGER A","TRIGGER B","RANDOM NEXT"}};*label=labels[page][knob];
        if(!knob)number_text((int32_t)seq_ui_cursor+1,value);
        else if(page)mod_ui_on(value,s->triggers&(1u<<(knob-1u)));
        else if(knob==1u)mod_ui_copy(value,SEQ_UI_TYPES[s->kind]);
        else number_text(knob==2u?s->note:s->gate_percent,value);
    }else{
        const XenolithSequenceLane *l=&seq_edit.pattern.lane[object-2u];float lane_value;uint32_t lane_enabled;
        xenolith_sequence_get_lane_step(&seq_edit.pattern,object-2u,seq_ui_cursor,&lane_value,&lane_enabled);
        static const char *const labels[2][4]={{"TARGET","ENABLE","SLEW","VALUE 0..1"},
            {"EDIT STEP","VALUE 0..1","STEP ON","FILL VALUES"}};*label=labels[page][knob];
        if(!page){
            if(knob==0u)mod_ui_copy(value,SEQ_UI_DESTINATIONS[l->destination]);
            else if(knob==1u)mod_ui_on(value,l->enabled);
            else if(knob==2u)seq_ui_slew_text(l,value);
            else{seq_ui_value_text(l->destination,lane_value,label,value);if(!(lane_enabled))*label="VALUE OFF";}
        }else{
            if(knob==0u)number_text((int32_t)seq_ui_cursor+1,value);
            else if(knob==1u)seq_ui_value_text(l->destination,lane_value,label,value);
            else if(knob==2u)mod_ui_on(value,lane_enabled);
            else mod_ui_copy(value,"TURN +");
        }
    }
}
static void seq_ui_draw(void)
{
    char title[24],page[4],value[24],label[24];uint32_t i,play=seq_playhead;
    seq_ui_sync_cursor();mod_ui_copy(title,seq_ui_object==0u?"SYSTEM":seq_ui_object==1u?"TONIC NOTES":"CV LANE ");
    if(seq_ui_object>=2u)number_text((int32_t)seq_ui_object-1,title+8);
    page[0]=(char)('1'+seq_ui_page);page[1]='/';page[2]='2';page[3]=0;
    cv_begin(240,36,C_BLACK);
    draw_panel_header("[ SEQUENCE ]");
    cv_text(16,20,&FONT_XS,title,C_HI);cv_text(169,20,&FONT_XS,page,C_GRAY);
    cv_text(207,20,&FONT_XS,seq_edit.config.enabled?"ON":"OFF",seq_edit.config.enabled?C_HI:C_DIM);cv_blit(0,0);
    cv_begin(240,120,C_BLACK);
    for(i=0;i<4u;i++){
        const char *name;uint32_t n=3u,j=0u;int32_t y=(int32_t)i*29;
        seq_ui_field(i,&name,value);label[0]='K';label[1]=(char)('1'+i);label[2]=' ';
        while(name[j]&&n<23u)label[n++]=name[j++];label[n]=0;
        cv_text(16,y+5,&FONT_XS,label,C_GRAY);
        if(text_w(&FONT_B,value)>106)cv_text(222-text_w(&FONT_XS,value),y+5,&FONT_XS,value,C_HI);
        else cv_text(222-text_w(&FONT_B,value),y+1,&FONT_B,value,C_HI);
        cv_line(16,y+27,223,y+27,C_LINE);
    }
    cv_blit(0,36);cv_begin(240,84,C_BLACK);
    {uint32_t length=seq_ui_length(),shown=length>16u?64u:16u,rows=shown/16u;
    for(i=0;i<shown;i++){
        uint32_t column=i&15u;
        int32_t x=16+(int32_t)column*13,y=4+(int32_t)(i>>4)*9,h=rows>1u?7:20;
        if(rows>1u&&seq_edit.config.division==XSEQ_SIXTYFOURTH)
            x=18+(int32_t)column*12+(int32_t)(column>>2)*4;
        int active=i<length;uint32_t kind=seq_edit.pattern.step[i].kind,triggers=seq_edit.pattern.step[i].triggers;
        uint16_t edge=i==seq_ui_cursor?C_HI:active?C_LINE:C_DIM;
        if(seq_ui_object>=2u){float cv;uint32_t enabled=0;triggers=0;
            if(i<length)xenolith_sequence_get_lane_step(&seq_edit.pattern,seq_ui_object-2u,i,&cv,&enabled);kind=enabled?XSEQ_NOTE:XSEQ_REST;
        }
        cv_rect(x,y,11,h,C_BLACK);cv_line(x,y,x+10,y,edge);cv_line(x,y+h-1,x+10,y+h-1,edge);
        cv_line(x,y,x,y+h-1,edge);cv_line(x+10,y,x+10,y+h-1,edge);
        if(rows==1u){value[0]=kind==XSEQ_NOTE?'N':kind==XSEQ_TIE?'=':'.';value[1]=0;cv_text(x+2,y+1,&FONT_XS,value,active?C_GRAY:C_DIM);}
        else if(active&&kind!=XSEQ_REST)cv_rect(x+2,y+2,7,kind==XSEQ_TIE?1:3,C_GRAY);
        if(rows==1u){
            if(triggers&XSEQ_TRIGGER_A)cv_rect(x+1,y+h-3,2,1,C_AMB);
            if(triggers&XSEQ_TRIGGER_B)cv_rect(x+4,y+h-3,2,1,C_AMB);
            if(triggers&XSEQ_TRIGGER_RANDOM)cv_rect(x+7,y+h-3,2,1,C_AMB);
        }
        if(seq_running&&(seq_ui_object>=2u?xenolith_sequence_lane_position(&seq_edit.config,seq_ui_object-2u,play):play)==i)cv_rect(x,y+h+1,11,1,C_AMB);
    }}
    cv_text(16,44,&FONT_XS,"EDIT",C_DIM);number_text((int32_t)seq_ui_cursor+1,value);cv_text(46,44,&FONT_XS,value,C_HI);
    cv_text(79,44,&FONT_XS,"PLAY",C_DIM);
    if(seq_running&&play<64u)number_text((int32_t)(seq_ui_object>=2u?xenolith_sequence_lane_position(&seq_edit.config,seq_ui_object-2u,play):play)+1,value);else mod_ui_copy(value,"--");
    cv_text(109,44,&FONT_XS,value,C_AMB);cv_text(194,44,&FONT_XS,seq_running?"RUN":"STOP",C_GRAY);
    if(seq_recording==XSEQ_REC_ARMED)cv_text(148,44,&FONT_XS,"ARMED",C_HI);
    else if(seq_recording==XSEQ_REC_ACTIVE)cv_text(148,44,&FONT_XS,"REC",C_HI);
    if(seq_ui_object>=2u)cv_text(16,57,&FONT_XS,"CV / TARGET UNITS",C_GRAY);
    else if(seq_ui_object==1u)cv_text(16,57,&FONT_XS,"TONIC MIDI",C_GRAY);
    if(seq_ui_status==SEQ_UI_FULL)mod_ui_copy(value,"QUEUE FULL / RETRY");
    else if(seq_ui_status==SEQ_UI_BUSY)mod_ui_copy(value,"TARGET BUSY");
    else if(seq_ui_status==SEQ_UI_RECORD_LOCK)mod_ui_copy(value,"REC LOCK / STOP REC");
    else if(seq_ui_status==SEQ_UI_INVALID)mod_ui_copy(value,"INVALID EDIT");
    else if(seq_ui_status==SEQ_UI_FILLED)mod_ui_copy(value,"VALUES FILLED");
    else if(seq_ui_status==SEQ_UI_RANGE)mod_ui_copy(value,"OUTSIDE CV RANGE");
    else if(seq_ui_status==SEQ_UI_TARGET)mod_ui_copy(value,"SELECT CV TARGET");
    else if(seq_ui_status==SEQ_UI_EXP_RANGE)mod_ui_copy(value,"USE EXP DEPTH TARGET");
    else if(seq_ui_object>=2u&&seq_edit.pattern.lane[seq_ui_object-2u].rise_seconds
        !=seq_edit.pattern.lane[seq_ui_object-2u].fall_seconds)mod_ui_copy(value,"ASYM -> SLEW ON EDIT");
    else if(seq_recording==XSEQ_REC_ARMED)mod_ui_copy(value,seq_running?"REC NEXT LOOP":"REC / FIRST KEY");
    else if(seq_recording==XSEQ_REC_ACTIVE)mod_ui_copy(value,"REC ACTIVE");
    else{
        char *p;mod_ui_copy(value,"REC ");p=value+4;
        mod_ui_copy(p,SEQ_UI_RECORDS[seq_record_mode<=3u?seq_record_mode:0u]);
    }
    cv_text(16,69,&FONT_XS,value,C_DIM);cv_blit(0,156);
}
