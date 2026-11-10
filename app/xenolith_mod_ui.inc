/* SPDX-License-Identifier: GPL-3.0-or-later
 * XX29 four-face constructor. Foreground only; no hidden graph edits.
 * SELECT rotation chooses A/B/RANDOM/MIX; SELECT press bypasses globally.
 * Parent LFO handler calls mod_ui_next_page() while this panel is open.
 */
#include "xenolith_modulation.h"
#include "fastmath.h"
static uint32_t mod_ui_object,mod_ui_page;
static const uint8_t MOD_UI_SOURCE_IDS[]={XM_SRC_NONE,XM_SRC_FUNCTION_A,XM_SRC_FUNCTION_B,
    XM_SRC_STEP,XM_SRC_WALK,XM_SRC_MIX_A,XM_SRC_KEY,XM_SRC_SEQ_1,XM_SRC_SEQ_2,XM_SRC_SEQ_3,XM_SRC_SEQ_4,XM_SRC_ENV_AMP,XM_SRC_ENV_FILTER,XM_SRC_ENV_INDEX,XM_SRC_ENV_TONIC};
static const char *const MOD_UI_SOURCES[]={"NONE","FUNC A","FUNC B","STEP","WALK","MIX","KEY","SEQ 1","SEQ 2","SEQ 3","SEQ 4","ENV AMP","ENV FILTER","ENV INDEX","ENV TONIC"};
static const uint8_t MOD_UI_DESTINATION_IDS[]={XM_DST_NONE,XM_DST_PITCH,XM_DST_INDEX,XM_DST_RATIO,
    XM_DST_DRIFT,XM_DST_WET_MIX,XM_DST_RANDOM_RATE,XM_DST_RANDOM_CENTRE,XM_DST_RANDOM_SPREAD,
    XM_DST_RANDOM_CORRELATE,XM_DST_RANDOM_BIAS,XM_DST_A_RISE,XM_DST_A_FALL,XM_DST_B_RISE,
    XM_DST_B_FALL,XM_DST_MIX_A_GAIN_A,XM_DST_MIX_A_GAIN_B,XM_DST_MIX_A_OFFSET,XM_DST_AMPLITUDE,XM_DST_BITS,XM_DST_FILTER_CUTOFF,XM_DST_FILTER_RESONANCE,XM_DST_CORE_RATE,XM_DST_WET_RATE,XM_DST_ECHO_TIME,XM_DST_CELLS,XM_DST_FOLD};
static const char *const MOD_UI_DESTINATIONS[]={"NONE","PITCH","INDEX /64","RATIO","DRIFT","WET MIX",
    "RANDOM RATE","RND CENTRE","RND SPREAD","RND CORRELATE","RND BIAS","A RISE","A FALL",
    "B RISE","B FALL","MIX GAIN A","MIX GAIN B","MIX OFFSET","AMPLITUDE","BIT DEPTH","FILTER OCT","FILTER RES","CORE RATE","WET RATE","ECHO TIME","CELLS","VCA FOLD"};
