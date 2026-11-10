/* SPDX-License-Identifier: GPL-2.0-or-later
 * XX29 Xenolith I dry-core probe. See xenolith_core.c for provenance.
 */
#ifndef XX29_XENOLITH_CORE_H
#define XX29_XENOLITH_CORE_H
#include <stdint.h>
#include "xenolith_graph.h"
#include "xenolith_flavour.h"
#include "xenolith_coupling.h"

#define XENOLITH_CELLS 8
#define XENOLITH_MAX_CELLS 64
#define XENOLITH_CONTROL_BLOCK 32

enum { XTOPO_LINEAR, XTOPO_PHASE, XTOPO_EXPONENTIAL, XTOPO_PULSE, XTOPO_WARP, XTOPO_MODE_COUNT };
#define XENOLITH_INDEX_MAX 64.0f
#define XENOLITH_EXP_DEPTH_MAX 120.0f
/* The stored numeric depth keeps each mode's existing units. Only EXP has
 * additional semitone headroom; all other modes retain their original64. */
static inline float xenolith_depth_limit(uint32_t mode) {
    return mode==XTOPO_EXPONENTIAL?XENOLITH_EXP_DEPTH_MAX:XENOLITH_INDEX_MAX;
}
typedef struct {
    uint32_t growth,mode;
    float link,feedback;
} XenolithTopologyConfig;
_Static_assert(sizeof(XenolithTopologyConfig)==16,"Topology config ABI changed");
void xenolith_topology_defaults(XenolithTopologyConfig *config);
int xenolith_topology_valid(const XenolithTopologyConfig *config);

enum { XCALC_HOLD,XCALC_LINEAR,XCALC_INTERPOLATION_COUNT };
enum { XCALC_MOD,XCALC_CARRIER,XCALC_BOTH,XCALC_TARGET_COUNT };
typedef struct {
    uint32_t table_bits,interpolation,target;
    float harmonic;
} XenolithCalculationConfig;
_Static_assert(sizeof(XenolithCalculationConfig)==16,"Calculation config ABI changed");
/* Table2..11 bits (4..2048 points), HOLD/LINEAR, third-harmonic mix0..1.
 * Defaults11/LINEAR/BOTH/0 preserve the existing lookup exactly. TARGET alone
 * does not activate the optional path when the waveform remains the default. */
void xenolith_calculation_defaults(XenolithCalculationConfig *config);
int xenolith_calculation_valid(const XenolithCalculationConfig *config);
int xenolith_calculation_active(const XenolithCalculationConfig *config);

typedef struct {
    float level, slope, curve, next_value, next_midpoint;
    int32_t remaining;
    float remaining_fraction; /* continuous retime: exact count minus ceil */
} XenolithNoise;

