/* SPDX-License-Identifier: MPL-2.0
 * XX29 output-sample-clocked64-note /16-step CV transport and recorder. */
#ifndef XX29_XENOLITH_SEQUENCE_H
#define XX29_XENOLITH_SEQUENCE_H
#include <stdint.h>

#define XENOLITH_SEQUENCE_STEPS 64u
#define XENOLITH_SEQUENCE_CV_STEPS 16u
#define XENOLITH_SEQUENCE_LANES 4u
#define XENOLITH_SEQUENCE_RATE 44100u
#define XENOLITH_SEQUENCE_MAGIC UINT32_C(0x58535131)
#define XENOLITH_SEQUENCE_VERSION 5u

/* Values0..3 preserve earlier patches' one-note/one-CV-step timing. The grouped
 * grid uses four note positions per BPM beat and16-step CV position. Its name
 * XSEQ_SIXTYFOURTH is a retained storage ID, not a musical note duration. */
enum { XSEQ_QUARTER, XSEQ_EIGHTH, XSEQ_SIXTEENTH, XSEQ_EIGHTH_TRIPLET, XSEQ_SIXTYFOURTH };
enum { XSEQ_QUANT_STEP, XSEQ_QUANT_TWO_STEPS, XSEQ_QUANT_FOUR_STEPS };
enum { XSEQ_REST, XSEQ_NOTE, XSEQ_TIE };
enum { XSEQ_TRIGGER_A=1u, XSEQ_TRIGGER_B=2u, XSEQ_TRIGGER_RANDOM=4u };
enum { XSEQ_DEST_PITCH, XSEQ_DEST_INDEX, XSEQ_DEST_RATIO, XSEQ_DEST_DRIFT,
       XSEQ_DEST_WET_MIX, XSEQ_DEST_CORE_RATE, XSEQ_DEST_WET_RATE, XSEQ_DEST_NONE,
       XSEQ_DEST_AMPLITUDE, XSEQ_DEST_ANS_CELL, XSEQ_DEST_BITS, XSEQ_DEST_EXP_DEPTH, XSEQ_DEST_FILTER_CUTOFF, XSEQ_DEST_FILTER_RESONANCE, XSEQ_DEST_COUNT };
/* ANS_CELL is a normalized0..1 selector. The parent rounds to the nearest
 * physical key0..26 and reuses keyboard cell-band selection; no note/octave
 * mapping or output-clock change belongs to this pure transport module. */
enum { XSEQ_RECORD_OFF, XSEQ_RECORD_STEP, XSEQ_RECORD_REPLACE, XSEQ_RECORD_OVERDUB };
enum { XSEQ_REC_IDLE, XSEQ_REC_ARMED, XSEQ_REC_ACTIVE };
enum { XSEQ_ADSR, XSEQ_CONTINUOUS };

typedef struct {
    uint8_t note, kind, gate_percent, triggers; /* MIDI0..127; gate1..99; trigger bits0..2 */
} XenolithSequenceStep;
typedef struct {
    uint8_t note, kind, gate_percent, triggers;
    uint32_t reserved;
} XenolithSequenceStepV4;
typedef struct {
    uint32_t destination, enabled, step_mask, reserved; /* only low16 mask bits */
    float rise_seconds, fall_seconds; /* 0=immediate; otherwise0..60s full normalized travel */
    float value[XENOLITH_SEQUENCE_CV_STEPS]; /* finite0..1 base-coordinate values */
} XenolithSequenceLane;
typedef struct {
    XenolithSequenceStep step[XENOLITH_SEQUENCE_STEPS];
    XenolithSequenceLane lane[XENOLITH_SEQUENCE_LANES];
    float cv0_extra[48]; /* CV1 steps16..63; other lanes retain16 values. */
    uint32_t cv0_extra_mask[2]; /* steps16..47,48..63; upper16 bits of second word zero */
} XenolithSequencePattern;
typedef struct {
    XenolithSequenceStepV4 step[16];
    XenolithSequenceLane lane[XENOLITH_SEQUENCE_LANES];
} XenolithSequencePatternV4;
typedef struct {
    uint32_t enabled, length, division;
    union { uint32_t note_mode; uint32_t reserved; }; /* V1 reserved=0; V2 ADSR0 / CONTINUOUS1 */
    float bpm; /* finite20..300 */
    uint32_t quantize; /* capture/playback grid:1,2 or4 subslots; STEP entry and BPM unchanged */
} XenolithSequenceConfig;
typedef struct {
    uint32_t enabled, length, division;
    union { uint32_t note_mode; uint32_t reserved; };
    float bpm;
} XenolithSequenceConfigV4;
/* Fixed32-bit little-endian target schema. Storage I/O/byte order belong to the
 * parent. Validate exact bytes/version; no automatic storage write on load. */
