/* SPDX-License-Identifier: GPL-3.0-or-later
 * XX29 original bounded modulation constructor, 2026. See engine notes.
 * Audio-owner API: no allocation, I/O, UI, threads or audio-core dependency.
 */
#ifndef XX29_XENOLITH_MODULATION_H
#define XX29_XENOLITH_MODULATION_H
#include <stdint.h>
#include "xenolith_random_memory.h"

#define XENOLITH_MOD_SAMPLE_RATE 44100u
#define XENOLITH_MOD_QUANTUM 32u
#define XENOLITH_MOD_ROUTES 8u
#define XENOLITH_MOD_LINKS 8u

enum { XM_SRC_NONE, XM_SRC_LFO_A, XM_SRC_LFO_B, XM_SRC_STEP, XM_SRC_SMOOTH,
       XM_SRC_WALK, XM_SRC_FUNCTION_A, XM_SRC_FUNCTION_B, XM_SRC_MIX_A,
       XM_SRC_MIX_B, XM_SRC_KEY, XM_SRC_SEQ_1, XM_SRC_SEQ_2, XM_SRC_SEQ_3,
       XM_SRC_SEQ_4, XM_SRC_ENV_AMP, XM_SRC_ENV_FILTER, XM_SRC_ENV_INDEX, XM_SRC_ENV_TONIC, XM_SOURCE_COUNT };
enum { XM_TRIANGLE, XM_SINE, XM_RAMP, XM_PULSE, XM_SHAPE_COUNT };
enum { XM_AD, XM_GATE, XM_CYCLE, XM_SLEW, XM_FUNCTION_MODE_COUNT };
enum { XM_SUM, XM_PRODUCT, XM_MAX, XM_MIX_MODE_COUNT };
enum { XM_CLOCK_FREE, XM_CLOCK_STEP, XM_CLOCK_FUNCTION, XM_CLOCK_KEY,
       XM_CLOCK_COUNT };
enum { XM_EVENT_NONE, XM_EVENT_KEY, XM_EVENT_STEP, XM_EVENT_A_RISE,
       XM_EVENT_A_FALL, XM_EVENT_B_RISE, XM_EVENT_B_FALL, XM_EVENT_COUNT };
#define XM_EVENT_BIT(event) (1u << (event))
enum { XM_FIRE_A=1u, XM_FIRE_B=2u, XM_NEXT=4u, XM_RESET_A=8u, XM_RESET_B=16u };
enum { XM_LINK_NONE, XM_LINK_A, XM_LINK_B, XM_LINK_RANDOM, XM_LINK_COUNT };

/* Offsets only: the integration owner bounds and smooths base + offset.
 * INDEX is a fraction of 64 index units; RATIO is log2 octaves. Rates/times
 * are octave offsets (effective = base * 2^offset), never seconds/Hz deltas.
 * AMPLITUDE is a normalized offset to base unity; the integration owner applies
 * clamp(1 + offset,0,1) before the tank. FUNCTION A/B route outputs are centered
 * -1..1 for direct routes and mixer inputs; negative depth reverses direction.
 * Raw function snapshots remain 0..1 for gates and slew followers. ADSRs remain unipolar.
 * FILTER_CUTOFF is log2 octaves above an absolute Hz base; FILTER_RESONANCE
 * is an additive0..1 control offset. Envelope inputs are unipolar, finite0..1
 * snapshots (AMP plus three custom contours); masks clear inactive sources.
 * FOLD is an additive normalized offset; parent bounds base+offset to0..1.
 * CORE/WET/ECHO offsets are octaves. CELLS offsets are physical cells. The
 * parent consumes these through a bounded foreground transaction, never as
 * audio-rate clock/count changes; stored base settings remain separate.
 */
enum {
    XM_DST_NONE, XM_DST_PITCH, XM_DST_INDEX, XM_DST_RATIO, XM_DST_DRIFT,
    XM_DST_WET_MIX, XM_DST_LFO_A_RATE, XM_DST_LFO_B_RATE, XM_DST_RANDOM_RATE,
    XM_DST_RANDOM_CENTRE, XM_DST_RANDOM_SPREAD, XM_DST_RANDOM_CORRELATE,
    XM_DST_RANDOM_BIAS, XM_DST_RANDOM_RISE, XM_DST_RANDOM_FALL,
    XM_DST_A_RISE, XM_DST_A_FALL, XM_DST_B_RISE, XM_DST_B_FALL,
    XM_DST_MIX_A_GAIN_A, XM_DST_MIX_A_GAIN_B, XM_DST_MIX_A_OFFSET,
    XM_DST_MIX_B_GAIN_A, XM_DST_MIX_B_GAIN_B, XM_DST_MIX_B_OFFSET,
    XM_DST_AMPLITUDE, XM_DST_BITS, XM_DST_FILTER_CUTOFF, XM_DST_FILTER_RESONANCE, XM_DST_CORE_RATE, XM_DST_WET_RATE, XM_DST_ECHO_TIME, XM_DST_CELLS, XM_DST_FOLD, XM_DESTINATION_COUNT
};
enum { XM_UNIT_NONE, XM_UNIT_SEMITONES, XM_UNIT_NORMALIZED, XM_UNIT_OCTAVES, XM_UNIT_BITS, XM_UNIT_CELLS };
typedef struct { float offset_limit; uint8_t unit, reserved[3]; } XenolithModDestination;

