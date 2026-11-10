/* SPDX-License-Identifier: GPL-3.0-or-later
 * Foreground keyboard assignment surface. Include after mod UI helpers.
 * Parent owns configuration publication, held/latch state and audio snapshots.
 */
#include "xenolith_keyboard.h"

static uint32_t keyboard_ui_page,keyboard_ui_log_warning;
static const char *const KEY_UI_DEST[]={"TONIC","INDEX","RATIO","CORE RATE","WET RATE","DRIFT","WET MIX","ANS CELLS","AMPLITUDE","BIT DEPTH","EXP DEPTH","FILTER HZ","FILTER RES"};
static const char *const KEY_UI_RESPONSE[]={"CHROMATIC","LINEAR","LOG"};
/* UI modes reuse destination CELLS; this session cache is not a second patch
 * payload. Root calls config_loaded after recall/default load. A recalled ANS
 * patch returns to ordinary TONIC when no scalar setting has been used yet. */
static XenolithKeyboardConfig keyboard_ui_global;
static float keyboard_ui_ans_low=20.0f,keyboard_ui_ans_high=8000.0f;
static uint32_t keyboard_ui_cache_ready;
static const uint8_t KEY_UI_GLOBAL_IDS[]={XKEY_TONIC,XKEY_INDEX,XKEY_RATIO,XKEY_CORE_RATE,
    XKEY_WET_RATE,XKEY_DRIFT,XKEY_WET_MIX,XKEY_AMPLITUDE,XKEY_BITS,XKEY_EXP_DEPTH,XKEY_FILTER_CUTOFF,XKEY_FILTER_RESONANCE};
