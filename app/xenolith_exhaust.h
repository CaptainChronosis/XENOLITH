/* SPDX-License-Identifier: MPL-2.0
 * XX29 compact exhaust: independently implemented comb + diffuse stereo FDN.
 * Inspired by the original X_BUS structure; not a GVerb port or room model.
 */
#ifndef XX29_XENOLITH_EXHAUST_H
#define XX29_XENOLITH_EXHAUST_H
#include <stdint.h>
#include "xenolith_dispersion.h"

#define XENOLITH_EXHAUST_COMB 44102u
#define XENOLITH_EXHAUST_FDN 18122u
#define XENOLITH_EXHAUST_DIFFUSERS 1858u
#define XENOLITH_EXHAUST_LINES 8u
#define XENOLITH_EXHAUST_ZOH 0u
#define XENOLITH_EXHAUST_LINEAR 1u
#define XENOLITH_EXHAUST_TANK_ZOH 0u
#define XENOLITH_EXHAUST_TANK_LINEAR 1u
#define XENOLITH_EXHAUST_CLOUD4 2u
#define XENOLITH_EXHAUST_CLOUD8 3u
#define XENOLITH_EXHAUST_RING 4u
#define XENOLITH_EXHAUST_PAIRS 5u
#define XENOLITH_EXHAUST_CLOUD4_ZOH 6u
#define XENOLITH_EXHAUST_CLOUD8_ZOH 7u
#define XENOLITH_EXHAUST_RING_ZOH 8u
#define XENOLITH_EXHAUST_PAIRS_ZOH 9u
#define XENOLITH_EXHAUST_STRUCTURE_COUNT 10u
#define XENOLITH_EXHAUST_FAMILY_COUNT 5u
/* UI families:0TANK,1CLOUD4,2CLOUD8,3RING,4PAIRS. IDs0..3 retain V12 meaning. */
uint32_t xenolith_exhaust_structure_family(uint32_t mode);
uint32_t xenolith_exhaust_structure_reconstruction(uint32_t mode);
uint32_t xenolith_exhaust_structure_mode(uint32_t family,uint32_t reconstruction);
#define XENOLITH_EXHAUST_ECHO_MIN 0.002f
#define XENOLITH_EXHAUST_ECHO_MAX 32.0f
/* Foreground controls: fixed memory buys longer time at a slower wet clock.
 * Requested seconds remain stored; prepared time is capped to actual capacity. */
float xenolith_exhaust_echo_limit(float wet_rate);
float xenolith_exhaust_echo_step(float seconds,int32_t delta,float wet_rate);

/* Prepared in foreground, published as a completed snapshot, then copied by
 * the audio owner at a block boundary. Clock coefficients are prepared here,
 * never by the sample loop. 160-byte prepared snapshot including the optional short allpass bank.
 * divider1..32 selects the exact legacy scheduler; divider0 is fractional. */
typedef struct {
    float time, absorb, decay, mix;
    float comb_gain, comb_pole, damping_pole, dc_pole;
    float fdn_gain[8];
    float host_rate, sample_rate, delay_slew, damp_hz, fade_step;
    uint32_t divider;
    uint32_t reconstruction, phase_increment;
    float diffusion, width;
    uint32_t structure;
    float requested_damp_hz;
    uint32_t damp_model;
    float input_pole;
    XenolithDispersionPrepared dispersion;
} XenolithExhaustConfig;
_Static_assert(sizeof(XenolithExhaustConfig)==160, "Prepared FX layout changed");

