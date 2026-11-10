/* SPDX-License-Identifier: MPL-2.0 */
#ifndef XENOLITH_PERFORMANCE_H
#define XENOLITH_PERFORMANCE_H

#include "xenolith_control_response.h"

typedef struct {
    uint32_t curve[XCTRL_PARAMETER_COUNT];
    uint32_t sensitivity[XCTRL_PARAMETER_COUNT];
    uint32_t filter_response;
    uint32_t voices;
    float glide_ms;
    float spread;
    uint32_t ratio_snap;
} XenolithPerformanceConfig;

static inline void xenolith_performance_defaults(XenolithPerformanceConfig *p) {
    uint32_t i;
    if(!p)return;
    for(i=0;i<XCTRL_PARAMETER_COUNT;i++) {
        p->curve[i]=XCTRL_DEFAULT;
        p->sensitivity[i]=XCTRL_NORMAL;
    }
    p->filter_response=XCTRL_FILTER_SWEEP;
    p->voices=1u;p->glide_ms=0.0f;p->spread=1.0f;p->ratio_snap=0u;
}

static inline int xenolith_performance_valid(const XenolithPerformanceConfig *p) {
    uint32_t i;
    if(!p)return 0;
    for(i=0;i<XCTRL_PARAMETER_COUNT;i++)
        if(p->curve[i]>=XCTRL_CURVE_COUNT || p->sensitivity[i]>=XCTRL_SENSITIVITY_COUNT)return 0;
    return p->filter_response<XCTRL_FILTER_COUNT && p->voices>=1u && p->voices<=8u
        && p->glide_ms>=0.0f && p->glide_ms<=2000.0f
        && p->spread>=0.0f && p->spread<=2.0f && p->ratio_snap<=2u;
}

_Static_assert(sizeof(XenolithPerformanceConfig)==52u,"Performance config ABI changed");
#endif
