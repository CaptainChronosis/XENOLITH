/* SPDX-License-Identifier: MPL-2.0
 * Foreground response editors. HOME keeps its four performance controls. */
static const char *const home_response_name[]={"DEFAULT","LINEAR","EXP"};
static const char *const home_sensitivity_name[]={"0.25 X","0.5 X","1 X","2 X","4 X"};
static const char *const home_flavour_name[]={"NORMAL","RETURN -","RETURN +-","CASCADE"};
_Static_assert(sizeof home_flavour_name/sizeof *home_flavour_name==XFLAVOUR_COUNT,"Flavour labels must match engine");
static float home_glide_step(float value,int32_t delta)
{
    static const float times[]={0,5,10,20,35,50,75,100,150,200,300,400,600,800,1000,1500,2000};
    delta=clamp(delta,-17,17);
    while(delta){uint32_t i;
        if(delta>0){for(i=0;i<17u;i++)if(times[i]>value){value=times[i];break;}--delta;}
        else{for(i=17u;i>0u;i--)if(times[i-1u]<value){value=times[i-1u];break;}++delta;}
    }
    return value;
}

static float home_ratio_step(float value,int32_t delta)
{
    static const float ratios[]={.0001f,.00025f,.0005f,.001f,.002f,.004f,.008f,.016f,.03125f,
        .0625f,.125f,.25f,.333333333f,.5f,.666666667f,.75f,1.0f,1.25f,1.333333333f,1.5f,
        1.666666667f,2.0f,2.5f,3.0f,4.0f,5.0f,6.0f,8.0f,12.0f,16.0f,24.0f,32.0f,48.0f,64.0f};
    if(!performance_edit.ratio_snap)return xcontrol_home_step(XCTRL_RATIO,value,delta,
        performance_edit.curve[2],performance_edit.sensitivity[2],0u);
    delta=clamp(delta,-128,128);
    while(delta){
        if(performance_edit.ratio_snap==1u){
            float p=12.0f*fm_log2f(value)+.0001f;int32_t whole=(int32_t)p;
            if((float)whole>p)--whole;
            if(delta>0)value=fm_exp2f((float)(whole+1)/12.0f);
            else value=fm_exp2f((float)(p-(float)whole<.0003f?whole-1:whole)/12.0f);
        }else{
            uint32_t i;
            if(delta>0){for(i=0;i<sizeof ratios/sizeof *ratios;i++)if(ratios[i]>value+value*.00001f){value=ratios[i];break;}}
            else{for(i=sizeof ratios/sizeof *ratios;i>0;i--)if(ratios[i-1]<value-value*.00001f){value=ratios[i-1];break;}}
        }
        value=fm_clampf(value,XENOLITH_RATIO_MIN,XENOLITH_RATIO_MAX);
        delta+=delta>0?-1:1;
    }
    return value;
}
static void home_detail_turn(uint32_t knob,int32_t delta)
{
    uint32_t p=home_detail_page-1u;
    if(knob==0u)performance_edit.curve[p]=mod_ui_enum(performance_edit.curve[p],delta,p==0u?2u:XCTRL_CURVE_COUNT);
    else if(knob==1u)performance_edit.sensitivity[p]=mod_ui_enum(performance_edit.sensitivity[p],delta,XCTRL_SENSITIVITY_COUNT);
    else if(p==0u){
        if(knob==2u)performance_edit.glide_ms=home_glide_step(performance_edit.glide_ms,delta);
        else performance_edit.voices=(uint32_t)clamp((int32_t)performance_edit.voices+clamp(delta,-8,8),1,8);
    }else if(p==1u){
        if(knob==2u){fm_index=xcontrol_home_step(XCTRL_INDEX,fm_index,delta,performance_edit.curve[p],performance_edit.sensitivity[p],topology_edit.mode==XTOPO_EXPONENTIAL);
            seq_capture_control(topology_edit.mode==XTOPO_EXPONENTIAL?XSEQ_DEST_EXP_DEPTH:XSEQ_DEST_INDEX,fm_index);}
        else{flavour_edit.flavour[topology_edit.mode]=mod_ui_enum(flavour_edit.flavour[topology_edit.mode],delta,XFLAVOUR_COUNT);calculation_serial++;}
    }else if(p==2u){
        if(knob==2u){fm_ratio=home_ratio_step(fm_ratio,delta);seq_capture_control(XSEQ_DEST_RATIO,fm_ratio);}
        else performance_edit.ratio_snap=mod_ui_enum(performance_edit.ratio_snap,delta,3u);
    }else{
        if(knob==2u){drift_amount=xcontrol_home_step(XCTRL_DRIFT,drift_amount,delta,performance_edit.curve[p],performance_edit.sensitivity[p],0u);seq_capture_control(XSEQ_DEST_DRIFT,drift_amount);}
        else performance_edit.spread=mod_ui_linear(performance_edit.spread,delta,.05f,0.0f,2.0f);
    }
    performance_serial++;keyboard_dirty=1u;digital_publish();redraw=1u;
}
static void home_detail_box(int x,int y,int w,int h,uint16_t ink)
{
    cv_line(x,y,x+w,y,ink);cv_line(x,y+h,x+w,y+h,ink);
    cv_line(x,y,x,y+h,ink);cv_line(x+w,y,x+w,y+h,ink);
    cv_line(x+2,y+h+2,x+w+2,y+h+2,C_DIM);cv_line(x+w+2,y+2,x+w+2,y+h+2,C_DIM);
}
static void home_detail_wave(int x,int y,int pulse,uint16_t ink)
{
    static const int8_t sine[]={0,-3,-5,-6,-5,-3,0,3,5,6,5,3,0};uint32_t i;
    if(pulse){cv_line(x,y+5,x+5,y+5,ink);cv_line(x+5,y-5,x+5,y+5,ink);cv_line(x+5,y-5,x+17,y-5,ink);cv_line(x+17,y-5,x+17,y+5,ink);cv_line(x+17,y+5,x+24,y+5,ink);}
    else for(i=1;i<13u;i++)cv_line(x+(int)(i-1u)*2,y+sine[i-1u],x+(int)i*2,y+sine[i],ink);
}
/* Show the live frontier and its compiled links. Phase ticks are available
 * only for the published first eight cells; later cells stay unanimated. */
