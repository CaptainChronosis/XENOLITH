/* SPDX-License-Identifier: MPL-2.0
 * XX29 numbered performance bank. Version25 appends optional coupling controls; prior records migrate explicitly.
 * Flash I/O, UI, and audio ownership remain the application's responsibility.
 */
#ifndef XX29_XENOLITH_PATCHES_H
#define XX29_XENOLITH_PATCHES_H
#include <stdint.h>
#include <stddef.h>
#include "xenolith_modulation.h"
#include "xenolith_sequence.h"
#include "xenolith_keyboard.h"
#include "xenolith_amplifier.h"
#include "xenolith_core.h"
#include "xenolith_filter.h"
#include "xenolith_envelope_bank.h"
#include "xenolith_random_memory.h"
#include "xenolith_dispersion.h"
#include "xenolith_governor.h"
#include "xenolith_performance.h"
#include "xenolith_graph.h"
#include "xenolith_flavour.h"
#include "xenolith_coupling.h"
#define XENOLITH_PATCH_COUNT 16u
#define XENOLITH_PATCH_MAGIC 0x58504231u
#define XENOLITH_PATCH_VERSION 25u
typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells, quality, clock_divider;
    int32_t trim_db;
} XenolithPatchV1;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells, quality, clock_divider;
    int32_t trim_db;
    float crush_rate;
    uint32_t crush_bits, crush_quantizer, crush_reconstruction;
} XenolithPatchV2;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; }; /* v3 must be zero */
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
} XenolithPatchV3;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; }; /* v4 must be zero */
    uint32_t clock_divider; /* v4 reserved zero: wet_rate is authoritative */
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
} XenolithPatchV4;

/* The first88 bytes retain the exact v4 sound prefix. The appended objects
 * are explicitly versioned by the bank and sequence image headers, never
 * silently interpreted as an older record. No live transport/RNG state. */
typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
} XenolithPatchV5;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
} XenolithPatchV6;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
} XenolithPatchV7;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
} XenolithPatchV8;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
} XenolithPatchV9;
/* Version10 retains the exact Version9 byte layout. Its sequence image is V2,
 * with note_mode replacing the formerly zero reserved configuration word.
 * Version11 retains these bytes with sequence imageV3, explicit EXP targets,
 * and a120ST base-depth ceiling only in EXP mode. */
typedef XenolithPatchV9 XenolithPatchV10;
typedef XenolithPatchV10 XenolithPatchV11;
typedef XenolithPatchV11 XenolithPatchV12;
typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
} XenolithPatchV13;
typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
} XenolithPatchV14;
typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
} XenolithPatchV15;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
} XenolithPatchV16;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
} XenolithPatchV17;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImageV4 sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
} XenolithPatchV18;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
} XenolithPatchV19;

typedef struct {
    float onset, span, recovery_ms, depth;
} XenolithGovernorConfigV20;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfigV20 governor;
} XenolithPatchV20;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfig governor;
} XenolithPatchV21;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfig governor;
    XenolithPerformanceConfig performance;
} XenolithPatchV22;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfig governor;
    XenolithPerformanceConfig performance;
    XenolithGraphConfig graph;
} XenolithPatchV23;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfig governor;
    XenolithPerformanceConfig performance;
    XenolithGraphConfig graph;
    XenolithFlavourConfig flavour;
} XenolithPatchV24;

