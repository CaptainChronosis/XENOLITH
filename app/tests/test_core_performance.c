/* Focused host-only tests. Pre-change core SHA256:
 *89d99034fa79dbf875b141103a8717cf8cb7243ceb17370b2d1ed34ae3969bba
 * Legacy FNV word hash19c53dbd captured with GCC-O2,-ffp-contract=off.
 *189 scenes /774144 samples include every optimized parallel renderer,
 * graph, absolute gates and count transitions. No hardware timing claim. */
#include "../xenolith_core.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static unsigned hash_word(unsigned h,float value) {
    unsigned word;memcpy(&word,&value,4);return (h^word)*16777619u;
}
static unsigned legacy_hash(void) {
    static const unsigned counts[]={1,8,9,16,17,32,64};
    static const float rates[]={1000,44100,176400};
    XenolithCore s;unsigned c,r,scene,k,i,h=2166136261u;float frequencies[64];
    for(r=0;r<3;r++)for(c=0;c<7;c++)for(scene=0;scene<9;scene++){
        memset(&s,0,sizeof s);xenolith_init(&s,rates[r],29);xenolith_set_cells(&s,counts[c]);
        xenolith_set(&s,84,10,1.2f,.2f);
        if(scene==1)xenolith_set_interpolation_weight(&s,.37f);
        if(scene==2)xenolith_set_load_erosion(&s,0,0,.625f,1);
        if(scene==3)xenolith_set_load_erosion(&s,1.7f,.3f,.625f,.8f);
        if(scene==4){xenolith_set_interpolation_weight(&s,0);xenolith_set_load_erosion(&s,1,0,.625f,1);}
        if(scene==5){XenolithCalculationConfig calc={8,XCALC_LINEAR,XCALC_BOTH,.3f};CHECK(xenolith_set_calculation(&s,&calc));}
        if(scene==6){XenolithTopologyConfig topo={XTOPO_CHAIN4,XTOPO_PHASE,.4f,0};CHECK(xenolith_set_topology(&s,&topo));}
        if(scene==7){for(i=0;i<64;i++)frequencies[i]=84+(float)i*3;CHECK(xenolith_set_cell_gates(&s,1,0x55555555u,0x55555555u,1,frequencies));}
        for(k=0;k<4096;k++){
            if(k==256)xenolith_set(&s,120,4,.8f,.6f);
            if(scene==8&&k==512)CHECK(xenolith_transition_cells(&s,64-counts[c]+1,1));
            h=hash_word(h,xenolith_sample(&s));
        }
    }
    return h;
}
static void render(XenolithCore *s,unsigned frames){
    unsigned i;for(i=0;i<frames;i++){float y=xenolith_sample(s);CHECK(isfinite(y)&&fabsf(y)<2.72f);}
}
static void spread_controls(void){
    static const unsigned counts[]={1,8,9,16,17,32,64};
    XenolithCore s,before;unsigned c,g,w,i;float frequencies[64];
    for(c=0;c<7;c++)for(g=0;g<5;g++)for(w=0;w<3;w++){
        XenolithTopologyConfig topo={g,XTOPO_LINEAR,0,0};
        xenolith_init(&s,44100,29);xenolith_set_cells(&s,counts[c]);CHECK(xenolith_set_topology(&s,&topo));
        xenolith_set(&s,100,0,1,0);render(&s,100);before=s;
        xenolith_set_performance(&s,0,(float)w);
        CHECK(!memcmp(s.noise,before.noise,sizeof s.noise));
        CHECK(!memcmp(s.car_phase,before.car_phase,sizeof s.car_phase));
        CHECK(!memcmp(s.mod_phase,before.mod_phase,sizeof s.mod_phase));
        CHECK(s.random_state==before.random_state&&s.extra_random_state==before.extra_random_state);
        render(&s,1100);CHECK(!s.count_smoothing);CHECK(s.spread_current==(float)w);
        for(i=0;i<s.cells;i++){
            CHECK(s.tonic[i]>0&&isfinite(s.tonic[i]));
            if(w==0)CHECK(s.tonic[i]==100);
            if(g||s.cells<=8)CHECK(fabsf(s.tonic[i]-(100+(float)i*.46f*(float)w))<.00001f);
            else if(w==1){unsigned offset=s.cells*(s.cells-1)/2-36;CHECK(s.spread_scale[i]==count_spread_table[offset+i]);}
            else if(w==2){unsigned offset=s.cells*(s.cells-1)/2-36;float base=count_spread_table[offset+i];
                CHECK(fabsf(s.spread_scale[i]/(base*base)-1)<.00001f);}
        }
        for(i=0;i<64;i++)frequencies[i]=200+(float)i*2;
        CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));render(&s,64);
        for(i=0;i<s.cells;i++)CHECK(s.tonic[i]==frequencies[i]);
    }
    /* Retarget widths/counts during their ramps: no phase or noise reset. */
    xenolith_init(&s,44100,29);
    for(i=0;i<96;i++){
        before=s;xenolith_set_performance(&s,0,(float)(i%3));
        CHECK(xenolith_transition_cells(&s,i%2?64:1,1));
        CHECK(!memcmp(s.noise,before.noise,sizeof s.noise));
        CHECK(!memcmp(s.car_phase,before.car_phase,sizeof s.car_phase));
        render(&s,17);
    }
    render(&s,1400);CHECK(!s.count_smoothing&&s.cells==64);
    xenolith_init(&s,44100,29);xenolith_set_performance(&s,0,0);
    render(&s,441);CHECK(s.spread_current>.45f&&s.spread_current<.55f);
    xenolith_retime(&s,88200,64);render(&s,882+64);
    CHECK(s.spread_current==0&&s.count_smoothing==0);
}
static void tonic_glide(void){
    static const float rates[]={1000,8000,44100,176400};
    XenolithCore s;unsigned r,k;float halfway=0,reference_half=0;
    for(r=0;r<4;r++){
        xenolith_init(&s,rates[r],29);xenolith_set(&s,100,0,1,0);
        xenolith_set_performance(&s,1000,1);xenolith_set(&s,400,0,1,0);CHECK(s.base==100);
        for(k=0;k<(unsigned)(rates[r]/2);k++){
            float previous=s.base;
            if(k%32==0)xenolith_set(&s,400,0,1,0);
            (void)xenolith_sample(&s);CHECK(s.base>=previous&&s.base<=400);
            CHECK(s.base-previous<300.0f/rates[r]+.0001f);
        }
        CHECK(fabsf(s.base-250)<.001f);
        if(r==0)reference_half=s.base;else CHECK(fabsf(s.base-reference_half)<.001f);
        render(&s,(unsigned)(rates[r]/2));CHECK(s.base==400&&s.base_glide_frames==0);
        xenolith_set(&s,100,0,1,0);render(&s,(unsigned)(rates[r]/4));
        CHECK(fabsf(s.base-325)<.001f);halfway=s.base;
        xenolith_set(&s,500,0,1,0);CHECK(s.base==halfway);
        render(&s,(unsigned)(rates[r]/2));CHECK(fabsf(s.base-412.5f)<.001f);
        xenolith_set_performance(&s,0,1);CHECK(s.base==500&&s.base_glide_frames==0);
    }
    xenolith_init(&s,44100,29);xenolith_set(&s,100,0,1,0);xenolith_set_performance(&s,1000,1);
    xenolith_set(&s,500,0,1,0);render(&s,11025);CHECK(fabsf(s.base-200)<.001f);
    xenolith_retime(&s,88200,64);CHECK(fabsf(s.base-200)<.001f);
    render(&s,22050);CHECK(fabsf(s.base-300)<.001f);
    xenolith_retime(&s,22050,16);render(&s,11025);CHECK(s.base==500);
}
static void absolute_glide(void){
    XenolithCore s,before;unsigned i,k;float frequencies[64];
    xenolith_init(&s,44100,29);xenolith_set(&s,100,0,1,0);
    for(i=0;i<64;i++)frequencies[i]=200+(float)i;
    CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));render(&s,64);
    xenolith_set_performance(&s,1000,1);
    for(i=0;i<64;i++)frequencies[i]+=400;
    before=s;CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));
    CHECK(!memcmp(s.car_phase,before.car_phase,sizeof s.car_phase));
    for(i=0;i<8;i++)CHECK(s.cell_gate_frequency[i]==200+(float)i);
    for(k=0;k<22050;k++){
        if(k%32==0)CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));
        (void)xenolith_sample(&s);
    }
    for(i=0;i<8;i++)CHECK(fabsf(s.cell_gate_frequency[i]-(400+(float)i))<.3f);
    xenolith_retime(&s,88200,64);render(&s,44100+64);CHECK(s.cell_glide_frames==0);
    for(i=0;i<8;i++)CHECK(s.cell_gate_frequency[i]==frequencies[i]);
    for(i=0;i<8;i++)frequencies[i]-=300;
    CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));render(&s,100);
    xenolith_set_performance(&s,0,1);
    for(i=0;i<8;i++)CHECK(s.cell_gate_frequency[i]==frequencies[i]);
    /* Count growth cannot expose an uninitialized glide start/target. */
    xenolith_set_performance(&s,2000,1);
    for(i=0;i<64;i++)frequencies[i]=500+(float)i;
    CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));render(&s,100);
    xenolith_set_cells(&s,64);render(&s,100);
    for(i=0;i<64;i++)CHECK(s.cell_gate_frequency[i]>=20&&s.cell_gate_frequency[i]<=8000);
    CHECK(xenolith_set_cell_gates(&s,1,~0u,~0u,1,frequencies));
    CHECK(xenolith_transition_cells(&s,1,1));render(&s,200);
    CHECK(xenolith_transition_cells(&s,64,1));render(&s,200);
    for(i=0;i<64;i++)CHECK(s.cell_gate_frequency[i]>=20&&s.cell_gate_frequency[i]<=8000);
}
int main(void){unsigned hash=legacy_hash();CHECK(hash==0x19c53dbdu);
    spread_controls();tonic_glide();absolute_glide();
    printf("{\"passed\":true,\"legacy_scenes\":189,\"legacy_samples\":774144,\"legacy_hash\":\"%08x\",\"checks\":%u,\"core_bytes\":%u}\n",hash,checks,(unsigned)sizeof(XenolithCore));return 0;}
