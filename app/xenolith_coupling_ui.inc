/* SPDX-License-Identifier: GPL-2.0-or-later
 * Foreground controls for optional local-modulator coupling transforms. */
#include "xenolith_coupling.h"

static const char *const COUPLING_UI_METHOD[]={"OFF","BIAS","LAG","ALLPASS","COMMON BUS"};
_Static_assert(sizeof COUPLING_UI_METHOD/sizeof *COUPLING_UI_METHOD==XCOUPLING_COUNT,"Coupling labels must match engine");

static int coupling_ui_turn(int role,int32_t step)
{
    XenolithCouplingConfig before,next;
    if(topology_page!=2u||role<EN_K1||role>EN_K4)return 0;
    if(!step)return 1;
    before=next=coupling_edit;
    if(role==EN_K1)next.method=mod_ui_enum(next.method,step,XCOUPLING_COUNT);
    else if(role==EN_K2)next.depth=mod_ui_linear(next.depth,step,.025f,0.0f,1.0f);
    else if(role==EN_K3)next.parameter=mod_ui_linear(next.parameter,step,.01f,0.0f,1.0f);
    else next.bias=mod_ui_linear(next.bias,step,.025f,-1.0f,1.0f);
    if(xenolith_coupling_valid(&next)&&(before.method!=next.method||before.depth!=next.depth||before.parameter!=next.parameter||before.bias!=next.bias)){
        coupling_edit=next;calculation_serial++;digital_publish();
    }
    redraw=1u;return 1;
}

static void coupling_ui_box(int x,int y,int w,int h,const char *text,uint16_t ink)
{
    cv_line(x,y,x+w,y,ink);cv_line(x,y+h,x+w,y+h,ink);
    cv_line(x,y,x,y+h,ink);cv_line(x+w,y,x+w,y+h,ink);
    cv_text(x+(w-text_w(&FONT_XS,text))/2,y+(h-FONT_XS.h)/2,&FONT_XS,text,ink);
}

static void coupling_ui_diagram(void)
{
    uint32_t method=coupling_edit.method;
    int active=xenolith_coupling_active(&coupling_edit);
    uint16_t ink=active?C_AMB:C_DIM;
    const char *operation=method==XCOUPLING_BIAS?"MAP":method==XCOUPLING_LAG?"LP":method==XCOUPLING_ALLPASS?"AP":method==XCOUPLING_COMMON?"SUM":"WIRE";
    cv_line(8,8,231,8,C_LINE);cv_line(8,76,231,76,C_LINE);
    cv_line(8,8,8,76,C_LINE);cv_line(231,8,231,76,C_LINE);
    coupling_ui_box(18,28,36,25,"MOD",C_LINE);
    coupling_ui_box(94,28,40,25,operation,ink);
    coupling_ui_box(179,28,36,25,"FM",C_LINE);
    cv_line(55,40,92,40,C_LINE);cv_line(135,40,177,40,ink);
    cv_line(216,40,225,40,C_LINE);
    cv_text(77,32,&FONT_XS,">",C_LINE);cv_text(164,32,&FONT_XS,">",ink);
    /* Dry and transformed paths meet at the depth-controlled sum. */
    if(method!=XCOUPLING_OFF){
        cv_line(66,40,66,18,C_DIM);cv_line(66,18,153,18,C_DIM);cv_line(153,18,153,40,C_DIM);
        cv_rect(151,38,5,5,ink);
    }
    if(method==XCOUPLING_COMMON){
        cv_text(20,58,&FONT_XS,"SUM/N z-1",ink);
        cv_line(79,63,114,63,ink);cv_line(114,54,114,63,ink);
        cv_text(121,56,&FONT_XS,"-",ink);
    }else if(method==XCOUPLING_LAG||method==XCOUPLING_ALLPASS){
        cv_text(28,58,&FONT_XS,"STATE z-1",ink);
        cv_line(89,63,114,63,ink);cv_line(114,54,114,63,ink);
    }else if(method==XCOUPLING_BIAS){
        cv_text(33,58,&FONT_XS,"BIAS",ink);
        cv_line(63,63,114,63,ink);cv_line(114,54,114,63,ink);
    }
    cv_text(16,82,&FONT_XS,"EDIT <> ARRAY",C_DIM);
}

static void coupling_ui_draw(void)
{
    char text[24];const char *parameter_label="K3 PARAMETER";float value=coupling_edit.parameter;
    fe_ui_header("[ COUPLING ]",TOPO_UI_MODE[topology_edit.mode]);
    cv_begin(240,104,C_BLACK);
    fe_ui_row(0,"K1 METHOD",COUPLING_UI_METHOD[coupling_edit.method]);
    fixed_text(coupling_edit.depth*100.0f,text);fe_ui_row(1,"K2 DEPTH %",text);
    if(coupling_edit.method==XCOUPLING_BIAS){parameter_label="K3 DRIVE X";value=xenolith_coupling_drive(coupling_edit.parameter);}
    else if(coupling_edit.method==XCOUPLING_LAG||coupling_edit.method==XCOUPLING_COMMON){parameter_label="K3 TIME MS";value=xenolith_coupling_time_ms(coupling_edit.parameter);}
    else if(coupling_edit.method==XCOUPLING_ALLPASS){parameter_label="K3 DELAY SAMP";value=xenolith_coupling_delay(coupling_edit.parameter);}
    if(coupling_edit.method==XCOUPLING_OFF)mod_ui_copy(text,"--");else fixed_text(value,text);
    fe_ui_row(2,parameter_label,text);fixed_text(coupling_edit.bias*100.0f,text);fe_ui_row(3,"K4 BIAS %",text);
    cv_blit(0,36);cv_begin(240,100,C_BLACK);coupling_ui_diagram();cv_blit(0,140);
}
