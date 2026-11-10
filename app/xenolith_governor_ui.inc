/* SPDX-License-Identifier: GPL-3.0-or-later */
static void governor_ui_onset(int32_t delta)
{
    governor_edit.onset=mod_ui_linear(governor_edit.onset,delta,.01f,0.0f,1.0f);
    governor_publish();redraw=1u;
}
static void governor_ui_span(int32_t delta)
{
    governor_edit.span=mod_ui_linear(governor_edit.span,delta,.01f,.05f,.50f);
}
static void governor_ui_turn(uint32_t knob,int32_t delta)
{
    if(governor_page==1u){
        if(knob==0u)governor_ui_span(delta);
        else if(knob==1u)governor_edit.filter_shape=mod_ui_linear(governor_edit.filter_shape,delta,.05f,0,2);
        else if(knob==2u)governor_edit.reverb_shape=mod_ui_linear(governor_edit.reverb_shape,delta,.05f,0,2);
        else {
            governor_edit.vca_bias=mod_ui_linear(governor_edit.vca_bias,delta,.025f,-1,1);
            /* Encoder arithmetic must reach the exact bypass, not a displayed
             * zero that keeps the automatic amplifier running indefinitely. */
            if(fm_fabsf(governor_edit.vca_bias)<.000001f)governor_edit.vca_bias=0.0f;
        }
    }else if(governor_page==2u){
        if(knob==0u){governor_ui_onset(delta);return;}
        else if(knob==1u)governor_ui_span(delta);
        else if(knob==2u)governor_edit.recovery_ms=mod_ui_log(governor_edit.recovery_ms,delta,20,2000,0);
        else governor_edit.erosion_curve=mod_ui_log(governor_edit.erosion_curve,delta,.25f,4,0);
    }else if(governor_page==3u){
        if(knob==0u)governor_edit.table_depth=mod_ui_linear(governor_edit.table_depth,delta,.025f,0,1);
        else if(knob==1u)governor_edit.table_loss=mod_ui_linear(governor_edit.table_loss,delta,.1f,0,5);
        else if(knob==2u)governor_edit.dither=mod_ui_linear(governor_edit.dither,delta,.025f,0,1);
        else governor_edit.erosion_curve=mod_ui_log(governor_edit.erosion_curve,delta,.25f,4,0);
    }else{
        if(knob==0u)governor_edit.cell_floor=mod_ui_linear(governor_edit.cell_floor,delta,.025f,.375f,1);
        else if(knob==1u)governor_edit.link_loss=mod_ui_linear(governor_edit.link_loss,delta,.025f,0,1);
        else if(knob==2u)governor_edit.clock_floor=mod_ui_linear(governor_edit.clock_floor,delta,.025f,.25f,1);
        else governor_edit.recovery_ms=mod_ui_log(governor_edit.recovery_ms,delta,20,2000,0);
    }
    governor_publish();redraw=1u;
}
static void governor_ui_bar(int y,const char *name,float value)
{
    uint32_t j,on=(uint32_t)(fm_clampf(value,0,1)*24.0f+.5f);
    cv_text(16,y,&FONT_XS,name,C_GRAY);
    for(j=0;j<24u;j++){
        int x=77+(int)j*6;
        uint32_t c=j<on?(j<16u?C_AMB:C_HI):C_DIM;
        cv_line(x,y+2,x+4,y+2,c);cv_line(x,y+9,x+4,y+9,c);
        if(j<on)cv_rect(x,y+3,5,6,c);
    }
}
static void governor_ui_value(uint32_t row,const char *label,float value,float scale)
{
    char text[24];number_text((int32_t)(value*scale+(value>=0?.5f:-.5f)),text);fe_ui_row(row,label,text);
}
static void governor_ui_draw(void)
{
    char title[24];
    number_text((int32_t)(governor_edit.onset*100+.5f),title);
    fe_ui_header(governor_page==1u?"[ LOAD ENGINE ]":governor_page==2u?"[ LOAD TIMING ]":
        governor_page==3u?"[ CORE EROSION ]":"[ CORE RELIEF ]",title);
    cv_begin(240,104,C_BLACK);
    if(governor_page==1u){
        governor_ui_value(0,"K1 SPAN %",governor_edit.span,100);
        governor_ui_value(1,"K2 FILTER %",governor_edit.filter_shape,100);
        governor_ui_value(2,"K3 REVERB %",governor_edit.reverb_shape,100);
        governor_ui_value(3,"K4 VCA BIAS %",governor_edit.vca_bias,100);
    }else if(governor_page==2u){
        governor_ui_value(0,"K1 ONSET %",governor_edit.onset,100);
        governor_ui_value(1,"K2 SPAN %",governor_edit.span,100);
        governor_ui_value(2,"K3 RECOVER ms",governor_edit.recovery_ms,1);
        governor_ui_value(3,"K4 CORE CURVE %",governor_edit.erosion_curve,100);
    }else if(governor_page==3u){
        governor_ui_value(0,"K1 INTERP LOSS %",governor_edit.table_depth,100);
        fixed_text(governor_edit.table_loss,title);fe_ui_row(1,"K2 ADDRESS LOSS",title);
        governor_ui_value(2,"K3 DITHER %",governor_edit.dither,100);
        governor_ui_value(3,"K4 CURVE %",governor_edit.erosion_curve,100);
    }else{
        governor_ui_value(0,"K1 CELL FLOOR %",governor_edit.cell_floor,100);
        governor_ui_value(1,"K2 LINK LOSS %",governor_edit.link_loss,100);
        governor_ui_value(2,"K3 CLOCK FLOOR %",governor_edit.clock_floor,100);
        governor_ui_value(3,"K4 RECOVER ms",governor_edit.recovery_ms,1);
    }
    cv_blit(0,36);cv_begin(240,100,C_BLACK);
    governor_ui_bar(12,"LOAD",governor_load_snapshot);
    governor_ui_bar(34,"SAG",governor_stress_snapshot);
    cv_line(16,60,224,60,C_LINE);
    cv_text(18,69,&FONT_XS,governor_page<3u?"FX > LUT > CLOCK":"LUT > CELLS > CLOCK",C_DIM);
    cv_blit(0,140);
}