typedef struct {
    float rate_hz, phase;
    uint8_t shape, enabled, reset_event, reserved;
} XenolithModLfo;
typedef struct {
    float rate_hz, centre, spread, correlate, bias, weight;
    float rise_s, fall_s, walk_spread;
    uint8_t levels, clock, hold, enabled;
} XenolithModRandom;
typedef struct {
    float rise_s, fall_s, curve;
    uint8_t mode, input, trigger_event, enabled;
} XenolithModFunction;
typedef struct {
    float gain_a, gain_b, offset;
    uint8_t input_a, input_b, mode, enabled;
} XenolithModMix;
typedef struct { uint8_t source, destination, enabled, reserved; float depth; } XenolithModRoute;
typedef struct { uint8_t event, target, enabled, reserved; } XenolithModLink;
typedef struct {
    uint8_t enabled, reserved[3];
    XenolithModLfo lfo[2];
    XenolithModRandom random;
    XenolithModFunction function[2];
    XenolithModMix mix[2];
    XenolithModRoute route[XENOLITH_MOD_ROUTES];
    XenolithModLink link[XENOLITH_MOD_LINKS];
} XenolithModConfig;

/* `events` accepts KEY/STEP edges, never held polls. Manual actions are one
 * sample pulses. KEY source is bounded -1..1; GATE mode with input NONE uses
 * gate, otherwise the previous source snapshot > .5. SLEW uses input 0..1.
 * sequence_mask bits0..3 select external16-step lane values. Active sources
 * publish finite0..1 at control ticks; inactive/nonfinite values publish0.
 * Unknown mask bits are ignored. No sequence or persisted state is added here.
 */
typedef struct {
    float key; uint32_t events, actions; uint8_t gate, reserved[3];
    float sequence[4];
    uint32_t sequence_mask;
    float envelope[4];
    uint32_t envelope_mask;
} XenolithModInput;
typedef struct {
    XenolithModConfig config;
    float source[XM_SOURCE_COUNT], offset[XM_DESTINATION_COUNT];
    uint32_t random_state, random_phase, lfo_phase[2], lfo_increment[2];
    uint32_t random_increment, smooth_phase, smooth_rise, smooth_fall;
    uint32_t function_phase[2], function_rise[2], function_fall[2];
    float function_input[2];
    uint32_t delayed_events[XENOLITH_MOD_QUANTUM], sample, quantum_position;
    float random_step, random_walk, random_centre, random_spread;
    float random_correlate, random_bias;
    uint8_t function_stage[2], reserved[2];
    XenolithRandomMemory random_memory;
} XenolithMod;

void xenolith_mod_defaults(XenolithModConfig *config);
/* Strict finite/range/enum/reserved-byte validation, including disabled fields.
 * No storage ABI or version is implied: persistence must encode explicitly.
 */
int xenolith_mod_validate(const XenolithModConfig *config);
void xenolith_mod_init(XenolithMod *state, uint32_t seed);
/* All-or-nothing; invalid config leaves state untouched. Audio owner only.
 * Keeps phases/RNG/envelopes on edits. Disable clears public offsets/sources
 * and queued links immediately, then freezes private generator time.
 * Enabled edits never restart the quantum; derived values prepare next tick.
 */
int xenolith_mod_apply(XenolithMod *state, const XenolithModConfig *config);
/* Advance exactly one 44.1-kHz output sample. NULL input means all zeros.
 * Returns one-sample END event bits; link effects arrive exactly 32 calls
 * later. Routes/mixers use previous snapshots every 32 samples. No recursion.
 * No fastmath/transcendental operation runs outside that control tick.
 */
int xenolith_mod_random_apply(XenolithMod *state,const XenolithRandomMemoryConfig *config);
uint32_t xenolith_mod_advance(XenolithMod *state, const XenolithModInput *input);
/* Unknown destinations safely report no unit/no range. */
XenolithModDestination xenolith_mod_destination(unsigned destination);
/* Routing/mixer/display projection only: FUNCTION A/B raw 0..1 becomes -1..1.
 * Random/legacy LFO/key/mix sources already have bipolar values; SEQ/ADSR
 * sources retain 0..1. Disabled generators must be excluded by the caller.
 * Invalid IDs/nonfinite samples return zero. Does not advance any state.
 */
float xenolith_mod_route_value(unsigned source_id, float raw_value);

#endif
