/* SPDX-License-Identifier: MPL-2.0
 * Foreground circuit and lookup editors. Audio receives complete snapshots. */
static void deep_ui_turn(uint32_t knob,int32_t delta)
{
    if(output_page==4u){
        if(knob==0u)circuit_edit.cathode_ohms=mod_ui_log(circuit_edit.cathode_ohms,delta,470,10000,0);
        else if(knob==1u)circuit_edit.headroom=mod_ui_log(circuit_edit.headroom,delta,.0625f,1,0);
        else if(knob==2u)circuit_edit.coupling_hz=mod_ui_log(circuit_edit.coupling_hz,delta,.5f,100,0);
        else circuit_edit.balance=mod_ui_linear(circuit_edit.balance,delta,.01f,.5f,1);
        filter_publish();
    }else{
        XenolithCalculationConfig old=calculation_edit;
        if(knob==0u)calculation_edit.table_bits=(uint32_t)clamp((int32_t)calculation_edit.table_bits+clamp(delta,-11,11),2,11);
        else if(knob==1u)calculation_edit.interpolation=mod_ui_enum(calculation_edit.interpolation,delta,XCALC_INTERPOLATION_COUNT);
        else if(knob==2u)calculation_edit.harmonic=grid_step(calculation_edit.harmonic,delta,40,0,1);
        else calculation_edit.target=mod_ui_enum(calculation_edit.target,delta,XCALC_TARGET_COUNT);
        if(old.table_bits!=calculation_edit.table_bits||old.interpolation!=calculation_edit.interpolation||
           old.harmonic!=calculation_edit.harmonic||old.target!=calculation_edit.target){
            calculation_serial++;digital_publish();
        }
    }
    redraw=1u;
}
static void deep_ui_draw(void)
{
    char text[24];uint32_t i;
    if(output_page==4u){
        fe_ui_header("[ CIRCUIT ]",filter_edit.placement?"ON":"OFF");cv_begin(240,104,C_BLACK);
        number_text((int32_t)(circuit_edit.cathode_ohms+.5f),text);fe_ui_row(0,"K1 CATHODE R",text);
        fixed_text(circuit_edit.headroom,text);fe_ui_row(1,"K2 HEADROOM",text);
        fixed_text(circuit_edit.coupling_hz,text);fe_ui_row(2,"K3 COUPLING Hz",text);
        fixed_text(circuit_edit.balance,text);fe_ui_row(3,"K4 T BALANCE",text);cv_blit(0,36);
        cv_begin(240,100,C_BLACK);
        cv_line(20,35,220,35,C_LINE);cv_line(48,35,48,70,C_AMB);cv_line(192,35,192,70,C_AMB);
        cv_line(48,70,192,70,C_AMB);cv_line(120,35,120,55,C_HI);
        cv_line(109,55,131,55,C_HI);cv_line(109,60,131,60,C_HI);cv_line(120,60,120,70,C_HI);
        for(i=0;i<2u;i++){int x=73+(int)i*75;cv_rect(x,29,19,12,C_BLACK);cv_line(x,29,x+18,29,C_AMB);
            cv_line(x,41,x+18,41,C_AMB);cv_line(x,29,x,41,C_AMB);cv_line(x+18,29,x+18,41,C_AMB);}
        cv_text(18,7,&FONT_XS,"[ R / C ]",C_DIM);cv_text(158,7,&FONT_XS,"PARALLEL T",C_GRAY);
        cv_blit(0,140);
    }else{
        static const char *const targets[]={"MOD","CARRIER","BOTH"};
        fe_ui_header("[ LOOKUP ]",xenolith_calculation_active(&calculation_edit)?"ON":"SINE");
        cv_begin(240,104,C_BLACK);number_text(1u<<calculation_edit.table_bits,text);fe_ui_row(0,"K1 TABLE",text);
        fe_ui_row(1,"K2 INTERP",calculation_edit.interpolation?"LINEAR":"ZOH");
        fixed_text(calculation_edit.harmonic,text);fe_ui_row(2,"K3 HARMONIC",text);
        fe_ui_row(3,"K4 TARGET",targets[calculation_edit.target]);cv_blit(0,36);
        cv_begin(240,100,C_BLACK);cv_text(18,7,&FONT_XS,"[ TABLE ]",C_DIM);
        cv_line(18,85,222,85,C_LINE);
        /* Schematic one-period preview; no audio state or table writes. */
        {int32_t px=18,py=51;uint32_t points=1u<<calculation_edit.table_bits;
            for(i=0;i<=200u;i++){
                float phase=(float)i*.005f*(float)points;uint32_t index=(uint32_t)phase;
                float y=fm_sinf(6.28318530718f*(float)index/(float)points);
                if(calculation_edit.interpolation){float next=fm_sinf(6.28318530718f*(float)(index+1u)/(float)points);
                    y+=(next-y)*(phase-(float)index);}
                y+=calculation_edit.harmonic*((3.0f*y-4.0f*y*y*y)-y);
                {int32_t x=18+(int32_t)i,sy=51-(int32_t)(27.0f*y);if(i)cv_line(px,py,x,sy,C_AMB);px=x;py=sy;}
            }
        }
        cv_blit(0,140);
    }
}
