/* SPDX-License-Identifier: MPL-2.0
 * XX29 independently written keyboard mappings and held-key band unions.
 * Uses platform MPL-2.0 fastmath; no external instrument code is adapted. */
#include "xenolith_keyboard.h"
#include "fastmath.h"

static int finite_value(float x) {
    fm_bits_t bits;bits.f=x;
    return (bits.u&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);
}
int xenolith_keyboard_destination_bounds(uint32_t destination,float *low,float *high) {
    float a,b;
    if(!low || !high)return 0;
    switch(destination){
    case XKEY_TONIC:a=20.0f;b=800.0f;break;
    case XKEY_CELLS:a=20.0f;b=8000.0f;break;
    case XKEY_INDEX:a=0.0f;b=64.0f;break;
    case XKEY_EXP_DEPTH:a=0.0f;b=120.0f;break;
    case XKEY_RATIO:a=0.001f;b=32.0f;break;
    case XKEY_CORE_RATE:a=1000.0f;b=44100.0f;break;
    case XKEY_WET_RATE:a=1378.125f;b=44100.0f;break;
    case XKEY_BITS:a=1.0f;b=24.0f;break;
    case XKEY_FILTER_CUTOFF:a=20.0f;b=20000.0f;break;
    case XKEY_DRIFT:case XKEY_WET_MIX:case XKEY_AMPLITUDE:case XKEY_FILTER_RESONANCE:a=0.0f;b=1.0f;break;
    default:return 0;
    }
    *low=a;*high=b;return 1;
}
int xenolith_keyboard_destination_defaults(uint32_t destination,float *low,float *high,
                                         uint32_t *response) {
    float a,b;uint32_t r=XKEY_LINEAR;
    if(!low || !high || !response || !xenolith_keyboard_destination_bounds(destination,&a,&b))return 0;
    if(destination==XKEY_TONIC){b=800.0f;r=XKEY_CHROMATIC;}
    else if(destination==XKEY_INDEX||destination==XKEY_EXP_DEPTH){a=.125f;r=XKEY_LOG;}
    else if(destination==XKEY_RATIO){a=0.25f;b=4.0f;r=XKEY_LOG;}
    else if(destination==XKEY_CORE_RATE || destination==XKEY_WET_RATE || destination==XKEY_CELLS || destination==XKEY_FILTER_CUTOFF)r=XKEY_LOG;
    *low=a;*high=b;*response=r;return 1;
}
void xenolith_keyboard_config_defaults(XenolithKeyboardConfig *c) {
    if(!c)return;
    c->destination=XKEY_TONIC;c->response=XKEY_CHROMATIC;c->latch=0u;c->gate=1u;
    c->low=20.0f;c->high=800.0f;c->cell_width=1u;c->cell_layout=XKEY_RELATIVE;
}
int xenolith_keyboard_config_valid(const XenolithKeyboardConfig *c) {
    float low,high;
    return c && xenolith_keyboard_destination_bounds(c->destination,&low,&high)
        && c->response<XKEY_RESPONSE_COUNT && (c->response!=XKEY_CHROMATIC || c->destination==XKEY_TONIC)
        && c->latch<=1u && c->gate<=1u && c->cell_width>=1u && c->cell_width<=16u
        && c->cell_layout<XKEY_LAYOUT_COUNT && finite_value(c->low) && finite_value(c->high)
        && c->low>=low && c->high<=high && c->low<=c->high
        && (c->response!=XKEY_LOG || c->low>0.0f);
}
static float logarithmic(float low,float high,float t) {
    return fm_clampf(low*fm_exp2f(fm_log2f(high/low)*t),low,high);
}
int xenolith_keyboard_map_value(const XenolithKeyboardConfig *c,uint32_t key,float *value) {
    float t;
    if(!value || key>=XENOLITH_KEYBOARD_KEYS || !xenolith_keyboard_config_valid(c)
       || c->response==XKEY_CHROMATIC)return 0;
    if(!key){*value=c->low;return 1;}
    if(key==26u){*value=c->high;return 1;}
    t=(float)key*(1.0f/26.0f);
    *value=c->response==XKEY_LOG?logarithmic(c->low,c->high,t)
         :fm_clampf(c->low+(c->high-c->low)*t,c->low,c->high);
    return 1;
}
int xenolith_keyboard_select_cells(uint32_t keys,uint32_t last_key,uint32_t count,
                                  uint32_t width,XenolithKeyboardSelection *out) {
    XenolithKeyboardSelection selected={0u,0u,0.0f};uint32_t key,cell,start,end,band;
    if(!out || count<1u || count>XENOLITH_KEYBOARD_CELLS || width<1u || width>16u
       || (keys&~XENOLITH_KEYBOARD_KEY_MASK))return 0;
    if(keys && (last_key>=27u || !(keys&(UINT32_C(1)<<last_key))))return 0;
    if(width>count)width=count;
    for(key=0u;key<27u;key++)if(keys&(UINT32_C(1)<<key)){
        if(count>=27u){start=key*count/27u;end=(key+1u)*count/27u;}
        else{start=(key*(count-1u)+13u)/26u;end=start+1u;}
        band=end-start;
        if(band<width){
            uint32_t below=(width-band)/2u;
            start=start>below?start-below:0u;
            if(start>count-width)start=count-width;
            end=start+width;
        }
        for(cell=start;cell<end;cell++){
            if(cell<32u)selected.mask_lo|=UINT32_C(1)<<cell;
            else selected.mask_hi|=UINT32_C(1)<<(cell-32u);
        }
    }
    if(keys)selected.normalized=(float)last_key*(1.0f/26.0f);
    *out=selected;return 1;
}
int xenolith_keyboard_fill_frequencies(float *out,uint32_t count,float low,float high) {
    uint32_t i;float span;
    if(!out || count<1u || count>XENOLITH_KEYBOARD_CELLS || !finite_value(low) || !finite_value(high)
       || low<20.0f || high>8000.0f || low>high)return 0;
    out[0]=low;
    if(count==1u)return 1;
    span=fm_log2f(high/low);
    for(i=1u;i<count-1u;i++)out[i]=fm_clampf(low*fm_exp2f(span*((float)i/(float)(count-1u))),low,high);
    out[count-1u]=high;return 1;
}
