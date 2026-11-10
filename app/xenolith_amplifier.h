/* SPDX-License-Identifier: MPL-2.0
 * XX29 independent asymmetric soft clipping with wet-only DC rejection. */
#ifndef XX29_XENOLITH_AMPLIFIER_H
#define XX29_XENOLITH_AMPLIFIER_H
#include <stdint.h>
typedef struct {
    float drive, bias, colour; /* drive0..1, bias-.75...75, colour0..1 */
} XenolithAmplifierConfig;

typedef enum { XAMP_CUBIC=0, XAMP_SPACE_3_2=1, XAMP_SOFTSIGN=2, XAMP_FOLD=3 } XenolithAmplifierCurve;
typedef struct {
    uint32_t curve;
    float highpass_hz,lowpass_hz,gain_db;
} XenolithAmplifierShapeConfig;
typedef struct {
    float amount,bias,mix; /* amount/mix0..1, bias-1..1 circuit volts */
    uint32_t stages; /* 0/1/2/3 selects1/2/4/6 cascaded diode/op-amp cells */
} XenolithFolderConfig;
typedef struct {
    XenolithFolderConfig config;
    float dc_pole,dc_scale;
    uint32_t ramp_samples;
} XenolithFolderPrepared;
typedef struct {
    XenolithFolderPrepared target;
    float amount,bias,mix,stage;
    float amount_step,bias_step,mix_step,stage_step;
    float offset,offset_target,offset_step;
    float previous_input,previous_u,previous_wet,dc_state;
    uint32_t remaining,offset_remaining,ready,configured;
    float previous_integral[4],bias_value[4],cached_bias;
    uint32_t primitive_mask,bias_mask;
} XenolithFolder;
/* A separate prepared snapshot avoids multiplying the existing small VCA
 * preparation ABI. Tables are CPU-owned after apply, not borrowed pointers. */
typedef struct {
    float table[128];
    float amount; /* normalized0..1; feedback coefficient k=.75*amount */
    uint32_t curve;
} XenolithFeedbackPrepared;
typedef struct {
    float current[128],target[128];
    float mix,mix_step,k,k_target,k_step,amount_target;
    float scale,scale_target,scale_step;
    uint32_t remaining,from_zero,to_zero,configured,curve_target;
} XenolithFeedback;
typedef struct {
    float gain,bias,colour,bias_offset,dc_pole,dc_scale;
    uint32_t ramp_samples;
    uint32_t curve;
    float tone_pole,tone_mix,output_gain;
} XenolithAmplifierPrepared;
typedef struct {
    XenolithAmplifierPrepared target;
    float gain,bias,colour,bias_offset;
    float gain_step,bias_step,colour_step;
    float previous_wet,dc_state;
    uint32_t remaining;
    float curve,curve_step,dc_pole,dc_scale,dc_pole_step,dc_scale_step;
    float tone_pole,tone_pole_step,tone_mix,tone_mix_step,tone_state;
    float output_gain,output_gain_step;
    uint32_t coefficients_ready;
    float fold_offset,fold_target,fold_step;
    uint32_t fold_remaining;
    XenolithFolder folder;
    XenolithFeedback feedback;
    float load_bias_control,load_bias_stress,load_bias,load_bias_target,load_bias_step;
    float load_bias_envelope,load_bias_attack,load_bias_release;
    uint32_t load_bias_remaining;
    float load_supply,load_supply_step;
} XenolithAmplifier;
_Static_assert(sizeof(XenolithAmplifierConfig)==12, "Amplifier config ABI changed");
_Static_assert(sizeof(XenolithAmplifierShapeConfig)==16, "Amplifier shape ABI changed");
_Static_assert(sizeof(XenolithFolderConfig)==16, "Folder config ABI changed");
/* Separate post-VCA diode folder: defaults amount0, bias0, mix1, stages1.
 * Amount0 is exact bypass; positive FOLD modulation activates it independently
 * of amplifier COLOUR. Legacy ShapeConfig remains valid for old patches. */
void xenolith_folder_defaults(XenolithFolderConfig *config);
int xenolith_folder_config_valid(const XenolithFolderConfig *config);
int xenolith_folder_prepare(XenolithFolderPrepared *prepared,
                           const XenolithFolderConfig *config,float sample_rate);
void xenolith_amplifier_apply_folder(XenolithAmplifier *state,
                                    const XenolithFolderPrepared *prepared);
/* Radiotron's static negative-feedback connection, y=f(u-k*y), around the
 * chosen generic VCA curve. Foreground resolves128-point transfer tables;
 * audio interpolates, with no iterative circuit solve. Partial input makeup
 * 1+.65*k retains more level; it is a design trim, not a derived valve law.
 * TRIFOLD is periodic
 * and has an exact analytic inverse. Amount0 preserves the old curve exactly. */