typedef struct {
    float sample_rate, phase_scale, base, index, ratio, drift;
    float tonic[XENOLITH_MAX_CELLS], drift_noise[XENOLITH_MAX_CELLS];
    uint32_t mod_phase[XENOLITH_MAX_CELLS], car_phase[XENOLITH_MAX_CELLS];
    int32_t mod_increment[XENOLITH_MAX_CELLS];
    XenolithNoise noise[XENOLITH_MAX_CELLS];
    uint32_t random_state, control_block, control_remaining;
    uint32_t cells, extra_random_state;
    float cell_gain;
    float spread_scale[XENOLITH_MAX_CELLS];
    float control_remaining_fraction;
    /* Optional cell keyboard. Kept after the preserved dry-core prefix.
     * All zero until enabled; default sample loop branches once then remains
     * unchanged. Gains/frequencies belong exclusively to the audio owner. */
    uint32_t cell_gates_enabled, cell_gates_absolute;
    uint32_t cell_gates_processing; /*0=original,1=masked/fading,2=all unity */
    uint32_t cell_gate_mask[2];
    float cell_gate_gain[XENOLITH_MAX_CELLS];
    float cell_gate_frequency[XENOLITH_MAX_CELLS];
    float cell_gate_step;
    XenolithTopologyConfig topology;
    /* Earlier indices contain this sample's gated carrier; later indices
     * still contain the preceding sample. LOOP4 reads its final node before
     * ascending evaluation overwrites it. SELF reads each own preceding value
     * before replacing it: exactly one INTERNAL sample of delay. Unused default
     * history stays zero. */
    float graph_carrier[XENOLITH_MAX_CELLS];
    /* Optional live count transition. Unused default path does no per-cell
     * extra work. cells is the requested prefix; render_cells temporarily
     * includes a fading outgoing suffix. Phase/noise/history are not reset. */
    uint32_t render_cells, count_smoothing, count_gain_remaining;
    uint32_t count_gain_finalize,count_mask_free,count_unity_prefix;
    float count_gain_target, count_gain_step;
    float count_weight,count_weight_step;
    float layout_remaining, layout_offset, layout_offset_target;
    float count_layout_fraction;
    float spread_target[XENOLITH_MAX_CELLS];
    float frequency_target[XENOLITH_MAX_CELLS];
    float count_cell_gain[XENOLITH_MAX_CELLS];
    float count_cell_step[XENOLITH_MAX_CELLS];
    XenolithCalculationConfig calculation;
    uint32_t calculation_active,calculation_shift,calculation_stride,calculation_mask;
    float calculation_fraction;
    float linear_tonic_bound; /* Conservative absolute tonic maximum; control-rate. */
    float count_fraction;
    uint32_t count_elapsed;
    float interpolation_weight; /* Runtime relief only;1 preserves patch lookup. */
    /* Runtime erosion, never serialized into the instrument's calculation or
     * graph configuration. A separate RNG leaves the drift streams intact. */
    float load_table_loss,load_dither,load_cell_ratio,load_link_gain;
    uint32_t load_erosion_active,load_random_state,load_shift[2];
    float load_fraction[2],load_phase_step[2];
    uint32_t load_chosen_shift[2],load_noise_word;
    /* Optional performance controls. Zero glide and unit spread preserve the
     * established sound exactly. Runtime targets are never patch state. */
    float glide_ms,spread_amount,spread_current;
    float base_target,base_glide_start,base_glide_elapsed,base_glide_frames,base_glide_inverse;
    float cell_glide_elapsed,cell_glide_frames,cell_glide_inverse;
    float cell_glide_start[XENOLITH_MAX_CELLS];
    XenolithGraphConfig graph_config;
    XenolithGraph graph;
    XenolithFlavourConfig flavour;
    uint32_t flavour_active;
    float flavour_tap_loss;
    uint8_t flavour_series_child[XENOLITH_MAX_CELLS];
    XenolithCouplingConfig coupling;
    uint32_t coupling_active;
    float coupling_coefficient,coupling_drive;
    float coupling_x[XENOLITH_MAX_CELLS],coupling_y[XENOLITH_MAX_CELLS];
    float coupling_mean,coupling_bus;
    uint32_t coupling_remaining;
    float coupling_depth,coupling_bias,coupling_coefficient_target,coupling_drive_target;
    float coupling_depth_step,coupling_bias_step,coupling_coefficient_step,coupling_drive_step;
} XenolithCore;

/* Protected audio-owner transaction; optional XIP admission is required.
 * Methods transform the local modulator, not the global receiving FM law.
 * COMMON subtracts the preceding sample's gain-normalized carrier mean.
 * BIAS is a dimensionless offset; TIME follows the internal calculation rate;
 * ALLPASS DELAY is in internal samples. Output is bounded to[-1,1] before
 * mixing, retaining the receiving FM laws' established input bounds.
 * OFF or DEPTH0 uses the exact existing dispatch. Method changes clear only
 * this stage's memory, preserving oscillator/drift/graph state. */
int xenolith_set_coupling(XenolithCore *state,const XenolithCouplingConfig *config);

/* Protected audio-owner transaction. Each FM law retains its own option.
 * RETURN_NEGATIVE reverses existing one-tick returns; RETURN_ALTERNATING
 * uses positive returns at even receivers and negative at odd receivers.
 * Neither adds edges. CASCADE softens only the audible tap of a SERIES
 * parent to1/(1+.5*LINK), leaving its internal carrier/edge signal intact.
 * The earliest SERIES child's existing count fade controls that tap.
 * No phase/noise/history reset. NORMAL keeps the established dispatch.
 * Non-NORMAL execution requires the parent's optional-lookup admission. */