static const char *const MOD_UI_EVENTS[XM_EVENT_COUNT]={"NONE","KEY","STEP","A END RISE","A END FALL","B END RISE","B END FALL"};
static const char *const MOD_UI_MODES[]={"OFF","AD","GATE","CYCLE","SLEW"};
static const char *const MOD_UI_MIX_MODES[]={"OFF","SUM","PRODUCT","MAX"};
static const char *const MOD_UI_CLOCKS[XM_CLOCK_COUNT]={"FREE","STEP","FUNCTION END","KEY"};
static float mod_ui_bound(float x,float lo,float hi){if(!(x>=lo))return lo;return x>hi?hi:x;}
static uint8_t mod_ui_enum(unsigned value,int32_t delta,unsigned count){
    int32_t next;if(!count)return 0;if(value>=count)value=count-1u;
    if(delta>=(int32_t)count)return (uint8_t)(count-1u);if(delta<=-(int32_t)count)return 0;
    next=(int32_t)value+delta;return next<0?0u:next>=(int32_t)count?(uint8_t)(count-1u):(uint8_t)next;
}
static int32_t mod_ui_delta(int32_t step){return step>2048?2048:step<-2048?-2048:step;}
static float mod_ui_linear(float value,int32_t step,float quantum,float lo,float hi){return mod_ui_bound(value+(float)mod_ui_delta(step)*quantum,lo,hi);}
static float mod_ui_log(float value,int32_t step,float lo,float hi,int zero){
    int32_t delta=mod_ui_delta(step);float next;
    if(zero&&!(value>0)){if(delta<=0)return 0;value=lo;--delta;}
    value=mod_ui_bound(value,lo,hi);next=value*fm_exp2f((float)delta*(1.0f/24.0f));
    if(zero&&delta<0&&next<lo)return 0;return mod_ui_bound(next,lo,hi);
}
static void mod_ui_copy(char *out,const char *text){uint32_t i=0;while(text[i]&&i<23u){out[i]=text[i];++i;}out[i]=0;}
static void mod_ui_number(float value,char *out){
    uint32_t scaled;char *p=out;value=mod_ui_bound(value,-10000,10000);
    if(value<0){*p++='-';value=-value;}scaled=(uint32_t)(value*1000+.5f);
    number_text((int32_t)(scaled/1000u),p);while(*p)++p;
    *p++='.';*p++=(char)('0'+(scaled/100u)%10u);*p++=(char)('0'+(scaled/10u)%10u);*p++=(char)('0'+scaled%10u);*p=0;
    while(p>out&&p[-1]=='0'&&p[-2]!='.')*--p=0;
}
static void mod_ui_on(char *out,unsigned enabled){mod_ui_copy(out,enabled?"ON":"OFF");}
static unsigned mod_ui_find(unsigned id,const uint8_t *ids,unsigned count){unsigned i;for(i=0;i<count;i++)if(ids[i]==id)return i;return count;}
static unsigned mod_ui_choose(unsigned id,int32_t step,const uint8_t *ids,unsigned count){unsigned i=mod_ui_find(id,ids,count);if(i==count)i=0;return ids[mod_ui_enum(i,step,count)];}
static const char *mod_ui_source_name(unsigned id){unsigned i=mod_ui_find(id,MOD_UI_SOURCE_IDS,sizeof MOD_UI_SOURCE_IDS);return i<sizeof MOD_UI_SOURCE_IDS?MOD_UI_SOURCES[i]:"LEGACY";}
static const char *mod_ui_destination_name(unsigned id){unsigned i=mod_ui_find(id,MOD_UI_DESTINATION_IDS,sizeof MOD_UI_DESTINATION_IDS);return i<sizeof MOD_UI_DESTINATION_IDS?MOD_UI_DESTINATIONS[i]:"LEGACY";}
static XenolithModRoute *mod_ui_primary(void){return &mod_edit.route[mod_ui_object];}
static unsigned mod_ui_primary_source(void){
    if(mod_ui_object<2u)return XM_SRC_FUNCTION_A+mod_ui_object;
    if(mod_ui_object==2u)return mod_edit.route[2].source==XM_SRC_WALK?XM_SRC_WALK:XM_SRC_STEP;return XM_SRC_MIX_A;
}
static int mod_ui_owned(void){const XenolithModRoute *r=mod_ui_primary();return r->destination==XM_DST_NONE||r->source==mod_ui_primary_source();}
static float mod_ui_default_depth(unsigned destination){unsigned unit=xenolith_mod_destination(destination).unit;if(destination==XM_DST_AMPLITUDE)return -1;if(destination==XM_DST_BITS)return -8;if(destination==XM_DST_CELLS)return 1;if(destination==XM_DST_CORE_RATE||destination==XM_DST_WET_RATE)return -1;return unit==XM_UNIT_SEMITONES?1:unit==XM_UNIT_OCTAVES?.25f:.1f;}
static void mod_ui_destination(int32_t step){
    XenolithModRoute *r=mod_ui_primary();unsigned old=r->destination,old_unit=xenolith_mod_destination(old).unit;
    r->source=(uint8_t)mod_ui_primary_source();r->destination=(uint8_t)mod_ui_choose(old,step,MOD_UI_DESTINATION_IDS,sizeof MOD_UI_DESTINATION_IDS);
    if(r->destination==XM_DST_NONE){r->enabled=0;r->depth=0;return;}
    {XenolithModDestination d=xenolith_mod_destination(r->destination);
        if(d.unit!=old_unit||r->depth==0||(r->destination==XM_DST_AMPLITUDE&&old!=XM_DST_AMPLITUDE))r->depth=mod_ui_default_depth(r->destination);
        r->depth=mod_ui_bound(r->depth,-d.offset_limit,d.offset_limit);}
    r->enabled=1;mod_edit.enabled=1;
    if(mod_ui_object<2u)mod_edit.function[mod_ui_object].enabled=1;
    else if(mod_ui_object==2u)mod_edit.random.enabled=1;else mod_edit.mix[0].enabled=1;
}
static void mod_ui_depth(int32_t step){
    XenolithModRoute *r=mod_ui_primary();XenolithModDestination d=xenolith_mod_destination(r->destination);
    if(!mod_ui_owned()||d.unit==XM_UNIT_NONE)return;
    r->depth=mod_ui_linear(r->depth,step,d.unit==XM_UNIT_BITS||d.unit==XM_UNIT_CELLS?1.0f:d.unit==XM_UNIT_SEMITONES?.25f:.02f,-d.offset_limit,d.offset_limit);
    if(fm_fabsf(r->depth)<.000001f)r->depth=0;
}
static void mod_ui_publish_change(const XenolithModConfig *before){
    const uint8_t *a=(const uint8_t *)before,*b=(const uint8_t *)&mod_edit;uint32_t i;
    for(i=0;i<sizeof *before;i++)if(a[i]!=b[i]){if(xenolith_mod_validate(&mod_edit))mod_publish();else mod_edit=*before;return;}
}
static int mod_ui_turn(int role,int32_t step){
    XenolithModConfig before;uint32_t k,o=mod_ui_object;int32_t delta=mod_ui_delta(step);
    if(!modulation_page||!step)return 0;
    if(role==EN_SELECT){mod_ui_object=mod_ui_enum(o,step,4u);mod_ui_page=0;redraw=1;return 1;}
    if(role<EN_K1||role>EN_K4)return 0;k=(uint32_t)(role-EN_K1);before=mod_edit;
    if(o==2u&&mod_ui_page==2u){
        XenolithRandomMemoryConfig old=random_memory_edit;uint32_t changed=0u,i;
        if(k==0u)random_memory_edit.law=mod_ui_enum(random_memory_edit.law,step,XRM_LAW_COUNT);
        else if(k==1u){unsigned v=random_memory_edit.length?random_memory_edit.length-1u:0u;
            v=mod_ui_enum(v,step,32u);random_memory_edit.length=(uint8_t)(v?v+1u:0u);}
        else if(k==2u)random_memory_edit.renew=mod_ui_linear(random_memory_edit.renew,delta,.05f,0,1);
        else mod_edit.random.bias=mod_ui_linear(mod_edit.random.bias,delta,.02f,-.98f,.98f);
        for(i=0;i<sizeof old;i++)if(((const uint8_t*)&old)[i]!=((const uint8_t*)&random_memory_edit)[i])changed=1u;
        if(changed)mod_publish();else mod_ui_publish_change(&before);
        redraw=1u;return 1;
    }
    if(o<2u){
        XenolithModFunction *f=&mod_edit.function[o];
        if(!mod_ui_page){
            if(k==0u)f->rise_s=mod_ui_log(f->rise_s,delta,.001f,60,f->mode==XM_SLEW);
            else if(k==1u)f->fall_s=mod_ui_log(f->fall_s,delta,.001f,60,f->mode==XM_SLEW);
            else if(k==2u)mod_ui_depth(step);else mod_ui_destination(step);
        }else{
            if(k==0u){unsigned mode=mod_ui_enum(f->enabled?f->mode+1u:0u,step,XM_FUNCTION_MODE_COUNT+1u);
                f->enabled=(uint8_t)(mode!=0);if(mode)f->mode=(uint8_t)(mode-1u);
                if(f->mode!=XM_SLEW){if(f->rise_s<.001f)f->rise_s=.001f;if(f->fall_s<.001f)f->fall_s=.001f;}}
            else if(k==1u)f->input=(uint8_t)mod_ui_choose(f->input,step,MOD_UI_SOURCE_IDS,sizeof MOD_UI_SOURCE_IDS);
            else if(k==2u)f->trigger_event=mod_ui_enum(f->trigger_event,step,XM_EVENT_COUNT);
            else f->curve=mod_ui_linear(f->curve,delta,.02f,-1,1);
        }
    }else if(o==2u){
        XenolithModRandom *r=&mod_edit.random;
        if(!mod_ui_page){
            if(k==0u)r->rate_hz=mod_ui_log(r->rate_hz,delta,.01f,50,0);
            else if(k==1u)r->spread=mod_ui_linear(r->spread,delta,.02f,0,1);
            else if(k==2u)mod_ui_depth(step);else mod_ui_destination(step);
        }else{
            if(k==0u)r->correlate=mod_ui_linear(r->correlate,delta,.02f,0,.98f);
            else if(k==1u){unsigned p=r->levels?r->levels-1u:0u;p=mod_ui_enum(p,step,24u);r->levels=(uint8_t)(p?p+1u:0u);}
            else if(k==2u)r->clock=mod_ui_enum(r->clock,step,XM_CLOCK_COUNT);
            else{XenolithModRoute *route=mod_ui_primary();unsigned out=route->source==XM_SRC_WALK?1u:0u;
                out=mod_ui_enum(out,step,2u);route->source=(uint8_t)(out?XM_SRC_WALK:XM_SRC_STEP);}
        }
    }else{
        XenolithModMix *m=&mod_edit.mix[0];
        if(!mod_ui_page){
            if(k==0u)m->input_a=(uint8_t)mod_ui_choose(m->input_a,step,MOD_UI_SOURCE_IDS,sizeof MOD_UI_SOURCE_IDS);
            else if(k==1u)m->input_b=(uint8_t)mod_ui_choose(m->input_b,step,MOD_UI_SOURCE_IDS,sizeof MOD_UI_SOURCE_IDS);
            else if(k==2u)mod_ui_depth(step);else mod_ui_destination(step);
        }else{
            if(k==0u)m->gain_a=mod_ui_linear(m->gain_a,delta,.02f,-1,1);
            else if(k==1u)m->gain_b=mod_ui_linear(m->gain_b,delta,.02f,-1,1);
            else if(k==2u)m->offset=mod_ui_linear(m->offset,delta,.02f,-1,1);
            else{unsigned mode=mod_ui_enum(m->enabled?m->mode+1u:0u,step,XM_MIX_MODE_COUNT+1u);
                m->enabled=(uint8_t)(mode!=0);if(mode)m->mode=(uint8_t)(mode-1u);}
        }
    }
    mod_ui_publish_change(&before);redraw=1;return 1;
}
static void mod_ui_select(void){mod_edit.enabled=(uint8_t)!mod_edit.enabled;mod_publish();redraw=1;}
static void mod_ui_next_page(void){mod_ui_page=mod_ui_page==2u?0u:mod_ui_page^1u;redraw=1;}
static void mod_ui_hold(void){if(mod_ui_object==2u){mod_ui_page=2u;redraw=1u;}}
static void mod_ui_field(unsigned k,const char **label,char value[24]){
    uint32_t o=mod_ui_object;const XenolithModRoute *route=mod_ui_primary();*label="--";mod_ui_copy(value,"--");
    if(o==2u&&mod_ui_page==2u){
        static const char *const labels[]={"LAW","MEMORY","RENEW %","BIAS"};
        static const char *const laws[]={"CURRENT","BELL","EDGE","BINARY"};*label=labels[k];
        if(k==0u)mod_ui_copy(value,laws[random_memory_edit.law]);
        else if(k==1u){if(random_memory_edit.length)number_text(random_memory_edit.length,value);else mod_ui_copy(value,"OFF");}
        else mod_ui_number(k==2u?random_memory_edit.renew*100.0f:mod_edit.random.bias,value);return;
    }
    if(!mod_ui_page&&k==2u){unsigned unit=xenolith_mod_destination(route->destination).unit;
        *label=unit==XM_UNIT_CELLS?"DEPTH CELLS":unit==XM_UNIT_BITS?"DEPTH BITS":unit==XM_UNIT_SEMITONES?"DEPTH SEMITONES":unit==XM_UNIT_OCTAVES?"DEPTH OCTAVES":"DEPTH NORMALIZED";
        if(!mod_ui_owned()||route->destination==XM_DST_NONE)mod_ui_copy(value,"SET DEST");else mod_ui_number(route->depth,value);return;}
    if(!mod_ui_page&&k==3u){*label="DESTINATION";mod_ui_copy(value,mod_ui_owned()?mod_ui_destination_name(route->destination):"LEGACY");return;}
    if(o<2u){const XenolithModFunction *f=&mod_edit.function[o];
        if(!mod_ui_page){*label=k?"FALL S":"RISE S";mod_ui_number(k?f->fall_s:f->rise_s,value);}
        else{static const char *const labels[]={"MODE","INPUT","TRIGGER","CURVE"};*label=labels[k];
            if(k==0u)mod_ui_copy(value,MOD_UI_MODES[f->enabled?f->mode+1u:0u]);else if(k==1u)mod_ui_copy(value,mod_ui_source_name(f->input));
            else if(k==2u)mod_ui_copy(value,MOD_UI_EVENTS[f->trigger_event]);else mod_ui_number(f->curve,value);}
    }else if(o==2u){const XenolithModRandom *r=&mod_edit.random;
        if(!mod_ui_page){*label=k?"SPREAD":"RATE HZ";mod_ui_number(k?r->spread:r->rate_hz,value);}
        else{static const char *const labels[]={"CORRELATE","LEVELS","CLOCK","OUTPUT"};*label=labels[k];
            if(k==0u)mod_ui_number(r->correlate,value);else if(k==1u){if(r->levels)number_text(r->levels,value);else mod_ui_copy(value,"OFF");}
            else if(k==2u)mod_ui_copy(value,MOD_UI_CLOCKS[r->clock]);else mod_ui_copy(value,route->source==XM_SRC_WALK?"WALK":mod_ui_owned()?"STEP":"LEGACY");}
    }else{const XenolithModMix *m=&mod_edit.mix[0];
        if(!mod_ui_page){*label=k?"INPUT B":"INPUT A";mod_ui_copy(value,mod_ui_source_name(k?m->input_b:m->input_a));}
        else{static const char *const labels[]={"GAIN A","GAIN B","OFFSET","MODE"};*label=labels[k];
            if(k<3u)mod_ui_number(k==0u?m->gain_a:k==1u?m->gain_b:m->offset,value);else mod_ui_copy(value,MOD_UI_MIX_MODES[m->enabled?m->mode+1u:0u]);}
    }
}
static const char *mod_ui_diagram_destination(unsigned d){
    switch(d){case XM_DST_NONE:return "NONE";case XM_DST_PITCH:return "PITCH";case XM_DST_INDEX:return "INDEX";case XM_DST_RATIO:return "RATIO";
    case XM_DST_CORE_RATE:return "CORE Hz";case XM_DST_WET_RATE:return "WET Hz";case XM_DST_ECHO_TIME:return "ECHO";case XM_DST_CELLS:return "CELLS";case XM_DST_FOLD:return "FOLD";
    case XM_DST_FILTER_CUTOFF:return "FILTER Hz";case XM_DST_FILTER_RESONANCE:return "FILTER Q";case XM_DST_DRIFT:return "DRIFT";case XM_DST_WET_MIX:return "WET MIX";case XM_DST_AMPLITUDE:return "AMP";case XM_DST_BITS:return "BITS";case XM_DST_RANDOM_RATE:return "RND Hz";
    case XM_DST_RANDOM_CENTRE:return "RND CEN";case XM_DST_RANDOM_SPREAD:return "RND SPR";case XM_DST_RANDOM_CORRELATE:return "RND COR";case XM_DST_RANDOM_BIAS:return "RND BIAS";
    case XM_DST_A_RISE:return "A RISE";case XM_DST_A_FALL:return "A FALL";case XM_DST_B_RISE:return "B RISE";case XM_DST_B_FALL:return "B FALL";
    case XM_DST_MIX_A_GAIN_A:return "GAIN A";case XM_DST_MIX_A_GAIN_B:return "GAIN B";case XM_DST_MIX_A_OFFSET:return "OFFSET";default:return "LEGACY";}
}
static void mod_ui_box(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t ink){
    cv_line(x,y,x+w-1,y,ink);cv_line(x,y+h-1,x+w-1,y+h-1,ink);cv_line(x,y,x,y+h-1,ink);cv_line(x+w-1,y,x+w-1,y+h-1,ink);
}
static void mod_ui_center(int32_t x,int32_t y,const char *text,uint16_t ink){cv_text(x-text_w(&FONT_XS,text)/2,y,&FONT_XS,text,ink);}
static void mod_ui_diagram(void){
    unsigned o=mod_ui_object,id=mod_ui_primary_source();const XenolithModRoute *r=mod_ui_primary();
    float live=mod_source_snapshot[id];char value[24];
    int source_enabled=o<2u?mod_edit.function[o].enabled:o==2u?mod_edit.random.enabled:mod_edit.mix[0].enabled;
    uint16_t ink=mod_edit.enabled&&source_enabled?C_AMB:C_DIM;
    int owned=mod_ui_owned(),routed=owned&&r->enabled&&r->destination!=XM_DST_NONE;
    live=mod_edit.enabled&&source_enabled?xenolith_mod_route_value(id,live):0.0f;
    mod_ui_box(8,27,56,48,C_LINE);mod_ui_box(84,18,72,60,ink);mod_ui_box(176,32,56,36,routed?ink:C_LINE);
    if(o<2u){const XenolithModFunction *f=&mod_edit.function[o];const char *input;float total=f->rise_s+f->fall_s;
        int32_t peak=total>0?90+(int32_t)(60*f->rise_s/total):120;
        if(f->mode==XM_CYCLE)input="LOOP";else if(f->mode==XM_SLEW||(f->mode==XM_GATE&&f->input!=XM_SRC_NONE))input=mod_ui_source_name(f->input);
        else if(f->mode==XM_GATE)input="GATE";else{static const char *const names[]={"MANUAL","KEY","STEP","A RISE","A FALL","B RISE","B FALL"};input=names[f->trigger_event];}
        if(peak<94)peak=94;if(peak>146)peak=146;
        mod_ui_center(36,43,input,C_GRAY);mod_ui_center(120,20,o?"B":"A",ink);cv_line(90,65,peak,39,ink);cv_line(peak,39,150,65,ink);cv_line(64,51,83,51,C_LINE);
    }else if(o==2u){const XenolithModRandom *random=&mod_edit.random;
        const char *input=random->hold?"HOLD":random->clock==XM_CLOCK_FREE?"FREE":random->clock==XM_CLOCK_STEP?"STEP":random->clock==XM_CLOCK_KEY?"KEY":"FN END";
        mod_ui_center(36,43,input,C_GRAY);mod_ui_center(120,20,"RND",ink);cv_line(90,61,104,61,ink);cv_line(104,61,104,43,ink);cv_line(104,43,118,43,ink);
        cv_line(118,43,118,66,ink);cv_line(118,66,134,66,ink);cv_line(134,66,134,38,ink);cv_line(134,38,150,38,ink);cv_line(64,51,83,51,C_LINE);
    }else{const XenolithModMix *m=&mod_edit.mix[0];char a[24],b[24];a[0]='A';a[1]=' ';mod_ui_copy(a+2,mod_ui_source_name(m->input_a));
        b[0]='B';b[1]=' ';mod_ui_copy(b+2,mod_ui_source_name(m->input_b));mod_ui_center(36,30,a,C_GRAY);mod_ui_center(36,54,b,C_GRAY);mod_ui_center(120,20,"MIX",ink);
        mod_ui_center(120,45,m->mode==XM_PRODUCT?"x":m->mode==XM_MAX?"MAX":"+",ink);cv_line(64,38,83,45,C_LINE);cv_line(64,62,83,55,C_LINE);}
    cv_line(156,51,175,51,routed?ink:C_LINE);cv_line(171,47,175,51,routed?ink:C_LINE);cv_line(171,55,175,51,routed?ink:C_LINE);
    mod_ui_center(166,28,r->depth<0?"-":r->depth>0?"+":"0",routed?ink:C_DIM);mod_ui_center(204,10,"DEST",C_DIM);
    mod_ui_center(204,43,owned?mod_ui_diagram_destination(r->destination):"LEGACY",routed?C_HI:C_DIM);
    mod_ui_number(live,value);mod_ui_center(120,84,value,C_GRAY);
    mod_ui_center(84,84,"-1",C_DIM);mod_ui_center(156,84,"+1",C_DIM);
    {int32_t end=120+(int32_t)(live*32);cv_line(88,105,152,105,C_LINE);
        cv_line(88,103,88,107,C_DIM);cv_line(152,103,152,107,C_DIM);
        if(end!=120){cv_line(120,104,end,104,ink);cv_line(120,105,end,105,ink);}
        cv_line(end,102,end,108,ink);cv_line(120,100,120,110,C_GRAY);}
}
static void mod_ui_draw(void){
    static const char *const titles[]={"FUNCTION A","FUNCTION B","RANDOM","MIX"};char text[24],label[24],page[4];unsigned i;
    if(mod_ui_page==2u)page[0]=0;else{page[0]=(char)('1'+mod_ui_page);page[1]='/';page[2]='2';page[3]=0;}
    cv_begin(240,36,C_BLACK);draw_panel_header("[ MODULATION ]");
    cv_text(16,20,&FONT_XS,mod_ui_object==2u&&mod_ui_page==2u?"RANDOM MEMORY":titles[mod_ui_object],C_HI);cv_text(169,20,&FONT_XS,page,C_GRAY);
    cv_text(207,20,&FONT_XS,mod_edit.enabled?"ON":"OFF",mod_edit.enabled?C_HI:C_DIM);cv_blit(0,0);cv_begin(240,92,C_BLACK);
    for(i=0;i<4u;i++){const char *name;uint32_t n=3u,j=0u;int32_t y=(int32_t)i*23;mod_ui_field(i,&name,text);
        label[0]='K';label[1]=(char)('1'+i);label[2]=' ';while(name[j]&&n<23u)label[n++]=name[j++];label[n]=0;cv_text(16,y+3,&FONT_XS,label,C_GRAY);
        if(text_w(&FONT_B,text)>100)cv_text(222-text_w(&FONT_XS,text),y+3,&FONT_XS,text,C_HI);else cv_text(222-text_w(&FONT_B,text),y,&FONT_B,text,C_HI);
        cv_line(16,y+22,223,y+22,C_LINE);}
    cv_blit(0,36);cv_begin(240,112,C_BLACK);mod_ui_diagram();cv_blit(0,128);
}