typedef struct {
    int16_t comb[XENOLITH_EXHAUST_COMB];
    float fdn[XENOLITH_EXHAUST_FDN];
    float diffuser[XENOLITH_EXHAUST_DIFFUSERS];
    uint32_t comb_write, fdn_write[8], diffuser_write[4];
    uint32_t random_state, modulation_count, coefficient_count, clipped_writes;
    float sample_rate, time, absorb, decay, mix;
    float comb_delay, target_delay, comb_gain, comb_lowpass;
    float comb_pole, damping_pole, dc_pole, previous_input, dc_state;
    float modulation, modulation_target, modulation_step;
    float fdn_gain[8], damping_state[8];
    XenolithExhaustConfig pending;
    float host_rate, delay_slew, damp_hz, fade_step, clock_fade;
    float held_left, held_right;
    uint32_t divider, clock_count, transition, wet_ticks;
    float previous_left, previous_right;
    uint32_t reconstruction, phase_increment, clock_phase;
    XenolithExhaustConfig sweep_target;
    uint32_t live_clock, sweep_remaining;
    float wander_seconds, wander_elapsed;
    float diffusion, width, diffusion_target, width_target;
    float diffusion_step, width_step;
    uint32_t tank_remaining;
    uint32_t structure;
    float cloud, cloud_target, cloud_step;
    float requested_damp_hz, input_pole, input_state;
    uint32_t damp_model;
    float input_blend, input_blend_target, input_blend_step;
    float ring_weight, pairs_weight, ring_target, pairs_target, ring_step, pairs_step;
    XenolithDispersion dispersion;
    float load_sag, sag_diffuser, sag_interpolation,load_shape,sag_damping;
    float sag_diffuser_target,sag_interpolation_target,sag_damping_target;
    float sag_diffuser_step,sag_interpolation_step,sag_damping_step;
    uint32_t sag_remaining;
} XenolithExhaust;

/* Audio-owner block control. Base clock/decay/patch settings remain unchanged.
 * Targets ramp over20ms of OUTPUT time, independent of the wet clock. Darkens
 * feedback and fades the last two driven input allpasses to bypass. Bypassed
 * histories still advance/decay; no frozen tail returns. Comb reads remain
 * linear until late stress, with at most15% hold blend. Zero preserves old PCM. */
void xenolith_exhaust_set_load_sag(XenolithExhaust *state,float stress);
/* Runtime shape0..2 scales only added feedback damping; default1. Diffuser
 * pruning and comb interpolation retain their independent stress schedules. */
void xenolith_exhaust_set_load_shape(XenolithExhaust *state,float shape);

/* Put this large object in PLAT_POOL, never on the callback stack.
 * The host runs at44100 Hz. CLOCK divides only the wet engine by1..32.
 */
void xenolith_exhaust_init(XenolithExhaust *s, float sample_rate);
/* Single-precision coefficient exponentials occur in prepare(), never in
 * process() or apply(). Existing prepare() selects divider1.
 * time=.002..32 seconds requested, capped to44100/wet_rate seconds,
 * absorb=0..1 (bright..dark; .6 gives the original 800-Hz comb lowpass),
 * decay=.3..12 seconds (low-frequency FDN T60), mix=0..1.
 */
void xenolith_exhaust_prepare(XenolithExhaustConfig *config, float sample_rate,
                             float time, float absorb, float decay, float mix);
/* Effective wet rate=host_rate/divider. DAMP is capped to0.4*wet_rate;
 * config.damp_hz is the actual cutoff for display. ECHO remains seconds,
 * DECAY remains nominal low-frequency FDN T60. Fixed tank/allpass lengths
 * grow in seconds as CLOCK falls. Input aliasing and output hold images are
 * intentional colour, not an anti-aliased sample-rate converter.
 */
void xenolith_exhaust_prepare_clock(XenolithExhaustConfig *config, float host_rate,
                                   uint32_t divider, float time, float absorb,
                                   float decay, float mix);
/* Explicit wet Hz, bounded to host_rate/32..host_rate. Exact host/divider
 * values reuse legacy coefficients and scheduler, preserving ZOH PCM.
 * Otherwise a Q32 phase accumulator schedules at most one wet tick/output.
 * LINEAR interpolates two completed wet samples: causal, approximately one
 * wet-tick added latency, with no input anti-alias filtering. Invalid recon
 * selects ZOH. Configs are not a serialized patch ABI. */
void xenolith_exhaust_prepare_rate(XenolithExhaustConfig *config, float host_rate,
                                  float wet_rate, uint32_t reconstruction,
                                  float time, float absorb, float decay, float mix);
/* Existing preparation APIs retain DIFFUSION=.65 and WIDTH=1 exactly.
 * DIFFUSION0...85 is the coefficient of the four existing input allpasses;
 * zero retains their pure delays. It does not alter FDN decay/feedback.
 * WIDTH0..1 scales only the wet stereo side signal:0=mono,1=original stereo.
 * It never changes the tank's feedback or memory, nor widens past the original
 * decoder. Invalid values clamp as with the original preparation APIs.
 */