int xenolith_set_flavour(XenolithCore *state,const XenolithFlavourConfig *config);

/* Audio-owner snapshot transaction. Normal prefix append/pop retains phases,
 * drift and delayed carrier history. Config preparation is bounded to64cells;
 * callers may prepare the graph in foreground and use the prepared entrance.
 * Ordinary count changes still go through xenolith_transition_cells. */
int xenolith_set_topology_graph(XenolithCore *state,const XenolithTopologyConfig *topology,
    const XenolithGraphConfig *graph);
int xenolith_set_topology_graph_prepared(XenolithCore *state,const XenolithTopologyConfig *topology,
    const XenolithGraphConfig *config,const XenolithGraph *prepared);

/* Audio owner, normally once per output block. Clamp0..1 without touching
 * stored calculation controls, phases, drift or graph layout.1 is the exact
 * existing renderer;0 omits fractional interpolation and its second table
 * fetch. Intermediate values taper the interpolation delta. The caller ramps
 * this value; partial blends are not promised to reduce execution cost. */
void xenolith_set_interpolation_weight(XenolithCore *state,float weight);
/* Audio-owner controls, normally supplied by a smoothed governor frame.
 * loss0..5 reduces each oscillator's effective GLO table address bits, floored
 * at2bits. Fractional loss selects adjacent resolutions once per control tick;
 * dither0..1 adds up to +/-half of that address interval before lookup.
 * These deliberate spectral changes are not anti-aliasing or free CPU work.
 * cell_ratio.375..1 fades a four-cell PARALLEL suffix; silent carriers skip
 * lookup but their phase integration, modulators and drift continue. Linked
 * graphs ignore aperture. link_gain0..1 scales only existing graph edges.
 * Defaults(0,0,1,1) retain the original kernels and output exactly. This call
 * neither resets oscillators nor edits the stored count/topology/calculation.
 * The caller must ramp controls; no internal second smoothing clock is added. */
void xenolith_set_load_erosion(XenolithCore *state,float table_loss,float dither,
    float cell_ratio,float link_gain);

/* Audio owner under the external protected update gate. Strict validation
 * precedes mutation; no phase/noise/feedback/history reset or table allocation.
 * PULSE's rectangle modulator bypasses sine lookup and is unaffected by MOD
 * targeting; its carrier still supports the calculation controls. */
int xenolith_set_calculation(XenolithCore *state,const XenolithCalculationConfig *config);

/* No heap allocation, libm, reverb, envelope or output conversion.
 * Device init copies the shared8KB sine table to .pool RAM once. Call init
 * before audio/exclusive ownership; later states reuse that exact table.
 * Ordinary host builds use the immutable original without the extra buffer. */
void xenolith_init(XenolithCore *state, float sample_rate, uint32_t seed);
/* Base20..1200Hz, depth0..120ST in EXP or index0..64 in every other mode,
 * modulator/carrier ratio0.001..32, drift0..1.
 * A ratio of0.001 gives a0.084Hz modulator at tonic84Hz. The sampled sine
 * modulator and signed carrier equations remain unchanged at slow rates.
 * DRIFT scales the original LFNoise2 by0.10: .20 gives nominal +/-2%,
 * 1 gives nominal +/-10%. LFNoise2 may overshoot its nominal bipolar range.
 * Wide FM may alias at the chosen sample rate; phase increments wrap without
 * clipping or rectifying negative instantaneous carrier frequency.
 */
void xenolith_set(XenolithCore *state, float base, float index, float ratio, float drift);
/* Audio owner. TONIC glide0..2000ms is a linear-Hz target ramp, timed by
 * actual core samples and retained across reclocking. Repeated targets do
 * not restart it. Applies to both xenolith_set and absolute cell assignments.
 * SPREAD0..2 scales the existing compact offsets / parallel octave span;
 *1 copies the original exact coefficients,0 collapses tuning around TONIC.
 * Width edits use the established20ms layout glide. Absolute keyboard notes
 * retain their supplied intervals; their allocator owns any per-note spread.
 * No RNG/phase reset, allocation or per-sample transcendental calculation. */