typedef struct {
    float pitch_hz, index, ratio, drift;
    float attack_ms, decay_ms, sustain, release_ms;
    float echo_seconds, absorb, tail_seconds, mix;
    uint32_t cells;
    union { uint32_t reserved; uint32_t quality; };
    uint32_t clock_divider;
    int32_t trim_db;
    union { float core_rate; float crush_rate; };
    union { uint32_t fractional_bits; uint32_t crush_bits; };
    union { uint32_t quantizer; uint32_t crush_quantizer; };
    union { uint32_t reconstruction; uint32_t crush_reconstruction; };
    float wet_rate;
    uint32_t wet_reconstruction;
    XenolithModConfig modulation;
    XenolithSequenceImage sequence;
    XenolithKeyboardConfig keyboard;
    XenolithAmplifierConfig amplifier;
    XenolithTopologyConfig topology;
    float diffusion, width;
    float damp_hz;
    uint32_t damp_model;
    XenolithAmplifierShapeConfig amplifier_shape;
    XenolithFilterConfig filter;
    XenolithEnvelopeBankConfig envelopes;
    XenolithRandomMemoryConfig random_memory;
    XenolithDispersionConfig dispersion;
    XenolithFilterCircuitConfig circuit;
    XenolithCalculationConfig calculation;
    XenolithFolderConfig folder;
    float amplifier_feedback;
    XenolithGovernorConfig governor;
    XenolithPerformanceConfig performance;
    XenolithGraphConfig graph;
    XenolithFlavourConfig flavour;
    XenolithCouplingConfig coupling;
} XenolithPatch;
typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV1 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV1;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV2 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV2;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV3 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV3;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV4 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV4;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV5 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV5;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV6 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV6;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV7 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV7;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV8 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV8;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV9 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV9;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV10 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV10;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV11 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV11;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV12 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV12;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV13 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV13;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV14 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV14;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV15 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV15;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV16 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV16;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV17 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV17;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV18 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV18;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV19 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV19;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV20 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV20;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV21 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV21;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV22 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV22;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV23 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV23;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatchV24 slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBankV24;

typedef struct {
    uint32_t magic, version, bytes, occupied;
    XenolithPatch slots[XENOLITH_PATCH_COUNT];
} XenolithPatchBank;
_Static_assert(offsetof(XenolithPatch,damp_hz)==1232 && offsetof(XenolithPatch,amplifier_shape)==1240, "Version13 append changed");
_Static_assert(sizeof(XenolithPatchV12)==900 && sizeof(XenolithPatchBankV12)==14416, "Version12 layout changed");
_Static_assert(sizeof(XenolithPatchV13)==924 && sizeof(XenolithPatchBankV13)==14800,"V13 frozen ABI");
_Static_assert(offsetof(XenolithPatch,filter)==1256 && offsetof(XenolithPatch,envelopes)==1276,"V14 append ABI");
_Static_assert(sizeof(XenolithPatchV14)==1040 && sizeof(XenolithPatchBankV14)==16656,"V14 frozen ABI");
_Static_assert(offsetof(XenolithPatch,random_memory)==1372 && offsetof(XenolithPatch,dispersion)==1380,"V15 append ABI");
_Static_assert(sizeof(XenolithPatchV15)==1060 && sizeof(XenolithPatchBankV15)==16976,"V15 frozen ABI");
_Static_assert(offsetof(XenolithPatch,circuit)==1392 && offsetof(XenolithPatch,calculation)==1408,"V16 append ABI");
_Static_assert(sizeof(float)==4, "Patch format requires 32-bit floats");
_Static_assert(sizeof(XenolithPatchV16)==1092 && sizeof(XenolithPatchBankV16)==17488,"V16 frozen ABI");
_Static_assert(offsetof(XenolithPatch,folder)==1424,"V17 append ABI");
_Static_assert(sizeof(XenolithPatchV17)==1108 && sizeof(XenolithPatchBankV17)==17744,"V17 frozen ABI");
_Static_assert(offsetof(XenolithPatch,amplifier_feedback)==1440,"V18 append ABI");
_Static_assert(sizeof(XenolithPatchV18)==1112 && sizeof(XenolithPatchBankV18)==17808,"V18 frozen ABI");
_Static_assert(sizeof(XenolithPatchV19)==1444 && sizeof(XenolithPatchBankV19)==23120,"V19 frozen ABI");
_Static_assert(sizeof(XenolithGovernorConfigV20)==16 && sizeof(XenolithPatchV20)==1460 && sizeof(XenolithPatchBankV20)==23376,"V20 frozen ABI");
_Static_assert(offsetof(XenolithPatch,governor)==1444,"V21 governor offset ABI");
_Static_assert(sizeof(XenolithPatchV21)==1496 && sizeof(XenolithPatchBankV21)==23952,"V21 frozen ABI");
_Static_assert(offsetof(XenolithPatch,performance)==1496,"V22 append ABI");
_Static_assert(sizeof(XenolithPatchV22)==1548,"V22 frozen ABI");
_Static_assert(offsetof(XenolithPatch,graph)==1548,"V23 graph append ABI");
_Static_assert(sizeof(XenolithPatchV23)==1688 && sizeof(XenolithPatchBankV23)==27024,"V23 frozen ABI");
_Static_assert(offsetof(XenolithPatch,flavour)==1688,"V24 flavour append ABI");
_Static_assert(sizeof(XenolithPatchV24)==1696 && sizeof(XenolithPatchBankV24)==27152,"V24 frozen ABI");
_Static_assert(offsetof(XenolithPatch,coupling)==1696,"V25 coupling append ABI");
_Static_assert(sizeof(XenolithPatch)==1712, "Patch record layout changed: bump version");
_Static_assert(sizeof(XenolithPatchBank)==27408, "Patch bank layout changed: bump version");
_Static_assert(sizeof(XenolithPatchV9)==900 && sizeof(XenolithPatchBankV9)==14416, "Version9 layout changed");
_Static_assert(sizeof(XenolithPatchV10)==900 && sizeof(XenolithPatchBankV10)==14416, "Version10 layout changed");
_Static_assert(sizeof(XenolithPatchV11)==900 && sizeof(XenolithPatchBankV11)==14416, "Version11 layout changed");
_Static_assert(sizeof(XenolithPatchV8)==892 && sizeof(XenolithPatchBankV8)==14288, "Version8 layout changed");
_Static_assert(offsetof(XenolithPatch,diffusion)==1224 && offsetof(XenolithPatch,width)==1228, "Version9 append offsets changed");
_Static_assert(sizeof(XenolithPatchV7)==876 && sizeof(XenolithPatchBankV7)==14032, "Version7 layout changed");
_Static_assert(sizeof(XenolithPatchV6)==864 && sizeof(XenolithPatchBankV6)==13840, "Version6 layout changed");
_Static_assert(sizeof(XenolithPatchV5)==832 && sizeof(XenolithPatchBankV5)==13328, "Version5 layout changed");
_Static_assert(sizeof(XenolithPatchV4)==88 && sizeof(XenolithPatchBankV4)==1424, "Version4 layout changed");
_Static_assert(offsetof(XenolithPatch,modulation)==88 && offsetof(XenolithPatch,sequence)==316,
               "Version5 append offsets changed");