void xenolith_exhaust_prepare_tank_rate(XenolithExhaustConfig *config,float host_rate,
    float wet_rate,uint32_t reconstruction,float time,float absorb,float decay,float mix,
    float diffusion,float width);
/* Five families, ten paired IDs as mapped above. Original IDs0..3 retain their
 * exact meanings. CLOUD reads early existing histories; RING permutes feedback;
 * PAIRS uses four normalized2x2 Hadamard blocks. No delay-memory growth.
 * Invalid structure selects TANK_ZOH. Decoder/matrix weights morph over50ms;
 * reconstruction changes retain the protected fade. This API keeps old DAMP. */
void xenolith_exhaust_prepare_structure_rate(XenolithExhaustConfig *config,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width);
/* New physical DAMP. Model0 retains legacy absorb coefficients and no inputLP.
 * Model1 requests20..16000Hz, capped to.45*wet_rate; exact digital one-pole
 * -3dB preparation feeds excitation and existing line loss filters. Existing
 * damp_hz reports effective Hz; requested_damp_hz retains the bounded request.
 * All older prepare APIs remain legacy model0 wrappers. No audio transcendental.
 * RING/PAIRS matrix weights and excitation blend ramp50ms from current values. */
void xenolith_exhaust_prepare_fx_rate(XenolithExhaustConfig *config,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width,float damp_hz,uint32_t damp_model);
/* Optional short normalized-allpass groups inside the existing FDN.
 * Existing preparation APIs select exact OFF; this adds no moving delays. */
void xenolith_exhaust_prepare_extended_rate(XenolithExhaustConfig *config,float host_rate,
    float wet_rate,uint32_t structure,float time,float absorb,float decay,float mix,
    float diffusion,float width,float damp_hz,uint32_t damp_model,
    const XenolithDispersionConfig *dispersion);
/* Audio owner only. Same-clock controls apply immediately; clock changes
 * queue a small snapshot and fade wet down/up over5ms each. Reconstruction
 * changes use the same fade. Buffers survive; no clear in process/apply.
 * New diffusion/width/structure weights ramp over50ms of OUTPUT samples, independently
 * of clock sweeps and without clearing state. Replacement targets start from
 * the current values. Structural targets start when their muted clock adopts.
 * transition:0=settled,1=fading out,2=fading in. divider0 means fractional.
 */
void xenolith_exhaust_apply(XenolithExhaust *s, const XenolithExhaustConfig *config);
/* Audio owner: compatible same-host/same-reconstruction live control path.
 * Returns1 if accepted,0 if apply() must handle a structural transition.
 * Rate gestures keep tank/held history and clock phase. Integer anchors join
 * one fractional scheduler; only a rate gesture enables this new live path.
 * Same-clock controls before that retain legacy immediate/PCM semantics.
 * Call begin_block() once per audio block after accepting requests. */
int xenolith_exhaust_retime(XenolithExhaust *s, const XenolithExhaustConfig *config);
/* Bounded50ms linear ramp of rate and prepared coefficients, once/block.
 * No exp/log/heap/clear. Intermediate cutoff/T60 are interpolated approximations;
 * settled coefficients equal the exact prepared target. Not an SRC guarantee. */
void xenolith_exhaust_begin_block(XenolithExhaust *s, uint32_t output_frames);
/* Convenience for initialization/tests with exclusive state ownership.
 * Retains latest requested diffusion/width/structure, including during a clock fade. */
void xenolith_exhaust_set(XenolithExhaust *s, float time, float absorb,
                         float decay, float mix);
/* Clears all delay/filter memory but retains controls. Call while processing
 * is stopped/owned; this is a bulk operation, not an operation per sample.
 * Adopts the latest pending clock, resets held output and completes its fade.
 */
void xenolith_exhaust_clear(XenolithExhaust *s);
/* Finite normalized mono input, stereo output. MIX=0 returns input exactly;
 * wet state still advances, so bringing the mix up cannot resurrect a frozen
 * old tail. No allocation, trig/exp/pow, file I/O or final limiter in this call.
 */
void xenolith_exhaust_process(XenolithExhaust *s, float input,
                             float *left, float *right);
#endif