void xenolith_set_performance(XenolithCore *state,float glide_ms,float spread);
/* Configure before playback. The original .kr timing depends on server block size. */
void xenolith_set_control_block(XenolithCore *state, uint32_t frames);
/* Audio-owner rate change for optional oversampling. Preserve oscillator and
 * LFNoise2 state; scale control frames with sample rate to keep .kr timing.
 * Internal sample rate supports1000..192000Hz. When rounded control frames
 * change the control tick rate, retime remaining noise segments and derivatives.
 * Call while the external transition gate is muted. No reseed or phase reset.
 */
void xenolith_reclock(XenolithCore *state, float sample_rate, uint32_t control_frames);
/* Continuous-clock variant. Retains fractional remaining sample/control-tick
 * counts across repeated rate changes instead of accumulating ceil bias.
 * No phase reset or RNG draws. Audio owner, once per output block; unlike a
 * structural reclock this does not require the application's mute gate.
 */
void xenolith_retime(XenolithCore *state, float sample_rate, uint32_t control_frames);
/* Runtime active prefix, clamped to 1..64. Call from the audio owner between
 * samples/blocks, with an external fade for a click-free audible transition.
 * Retained cells keep state; newly active cells restart phase and drift state.
 * Setting the current count is a no-op. The default eight-cell equations are retained.
 */
void xenolith_set_cells(XenolithCore *state, uint32_t count);
/* Audio owner. Same-clock smooth1 adds/removes only the changed suffix with
 * 5ms cell/density gain ramps and a20ms layout glide at the ordinary control
 * cadence. Existing oscillator/noise state is retained, including regrowth.
 * smooth0 finalizes the prefix immediately, for an externally muted true
 * clock/topology boundary; no outgoing suffix may run at an accelerated rate.
 * Combine with the existing cell mask. No heap or per-sample transcendental.
 * Returns0 for null/invalid smooth; count is clamped1..64. */
int xenolith_transition_cells(XenolithCore *state,uint32_t count,uint32_t smooth);
/* Audio owner, under the parent's structural transition gate. Strict invalid
 * requests leave the entire core unchanged; identical requests are a no-op.
 * Retains oscillator/noise/control phases, clears only graph edge history and
 * prepares pitch from the current drift without ticking or drawing RNG.
 * PARALLEL/LINEAR is the unchanged original path; link/feedback are ignored.
 * CHAIN4: i-1 parent except roots i%4==0. BRANCH: parent(i-1)/2.
 * LOOP4 adds each complete four-cell group's final carrier from the PREVIOUS
 * sample to its root; incomplete groups stay open. Edge addresses do not
 * move with count. SELF adds each cell's own preceding gated carrier, with
 * no feedforward edges; LINK is ignored. All non-PARALLEL graphs, including
 * SELF, use compact base*(1+ordinary drift)+i*.46Hz tuning unless a
 * spectral keyboard overrides it. Every cell retains its own two oscillators.
 * LINEAR adds tonic*(link*parent+feedback*delayed) to signed frequency;
 * PHASE adds index*modsin+link*parent+feedback*delayed radians to carrier
 * output phase, while carrier phase advances at unmodulated tonic.
 * EXPONENTIAL advances carrier at tonic*2^(index/12*modsin + link*parent
 * + feedback*delayed). INDEX0..120 is SEMITONES; LINK and FEEDBACK are OCTAVES
 * in this mode. For LINEAR they are fractions of tonic; for PHASE radians.
 * No exponential DC/pitch correction is applied: mean frequency generally
 * rises with depth, even for a zero-mean sine. Arbitrary graph/feedback sources
 * have no universal tuning correction. This is not through-zero FM.
 * The complete control domain bounds the exponent to +/-14.95octaves.
 * A65-point fractional-octave exp2 table plus exact powers of two has no
 * per-sample libm call or abrupt exponent/frequency clamp. Relative lookup
 * error is below1.48e-5 (0.026cents); sample/float phase quantization remains.
 * Large instantaneous frequencies wrap and may alias at the chosen internal
 * rate; extreme wrapping also loses float phase resolution. The parent chooses
 * measured workload caps; this API has no count cap. Requests at0..64 retain
 * the earlier equations; new headroom can change formerly clipped modulation.
 * Leaving EXP clamps the current depth to64 before another mode processes it.
 * PULSE uses the LINEAR frequency law with a bipolar50%-duty rectangle in
 * place of the sine modulator. INDEX0..64 is deviation/tonic, LINK/FEEDBACK
 * likewise retain LINEAR units. RATIO sets the switching rate. Carrier phase
 * remains continuous at switches; its frequency may cross zero. No BLEP or
 * alias-free rectangle claim is made.
 * WARP advances carrier at tonic, offsets lookup phase by the graph edges in
 * PHASE radians, then warps each half-cycle identically. Within a half-cycle
 * h0..1, W(h)=5h for h<=.1, else .5+(h-.1)*5/9. The actual lookup coordinate
 * is h+a*(W(h)-h), a=(index/64)*(1+modsin)/2. RATIO is the warp-motion rate;
 * UI may display INDEX0..64 as normalized0..100%. Half/full-cycle endpoints
 * stay continuous and the static warp preserves half-wave antisymmetry.
 * Moving warp or graph coupling can still generate DC and aliasing. No CZ or
 * analogue oscillator emulation is claimed. Zero local depth bypasses the
 * warp exactly; graph phase offsets remain active, as in PHASE at index0.
 * Parent signals include their cell-mask gain: fully gated nodes send zero.
 * All carriers are still summed with the existing density gain. A chain is
 * modulation coupling, not serial audio filtering. At most one feedforward
 * and one delayed feedback edge per cell; no allocation/recursion or hidden
 * count cap. PARALLEL in every mode uses the ordinary bank's tuning.
 */
