/* SPDX-License-Identifier: MPL-2.0
 * Foreground-only four-knob filter and assignable-envelope panels. */
static const uint8_t ENV_DESTINATIONS[]={XM_DST_NONE,XM_DST_PITCH,XM_DST_INDEX,XM_DST_RATIO,
    XM_DST_DRIFT,XM_DST_FILTER_CUTOFF,XM_DST_FILTER_RESONANCE,XM_DST_AMPLITUDE,XM_DST_WET_MIX,XM_DST_BITS,XM_DST_FOLD};
static const char *const ENV_NAMES[]={"AMP","FILTER","INDEX","TONIC"};
static void env_ui_turn(uint32_t knob,int32_t delta)
{
    XenolithEnvelopeRoute *e;
    if(!envelope_selection){
        if(envelope_page==4u)return;
        if(knob==0u)envelope_attack=mod_ui_linear(envelope_attack,delta,5.0f,0,10000);
        else if(knob==1u)envelope_decay=mod_ui_linear(envelope_decay,delta,10.0f,0,10000);
        else if(knob==2u)envelope_sustain=mod_ui_linear(envelope_sustain,delta,.05f,0,1);
        else envelope_release=mod_ui_linear(envelope_release,delta,10.0f,0,10000);
        envelope_publish();return;
    }
    e=&envbank_edit.envelope[envelope_selection-1u];
    if(envelope_page==4u){
        if(knob==0u){uint32_t old=e->destination;
            e->destination=mod_ui_choose(e->destination,delta,ENV_DESTINATIONS,sizeof ENV_DESTINATIONS);
            if(old!=e->destination)e->depth=0.0f;
        }else if(knob==1u){XenolithModDestination d=xenolith_mod_destination(e->destination);
            e->depth=mod_ui_linear(e->depth,delta,d.unit==XM_UNIT_BITS?1.0f:d.unit==XM_UNIT_SEMITONES?.25f:d.unit==XM_UNIT_OCTAVES?.125f:.05f,-d.offset_limit,d.offset_limit);
        }else if(knob==2u)e->trigger=mod_ui_enum(e->trigger,delta,XENV_TRIGGER_COUNT);
        else e->enabled=delta>0?1u:0u;
    }else{
        if(knob==0u)e->attack_ms=mod_ui_log(e->attack_ms,delta,.5f,10000,1);
        else if(knob==1u)e->decay_ms=mod_ui_log(e->decay_ms,delta,.5f,10000,1);
        else if(knob==2u)e->sustain=mod_ui_linear(e->sustain,delta,.05f,0,1);
        else e->release_ms=mod_ui_log(e->release_ms,delta,.5f,10000,1);
    }
    envbank_publish();
}
static float filter_ui_cutoff_step(float value,int32_t delta)
{
    float ceiling=filter_edit.placement==XFILTER_PRE_DIGITAL?
        fm_clampf(filter_rate_snapshot*.45f,20.0f,20000.0f):20000.0f;
    if(!delta)return value;
    /* A reduced clock retains the request until the operator turns K1.
     * That gesture starts at the audible ceiling, without hidden wind-up. */
    if(value>ceiling)value=ceiling;
    value=xcontrol_filter_step(value,delta,performance_edit.filter_response);
    return value>ceiling?ceiling:value;
}
static void filter_ui_turn(uint32_t knob,int32_t delta)
{
    if(knob==0u){filter_edit.cutoff_hz=filter_ui_cutoff_step(filter_edit.cutoff_hz,delta);
        seq_capture_control(XSEQ_DEST_FILTER_CUTOFF,filter_edit.cutoff_hz);}
    else if(knob==1u)filter_edit.mode=mod_ui_enum(filter_edit.mode,delta,XFILTER_MODE_COUNT);
    else if(knob==2u)filter_edit.slope=mod_ui_enum(filter_edit.slope,delta,XFILTER_SLOPE_COUNT);
    else {filter_edit.resonance=mod_ui_linear(filter_edit.resonance,delta,.025f,0,1);
        seq_capture_control(XSEQ_DEST_FILTER_RESONANCE,filter_edit.resonance);}
    filter_publish();
}
static void fe_ui_header(const char *title,const char *state)
{
    cv_begin(240,36,C_BLACK);draw_panel_header("[ X E N O L I T H ]");
    cv_text(16,21,&FONT_XS,title,C_AMB);cv_text(226-text_w(&FONT_XS,state),21,&FONT_XS,state,C_HI);cv_blit(0,0);
}
static void fe_ui_row(uint32_t row,const char *label,const char *value)
{
    int32_t y=(int32_t)row*25;cv_text(16,y+7,&FONT_XS,label,C_GRAY);
    cv_text(224-text_w(&FONT_B,value),y+3,&FONT_B,value,C_HI);
    if(row<3u)cv_line(8,y+24,12,y+24,C_LINE);
}
static void filter_ui_draw(void)
{
    static const char *const place[]={"OFF","PRE DIGITAL","POST DIGITAL","POST FX","PRE FX"};
    static const char *const modes[]={"LP","BP","HP","NOTCH","PHASE","TWIN T"};
    static const char *const response[]={"FINE","SWEEP","FAST","LINEAR"};
    char text[24];uint32_t i;
    fe_ui_header("[ FILTER ]",place[filter_edit.placement]);cv_begin(240,104,C_BLACK);
    fixed_text(filter_edit.placement?filter_cutoff_snapshot:filter_edit.cutoff_hz,text);fe_ui_row(0,filter_edit.mode==XFILTER_TWIN_T?"K1 CENTRE HZ":"K1 CUTOFF HZ",text);
    fe_ui_row(1,"K2 MODE",modes[filter_edit.mode]);fe_ui_row(2,"K3 TOPOLOGY",filter_edit.slope==XFILTER_BRIDGE?"BRIDGE":filter_edit.mode==XFILTER_TWIN_T?(filter_edit.slope?"TWO":"ONE"):(filter_edit.slope?"4 POLE":"2 POLE"));
    fixed_text(filter_edit.placement?filter_resonance_snapshot:filter_edit.resonance,text);fe_ui_row(3,"K4 RESONANCE",text);cv_blit(0,36);
    cv_begin(240,100,C_BLACK);
    if(filter_edit.placement==XFILTER_PRE_DIGITAL&&
       filter_cutoff_snapshot>=fm_clampf(filter_rate_snapshot*.45f,20.0f,20000.0f)*.999f)
        cv_text(16,1,&FONT_XS,"RATE LIMIT",C_DIM);
    cv_line(162,0,224,0,C_LINE);cv_line(162,16,224,16,C_LINE);cv_line(162,0,162,16,C_LINE);cv_line(224,0,224,16,C_LINE);
    cv_text(193-text_w(&FONT_XS,response[performance_edit.filter_response])/2,1,&FONT_XS,response[performance_edit.filter_response],C_HI);
    cv_line(16,46,224,46,C_LINE);
    {static const char *const node[]={"CORE","DIGITAL","VCA","FX"};
        for(i=0;i<4u;i++){int32_t x=14+(int32_t)i*56;cv_rect(x,32,48,25,RGB(24,4,4));cv_text(x+3,39,&FONT_XS,node[i],C_GRAY);}}
    if(filter_edit.placement){int32_t x=filter_edit.placement==XFILTER_PRE_DIGITAL?62:filter_edit.placement==XFILTER_POST_DIGITAL?118:filter_edit.placement==XFILTER_PRE_FX?174:222;
        cv_line(x,21,x,66,C_HI);cv_text(x>200?198:x-8,71,&FONT_XS,"FLT",C_HI);}
    cv_blit(0,140);
}
static void env_ui_draw(void)
{
    char text[24],title[24];uint32_t i;float a,d,s,r,level;
    const XenolithEnvelopeRoute *e=envelope_selection?&envbank_edit.envelope[envelope_selection-1u]:0;
    mod_ui_copy(title,"[ ENV ");{uint32_t n=6u,k=0u;while(ENV_NAMES[envelope_selection][k])title[n++]=ENV_NAMES[envelope_selection][k++];title[n++]=' ';title[n++]=']';title[n]=0;}
    level=envelope_selection?envbank_level_snapshot[envelope_selection-1u]:envelope_level;
    a=e?e->attack_ms:envelope_attack;d=e?e->decay_ms:envelope_decay;s=e?e->sustain:envelope_sustain;r=e?e->release_ms:envelope_release;
    fe_ui_header(title,envelope_page==4u?"ROUTE":e?(e->enabled?"ON":"OFF"):"AMPLITUDE");
    cv_begin(240,104,C_BLACK);
    if(envelope_page==4u){
        static const char *const triggers[]={"KEY","SEQ NOTE","KEY + SEQ"};
        XenolithModDestination desc=xenolith_mod_destination(e?e->destination:XM_DST_AMPLITUDE);
        fe_ui_row(0,e?"K1 DEST":"DEST",e?mod_ui_destination_name(e->destination):"AMPLITUDE");
        fixed_text(e?e->depth:1.0f,text);fe_ui_row(1,desc.unit==XM_UNIT_OCTAVES?"K2 DEPTH OCT":desc.unit==XM_UNIT_SEMITONES?"K2 DEPTH ST":desc.unit==XM_UNIT_BITS?"K2 DEPTH BITS":"K2 DEPTH",text);
        fe_ui_row(2,"K3 TRIGGER",e?triggers[e->trigger]:"KEY + SEQ");fe_ui_row(3,"K4 ENABLE",e?(e->enabled?"ON":"OFF"):"FIXED ON");
    }else{fixed_text(a,text);fe_ui_row(0,"K1 ATTACK MS",text);fixed_text(d,text);fe_ui_row(1,"K2 DECAY MS",text);
        fixed_text(s*100.0f,text);fe_ui_row(2,"K3 SUSTAIN %",text);fixed_text(r,text);fe_ui_row(3,"K4 RELEASE MS",text);}
    cv_blit(0,36);cv_begin(240,100,C_BLACK);
    cv_line(16,76,222,76,C_LINE);cv_line(16,76,48,20,C_AMB);cv_line(48,20,90,48,C_AMB);
    cv_line(90,48,158,48,C_AMB);cv_line(158,48,218,76,C_AMB);
    cv_rect(16,86,(int32_t)(206.0f*fm_clampf(level,0,1)),4,C_HI);
    for(i=0;i<4u;i++)cv_text(16+(int32_t)i*55,3,&FONT_XS,ENV_NAMES[i],i==envelope_selection?C_HI:C_DIM);
    cv_blit(0,140);
}
