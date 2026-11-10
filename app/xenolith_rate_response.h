/* SPDX-License-Identifier: MPL-2.0
 * Foreground MAX-rate encoder travel. Audio clocks and smoothing are separate. */
#ifndef XX29_XENOLITH_RATE_RESPONSE_H
#define XX29_XENOLITH_RATE_RESPONSE_H
#include <stdint.h>

enum { XRATE_FINE, XRATE_SWEEP, XRATE_FAST, XRATE_LINEAR, XRATE_RESPONSE_COUNT };

static inline float xenolith_rate_response_step(float rate,int32_t delta,uint32_t response)
{
    int32_t count=delta>768?768:delta< -768?-768:delta;
    float up,down;
    if(!(rate>=1000.0f))rate=1000.0f;
    if(rate>176400.0f)rate=176400.0f;
    if(response>=XRATE_RESPONSE_COUNT)response=XRATE_SWEEP;
    up=response==XRATE_FINE?1.0018067009f:response==XRATE_FAST?1.0145453349f:1.0072464122f;
    down=response==XRATE_FINE?0.9981965574f:response==XRATE_FAST?0.9856631986f:0.9928057205f;
    while(count>0){
        if(rate>=88200.0f)rate=176400.0f;
        else if(rate>=44100.0f)rate=88200.0f;
        else{
            rate=response==XRATE_LINEAR?rate+250.0f:rate*up;
            if(rate>44100.0f)rate=44100.0f;
        }
        count--;
    }
    while(count<0){
        if(rate>88200.0f)rate=88200.0f;
        else if(rate>44100.0f)rate=44100.0f;
        else{
            rate=response==XRATE_LINEAR?rate-250.0f:rate*down;
            if(rate<1000.0f)rate=1000.0f;
        }
        count++;
    }
    /* Keep the established anchors exact after rounded encoder steps. */
    if(rate>44099.5f&&rate<44100.5f)rate=44100.0f;
    if(rate>22049.5f&&rate<22050.5f)rate=22050.0f;
    if(rate>11024.5f&&rate<11025.5f)rate=11025.0f;
    return rate;
}
#endif