static void keyboard_ui_config_loaded(void)
{
    xenolith_keyboard_config_defaults(&keyboard_ui_global);
    keyboard_ui_ans_low=20.0f;keyboard_ui_ans_high=8000.0f;
    if(keyboard_edit.destination==XKEY_CELLS){keyboard_ui_ans_low=keyboard_edit.low;keyboard_ui_ans_high=keyboard_edit.high;}
    else keyboard_ui_global=keyboard_edit;
    keyboard_ui_cache_ready=1u;keyboard_ui_log_warning=0u;
}
static void keyboard_ui_mode(int32_t step)
{
    uint32_t bands=keyboard_edit.destination==XKEY_CELLS;
    if(!keyboard_ui_cache_ready)keyboard_ui_config_loaded();
    if(mod_ui_enum(bands,step,2u)==bands)return;
    if(!bands){
        keyboard_ui_global=keyboard_edit;
        keyboard_edit.destination=XKEY_CELLS;keyboard_edit.response=XKEY_LOG;
        keyboard_edit.low=keyboard_ui_ans_low;keyboard_edit.high=keyboard_ui_ans_high;
    }else{
        keyboard_ui_ans_low=keyboard_edit.low;keyboard_ui_ans_high=keyboard_edit.high;
        keyboard_edit.destination=keyboard_ui_global.destination;keyboard_edit.response=keyboard_ui_global.response;
        keyboard_edit.low=keyboard_ui_global.low;keyboard_edit.high=keyboard_ui_global.high;
    }
}
static void keyboard_ui_response(int32_t step)
{
    uint32_t first=keyboard_edit.destination==XKEY_TONIC?XKEY_CHROMATIC:XKEY_LINEAR;
    uint32_t response=first+mod_ui_enum(keyboard_edit.response-first,step,XKEY_RESPONSE_COUNT-first);
    if(response==XKEY_LOG&&!(keyboard_edit.low>0.0f))keyboard_ui_log_warning=1u;
    else keyboard_edit.response=response;
}
static void keyboard_ui_append(char *out,const char *text)
{
    uint32_t n=0u,i=0u;while(out[n]&&n<23u)++n;
    while(text[i]&&n<23u)out[n++]=text[i++];out[n]=0;
}
static void keyboard_ui_value(float value,char *out)
{
    uint32_t dest=keyboard_edit.destination;
    if(dest==XKEY_CORE_RATE||dest==XKEY_WET_RATE||((dest==XKEY_TONIC||dest==XKEY_CELLS||dest==XKEY_FILTER_CUTOFF)&&value>=1000.0f)){
        mod_ui_number(value*0.001f,out);keyboard_ui_append(out," kHz");
    }else{
        mod_ui_number((dest==XKEY_DRIFT||dest==XKEY_WET_MIX||dest==XKEY_AMPLITUDE||dest==XKEY_FILTER_RESONANCE)?value*100.0f:value,out);
        if(dest==XKEY_TONIC||dest==XKEY_CELLS||dest==XKEY_FILTER_CUTOFF)keyboard_ui_append(out," Hz");
        else if(dest==XKEY_DRIFT||dest==XKEY_WET_MIX||dest==XKEY_AMPLITUDE||dest==XKEY_FILTER_RESONANCE)keyboard_ui_append(out," %");
        else if(dest==XKEY_BITS)keyboard_ui_append(out," bits");
        else if(dest==XKEY_EXP_DEPTH)keyboard_ui_append(out," ST");
    }
}
static float keyboard_ui_endpoint(float value,int32_t step,float low,float high)
{
    float quantum=1.0f;uint32_t d=keyboard_edit.destination;
    if(d==XKEY_FILTER_CUTOFF)return mod_ui_bound(value*fm_exp2f((float)mod_ui_delta(step)/96.0f),low,high);
    if(d==XKEY_INDEX||d==XKEY_EXP_DEPTH)quantum=0.1f;
    else if(d==XKEY_RATIO)quantum=0.01f;
    else if(d==XKEY_DRIFT||d==XKEY_WET_MIX||d==XKEY_AMPLITUDE||d==XKEY_FILTER_RESONANCE)quantum=0.01f;
    else if(d==XKEY_CORE_RATE||d==XKEY_WET_RATE)quantum=value>=10000.0f?250.0f:125.0f;
    else if(d==XKEY_CELLS)quantum=value>=1000.0f?25.0f:value>=200.0f?5.0f:1.0f;
    return mod_ui_linear(value,step,quantum,low,high);
}
static int keyboard_ui_turn(int role,int32_t step)
{
    XenolithKeyboardConfig before;uint32_t k;float low,high;
    if(!keyboard_page)return 0;
    if(role==EN_SELECT){keyboard_ui_page=mod_ui_enum(keyboard_ui_page,step,2u);keyboard_ui_log_warning=0;redraw=1;return 1;}
    if(role<EN_K1||role>EN_K4)return 0;
    if(!step)return 1;
    before=keyboard_edit;k=(uint32_t)(role-EN_K1);keyboard_ui_log_warning=0;
    if(!keyboard_ui_page){
        if(!k&&keyboard_edit.destination!=XKEY_CELLS){
            uint32_t d=mod_ui_choose(keyboard_edit.destination,step,KEY_UI_GLOBAL_IDS,sizeof KEY_UI_GLOBAL_IDS);
            if(d!=keyboard_edit.destination){
                keyboard_edit.destination=d;
                xenolith_keyboard_destination_defaults(d,&keyboard_edit.low,&keyboard_edit.high,&keyboard_edit.response);
            }
        }else if(k==1u)keyboard_ui_mode(step);
        else if(k==2u)keyboard_edit.latch=mod_ui_enum(keyboard_edit.latch,step,2u);
        else if(k==3u)keyboard_edit.gate=mod_ui_enum(keyboard_edit.gate,step,2u);
    }else{
        xenolith_keyboard_destination_bounds(keyboard_edit.destination,&low,&high);
        if(!k){
            float value=keyboard_ui_endpoint(keyboard_edit.low,step,low,keyboard_edit.high);
            if(keyboard_edit.response==XKEY_LOG&&!(value>0.0f))keyboard_ui_log_warning=1u;
            else keyboard_edit.low=value;
        }else if(k==1u)keyboard_edit.high=keyboard_ui_endpoint(keyboard_edit.high,step,keyboard_edit.low,high);
        else if(keyboard_edit.destination==XKEY_CELLS){
            if(k==2u)keyboard_edit.cell_width=1u+mod_ui_enum(keyboard_edit.cell_width-1u,step,16u);
            else keyboard_edit.cell_layout=mod_ui_enum(keyboard_edit.cell_layout,step,XKEY_LAYOUT_COUNT);
        }else if(k==3u)keyboard_ui_response(step);
    }
    if(!xenolith_keyboard_config_valid(&keyboard_edit))keyboard_edit=before;
    if(before.destination!=keyboard_edit.destination||before.response!=keyboard_edit.response||before.latch!=keyboard_edit.latch
       ||before.gate!=keyboard_edit.gate||before.low!=keyboard_edit.low||before.high!=keyboard_edit.high
       ||before.cell_width!=keyboard_edit.cell_width||before.cell_layout!=keyboard_edit.cell_layout)keyboard_publish();
    redraw=1;return 1;
}
static void keyboard_ui_select(void)
{
    keyboard_ui_page=(keyboard_ui_page+1u)%2u;keyboard_ui_log_warning=0;redraw=1;
}
static void keyboard_ui_field(uint32_t k,const char **label,char *out)
{
    if(!keyboard_ui_page){
        static const char *const labels[]={"DESTINATION","KEY MODE","LATCH","KEY GATE"};*label=labels[k];
        if(!k)mod_ui_copy(out,keyboard_edit.destination==XKEY_CELLS?"CELL BANDS":KEY_UI_DEST[keyboard_edit.destination]);
        else if(k==1u)mod_ui_copy(out,keyboard_edit.destination==XKEY_CELLS?"ANS CELLS":"GLOBAL");
        else mod_ui_on(out,k==2u?keyboard_edit.latch:keyboard_edit.gate);
    }else{
        static const char *const labels[]={"LOW","HIGH","BAND WIDTH","LAYOUT"};*label=labels[k];
        if(k<2u){
            if(keyboard_edit.destination==XKEY_CELLS)*label=k?"HIGH SPEC":"LOW SPEC";
            else if(keyboard_edit.response==XKEY_CHROMATIC)*label=k?"HIGH UNUSED":"LOW UNUSED";
            keyboard_ui_value(k?keyboard_edit.high:keyboard_edit.low,out);
        }else if(keyboard_edit.destination!=XKEY_CELLS){
            if(k==2u){*label="--";mod_ui_copy(out,"--");}
            else{*label="RESPONSE";mod_ui_copy(out,KEY_UI_RESPONSE[keyboard_edit.response]);}
        }else if(k==2u){
            number_text((int32_t)keyboard_edit.cell_width,out);
            if(requested_cells<keyboard_edit.cell_width){
                char effective[24];keyboard_ui_append(out,"/");number_text((int32_t)requested_cells,effective);keyboard_ui_append(out,effective);
            }
        }else mod_ui_copy(out,keyboard_edit.cell_layout==XKEY_SPECTRAL?"SPECTRAL":"RELATIVE");
    }
}
static void keyboard_ui_draw(void)
{
    char text[24],label[24],range[24],high[24];uint32_t i,count=0u,cells=requested_cells;
    uint32_t held=plat_keys()&XENOLITH_KEYBOARD_KEY_MASK;
    uint32_t mask_lo=keyboard_mask_lo_snapshot,mask_hi=keyboard_mask_hi_snapshot;
    int bands=keyboard_edit.destination==XKEY_CELLS;
    if(cells<1u)cells=1u;if(cells>64u)cells=64u;
    cv_begin(240,36,C_BLACK);draw_panel_header("[ KEY ASSIGN ]");
    cv_text(16,20,&FONT_XS,keyboard_ui_page?"RANGE 2/2":"ASSIGN 1/2",C_HI);
    cv_blit(0,0);
    cv_begin(240,120,C_BLACK);
    for(i=0;i<4u;i++){
        const char *name;uint32_t n=3u,j=0u;int32_t y=(int32_t)i*29;
        int inactive=(!keyboard_ui_page&&bands&&!i)||(keyboard_ui_page&&!bands&&i==2u);
        keyboard_ui_field(i,&name,text);label[0]='K';label[1]=(char)('1'+i);label[2]=' ';
        while(name[j]&&n<23u)label[n++]=name[j++];label[n]=0;
        cv_text(16,y+5,&FONT_XS,label,inactive?C_DIM:C_GRAY);
        if(text_w(&FONT_B,text)>106)cv_text(222-text_w(&FONT_XS,text),y+5,&FONT_XS,text,inactive?C_DIM:C_HI);
        else cv_text(222-text_w(&FONT_B,text),y+1,&FONT_B,text,inactive?C_DIM:C_HI);
        cv_line(16,y+27,223,y+27,C_LINE);
    }
    cv_blit(0,36);cv_begin(240,84,C_BLACK);
    if(bands){
        for(i=0;i<cells;i++)if((i<32u?mask_lo:mask_hi)&(UINT32_C(1)<<(i&31u)))++count;
        number_text((int32_t)count,text);keyboard_ui_append(text,"/");number_text((int32_t)cells,high);keyboard_ui_append(text,high);
    }else if(keyboard_edit.response==XKEY_CHROMATIC)mod_ui_copy(text,"CHROMATIC");
    else if(keyboard_active_snapshot)keyboard_ui_value(keyboard_value_snapshot,text);
    else mod_ui_copy(text,"IDLE");
    cv_text(16,0,&FONT_XS,bands?"CELLS":"VALUE",C_DIM);cv_text(222-text_w(&FONT_XS,text),0,&FONT_XS,text,C_AMB);
    if(keyboard_ui_log_warning)mod_ui_copy(range,"LOG NEEDS LOW >0");
    else if(bands&&keyboard_edit.cell_layout==XKEY_RELATIVE)mod_ui_copy(range,"RELATIVE TUNING RETAINED");
    else if(!bands&&keyboard_edit.response==XKEY_CHROMATIC)mod_ui_copy(range,"CHROMATIC: RANGE UNUSED");
    else{
        keyboard_ui_value(keyboard_edit.low,range);keyboard_ui_append(range,"..");
        keyboard_ui_value(keyboard_edit.high,high);keyboard_ui_append(range,high);
    }
    cv_text(16,16,&FONT_XS,range,keyboard_ui_log_warning?C_AMB:C_GRAY);
    /* Physical keys above; applied union mask below. Inactive bank positions
     * are dark. No invented frequency is attached to a relative cell band. */
    for(i=0;i<27u;i++)cv_rect(16+(int32_t)i*7,35,6,9,(held&(UINT32_C(1)<<i))?C_HI:C_DIM);
    for(i=0;i<64u;i++){
        int selected=bands&&((i<32u?mask_lo:mask_hi)&(UINT32_C(1)<<(i&31u)));
        cv_rect(16+(int32_t)(i%16u)*12,47+(int32_t)(i/16u)*5,10,4,i>=cells?C_BLACK:selected?C_AMB:C_LINE);
    }
    seq_keyboard_record_text(text);
    cv_text(16,69,&FONT_XS,text,seq_recording?C_AMB:C_DIM);cv_blit(0,156);
}
