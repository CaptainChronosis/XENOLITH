/* SPDX-License-Identifier: GPL-3.0-or-later
 * Foreground ARRAY controls. Include after mod UI helpers; parent publishes
 * validated topology_edit under its structural audio transition gate.
 */
#include "xenolith_core.h"
#include "xenolith_graph.h"

static const char *const TOPO_UI_GROWTH[]={"PARALLEL","CHAIN4","BRANCH","LOOP4","SELF"};
static const char *const TOPO_UI_ADD[]={"PARALLEL","SERIES","BRANCH","LOOP","SELF"};
static const char *const TOPO_UI_MODE[]={"LINEAR","PHASE","EXP","PULSE FM","PHASE WARP"};
_Static_assert(sizeof TOPO_UI_MODE/sizeof *TOPO_UI_MODE==XTOPO_MODE_COUNT,"Mode labels must match engine");
_Static_assert(sizeof TOPO_UI_GROWTH/sizeof *TOPO_UI_GROWTH==XTOPO_GROWTH_COUNT,"Growth labels must match engine");
/* Foreground monotone grid, including off-grid recalled values. Endpoints
 * remain exact after separate inverse detents; no sample-loop math here. */
static float topology_ui_grid(float value,int32_t step,unsigned divisions,float high)
{
    int32_t count=step>64?64:step< -64?-64:step;
    while(count){
        float units=value*(float)divisions;int32_t base=(int32_t)units;
        if(count>0){value=(float)(base+1)/(float)divisions;--count;}
        else{if(units-(float)base<0.00001f)--base;value=(float)base/(float)divisions;++count;}
        value=mod_ui_bound(value,0.0f,high);
    }
    return value;
}
static int topology_ui_turn(int role,int32_t step)
{
    XenolithTopologyConfig before,next;XenolithGraph graph;uint32_t kind;
    if(topology_page!=1u||role<EN_K1||role>EN_K4)return 0;
    if(!step)return 1;
    if(role==EN_K1&&graph_edit.scope==XGRAPH_ADD){
        graph_edit.next_kind=mod_ui_enum(graph_edit.next_kind,step,XTOPO_GROWTH_COUNT);
        graph_next_kind_changed();
        redraw=1u;return 1;
    }
    before=next=topology_edit;
    xenolith_graph_compile(&graph_edit,next.growth,active_cells,&graph);
    kind=graph_edit.scope==XGRAPH_ADD?graph_edit.next_kind:next.growth;
    if(role==EN_K1)next.growth=mod_ui_enum(next.growth,step,XTOPO_GROWTH_COUNT);
    else if(role==EN_K2)next.mode=mod_ui_enum(next.mode,step,XTOPO_MODE_COUNT);
    else if(role==EN_K3&&(graph.edge_count||kind==XTOPO_CHAIN4||kind==XTOPO_BRANCH||kind==XTOPO_LOOP4||(graph_edit.scope==XGRAPH_ADD&&kind==XTOPO_SELF&&graph_edit.attachment)))next.link=topology_ui_grid(next.link,step,10u,4.0f);
    else if(role==EN_K4&&(graph.feedback_count||kind==XTOPO_LOOP4||kind==XTOPO_SELF))next.feedback=topology_ui_grid(next.feedback,step,20u,0.95f);
    if(xenolith_topology_valid(&next)&&(before.growth!=next.growth||before.mode!=next.mode||before.link!=next.link||before.feedback!=next.feedback)){
        if(role==EN_K1&&before.growth!=next.growth)graph_global_dirty=1u;
        topology_edit=next;topology_publish();
    }
    redraw=1u;return 1;
}
static void topology_ui_field(uint32_t k,const char **label,char *out,int *enabled)
{
    XenolithGraph graph;uint32_t kind=graph_edit.scope==XGRAPH_ADD?graph_edit.next_kind:topology_edit.growth;
    static const char *const labels[]={"GROWTH","MODE","LINK","FEEDBACK"};*label=labels[k];*enabled=1;
    xenolith_graph_compile(&graph_edit,topology_edit.growth,active_cells,&graph);
    if(!k){
        *label=graph_edit.scope==XGRAPH_ADD?"ADD TYPE":"GROWTH";
        mod_ui_copy(out,graph_edit.scope==XGRAPH_ADD?TOPO_UI_ADD[graph_edit.next_kind]:TOPO_UI_GROWTH[topology_edit.growth]);
    }
    else if(k==1u)mod_ui_copy(out,TOPO_UI_MODE[topology_edit.mode]);
    else if(k==2u){mod_ui_number(topology_edit.link,out);*enabled=graph.edge_count||kind==XTOPO_CHAIN4||kind==XTOPO_BRANCH||kind==XTOPO_LOOP4||(graph_edit.scope==XGRAPH_ADD&&kind==XTOPO_SELF&&graph_edit.attachment);}
    else{mod_ui_number(topology_edit.feedback,out);*enabled=graph.feedback_count||kind==XTOPO_LOOP4||kind==XTOPO_SELF;}
}
/* Both views use the renderer's compiled graph. A compact row-major block
 * grows toward a square, retaining index order as its column count changes.
 * Lines are modulation, not the audible mix: every cell remains an output tap.
 * Solid elbows are current-tick links; dotted elbows are one-tick returns.
 * Crossing lines are not junctions. HOME is necessarily a tiny overview; the
 * ARRAY page retains all 64 endpoints in a larger orthogonal raster. */