static void home_detail_fm_diagram(void)
{
    XenolithGraph graph;GaugeSnapshot sample;uint32_t serial=gauge_serial;
    uint32_t count=active_cells>64u?64u:active_cells,cell=count?count-1u:0u;
    uint32_t mod_phase=0u,car_phase=0u,valid=0u,car_valid=cell<8u&&cell<phase_cells;char text[16];
    uint16_t ink=topology_ui_phase_ink(cell,C_AMB);
    if(car_valid)car_phase=phase_published[cell];
    if(cell<8u&&!(serial&1u)){X_PUBLISH_BARRIER();sample=gauge_published;X_PUBLISH_BARRIER();if(serial==gauge_serial&&cell<sample.cells){mod_phase=sample.phase[cell];valid=1u;}}
    xenolith_graph_compile(&graph_edit,topology_edit.growth,count,&graph);
    cv_line(8,5,231,5,C_LINE);cv_line(8,94,231,94,C_LINE);
    cv_line(8,5,8,94,C_LINE);cv_line(231,5,231,94,C_LINE);
    cv_rect(18,0,62,15,C_BLACK);mod_ui_copy(text,"CELL 00");text[5]=(char)('0'+(cell+1u)/10u);text[6]=(char)('0'+(cell+1u)%10u);cv_text(20,0,&FONT_XS,text,C_AMB);
    cv_rect(151,0,76,15,C_BLACK);cv_text(224-text_w(&FONT_XS,TOPO_UI_MODE[topology_edit.mode]),0,&FONT_XS,TOPO_UI_MODE[topology_edit.mode],C_DIM);
    cv_text(25,17,&FONT_XS,"MOD",C_DIM);cv_text(161,17,&FONT_XS,"CAR",C_DIM);
    home_detail_box(20,33,34,26,C_LINE);home_detail_box(87,33,38,26,C_LINE);home_detail_box(160,33,34,26,C_LINE);
    home_detail_wave(25,46,topology_edit.mode==XTOPO_PULSE,ink);home_detail_wave(165,46,0,ink);
    cv_line(57,46,83,46,ink);cv_line(79,43,83,46,ink);cv_line(79,49,83,46,ink);
    cv_line(128,46,156,46,ink);cv_line(152,43,156,46,ink);cv_line(152,49,156,46,ink);
    cv_line(197,46,223,46,ink);cv_line(219,43,223,46,ink);cv_line(219,49,223,46,ink);
    if(topology_edit.mode==XTOPO_PHASE)cv_text(100,37,&FONT_XS,"PH",C_HI);
    else if(topology_edit.mode==XTOPO_EXPONENTIAL)cv_text(100,37,&FONT_XS,"2^",C_HI);
    else if(topology_edit.mode==XTOPO_WARP){cv_line(96,52,104,50,C_HI);cv_line(104,50,108,39,C_HI);cv_line(108,39,117,37,C_HI);}
    else{cv_line(99,46,113,46,C_HI);cv_line(106,39,106,53,C_HI);}
    if(valid&&envelope_level>0.0001f)cv_rect(22+(int)((mod_phase>>24)&31u),62,2,2,ink);
    if(car_valid&&envelope_level>0.0001f)cv_rect(162+(int)((car_phase>>24)&31u),62,2,2,ink);
    if(graph.parent[cell]<count){
        cv_line(72,80,99,80,C_LINE);cv_line(99,63,99,80,C_LINE);cv_line(96,67,99,63,C_LINE);cv_line(102,67,99,63,C_LINE);
        text[0]='C';text[1]=(char)('0'+(graph.parent[cell]+1u)/10u);text[2]=(char)('0'+(graph.parent[cell]+1u)%10u);text[3]=0;cv_text(47,72,&FONT_XS,text,C_AMB);
    }
    if(graph.feedback[cell]<count){
        topology_ui_segment(113,63,113,80,C_AMB,1);topology_ui_segment(113,80,141,80,C_AMB,1);
        text[0]='C';text[1]=(char)('0'+(graph.feedback[cell]+1u)/10u);text[2]=(char)('0'+(graph.feedback[cell]+1u)%10u);text[3]=0;cv_text(144,72,&FONT_XS,text,C_AMB);cv_text(179,72,&FONT_XS,"z-1",C_DIM);
    }
}
static void home_detail_draw(void)
{
    static const char *const titles[]={"[ TONIC ]","[ INDEX ]","[ RATIO ]","[ DRIFT ]"};
    static const char *const snap[]={"OFF","12-TET","RATIOS"};
    uint32_t p=home_detail_page-1u,i;char text[32];
    fe_ui_header(titles[p],"CONTROL");cv_begin(240,104,C_BLACK);
    fe_ui_row(0,"K1 RESPONSE",p==0u&&performance_edit.curve[0]==0u?"EXP":home_response_name[performance_edit.curve[p]]);
    fe_ui_row(1,"K2 SENSITIVITY",home_sensitivity_name[performance_edit.sensitivity[p]]);
    if(!p){
        fixed_text(performance_edit.glide_ms,text);fe_ui_row(2,"K3 GLIDE MS",text);
        if(performance_edit.voices==1u)mod_ui_copy(text,"MONO");else{text[0]=(char)('0'+performance_edit.voices);text[1]=0;}
        fe_ui_row(3,"K4 PARAPHONY",text);
    }else if(p==1u){fixed_text(fm_index,text);fe_ui_row(2,topology_edit.mode==XTOPO_EXPONENTIAL?"K3 DEPTH ST":"K3 INDEX",text);fe_ui_row(3,"K4 VARIANT",home_flavour_name[flavour_edit.flavour[topology_edit.mode]]);}
    else if(p==2u){mod_ui_number(fm_ratio,text);fe_ui_row(2,"K3 RATIO",text);fe_ui_row(3,"K4 SNAP",snap[performance_edit.ratio_snap]);}
    else{fixed_text(drift_amount,text);fe_ui_row(2,"K3 INTENSITY",text);fixed_text(performance_edit.spread,text);fe_ui_row(3,"K4 SPREAD X",text);}
    cv_blit(0,36);cv_begin(240,100,C_BLACK);
    cv_line(16,18,224,18,C_LINE);cv_line(16,74,224,74,C_LINE);
    if(!p){
        uint32_t available=paraphony_supported()?performance_edit.voices:1u;
        if(available>requested_cells)available=requested_cells;
        for(i=0;i<performance_edit.voices;i++){
            int32_t x=22+(int32_t)i*25;cv_rect(x,36,17,17,i<available?C_LINE:C_DIM);
            if(i<available)cv_rect(x+3,39,11,11,C_HI);
        }
        mod_ui_copy(text,paraphony_supported()?"SHARED ADSR / FX":"MONO IN THIS ROUTE");
        cv_text(16,80,&FONT_XS,text,C_GRAY);
    }else if(p==1u){
        cv_rect(0,0,240,100,C_BLACK);home_detail_fm_diagram();
    }else if(p==3u){
        for(i=0;i<17u;i++){int32_t x=120+((int32_t)i-8)*(int32_t)(2.0f+4.0f*performance_edit.spread);
            int32_t height=3+(int32_t)(12.0f*drift_amount);cv_line(x,47-height,x,47+height,i==8u?C_HI:C_AMB);}
        cv_text(16,80,&FONT_XS,"SPREAD / WANDER",C_GRAY);
    }else{
        for(i=0;i<24u;i++){
            float a=xcontrol_home_step(p,p==1u?0.0f:1.0f,(int32_t)i,performance_edit.curve[p],performance_edit.sensitivity[p],topology_edit.mode==XTOPO_EXPONENTIAL);
            float b=xcontrol_home_step(p,p==1u?0.0f:1.0f,23,performance_edit.curve[p],performance_edit.sensitivity[p],topology_edit.mode==XTOPO_EXPONENTIAL);
            int32_t h=(int32_t)(42.0f*a/(b>0?b:1.0f));cv_rect(20+(int32_t)i*8,69-h,5,h+1,C_LINE);
        }
        cv_text(16,80,&FONT_XS,p==2u&&performance_edit.ratio_snap?"SNAP: ONE INTERVAL / TURN":"ENCODER RESPONSE",C_GRAY);
    }
    cv_blit(0,140);
}
