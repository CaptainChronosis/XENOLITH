/* SPDX-License-Identifier: MPL-2.0 */
#ifndef XX29_XENOLITH_ADMISSION_H
#define XX29_XENOLITH_ADMISSION_H
#include <stdint.h>

/* Foreground policy only. These flags describe admitted and retiring work,
 * never an instantaneous audio level. The timer guards remain authoritative. */
enum {
    XADMIT_GRAPH=1u, XADMIT_EXP_WARP=2u, XADMIT_CONTROL=4u,
    XADMIT_COMPOSITE=8u, XADMIT_EXTENDED=16u, XADMIT_DISPERSION=32u,
    XADMIT_PRE_FILTER=64u, XADMIT_CALCULATION=128u,
    XADMIT_EXP_COMPOSITE=256u, XADMIT_MIXED=512u,
    XADMIT_COUPLING=1024u, XADMIT_TRANSFER=2048u
};
enum {
    XADMIT_BASE_LIMIT=1u, XADMIT_GRAPH_LIMIT=2u, XADMIT_CONTROL_LIMIT=4u,
    XADMIT_COMPOSITE_LIMIT=8u, XADMIT_EXTENDED_LIMIT=16u,
    XADMIT_DISPERSION_LIMIT=32u, XADMIT_CALCULATION_LIMIT=64u,
    XADMIT_COUPLING_LIMIT=128u, XADMIT_TRANSFER_LIMIT=256u
};
typedef struct {
    float requested_rate;
    uint32_t cells,flags;
} XenolithAdmissionRequest;
typedef struct {
    float rate,ceiling;
    uint32_t reasons;
} XenolithAdmission;

static inline void xenolith_admission_limit(XenolithAdmission *a,float cap,uint32_t reason)
{
    if(cap<a->ceiling){a->ceiling=cap;a->reasons=reason;}
    else if(cap==a->ceiling)a->reasons|=reason;
}

/* The old lower tier is reached at boundary+1. Taper only the preceding four
 * count intervals; this can never exceed either old tier. Reciprocal rate
 * interpolates work per update. This is a conservative bridge, not a measured
 * claim that every point on a relaxed cells*rate envelope is safe. */
static inline float xenolith_admission_bridge(uint32_t cells,uint32_t boundary,
    float high,float low,uint32_t continuous)
{
    uint32_t start=boundary-3u;
    float p;
    if(cells>boundary)return low;
    if(!continuous||cells<=start)return high;
    p=(float)(cells-start)*.25f;
    return 1.0f/((1.0f-p)/high+p/low);
}

static inline XenolithAdmission xenolith_admission_prepare(
    XenolithAdmissionRequest r,uint32_t continuous)
{
    XenolithAdmission a={0.0f,176400.0f,XADMIT_BASE_LIMIT};
    uint32_t n=r.cells<1u?1u:r.cells>64u?64u:r.cells,f=r.flags;
    float cap,request=r.requested_rate;
    if(!(request>=1000.0f))request=1000.0f;
    if(request>176400.0f)request=176400.0f;
    cap=n<=8u?176400.0f:n<=16u?88200.0f:44100.0f;
    xenolith_admission_limit(&a,cap,XADMIT_BASE_LIMIT);
    if(f&XADMIT_GRAPH){
        float middle=f&XADMIT_EXP_WARP?28000.0f:32000.0f;
        float bottom=middle*.5f;
        cap=n<=4u?176400.0f:n<=8u?88200.0f:n<=16u?
            xenolith_admission_bridge(n,16u,44100.0f,middle,continuous):
            xenolith_admission_bridge(n,32u,middle,bottom,continuous);
        if(n>=3u&&n<=4u&&(f&XADMIT_EXP_WARP)&&(f&XADMIT_EXP_COMPOSITE))cap=88200.0f;
        xenolith_admission_limit(&a,cap,XADMIT_GRAPH_LIMIT);
    }else if(f&XADMIT_CONTROL){
        cap=n<=29u?176400.0f:xenolith_admission_bridge(n,32u,44100.0f,32000.0f,continuous);
        xenolith_admission_limit(&a,cap,XADMIT_CONTROL_LIMIT);
    }
    if(continuous&&n>=9u&&(f&XADMIT_MIXED)){
        /* The mixed renderer needs a separate silicon-measured allowance.
         * Budget the reserved prefix, including dormant CELL-modulation links.
         * Existing PARALLEL and legacy graph limits remain independent. */
        cap=(f&XADMIT_EXP_WARP?392000.0f:448000.0f)/(float)n;
        if(cap>44100.0f)cap=44100.0f;
        xenolith_admission_limit(&a,cap,XADMIT_GRAPH_LIMIT);
    }
    if(n>=7u&&n<=8u&&(f&XADMIT_COMPOSITE))
        xenolith_admission_limit(&a,88200.0f,XADMIT_COMPOSITE_LIMIT);
    if(f&XADMIT_EXTENDED){
        cap=n<=8u?88200.0f:n<=16u?
            xenolith_admission_bridge(n,16u,44100.0f,24000.0f,continuous):
            xenolith_admission_bridge(n,32u,24000.0f,12000.0f,continuous);
        xenolith_admission_limit(&a,cap,XADMIT_EXTENDED_LIMIT);
    }
    if(f&XADMIT_DISPERSION)
        xenolith_admission_limit(&a,n>4u&&(f&XADMIT_PRE_FILTER)?44100.0f:88200.0f,XADMIT_DISPERSION_LIMIT);
    if(f&XADMIT_CALCULATION){
        cap=n<=4u?88200.0f:n<=8u?
            xenolith_admission_bridge(n,8u,44100.0f,32000.0f,continuous):
            n<=16u?xenolith_admission_bridge(n,16u,32000.0f,16000.0f,continuous):
            xenolith_admission_bridge(n,32u,16000.0f,8000.0f,continuous);
        xenolith_admission_limit(&a,cap,XADMIT_CALCULATION_LIMIT);
    }
    if(continuous&&(f&XADMIT_COUPLING)){
        /* Optional graph colour runs from XIP. The SRAM graph allowance does
         * not cover that fetch cost. NORMAL and existing RAM paths are intact.
         * This initial margin still requires physical qualification. */
        cap=(f&XADMIT_EXP_WARP?196000.0f:224000.0f)/(float)n;
        if(cap>44100.0f)cap=44100.0f;
        xenolith_admission_limit(&a,cap,XADMIT_COUPLING_LIMIT);
    }
    if(continuous&&(f&XADMIT_TRANSFER)){
        /* Stateful transfer kernels share the optional XIP renderer and retain
         * an additional conservative allowance pending silicon qualification. */
        cap=(f&XADMIT_EXP_WARP?154000.0f:176000.0f)/(float)n;
        if(cap>44100.0f)cap=44100.0f;
        xenolith_admission_limit(&a,cap,XADMIT_TRANSFER_LIMIT);
    }
    a.rate=request<a.ceiling?request:a.ceiling;
    return a;
}
#endif