static void topology_ui_segment(int x0,int y0,int x1,int y1,int color,int dotted)
{
    int lo,hi,i;
    if(!dotted){cv_line(x0,y0,x1,y1,color);return;}
    if(y0==y1){lo=x0<x1?x0:x1;hi=x0>x1?x0:x1;for(i=lo;i<=hi;i+=2)cv_rect(i,y0,1,1,color);}
    else{lo=y0<y1?y0:y1;hi=y0>y1?y0:y1;for(i=lo;i<=hi;i+=2)cv_rect(x0,i,1,1,color);}
}
/* Existing first-eight carrier snapshots drive phase colour only. This is
 * stroboscopic phase indication, not an audio-rate trace or a level meter. */
static uint16_t topology_ui_phase_ink(uint32_t cell,uint16_t idle)
{
    uint32_t light,phase;
    if(cell>=8u||cell>=phase_cells||envelope_level<0.0001f)return idle;
    phase=(phase_published[cell]>>21)&255u;
    light=phase<128u?phase*2u:(255u-phase)*2u;
    return RGB(96u+light*92u/255u,14u+light*28u/255u,12u+light*20u/255u);
}
typedef struct {int cols,left,top,pitch_x,pitch_y,half,radius;} TopologyUiGeometry;
static TopologyUiGeometry topology_ui_geometry(uint32_t count,int small)
{
    TopologyUiGeometry g={1,120,48,12,8,2,1};int rows;
    if(count>64u)count=64u;
    while((uint32_t)(g.cols*g.cols)<count)++g.cols;
    rows=((int)count+g.cols-1)/g.cols;if(!rows)rows=1;
    if(small){
        g.pitch_x=count<=9u?5:count<=16u?4:3;
        g.pitch_y=rows<=4?4:rows<=5?3:2;
        g.left=152-(g.cols-1)*g.pitch_x/2;g.top=26-(rows-1)*g.pitch_y/2;
        g.half=0;g.radius=0;
    }else{
        g.pitch_x=count<=9u?32:count<=16u?24:count<=36u?18:12;
        g.pitch_y=rows<=4?16:56/(rows-1);
        g.left=120-(g.cols-1)*g.pitch_x/2;g.top=48-(rows-1)*g.pitch_y/2;
        if(count<=16u){g.half=5;g.radius=3;}else if(count<=36u){g.half=3;g.radius=2;}
    }
    return g;
}
static void topology_ui_edge(uint32_t source,uint32_t target,uint32_t count,int small,int delayed,int color)
{
    TopologyUiGeometry g=topology_ui_geometry(count,small);
    int sx=g.left+(int)(source%(uint32_t)g.cols)*g.pitch_x,sy=g.top+(int)(source/(uint32_t)g.cols)*g.pitch_y;
    int tx=g.left+(int)(target%(uint32_t)g.cols)*g.pitch_x,ty=g.top+(int)(target/(uint32_t)g.cols)*g.pitch_y;
    int half=g.half,gutter=sx+half+(small?2:5),row=ty-(small?1:g.radius+2);
    if(source==target){
        if(small){cv_rect(sx+2,sy+1,1,1,color);return;}
        topology_ui_segment(sx+half,sy,gutter,sy,color,1);
        topology_ui_segment(gutter,sy,gutter,sy+g.radius+3,color,1);
        topology_ui_segment(sx-half-3,sy+g.radius+3,gutter,sy+g.radius+3,color,1);
        topology_ui_segment(sx-half-3,sy,sx-half-3,sy+g.radius+3,color,1);
        topology_ui_segment(sx-half-3,sy,sx-half,sy,color,1);return;
    }
    /* Feedforward and return routes use distinct horizontal gutters. */
    if(delayed)row=ty+(small?1:g.radius+2);
    /* Sparse arrays have room for paired rails. This separates common
     * same-column/same-row runs without moving endpoints or HOME pixels. */
    if(!small&&count<=36u){
        gutter+=((source/(uint32_t)g.cols)&1u)?1:-1;
        row+=((target%(uint32_t)g.cols)&1u)?1:-1;
    }
    topology_ui_segment(sx+half,sy,gutter,sy,color,delayed);
    topology_ui_segment(gutter,sy,gutter,row,color,delayed);
    topology_ui_segment(gutter,row,tx,row,color,delayed);
    topology_ui_segment(tx,row,tx,ty+(delayed?g.radius:-g.radius),color,delayed);
    if(!small&&!delayed)cv_rect(tx,ty-g.radius-1,1,1,color);
}
static void topology_ui_graph(const XenolithGraph *graph,uint32_t count,int small)
{
    uint32_t i;
    TopologyUiGeometry g=topology_ui_geometry(count,small);
    int link_ink=topology_edit.link>0.0f?C_LINE:C_DIM;
    int return_ink=topology_edit.feedback>0.0f?C_AMB:C_DIM;
    for(i=0;i<count;i++){
        if(graph->parent[i]<count)topology_ui_edge(graph->parent[i],i,count,small,0,small||!topology_edit.link?link_ink:topology_ui_phase_ink(graph->parent[i],link_ink));
        if(graph->feedback[i]<count)topology_ui_edge(graph->feedback[i],i,count,small,1,return_ink);
    }
    for(i=0;i<count;i++){
        int x=g.left+(int)(i%(uint32_t)g.cols)*g.pitch_x;
        int y=g.top+(int)(i/(uint32_t)g.cols)*g.pitch_y;
        int ink=i+1u==count?C_HI:small?C_AMB:topology_ui_phase_ink(i,C_AMB);
        if(small)cv_rect(x,y,2,1,ink);
        else{
            /* Node borders conceal crossed routes; only their own ports join. */
            cv_rect(x-g.half,y-g.radius,g.half*2+1,g.radius*2+1,C_BLACK);
            cv_line(x-g.half,y-g.radius,x+g.half,y-g.radius,ink);cv_line(x-g.half,y+g.radius,x+g.half,y+g.radius,ink);
            cv_rect(x-g.half,y-g.radius+1,1,g.radius*2-1,ink);cv_rect(x+g.half,y-g.radius+1,1,g.radius*2-1,ink);
            if(graph->feedback[i]==i)cv_rect(x,y,1,1,return_ink);
            if(graph_edit.scope==XGRAPH_ADD&&graph_edit.attachment==i+1u){
                cv_line(x-g.half-2,y-g.radius-2,x+g.half+2,y-g.radius-2,C_AMB);cv_line(x-g.half-2,y+g.radius+2,x+g.half+2,y+g.radius+2,C_AMB);
                cv_line(x-g.half-2,y-g.radius-2,x-g.half-2,y+g.radius+2,C_AMB);cv_line(x+g.half+2,y-g.radius-2,x+g.half+2,y+g.radius+2,C_AMB);
            }
        }
    }
}
static void topology_ui_home_glyph(void)
{
    XenolithGraph graph;uint32_t count=active_cells>64u?64u:active_cells;
    xenolith_graph_compile(&graph_edit,topology_edit.growth,count,&graph);
    cv_rect(126,18,54,18,C_BLACK);
    cv_line(132,18,136,18,C_LINE);cv_line(132,18,132,21,C_LINE);
    cv_line(168,18,172,18,C_LINE);cv_line(172,18,172,21,C_LINE);
    cv_line(132,35,136,35,C_LINE);cv_line(132,32,132,35,C_LINE);
    cv_line(168,35,172,35,C_LINE);cv_line(172,32,172,35,C_LINE);
    topology_ui_graph(&graph,count,1);
}
static void topology_ui_home_count(void)
{
    static const uint8_t digits[10][7]={
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
    };
    uint32_t count=active_cells>64u?64u:active_cells;
    uint32_t places=count>=10u?2u:1u,i,x,y;
    int left=186-(int)(places*6u-1u)/2;
    cv_rect(121,18,5,18,C_BLACK);
    cv_rect(180,18,52,18,C_BLACK);
    for(i=0;i<places;i++){
        uint32_t digit=places==2u&&!i?count/10u:count%10u;
        for(y=0;y<7u;y++)for(x=0;x<5u;x++)
            if(digits[digit][y]&(16u>>x))cv_rect(left+(int)(i*6u+x),23+(int)y,1,1,RGB(112,96,208));
    }
}
static void topology_ui_draw(void)
{
    XenolithGraph graph;char text[24],label[24],number[24];uint32_t i,cells=active_cells;
    if(cells>64u)cells=64u;
    xenolith_graph_compile(&graph_edit,topology_edit.growth,cells,&graph);
    cv_begin(240,36,C_BLACK);draw_panel_header("[ ARRAY ]");
    cv_text(16,20,&FONT_XS,"REQ",C_DIM);number_text((int32_t)requested_cells,number);cv_text(42,20,&FONT_XS,number,C_HI);
    cv_text(69,20,&FONT_XS,"LIVE",C_DIM);number_text((int32_t)cells,number);cv_text(99,20,&FONT_XS,number,C_HI);
    cv_line(121,18,169,18,C_LINE);cv_line(121,34,169,34,C_LINE);
    cv_line(121,18,121,34,C_LINE);cv_line(169,18,169,34,C_LINE);
    if(!graph_edit.attachment)mod_ui_copy(text,"AUTO");
    else{text[0]='C';text[1]=(char)('0'+graph_edit.attachment/10u);text[2]=(char)('0'+graph_edit.attachment%10u);text[3]=0;}
    cv_text(145-text_w(&FONT_XS,text)/2,20,&FONT_XS,text,graph_edit.scope==XGRAPH_ADD?C_AMB:C_DIM);
    cv_line(175,18,231,18,C_LINE);cv_line(175,34,231,34,C_LINE);
    cv_line(175,18,175,34,C_LINE);cv_line(231,18,231,34,C_LINE);
    mod_ui_copy(text,graph_edit.scope==XGRAPH_ADD?"CURRENT":graph_select_pending&&!graph_select_done?"ADOPT":graph_global_dirty?"GLOBAL*":"GLOBAL");
    if(graph_edit.scope==XGRAPH_ALL&&graph_select_pending&&!graph_select_done){
        uint32_t held=plat_ms()-graph_select_started;if(held>650u)held=650u;
        cv_line(176,33,176+(int)(54u*held/650u),33,C_AMB);
    }
    cv_text(203-text_w(&FONT_XS,text)/2,20,&FONT_XS,text,C_HI);cv_blit(0,0);
    cv_begin(240,120,C_BLACK);
    for(i=0;i<4u;i++){
        const char *name;uint32_t n=3u,j=0u;int enabled;int32_t row=(int32_t)i*29;
        topology_ui_field(i,&name,text,&enabled);label[0]='K';label[1]=(char)('1'+i);label[2]=' ';
        while(name[j]&&n<23u)label[n++]=name[j++];label[n]=0;
        cv_text(16,row+5,&FONT_XS,label,enabled?C_GRAY:C_DIM);
        cv_text(222-text_w(&FONT_B,text),row+1,&FONT_B,text,enabled?C_HI:C_DIM);
        cv_line(16,row+27,223,row+27,C_LINE);
    }
    cv_blit(0,36);cv_begin(240,84,C_BLACK);
    cv_text(16,0,&FONT_XS,"LINK",C_DIM);number_text((int32_t)graph.edge_count,number);cv_text(49,0,&FONT_XS,number,C_GRAY);
    cv_text(166,0,&FONT_XS,"z-1",C_AMB);number_text((int32_t)graph.feedback_count,number);cv_text(202,0,&FONT_XS,number,C_GRAY);
    cv_line(8,14,231,14,C_LINE);cv_line(8,82,231,82,C_LINE);
    cv_line(8,14,8,82,C_LINE);cv_line(231,14,231,82,C_LINE);
    cv_line(11,16,17,16,C_DIM);cv_line(222,80,228,80,C_DIM);
    topology_ui_graph(&graph,cells,0);cv_blit(0,156);
}
