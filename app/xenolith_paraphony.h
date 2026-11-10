/* SPDX-License-Identifier: MPL-2.0 */
#ifndef XX29_XENOLITH_PARAPHONY_H
#define XX29_XENOLITH_PARAPHONY_H
#include <stdint.h>

#define XENOLITH_PARAPHONY_VOICES 8u
#define XENOLITH_PARAPHONY_KEYS 27u
#define XENOLITH_PARAPHONY_CELLS 64u
#define XENOLITH_PARAPHONY_EMPTY UINT8_C(255)

typedef struct {
    uint32_t initialized;
    uint8_t slot_key[XENOLITH_PARAPHONY_VOICES];
} XenolithParaphonyState;

typedef struct {
    float frequency[XENOLITH_PARAPHONY_CELLS];
    uint8_t owner[XENOLITH_PARAPHONY_CELLS]; /* Physical key, or EMPTY. */
    uint32_t mask_lo, mask_hi;
    uint32_t requested_voices, effective_voices, active_voices;
} XenolithParaphonyOutput;

/* A zero-initialized state is valid. Reset only at an ownership boundary,
 * not at every key event: surviving notes retain their reserved cell groups. */
void xenolith_paraphony_reset(XenolithParaphonyState *state);

/* Bounded control-rate allocation; no heap, I/O, transcendental math or RNG.
 * held_order contains unique physical keys, oldest first. pitches[key] holds
 * that key's unmodulated pitch; only held entries are read. Capacities are
 * 1..8 requested voices and 1..64 cells. Effective capacity is min(request,
 * cells), while the request itself is never overwritten. Newest keys win
 * when more keys are held than the effective capacity.
 *
 * Cell i belongs to slot i % requested_voices. Unoccupied slots are silent;
 * adding a cell does not move existing cells to another note. Each slot has
 * a fixed compact detune sequence, scaled by spread0..2 (default1, roughly
 * +/-3.5 cents). Cell pitches follow pitches[key] / reference_pitch times
 * effective_pitch, preserving chord intervals under global pitch changes.
 * Inputs are20..8000Hz; final detuned frequencies clamp to the same limits.
 *
 * An empty held set returns an empty mask. The application must retain the
 * last sounding output through its shared ADSR release if a release tail is
 * wanted. MONO should bypass this allocator to preserve its existing tuning.
 * This allocator does not implement independent voice envelopes, sequencer
 * chords, ANS ownership, or graph partitioning. The application admits it only
 * for GLOBAL TONIC plus PARALLEL/SELF, and owns those mode checks.
 *
 * Invalid input returns0 without changing state or output. */
int xenolith_paraphony_allocate(XenolithParaphonyState *state,
    const uint8_t *held_order, const float *pitches, uint32_t held_count,
    uint32_t cells, uint32_t requested_voices, float reference_pitch,
    float effective_pitch, float spread, XenolithParaphonyOutput *output);

#endif
