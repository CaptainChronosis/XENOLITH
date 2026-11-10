/* SPDX-License-Identifier: MPL-2.0
 * XX29 bounded 6SL7/parallel-T model with derived buffered multimode taps.
 * See SHARED_FILTER_BASE_2026-11-02.md for circuit provenance and limits. */
#ifndef XX29_XENOLITH_FILTER_H
#define XX29_XENOLITH_FILTER_H
#include <stdint.h>

enum { XFILTER_OFF=0, XFILTER_PRE_DIGITAL=1, XFILTER_POST_DIGITAL=2,
       XFILTER_POST_FX=3, XFILTER_PRE_FX=4, XFILTER_PLACEMENT_COUNT=5 };
enum { XFILTER_LP=0, XFILTER_BP=1, XFILTER_HP=2, XFILTER_NOTCH=3,
       XFILTER_ALLPASS=4, XFILTER_TWIN_T=5, XFILTER_MODE_COUNT=6 };
enum { XFILTER_TWO_POLE=0, XFILTER_FOUR_POLE=1, XFILTER_BRIDGE=2,
       XFILTER_SLOPE_COUNT=3 };
typedef struct {
    uint32_t placement,mode,slope;
    float cutoff_hz,resonance;
} XenolithFilterConfig;
typedef struct {
    XenolithFilterConfig config;
    float sample_rate,effective_cutoff_hz;
    uint32_t ramp_samples;
} XenolithFilterPrepared;
typedef struct { float band,low; } XenolithFilterSection;

typedef struct {
    float cathode_ohms,headroom,coupling_hz,balance;
} XenolithFilterCircuitConfig;
_Static_assert(sizeof(XenolithFilterCircuitConfig)==16,"Filter circuit ABI changed");
typedef struct {
    XenolithFilterPrepared target;
    XenolithFilterSection section[2][2]; /* channel, cascade section */
    float sample_rate,inverse_rate;
    float cutoff_hz,cutoff_target,resonance,resonance_target;
    float g,k,inverse_denominator,g2,inverse_denominator2;
    float mix[3],mix_target[3],slope,slope_target,stagger,stagger_target,wet,wet_target;
    uint32_t remaining,control_remaining,wet_remaining;
    float wet_step;
    uint32_t faults;
    float tube_feedback,tube_linear[2],tube_dc_coefficient;
    float tube_dc[2][2],tube_residual_peak;
    uint32_t tube_solver_faults;
    XenolithFilterCircuitConfig circuit,circuit_target;
    uint32_t circuit_remaining;
    float tube_rho,tube_gain,tube_notch_band,tube_input_scale,tube_output_scale;
    float tube_band_scale,tube_notch_scale;
    float tube_feedback_total[2],tube_g_plus_k[2];
    float tube_excitation_scale,tube_input_feedback,tube_output_feedback;
    float load_sag,solver_linear_mix,load_shape;
    float load_sag_target,load_shape_target;
    uint32_t load_remaining,tube_early_solves;
} XenolithFilter;
/* Audio-owner block target: contract cutoff/Q and modestly change the existing
 * circuit's headroom/degeneration without overwriting panel/CV bases. Targets
 * slew over10ms at the existing coefficient tick. The nonlinear solve remains;
 * one Newton step is accepted only if the unchanged residual guard passes. */
void xenolith_filter_set_load_sag(XenolithFilter *state,float stress);
/* Runtime shape0..2 scales circuit colour; default1. Also slewed over10ms.
 * The bounded residual-checked solver optimization is independent of shape.
 * solver_linear_mix is retained as a zero runtime compatibility field. */
void xenolith_filter_set_load_shape(XenolithFilter *state,float shape);
_Static_assert(sizeof(XenolithFilterConfig)==20,"Filter config ABI changed");

/* OFF/LP/two-pole/8000Hz/resonance0. Every active mode uses the same nonlinear
 * 6SL7/parallel-T feedback cell. At default circuit settings normal modes
 * nominal Q .5..1.954; localQ<=2 throughout the circuit-control range;
 * no self-oscillation setting. Four-pole cascades identical derived taps,
 * not a fourth-order Butterworth alignment. BRIDGE feeds
 * both sections independently at center/sqrt2 and center*sqrt2, then averages
 * their outputs (upper corner capped .45rate). ALLPASS has unit magnitude in
 * TWO/FOUR_POLE only in the small-signal limit before the 3.183 Hz output
 * default coupling. Circuit controls can change damping/coupling.
 * Finite amplitude generates harmonics/changes magnitude; BRIDGE
 * also deliberately changes magnitude by phase interference. */
