/* SPDX-License-Identifier: MPL-2.0
 * Independent normalized-allpass bank for the eight existing FDN legs.
 * Source rationale: docs/FX_DISPERSION_PROPOSAL_2026-11-02.md. */
#ifndef XX29_XENOLITH_DISPERSION_H
#define XX29_XENOLITH_DISPERSION_H
#include <stdint.h>

#define XENOLITH_DISPERSION_LINES 8u
#define XENOLITH_DISPERSION_SAMPLES 366u
#define XENOLITH_DISPERSION_LONGEST 101u
/* Scale0=1/4,1=1/2,2=1 of the fixed maximum delay lengths. */
typedef struct { float amount_a,amount_b; uint32_t scale; } XenolithDispersionConfig;
typedef struct {
    float amount_a,amount_b;
    uint32_t scale;
    float wet_rate,a[2],b[2],inverse_ramp;
    uint32_t ramp_samples;
} XenolithDispersionPrepared;
typedef struct {
    float delay[XENOLITH_DISPERSION_SAMPLES];
    uint32_t write[XENOLITH_DISPERSION_LINES];
    XenolithDispersionPrepared target;
    float a[2],b[2],a_step[2],b_step[2],a_goal[2],b_goal[2];
    uint32_t remaining[2],phase[2],clear_cursor[2];
    uint32_t active_scale,geometry_transition,faults;
} XenolithDispersion;
_Static_assert(sizeof(XenolithDispersionConfig)==12,"Dispersion config ABI changed");
_Static_assert(sizeof(XenolithDispersionPrepared)==40,"Dispersion prepared ABI changed");
_Static_assert(sizeof(XenolithDispersion)==1620,"Dispersion state layout changed");

void xenolith_dispersion_defaults(XenolithDispersionConfig *config);
int xenolith_dispersion_config_valid(const XenolithDispersionConfig *config);
/* Foreground only: both amounts0..1, wet_rate1000..192000 Hz. Computes sqrt here,
 * never in process(). Invalid input leaves prepared unchanged. Default0 is
 * exact bypass. Nonzero coefficients have a small float contraction margin. */
int xenolith_dispersion_prepare(XenolithDispersionPrepared *prepared,
    const XenolithDispersionConfig *config,float wet_rate);
/* Init/clear contain a bounded bulk history clear and require stopped/exclusive
 * ownership. clear() retains the current control target and finishes its ramp. */
void xenolith_dispersion_init(XenolithDispersion *state,float wet_rate);
void xenolith_dispersion_clear(XenolithDispersion *state);
/* Audio owner, once per accepted block snapshot. 50ms coefficient morph in
 * actual wet ticks. Same target does not restart a ramp. Invalid prepared
 * values return0 without mutation. Retime rescales an unfinished ramp while
 * retaining history; call at block boundary when actual wet rate changes. */
int xenolith_dispersion_apply(XenolithDispersion *state,
    const XenolithDispersionPrepared *prepared);
int xenolith_dispersion_retime(XenolithDispersion *state,float wet_rate);
/* Includes bounded OFF history retirement: at most8zero writes/wet tick,
 * <=71ticks. Re-enable while retiring queues the newest target until the
 * history is empty. Geometry changes retire both groups before reopening.
 * phase0=running,1=ramping to bypass,2=clearing,3=clean bypass. */
static inline __attribute__((always_inline)) int xenolith_dispersion_active(const XenolithDispersion *state){
    return state&&(state->phase[0]!=3u||state->phase[1]!=3u||state->geometry_transition);
}
/* Once per actual wet tick, after line damping/gain and before the feedback
 * matrix. Eight-vector in place. No trig, sqrt, division, allocation or long
 * clearing loop. Coefficients stay in the unit disk even during interrupted
 * ramps. Input/state bounds and finite rejection remove energy on faults. */
void xenolith_dispersion_process(XenolithDispersion *state,float vector[8]);
#endif