typedef struct {
    uint32_t magic, version, bytes, reserved;
    XenolithSequenceConfig config;
    XenolithSequencePattern pattern;
} XenolithSequenceImage;
typedef struct {
    uint32_t magic, version, bytes, reserved;
    XenolithSequenceConfigV4 config;
    XenolithSequencePatternV4 pattern;
} XenolithSequenceImageV4;
typedef struct {
    uint32_t sample, playhead, running, step_event, triggers;
    uint32_t note, gate, note_on, note_off;
    uint32_t lane_mask, record_state, record_mode;
    float lane[XENOLITH_SEQUENCE_LANES];
    uint32_t envelope_on; /* NOTE attacks: every NOTE in ADSR, first owned gate in CONTINUOUS */
} XenolithSequenceFrame;
typedef struct {
    XenolithSequenceImage image; /* ONLY the audio owner edits/captures this pattern */
    XenolithSequenceFrame frame;
    uint32_t sample_count, phase, phase_increment, gate_phase;
    uint32_t running, playhead, edit_step, pending_start, pending_off, gate, note;
    uint32_t record_mode, record_state, record_notes, record_lanes;
    uint32_t play_quantize, quant_gate_slots;
    uint32_t capture_note, capture_step, capture_valid;
    uint32_t capture_future, record_elapsed, record_length;
    uint32_t lane_valid_mask;
    uint32_t keys_down[4];
    float lane_value[XENOLITH_SEQUENCE_LANES], lane_target[XENOLITH_SEQUENCE_LANES];
    float lane_error[XENOLITH_SEQUENCE_LANES]; /* compensated addition for long slow ramps */
    float rise_step[XENOLITH_SEQUENCE_LANES], fall_step[XENOLITH_SEQUENCE_LANES];
    /* Runtime-only external transport; never part of the saved pattern. */
    uint32_t external_clock, external_wait, external_tick, external_steps;
    uint32_t external_elapsed, external_period, external_increment;
    uint32_t external_last_ms, external_have_ms;
} XenolithSequence;

_Static_assert(sizeof(float)==4, "Sequence schema requires32-bit floats");
_Static_assert(sizeof(XenolithSequenceStep)==4, "Sequence step ABI changed");
_Static_assert(sizeof(XenolithSequenceStepV4)==8, "Legacy sequence step ABI changed");
_Static_assert(sizeof(XenolithSequenceLane)==88, "Sequence lane ABI changed");
_Static_assert(sizeof(XenolithSequenceConfig)==24, "Sequence config ABI changed");
_Static_assert(sizeof(XenolithSequenceConfigV4)==20, "Legacy sequence config ABI changed");
_Static_assert(sizeof(XenolithSequenceImage)==848, "Sequence image ABI changed");
_Static_assert(sizeof(XenolithSequenceImageV4)==516, "Legacy sequence image ABI changed");

int xenolith_sequence_config_valid(const XenolithSequenceConfig *config);
int xenolith_sequence_pattern_valid(const XenolithSequencePattern *pattern);
int xenolith_sequence_image_valid(const XenolithSequenceImage *image, uint32_t bytes);
/* Explicit legacy validator/migration: V1 requires its reserved config word0.
 * Other parent schema restrictions (old destination enums) remain the parent's.
 * Invalid input leaves out untouched. Migration supports out==image. */