_Static_assert(offsetof(XenolithPatch,keyboard)==1164, "Version6 append offset changed");
_Static_assert(offsetof(XenolithPatch,amplifier)==1196, "Version7 append offset changed");
_Static_assert(offsetof(XenolithPatch,topology)==1208, "Version8 append offset changed");
_Static_assert(sizeof(XenolithPatchV1)==64 && sizeof(XenolithPatchBankV1)==1040, "Legacy layout changed");
_Static_assert(sizeof(XenolithPatchV2)==80 && sizeof(XenolithPatchBankV2)==1296, "Version2 layout changed");
_Static_assert(sizeof(XenolithPatchV3)==80 && sizeof(XenolithPatchBankV3)==1296, "Version3 layout changed");
/* Strict exact-size/version v1..v25 decoder. Output and input must be4-byte aligned and not overlap.
 * Invalid data leaves the destination untouched. Legacy fields and occupied
 * slots survive. Native/high/ECO migrate with precisionOFF; CRUSH preserves
 * its controls (v1 receives11025Hz/Q7/TRUNC/ZOH). Decoding never writes flash;
 * old divider1..32 migrates to exactly44100.0f/divider, wet ZOH, and the old
 * clock_divider word becomes reserved zero. Only explicit SAVE commits the current version.
 * Same-sized v2/v3 dispatch by header. Current wet_rate1378.125..44100Hz,
 * wet_reconstruction0..9 (five paired structure/reconstruction families);
 * current precisionOFF or1..24 fractional bits. Legacyv1/v2/v3
 * retain their original precision bounds. Both reserved words must be zero.
 * Old v1..v4 receive disabled MOD/SEQ; v1..v5 receive ordinary chromatic keys.
 * V1..v6 receive clean amplifier defaults. V5/V6 retain old enum boundaries:
 * MOD sources0..10/destinations0..24, SEQ destinations0..7 and V6 keys0..7.
 * V7 keeps MOD sources0..14/destinations0..25 and SEQ/key destinations0..8.
 * V1..V7 receive PARALLEL/LINEAR, link.5, feedback0 topology defaults.
 * V8 freezes growth0..3/mode0..1; v1..v8 receive diffusion.65/width1.
 * V1..V8 never accept Q1. V9 adds MOD destination26, SEQ destinations9/10,
 * and keyboard destination9 within their current module validation bounds.
 * V5..V9 require sequence imageV1 and the old config reserved word zero;
 * they migrate in RAM to imageV3/note_modeADSR. V10 requires imageV2 and
 * migrates toV3 preserving ADSR/CONTINUOUS. V10 and earlier retain baseindex64,
 * old keyboard destinations<=9 and sequence destinations<=10. V11 adds
 * KEY EXP_DEPTH10 and SEQ EXP_DEPTH11 with physical0..120ST; legacy INDEX
 * lanes remain0..64. Old values are not rescaled. Expanded final EXP headroom
 * may change modulation that used to saturate at64; other modes remain64.
 * V11 ECHO requests span.002..32s; preparation caps them to wet-clock buffer
 * capacity. V1..V10 retain.02..1s. New WIDTH default.70; legacyV1..V8 append1,
 * while V9/V10 preserve their actual saved width.
 * V12 keeps the900-byte layout; V11 and all older formats freeze the wet
 * mode word to0/1. Old values migrate unchanged with no automatic writes.
 * V13 appends requestedDAMPHz/model and amplifier_shape; old versions append
 * DAMP800/model0 and CUBIC/HP10/LPoff/GAIN0, while fresh defaults use model1.
 * V12 freezes wet modes0..3; V13 accepts paired modes0..9.
 * V14 appends filter20B and three32B envelope routes; prior versions receive
 * OFF/LP/2POLE/8000Hz/Q.5 and disabled zero-depth FILTER/INDEX/TONIC contours.
 * Sequence imageV4 appends FILTER_CUTOFF12 and FILTER_RESONANCE13; keyboard
 * appends11/12, MOD appends27/28 and four envelope sources15..18. V13 and
 * older preserve their explicit source/destination bounds and image versions.
 * V15 appends random_memory8B and dispersion12B; all prior versions get
 * CURRENT/length0/renew1 and amountA0/amountB0/scale2. V14 freezes MOD
 * sources0..18/destinations0..28; V15 appends destinations29..32.
 * V16 appends circuit16B/calculation16B and FOLD destination33. All older
 * versions receive nominal circuit and11bit/LINEAR/BOTH/harmonic0 calculation
 * defaults; V15 freezes MOD destinations0..32 and ENV destinations<=28.
 * V17 appends folder16B; older patches receive amount0/bias0/mix1/stages1,
 * preserving their prior VCA shape/filter settings.
 * V18 appends amplifier_feedback0..1; all older versions receive0, preserving
 * their curve, gain, tone and independent folder settings.
 * V19 uses sequence imageV5:64 notes,64 CV1 values and three16-step CV lanes;
 * old sequence images migrate fieldwise with their original playback division.
 * V20 appends onset/span/recovery/depth; V21 expands the governor controls.
 * Older patches append current governor defaults; V20 retains its saved controls.
 * V22 appends performance controls; older records get DEFAULT/NORMAL responses,
 * SWEEP filter response, one voice, zero glide, spread1 and ratio snapOFF.
 * Current diffusion0...85/width0..1 and every appended configuration are checked. */