int xenolith_set_topology(XenolithCore *state,const XenolithTopologyConfig *config);
/* Optional per-cell mask, audio owner only. Returns0 without changing state
 * for invalid flags or a missing/nonfinite/out-of-range spectral prefix.
 * enabled/absolute are0/1. Masks cover64cells; bits outside active count are
 * retained for later count growth. RELATIVE(absolute0) keeps existing tuning;
 * SPECTRAL(absolute1) uses prepared20..8000Hz frequencies[0..cells-1], retaining
 * each cell's ordinary drift, index and modulator/carrier ratio equations.
 * No exp/table generation here. Repeated identical requests are a no-op.
 * Gains travel a full0..1 in5ms at the current INTERNAL sample rate. Silent
 * unselected cells may freeze oscillator phases, while all active-prefix drift
 * states keep their ordinary control clock and independent random streams.
 * Disabling fades cells back to unity, then restores the original fast path.
 * Bank gain remains unchanged: selecting fewer cells does not boost gain.
 * Count growth starts new gains at0 and supplies safe base-frequency fallback;
 * reapply a freshly prepared count-specific spectral layout after count edits.
 */
int xenolith_set_cell_gates(XenolithCore *state,uint32_t enabled,
    uint32_t mask_lo,uint32_t mask_hi,uint32_t absolute,const float *frequencies);
/* Mono carrier sum *0.12. Counts1..8 retain8/N gain and i*.46Hz offsets.
 * Above8: gain=sqrt(8/N). PARALLEL counts9..16 retain the original
 *0.025+0.025*(N-9)/23 total octave span. Counts17..32 smoothly open from
 * the16-cell span to0.125octaves; above32, span=.125+5.875*((N-32)/32)^3,
 * ending6octaves at64. Other graph layouts retain compact additive offsets.
 * Centered on base with deterministic irregular spacing. Prepared scales add
 * no transcendental work to the sample loop. The raw coherent peak bound
 * grows to0.12*sqrt(8*N), about2.7153 at64, before trim/effects. Equal RMS is
 * only an incoherent-power reference, not a guarantee for correlated cells.
 */
float xenolith_sample(XenolithCore *state);

#endif