void xenolith_filter_defaults(XenolithFilterConfig *config);
int xenolith_filter_config_valid(const XenolithFilterConfig *config);
/* Bounded preparation, foreground or audio-owner callback boundary:
 * rate1000..192000, requested20..20000Hz, effective cutoff<=.45*rate.
 * No heap/libm; invalid leaves out unchanged. */
int xenolith_filter_prepare(XenolithFilterPrepared *out,
                           const XenolithFilterConfig *config,float sample_rate);
void xenolith_filter_init(XenolithFilter *state);
/* Audio owner at callback boundary. Compatible edits retain histories and
 * ramp over5ms. Placement changes clear only this small history: parent must
 * switch insertion points behind its structural gate, including OFF changes.
 * Entering/leaving TWIN_T also requires the parent's structural gate; its
 * feedback mapping and output normalization change and reset at that boundary.
 * Applying an identical base snapshot is a no-op; runtime CV stays separate. */
int xenolith_filter_apply(XenolithFilter *state,const XenolithFilterPrepared *prepared);
/* Audio-owner block retime for a continuously changing PRE clock. Retains
 * history and requested cutoff. POST placements use44100Hz. */
int xenolith_filter_retime(XenolithFilter *state,float sample_rate);
/* Audio-owner control tick. Finite values clamp to physical bounds; nonfinite
 * leaves state unchanged. No exp/tan: cutoff uses an internal float table.
 * Calling apply with the same base snapshot never resets these CV targets. */
int xenolith_filter_set_cutoff(XenolithFilter *state,float cutoff_hz);
int xenolith_filter_set_resonance(XenolithFilter *state,float resonance);
/* Separate circuit snapshot: defaults2200ohm/.25/3.183098862Hz/1;
 * bounds470..10000ohm/.0625..1/.5..100Hz/.5..1. CATHODE changes local
 * loaded degeneration with fixed100k plate load. HEADROOM is a declared
 * digital voltage scale, COUPLING the output AC pole, BALANCE linked passive
 * T arms R0=R/(2b),C0=2bC. All modes share these coefficients.
 * Audio-owner callback setter: strict validation before mutation;20ms ramp,
 * histories retained, same target no-op. Old base apply/retime preserve it. */
void xenolith_filter_circuit_defaults(XenolithFilterCircuitConfig *config);
int xenolith_filter_circuit_valid(const XenolithFilterCircuitConfig *config);
int xenolith_filter_set_circuit(XenolithFilter *state,const XenolithFilterCircuitConfig *config);
/* PRE_FX is mono after the amplifier and before the wet/dry split at44100Hz.
 * Call exactly once per mono sample OR stereo frame at the active placement.
 * Stereo has independent histories and advances shared controls only once.
 * Coefficients update about every0.36ms (1..64 active frames) with bounded
 * float arithmetic; no exp/tan runtime call or heap allocation.
 * wet entry/exit ramps every frame. Settled OFF returns input bit-for-bit.
 * Active input bounds +/-8, output +/-64, state fail-safe +/-1024; no alias
 * repair/anti-alias-free or specific analogue-circuit fidelity claim. */
float xenolith_filter_process_mono(XenolithFilter *state,float input);
void xenolith_filter_process_stereo(XenolithFilter *state,float *left,float *right);
/* Panic/reset small histories, retaining base config, CV targets and rate. */
void xenolith_filter_clear(XenolithFilter *state);
uint32_t xenolith_filter_active(const XenolithFilter *state);
/* TWIN_T approximates Radiotron Fig15.15's finite-gain6SL7 + parallel-T NFB.
 * Fixed two-step monotone algebraic solve puts tube curvature INSIDE feedback;
 * no pow/trig or unbounded iteration. ONE/TWO/BRIDGE mean complete selective
 * amplifier cells, not12/24dB lowpass slopes. The output-coupling HP defaults
 * to3.183Hz and rejects generated DC. Local incrementalQ<=2; default-circuit
 * nominal Q .25..1.954. Tube
 * and input gains are normalized out at output. Extra solver diagnostics are
 * retained by clear. Not calibrated6SL7/SPICE or grid-current/rail simulation. */
#endif