int xenolith_feedback_prepare(XenolithFeedbackPrepared *prepared,float amount,uint32_t curve);
void xenolith_amplifier_apply_feedback(XenolithAmplifier *state,
                                      const XenolithFeedbackPrepared *prepared);
void xenolith_amplifier_defaults(XenolithAmplifierConfig *config);
int xenolith_amplifier_config_valid(const XenolithAmplifierConfig *config);
/* Shape defaults: CUBIC, HP10Hz, TONE0/OFF, GAIN0dB. Limits: HP5..200Hz;
 * TONE0/OFF or100..20000Hz; GAIN-18..+6dB. Existing config stays12bytes. */
void xenolith_amplifier_shape_defaults(XenolithAmplifierShapeConfig *config);
int xenolith_amplifier_shape_valid(const XenolithAmplifierShapeConfig *config);
/* Foreground: strict finite config, sample_rate1000..192000. Invalid requests
 * leave prepared unchanged. No I/O/heap. Only this preparation computes exp. */
int xenolith_amplifier_prepare(XenolithAmplifierPrepared *prepared,
                              const XenolithAmplifierConfig *config,float sample_rate);
/* Foreground extended preparation. Filters and output gain affect only the
 * coloured branch. Requested cutoffs are bounded below0.45*sample_rate.
 * Two table curves are generic transfer functions, not physical tube models.
 * FOLD is a continuous periodic triangle reflection with threshold.25, no
 * additional state/table. DRIVE/BIAS sweep its folds; existing filters/blend
 * remain. Corners produce harmonics/aliasing; no anti-aliasing claim. */
int xenolith_amplifier_prepare_extended(XenolithAmplifierPrepared *prepared,
                              const XenolithAmplifierConfig *config,
                              const XenolithAmplifierShapeConfig *shape,float sample_rate);
/* Audio owner: initialize before rendering; apply completed prepared snapshots
 * at a callback boundary. Compatible edits ramp over approximately5ms without
 * resetting filter history. Clean controls can change immediately while bypassed. */
void xenolith_amplifier_init(XenolithAmplifier *state);
void xenolith_amplifier_apply(XenolithAmplifier *state,const XenolithAmplifierPrepared *prepared);
/* Audio owner control tick. Finite offsets clamp to[-1,1], ramp5ms and add
 * to folder amount independently of COLOUR. MIX0 still bypasses. Unconfigured
 * legacy API users retain curve-FOLD drive modulation. Once the new folder is
 * configured it owns this destination exclusively. Nonfinite is rejected. */
int xenolith_amplifier_set_fold_offset(XenolithAmplifier *state,float offset);
/* Audio owner once per output block. Load0..1 drives VCA operating-point
 * colour and an averaged reservoir deficit (approximately80ms RC).
 * The independent folder retains its drive, family and ADAA history.
 * Nonfinite input is ignored; finite input clamps0..1. */
void xenolith_amplifier_set_load_sag(XenolithAmplifier *state,float stress);
/* Runtime signed operating-point drift, -1..1. The coloured branch follows
 * input magnitude and shifts its bias by at most .22*load*bias*envelope.
 * Five-ms control smoothing, quiescent subtraction and DC rejection remain.
 * Reservoir KCL: tau*d' = rho*a-(1+rho*a)*d; rho=.5*abs(load*bias),
 * a=clamp(abs(DRIVE*input),0,1), d<=1/3. H=1-d scales the coloured transfer
 * as H*(f(DRIVE*input/H+bias)-f(bias)). This changes available swing with
 * signal history, without moving folder settings. Automatic wet is the
 * maximum of user COLOUR, .35*abs(load*bias), and .70*d. At default-.15
 * automatic wet never exceeds5.25%. Zero load/bias returns to clean bypass
 * after reservoir recovery. This is not a calibrated valve/rectifier model. */
void xenolith_amplifier_set_load_bias(XenolithAmplifier *state,float bias);
/* Include this predicate in the callback admission test so zero user COLOUR
 * still renders automatic load colour and its pending five-ms ramps. */
int xenolith_amplifier_load_active(const XenolithAmplifier *state);
/* Mono after ADSR/amp gain, before tank. COLOUR0 plus folder amount0 (including
 * modulation), with inactive load colour, gives exact bypass. Folder uses
 * one ADAA quotient per settled
 * sample, two only during a stage morph; no runtime transcendental/iteration.
 * ADAA reduces new aliasing; neither stage is an alias-free or tube model. */
float xenolith_amplifier_process(XenolithAmplifier *state,float input);
/* Audio owner panic/clear: discard only this small DC history; retain controls. */
void xenolith_amplifier_clear(XenolithAmplifier *state);
#endif