int xenolith_patches_decode(XenolithPatchBank *out,const void *data,uint32_t bytes);
/* Boot-only descending migration; validates the entire bank before writes.
 * Scratch must be a distinct aligned current record. No second bank required. */
int xenolith_patches_decode_inplace(XenolithPatchBank *bank,uint32_t bytes,XenolithPatch *scratch);
/* Read one saved slot without materializing a full current bank. Both entry
 * points validate the complete supplied bank before changing any output. */
int xenolith_patches_bank_info(const void *data,uint32_t bytes,uint32_t *occupied);
int xenolith_patches_decode_slot(XenolithPatch *out,const void *data,uint32_t bytes,uint32_t slot);
/* Public single-slot v5 migration for the older860-byte storage envelope.
 * Strict validation precedes output; distinct aligned objects required. */
int xenolith_patch_v5_valid(const XenolithPatchV5 *patch);
int xenolith_patch_from_v5(XenolithPatch *out,const XenolithPatchV5 *patch);
int xenolith_patch_v6_valid(const XenolithPatchV6 *patch);
int xenolith_patch_from_v6(XenolithPatch *out,const XenolithPatchV6 *patch);
int xenolith_patch_v7_valid(const XenolithPatchV7 *patch);
int xenolith_patch_from_v7(XenolithPatch *out,const XenolithPatchV7 *patch);
int xenolith_patch_v8_valid(const XenolithPatchV8 *patch);
int xenolith_patch_from_v8(XenolithPatch *out,const XenolithPatchV8 *patch);
int xenolith_patch_v9_valid(const XenolithPatchV9 *patch);
int xenolith_patch_from_v9(XenolithPatch *out,const XenolithPatchV9 *patch);
int xenolith_patch_v19_valid(const XenolithPatchV19 *patch);
int xenolith_patch_from_v19(XenolithPatch *out,const XenolithPatchV19 *patch);
int xenolith_patch_v22_valid(const XenolithPatchV22 *patch);
int xenolith_patch_from_v22(XenolithPatch *out,const XenolithPatchV22 *patch);
int xenolith_patch_v24_valid(const XenolithPatchV24 *patch);
int xenolith_patch_from_v24(XenolithPatch *out,const XenolithPatchV24 *patch);
int xenolith_patch_v23_valid(const XenolithPatchV23 *patch);
int xenolith_patch_from_v23(XenolithPatch *out,const XenolithPatchV23 *patch);
int xenolith_patch_v21_valid(const XenolithPatchV21 *patch);
int xenolith_patch_from_v21(XenolithPatch *out,const XenolithPatchV21 *patch);
int xenolith_patch_v20_valid(const XenolithPatchV20 *patch);
int xenolith_patch_from_v20(XenolithPatch *out,const XenolithPatchV20 *patch);
int xenolith_patch_v18_valid(const XenolithPatchV18 *patch);
int xenolith_patch_from_v18(XenolithPatch *out,const XenolithPatchV18 *patch);
int xenolith_patch_v17_valid(const XenolithPatchV17 *patch);
int xenolith_patch_from_v17(XenolithPatch *out,const XenolithPatchV17 *patch);
int xenolith_patch_v16_valid(const XenolithPatchV16 *patch);
int xenolith_patch_from_v16(XenolithPatch *out,const XenolithPatchV16 *patch);
int xenolith_patch_v15_valid(const XenolithPatchV15 *patch);
int xenolith_patch_from_v15(XenolithPatch *out,const XenolithPatchV15 *patch);
int xenolith_patch_v14_valid(const XenolithPatchV14 *patch);
int xenolith_patch_from_v14(XenolithPatch *out,const XenolithPatchV14 *patch);
int xenolith_patch_v13_valid(const XenolithPatchV13 *patch);
int xenolith_patch_from_v13(XenolithPatch *out,const XenolithPatchV13 *patch);
int xenolith_patch_v12_valid(const XenolithPatchV12 *patch);
int xenolith_patch_from_v12(XenolithPatch *out,const XenolithPatchV12 *patch);
int xenolith_patch_v11_valid(const XenolithPatchV11 *patch);
int xenolith_patch_from_v11(XenolithPatch *out,const XenolithPatchV11 *patch);
int xenolith_patch_v10_valid(const XenolithPatchV10 *patch);
int xenolith_patch_from_v10(XenolithPatch *out,const XenolithPatchV10 *patch);
/* Completely initializes a valid default record, including disabled MOD/SEQ. */
void xenolith_patch_defaults(XenolithPatch *patch);
void xenolith_patches_init(XenolithPatchBank *bank);
int xenolith_patch_valid(const XenolithPatch *patch);
/* Exact byte count, own magic/version, mask and all occupied records checked.
 * Do not silently repair a foreign or corrupt payload into an audible patch. */
int xenolith_patches_valid(const XenolithPatchBank *bank, uint32_t bytes);
int xenolith_patches_get(const XenolithPatchBank *bank, uint32_t slot, XenolithPatch *out);
/* Apply to a candidate bank; commit to active RAM only after flash succeeds. */
int xenolith_patches_put(XenolithPatchBank *bank, uint32_t slot, const XenolithPatch *patch);
#endif

