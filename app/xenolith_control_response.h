/* SPDX-License-Identifier: MPL-2.0
 * Foreground encoder coordinates. These functions change stored targets only;
 * audio-owner interpolation remains separate from panel sensitivity. */
#ifndef XX29_XENOLITH_CONTROL_RESPONSE_H
#define XX29_XENOLITH_CONTROL_RESPONSE_H
#include <stdint.h>
#include "fastmath.h"
#include "xenolith_controls.h"
#include "xenolith_ratio.h"

enum { XCTRL_TONIC, XCTRL_INDEX, XCTRL_RATIO, XCTRL_DRIFT, XCTRL_PARAMETER_COUNT };
enum { XCTRL_DEFAULT, XCTRL_LINEAR, XCTRL_EXP, XCTRL_CURVE_COUNT };
enum { XCTRL_QUARTER, XCTRL_HALF, XCTRL_NORMAL, XCTRL_DOUBLE, XCTRL_QUADRUPLE,
       XCTRL_SENSITIVITY_COUNT };
enum { XCTRL_FILTER_FINE, XCTRL_FILTER_SWEEP, XCTRL_FILTER_FAST,
       XCTRL_FILTER_LINEAR, XCTRL_FILTER_COUNT };

static inline float xcontrol_gain(uint32_t sensitivity)
{
    static const float gains[]={.25f,.5f,1.0f,2.0f,4.0f};
    return gains[sensitivity<XCTRL_SENSITIVITY_COUNT?sensitivity:XCTRL_NORMAL];
}

static inline float xcontrol_bound(float value,float low,float high)
{
    if(!(value>=low))return low;
    return value>high?high:value;
}

static inline int32_t xcontrol_delta(int32_t delta)
{
    return delta>2048?2048:delta< -2048?-2048:delta;
}

/* DEFAULT/NORMAL calls the established detent functions without rescaling.
 * Fractional sensitivity uses their continuous coordinates between detents.
 * EXP INDEX has a unit offset so zero remains reachable. EXP DRIFT similarly
 * includes .05 of headroom below zero; neither requires a special zero jump.
 * TONIC's established default already has exponential Hz travel. */
static inline float xcontrol_home_step(uint32_t parameter,float value,int32_t delta,
                                       uint32_t curve,uint32_t sensitivity,uint32_t exp_fm)
{
    float gain=xcontrol_gain(sensitivity),steps,low=0.0f,high=1.0f;
    if(parameter>=XCTRL_PARAMETER_COUNT)return value;
    if(curve>=XCTRL_CURVE_COUNT)curve=XCTRL_DEFAULT;
    if(parameter==XCTRL_TONIC){low=20.0f;high=800.0f;}
    else if(parameter==XCTRL_INDEX)high=exp_fm?XENOLITH_EXP_DEPTH_MAX:64.0f;
    else if(parameter==XCTRL_RATIO){low=XENOLITH_RATIO_MIN;high=XENOLITH_RATIO_MAX;}
    value=xcontrol_bound(value,low,high);
    delta=xcontrol_delta(delta);
    if(!delta)return value;
    steps=(float)delta*gain;
    if(curve==XCTRL_DEFAULT){
        if(parameter==XCTRL_TONIC)value*=fm_exp2f(steps/48.0f);
        else if(parameter==XCTRL_INDEX){
            if(gain==1.0f)return exp_fm?xenolith_exp_depth_step(value,delta):xenolith_index_step(value,delta);
            value=exp_fm?xenolith_exp_depth_value(xenolith_exp_depth_position(value)+steps):
                         xenolith_index_value(xenolith_index_position(value)+steps);
        }else if(parameter==XCTRL_RATIO){
            if(gain==1.0f)return xenolith_ratio_step(value,delta);
            value=xenolith_ratio_value(xenolith_ratio_position(value)+steps);
        }else value+=steps*.05f;
    }else if(curve==XCTRL_LINEAR){
        float unit=parameter==XCTRL_TONIC?2.0f:parameter==XCTRL_INDEX?(exp_fm?1.0f:.1f):
                   parameter==XCTRL_RATIO?.01f:.05f;
        value+=steps*unit;
    }else if(parameter==XCTRL_TONIC)value*=fm_exp2f(steps/48.0f);
    else if(parameter==XCTRL_RATIO)value*=fm_exp2f(steps/24.0f);
    else if(parameter==XCTRL_INDEX){
        float offset=exp_fm?4.0f:1.0f;
        value=(value+offset)*fm_exp2f(steps/12.0f)-offset;
    }else value=(value+.05f)*fm_exp2f(steps/24.0f)-.05f;
    return xcontrol_bound(value,low,high);
}

/* FINE retains the old96detents/octave. SWEEP is4x and FAST8x that travel;
 * LINEAR has an explicit100Hz increment rather than a percentage increment. */
static inline float xcontrol_filter_step(float value,int32_t delta,uint32_t response)
{
    float divisions;
    value=xcontrol_bound(value,20.0f,20000.0f);
    delta=xcontrol_delta(delta);
    if(!delta)return value;
    if(response>=XCTRL_FILTER_COUNT)response=XCTRL_FILTER_SWEEP;
    if(response==XCTRL_FILTER_LINEAR)value+=(float)delta*100.0f;
    else{
        divisions=response==XCTRL_FILTER_FINE?96.0f:response==XCTRL_FILTER_SWEEP?24.0f:12.0f;
        value*=fm_exp2f((float)delta/divisions);
    }
    return xcontrol_bound(value,20.0f,20000.0f);
}
#endif