int xenolith_sequence_image_v1_valid(const XenolithSequenceImageV4 *image, uint32_t bytes);
int xenolith_sequence_image_from_v1(XenolithSequenceImage *out,const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_v2_valid(const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_from_v2(XenolithSequenceImage *out,const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_v3_valid(const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_from_v3(XenolithSequenceImage *out,const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_v4_valid(const XenolithSequenceImageV4 *image,uint32_t bytes);
int xenolith_sequence_image_from_v4(XenolithSequenceImage *out,const XenolithSequenceImageV4 *image,uint32_t bytes);
uint32_t xenolith_sequence_steps_per_beat(uint32_t division); /* valid grids only; invalid returns0 */
uint32_t xenolith_sequence_cv_length(const XenolithSequenceConfig *config);
uint32_t xenolith_sequence_lane_length(const XenolithSequenceConfig *config,uint32_t lane);
/* Playback position in the selected lane: CV1 keeps note resolution; CV2..4
 * advance once per four notes in grouped mode. Other grids retain legacy repeat. */
uint32_t xenolith_sequence_lane_position(const XenolithSequenceConfig *config,uint32_t lane,uint32_t note_position);
int xenolith_sequence_get_lane_step(const XenolithSequencePattern *pattern,uint32_t lane,
                                    uint32_t step,float *value,uint32_t *enabled);
/* Control-rate physical destination conversions; no transport state changes.
 * INDEX retains0..64; EXP_DEPTH is an explicit0..120-semitone destination.
 * NONE, nonfinite/out-of-domain input or null outputs fail without writes. */
int xenolith_sequence_value_bounds(uint32_t destination,float *low,float *high);
int xenolith_sequence_value_from_normalized(uint32_t destination,float value,float *physical);
int xenolith_sequence_value_to_normalized(uint32_t destination,float physical,float *value);
void xenolith_sequence_image_defaults(XenolithSequenceImage *image);
void xenolith_sequence_init(XenolithSequence *state); /* disabled,120BPM,16 beats x4 positions,length64,all REST */
/* All operations below are audio-owner calls at callback/sample boundaries.
 * UI must queue commands; it must never edit image or copy it during recording.
 * Host tests may own state exclusively. No internal locks, allocation or I/O. */
int xenolith_sequence_configure(XenolithSequence *state,const XenolithSequenceConfig *config);
int xenolith_sequence_edit_step(XenolithSequence *state,uint32_t step,const XenolithSequenceStep *value);
int xenolith_sequence_edit_lane(XenolithSequence *state,uint32_t lane,const XenolithSequenceLane *value);
int xenolith_sequence_edit_lane_step(XenolithSequence *state,uint32_t lane,uint32_t step,
                                     float value,uint32_t enabled);
int xenolith_sequence_select_step(XenolithSequence *state,uint32_t step);
int xenolith_sequence_apply_pattern(XenolithSequence *state,const XenolithSequencePattern *pattern);
int xenolith_sequence_play_toggle(XenolithSequence *state); /* enabled only; starts at0; exits STEP record; toggles stop */
void xenolith_sequence_stop(XenolithSequence *state);
void xenolith_sequence_panic(XenolithSequence *state); /* stops REC/transport, preserves image/take */
/* Audio-owner MIDI realtime input. FA/FB explicitly acquire24PPQN clock;
 * bareF8 never starts transport. FC and clock loss release only the sequence's
 * owned gate. Local PLAY returns to internal BPM. arrival_ms is the monotonic
 * foreground receive timestamp (uint32 wrap allowed), not a sample timestamp.
 * START waits for firstF8 at zero; CONTINUE resumes the retained clock position.
 * No Song Position Pointer or clock output is implemented. Returns1 if handled. */
int xenolith_sequence_midi_realtime(XenolithSequence *state,uint32_t status,uint32_t arrival_ms);
/* STEP: stopped fresh key-down writes edit cursor and advances it; parameter
 * capture writes selected cursor without advancing. REPLACE/OVERDUB: a stopped
 * transport starts a one-loop take on the first fresh
 * key. If already playing, arm at the next shared loop zero (legacy short CV
 * takes use their local zero). PLAY can also start an armed stopped take.
 * Live input selects the nearest division boundary; note releases retain gate
 * length. A take never wraps its final late input back onto its first step.
 * CV1 follows note length. On the grouped grid, CV2..4 span the same loop at quarter note-slot
 * resolution; legacy grids repeat every16 note steps. Recording quantization
 * also snaps playback without rewriting the recorded image or changing BPM.
 * Stopped CV-only STEP entry advances one CV value. */
int xenolith_sequence_record_arm(XenolithSequence *state,uint32_t mode,uint32_t notes,uint32_t lanes);
int xenolith_sequence_capture_key(XenolithSequence *state,uint32_t midi_note,uint32_t down);
/* One fresh key may capture notes and all requested armed CV lanes at the same
 * cursor. STEP advances once after accepted data; releases/held repeats do not.
 * Invalid requested armed values reject the complete event before any write. */
int xenolith_sequence_capture_key_values(XenolithSequence *state,uint32_t midi_note,uint32_t down,
                                         uint32_t lanes,const float value[XENOLITH_SEQUENCE_LANES]);
int xenolith_sequence_capture_parameter(XenolithSequence *state,uint32_t lane,float base_value);
/* One call per44.1kHz OUTPUT sample. Returned frame is internal and valid until
 * the next call. Triggers/step/note_on/off are one-sample events. No audio data.
 * REST is silent; orphan TIE is silent; lookahead TIE preserves the preceding
 * gate. ADSR mode attacks each NOTE. CONTINUOUS ignores gate_percent and holds
 * across NOTE/TIE until REST/stop; note_on still marks every pitch/key event,
 * while envelope_on marks only a new gate. Orphan TIE stays silent in both.
 * Stop/panic reports any owned note_off on the next sample. */
const XenolithSequenceFrame *xenolith_sequence_sample(XenolithSequence *state);
/* Snapshot only under audio ownership or externally acknowledged quiet ownership.
 * A stopped transport alone does not establish concurrency safety. Loading a
 * valid image resets runtime and keeps transport stopped, even if enabled.
 * Load also accepts a strict V1 image and migrates its default ADSR mode. */
int xenolith_sequence_snapshot(const XenolithSequence *state,XenolithSequenceImage *image);
int xenolith_sequence_load(XenolithSequence *state,const void *image,uint32_t bytes);
#endif
