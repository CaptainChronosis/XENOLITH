/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef XX29_XENOLITH_GOVERNOR_H
#define XX29_XENOLITH_GOVERNOR_H
#include <stdint.h>

typedef struct {
    float onset, span, recovery_ms;
    float filter_shape, reverb_shape, vca_bias;
    float table_depth, table_loss, dither, cell_floor, link_loss, clock_floor, erosion_curve;
} XenolithGovernorConfig;

typedef struct {
    XenolithGovernorConfig config;
    float stress, load, peak_load, colour;
    uint32_t blocks;
} XenolithGovernor;

typedef struct {
    float effects;       /* 0..1; downstream shaping precedes core degradation */
    float core;          /* 0..1; request, never an implicit cell/phase edit */
    float interpolation; /* 1..0; identity is1 */
    float duty;          /* 1..0.25; identity is1 */
    float table_loss;    /* 0..5 address-bit reduction */
    float dither;        /* 0..1, only when erosion is requested */
    float cell_ratio;    /* 1..0.375 window fraction */
    float link_gain;     /* 1..0 modulation-link gain */
} XenolithGovernorFrame;

/* Timing estimates only. A periodic32-frame control burst contributes once
 * to a quantum, rather than being extrapolated from its busiest8-frame slice. */
typedef struct {
    uint32_t post_chunk_peak, post_quantum_peak;
    uint32_t post_quantum_sum, post_quantum_chunks;
} XenolithGovernorCost;

void xenolith_governor_cost_init(XenolithGovernorCost *cost,uint32_t seed_ticks_per_frame);
/* Feed exactly one completed regular8-output-frame chunk, including the last
 * chunk before callback return. Exclude quality-call ticks and emergency fill.
 * Partial32-frame sums can span callbacks; they contain only processed frames. */
void xenolith_governor_cost_observe(XenolithGovernorCost *cost,uint32_t post_ticks);
void xenolith_governor_cost_decay(XenolithGovernorCost *cost,uint32_t floor_ticks_per_frame);
uint32_t xenolith_governor_post_reserve(const XenolithGovernorCost *cost,uint32_t frames_remaining);
/* Fixed callback allowance plus measured32-vs8-frame tail overreservation.
 * Used for feedback only; it does not modify either admission predicate. */
uint32_t xenolith_governor_feedback_headroom(const XenolithGovernorCost *cost,uint32_t fixed_reserve_ticks);
uint32_t xenolith_governor_demand(uint32_t elapsed_ticks,uint32_t denied_frames,
    uint32_t active_quality_ticks_per_frame,uint32_t executed_held_quality_ticks);
int xenolith_governor_admit_core_cost(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t frames_remaining,const XenolithGovernorCost *cost,
    uint32_t quality_chunk_ticks,uint32_t fixed_reserve_ticks);

void xenolith_governor_defaults(XenolithGovernorConfig *config);
int xenolith_governor_config_valid(const XenolithGovernorConfig *config);
void xenolith_governor_init(XenolithGovernor *state);
int xenolith_governor_apply(XenolithGovernor *state,const XenolithGovernorConfig *config);
void xenolith_governor_reset(XenolithGovernor *state);
XenolithGovernorFrame xenolith_governor_frame(const XenolithGovernor *state);
/* One measurement per completed block, used by the following block. Invalid
 * timing inputs leave state unchanged; the independent hard guard still fails
 * closed when its budget is zero. RATE is output transport rate, not core rate. */
XenolithGovernorFrame xenolith_governor_update(XenolithGovernor *state,
    uint32_t elapsed_ticks,uint32_t budget_ticks,uint32_t frames,float rate);
/* Continuously account for unavailable guard headroom in late target stress
 * only (.65 to .85 smooth onset). LOAD/peak retain raw demanded work/budget;
 * zero headroom is the original update, and early/mid targets are unchanged. */
XenolithGovernorFrame xenolith_governor_update_headroom(XenolithGovernor *state,
    uint32_t elapsed_ticks,uint32_t budget_ticks,uint32_t frames,float rate,uint32_t headroom_ticks);

/* TIMER4/plat_ticks24 is a32-bit count at24MHz. Unsigned subtraction handles
 * one wrap; caller must keep measured intervals shorter than a full wrap. */
uint32_t xenolith_governor_elapsed(uint32_t start,uint32_t now);
uint32_t xenolith_governor_remaining(uint32_t start,uint32_t now,uint32_t budget);
/* Reserve downstream work for all remaining frames BEFORE admitting another
 * bounded core chunk. Caller supplies conservative measured/derived costs;
 * this predicate cannot bound unchecked code or an underestimated reserve. */
int xenolith_governor_admit_core(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t frames_remaining,uint32_t post_ticks_per_frame,
    uint32_t core_chunk_ticks,uint32_t fixed_reserve_ticks);
int xenolith_governor_admit_post(uint32_t start,uint32_t now,uint32_t budget,
    uint32_t post_chunk_ticks,uint32_t final_reserve_ticks);

_Static_assert(sizeof(XenolithGovernorConfig)==52,"governor config layout");
#endif
