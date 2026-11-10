/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XX29_XENOLITH_COUPLING_H
#define XX29_XENOLITH_COUPLING_H
#include <stdint.h>

enum { XCOUPLING_OFF, XCOUPLING_BIAS, XCOUPLING_LAG,
       XCOUPLING_ALLPASS, XCOUPLING_COMMON, XCOUPLING_COUNT };
typedef struct {
    uint32_t method;
    float depth,parameter,bias;
} XenolithCouplingConfig;
_Static_assert(sizeof(XenolithCouplingConfig)==16,"Coupling config ABI changed");
static inline void xenolith_coupling_defaults(XenolithCouplingConfig *c) {
    if(!c)return;
    c->method=XCOUPLING_OFF;c->depth=0.0f;c->parameter=.5f;c->bias=0.0f;
}
static inline int xenolith_coupling_valid(const XenolithCouplingConfig *c) {
    return c&&c->method<XCOUPLING_COUNT&&c->depth>=0.0f&&c->depth<=1.0f
        &&c->parameter>=0.0f&&c->parameter<=1.0f&&c->bias>=-1.0f&&c->bias<=1.0f;
}
static inline int xenolith_coupling_active(const XenolithCouplingConfig *c) {
    return c&&c->method!=XCOUPLING_OFF&&c->depth>0.0f;
}
static inline float xenolith_coupling_drive(float p) {return 1.0f+15.0f*p;}
static inline float xenolith_coupling_time_ms(float p) {return .02f+99.98f*p*p*p;}
static inline float xenolith_coupling_delay(float p) {return .25f+7.75f*p*p;}
#endif
